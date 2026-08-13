# Adding parameters and incoming CSP services

The library provides protocol mechanics; the consuming application defines
what each parameter and packet means. Start with the wire contract and
execution boundary, then add only the smallest reusable API needed.

## Decide whether a change belongs in the library

Use this rule: put general CSP/libparam mechanics here and keep application
meaning in the consumer.

| Change | Owner |
|---|---|
| CSP lifecycle, transport abstraction, or reusable service-binding behavior | This library, primarily `FprimeCsp/Runtime/Stack.hpp` and its implementation |
| A typed libparam operation useful to several applications | This library, primarily `FprimeCsp/Libparam/Libparam.hpp` and its implementation |
| Linux SocketCAN behavior | This library's optional SocketCAN target |
| Node and port allocation, parameter table, payload schema, or access policy | Consuming application |
| FPP command, telemetry, event, component instance, scheduling, or topology | Consuming application |
| Retry, overload, safe-state, and hardware acceptance policy | Consuming application |

Do not add a second application-specific C bridge around libparam. Extend the
small typed C++ interface only when the operation is transport-independent and
has a clear reusable contract.

## Add a libparam parameter

A parameter is an application-owned `param_t` descriptor plus every object to
which that descriptor points. Registration stores pointers; it does not copy
the descriptor, strings, timestamp, or value.

### 1. Define the wire contract

Allocate and document:

- the owning CSP node and a unique parameter ID;
- the exact `PARAM_TYPE_*`, scalar or fixed-array shape, and byte-level type;
- the name, units, limits, persistence, and write policy; and
- the libparam protocol version expected by the peer.

Client and server descriptors must agree on node, ID, type, and shape. A
matching parameter name is not enough to establish compatibility.

### 2. Allocate stable storage

Keep the descriptor and all referenced state in an application object whose
lifetime extends beyond router shutdown. Fixed members or fixed-capacity arrays
are suitable; local variables and containers whose buffers may move are not.
For example, an application adapter can own:

```cpp
std::uint16_t m_parameterNode = 0;
std::uint32_t m_parameterValue = 0;
csp_timestamp_t m_parameterTimestamp{};
std::array<char, 32> m_parameterName{};
std::array<char, 16> m_parameterUnit{};
std::array<char, 96> m_parameterHelp{};
param_t m_parameter{};
```

Initialize every referenced field before registration:

```cpp
std::memset(&this->m_parameter, 0, sizeof(this->m_parameter));
this->m_parameter.node = &this->m_parameterNode;
this->m_parameter.name = this->m_parameterName.data();
this->m_parameter.unit = this->m_parameterUnit.data();
this->m_parameter.docstr = this->m_parameterHelp.data();
this->m_parameter.addr = &this->m_parameterValue;
this->m_parameter.timestamp = &this->m_parameterTimestamp;
this->m_parameter.id = PARAMETER_ID;
this->m_parameter.array_size = 1;
this->m_parameter.array_step = sizeof(std::uint32_t);
this->m_parameter.type = PARAM_TYPE_UINT32;
```

For a RAM-backed descriptor, `addr` and `array_step` must be naturally aligned
for the declared integer type (for example, use `alignof(std::uint32_t)` for
`PARAM_TYPE_UINT32`). VMEM descriptors may use byte-addressed offsets and
strides; their stride must still be large enough to hold one value.

The application must also select the appropriate upstream mask, callback, and
VMEM fields for its local or remote descriptor policy. Treat this example as
an ownership pattern, not a complete mission schema.

### 3. Register and bind before routing

Add each `(node, ID)` pair once through `FprimeCsp::Registry` and fail
configuration on an invalid or duplicate registration. Bind one
`FprimeCsp::Server` to the stack after registration and before
`Stack::startRouter()`:

```cpp
if (!this->m_registry.add(this->m_parameter)) {
    return false;
}
if (!this->m_server.bind(this->m_stack)) {
    return false;
}
```

The server services requests for all registered descriptors on CSP port 10.
Do not bind a separate port for every parameter.

### 4. Use a matching typed client operation

`FprimeCsp::Client::pullInteger` and `pushInteger` provide one checked interface
for the integer types normally needed by housekeeping and control parameters:

| Descriptor type | Accepted values for `pushInteger` |
|---|---:|
| `PARAM_TYPE_UINT8` | 0 to 255 |
| `PARAM_TYPE_INT8` | -128 to 127 |
| `PARAM_TYPE_UINT16` | 0 to 65,535 |
| `PARAM_TYPE_INT16` | -32,768 to 32,767 |
| `PARAM_TYPE_UINT32` | 0 to 4,294,967,295 |
| `PARAM_TYPE_INT32` | -2,147,483,648 to 2,147,483,647 |

Both methods take an explicit `std::uint8_t index`. Use `0` for a scalar; for
an array, use an index smaller than `array_size`. The pinned `csh/2.10`
serializer stores the offset in one signed byte, so the wrapper rejects indices
above `127` even if the descriptor declares a larger array. It also rejects a
descriptor whose node does not equal `peerNode`, whose storage references are
incomplete, whose stride is smaller than its integer type, or whose RAM address
or stride is not naturally aligned. The alignment rule applies only to RAM
descriptors; VMEM remains byte-addressed.

```cpp
std::int64_t busMillivolts = 0;
if (!this->m_client.pullInteger(
        this->m_busVoltage, this->m_pduNode, 1000, 0, busMillivolts)) {
    return false;
}

const std::uint8_t channel = 3;
std::int64_t channelMilliamps = 0;
if (!this->m_client.pullInteger(
        this->m_channelCurrent, this->m_pduNode, 1000, channel, channelMilliamps)) {
    return false;
}
```

The output of `pullInteger` is unchanged when validation or the transaction
fails. `pushInteger` checks the signed/unsigned value range before sending and
requests libparam's acknowledgement-with-pull behavior. Treat that response as
transport/protocol acknowledgement, not proof that attached hardware reached
the requested state. Read the authoritative status and telemetry parameters in
a separate transaction.

The original `pullU32` and `pushU32` methods remain available for existing
scalar `PARAM_TYPE_UINT32` consumers. New indexed code should use the checked
integer methods. For a type outside the table, first decide whether a narrow
wrapper belongs only in the application. Promote it into `FprimeCsp_Libparam`
when more than one consumer needs the same checked operation. Keep the protocol
version explicit and avoid exposing unchecked `void*` operations to a
component.

Registration alone does not make the value F Prime telemetry, enforce a range,
persist it, or authorize a write. Those decisions stay in the application.

## Handle an incoming CSP service

A custom incoming service is an application-defined protocol on an allocated
CSP port. It is neither an F Prime ground command nor a libparam update.

Define an ICD before writing the callback. Specify the destination port,
command identifiers, byte order, exact and maximum payload lengths, allowed
source nodes, CRC rules, response codes, timeouts, and queue-full behavior.
Avoid ports already assigned to libcsp standard services or libparam.

Bind the callback during configuration:

```cpp
if (!this->m_stack.bindCallback(serviceCallback, SERVICE_PORT)) {
    return false;
}
```

`Stack::bindCallback` is declared in `FprimeCsp/Runtime/Stack.hpp`.
`bindServiceHandler()` is for libcsp's standard service handler. With the
pinned libcsp build, a request intended for callback dispatch must be sent with
`CSP_O_CRC32`; a missing required CRC trailer is rejected before application
handling.

libcsp invokes the callback synchronously on the router task with a borrowed
`csp_packet_t` pointer. Keep the callback bounded:

1. validate the source node, command identifier, flags, and exact payload
   length;
2. decode fields explicitly and copy them into a fixed-size value record;
3. try to enqueue that record without blocking; and
4. send a bounded accepted/busy response or release the packet exactly once.

Never retain the packet pointer. A reply such as
`csp_sendto_reply(packet, packet, CSP_O_SAME)` consumes the packet; do not also
call `csp_buffer_free`. If no reply is sent, release the packet on every return
path. Keep ownership decisions beside the callback so success, malformed,
unauthorized, and queue-full paths are reviewable.

An immediate response should mean “validated and accepted for execution.” If a
peer needs the final result, include a request ID and define a separate
completion transaction in the ICD.

## Hand work to an F Prime component thread

The CSP callback should copy a valid request into an application-owned bounded
queue. Drain that queue from an **active component thread** or another explicit
F Prime scheduling context. State changes, output ports, telemetry, events, and
completion reports belong there, not on the router task.

If both an F Prime ground command and an incoming CSP request perform the same
action, factor the action into an ordinary application helper. Call that helper
from both active-thread paths. Do not call a generated F Prime command handler
with invented opcode or command-sequence values.

FPP remains the source of truth for F Prime commands, telemetry, and events.
A generated ground-command handler must emit one command response on every
path. An incoming CSP request has no F Prime command identity, so report its
acceptance or completion through the CSP protocol defined by the application.

Synchronous client calls such as `pullInteger` block the calling component thread
until success or timeout. Choose a dedicated component, queue depth, timeout,
and retry policy when that latency is unsuitable for other application work.

## Lifecycle and packet-ownership rules

- Construct one stack owner per process; libcsp and libparam state is global.
- Initialize the transport, stable descriptors, and callback targets before
  the router starts.
- Do not emit F Prime outputs during early topology configuration, when ports
  may not yet be connected.
- Keep every object referenced by a `param_t` stable until after the router has
  stopped and joined.
- Keep callbacks short, bounded, nonblocking, and allocation-free where
  practical.
- Copy validated data into application-owned storage; never queue or retain a
  borrowed `csp_packet_t` pointer.
- Transfer or release each packet exactly once. A reply consumes the packet;
  otherwise call the appropriate buffer-release function on every path.
- Stop work producers, request and join routing, then release callback targets,
  the transport, descriptors, and value storage.
- Exit without another libcsp/libparam call; start a new process rather than
  restarting the stack in place.

## Verification checklist

Before integrating a new parameter or service with hardware:

- test parameter IDs, nodes, types, shapes, stable storage, and duplicate
  registration;
- test malformed lengths, invalid values, unauthorized sources, CRC rejection,
  queue-full behavior, and every packet-ownership path;
- test the typed client operation against an independent CSP process;
- verify callbacks never block or call F Prime output ports;
- verify active-thread handlers publish the intended telemetry/events and that
  every F Prime ground-command path emits exactly one response;
- test start, bounded stop, join, and terminal teardown ordering;
- confirm protocol versions, ports, addressing, schema, and write permissions
  with the peer; and
- run the consuming application's transport and hardware acceptance procedure.
