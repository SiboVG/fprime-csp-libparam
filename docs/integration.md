# Integrating the library

This page documents how to integrate `fprime-csp-libparam` into your custom F Prime application.
It covers project discovery, target dependencies, the small adapter your
application owns, and the F Prime lifecycle and component thread from which to
call the library. Parameter definitions and custom CSP wire services are
covered separately in [Adding parameters and services](extending.md).

The library is an [external F Prime library][Develop an F Prime library], not a
ready-made deployment or topology. Your application still owns its FPP
commands, telemetry, events, component instances, CSP address and port plan,
parameter schema, and mission policy.

## 1. Supply the tested sources

The first supported combination is:

| Dependency | Tested revision |
|---|---|
| F Prime | [`35d4c0e881482adf372b8eee3eb56ad2dee74618`][Tested F Prime] |
| Space Inventor libcsp | [`csh/2.10` at `5a95e5c48826c3602bf4a34aa5d60e2af9e4a004`][libcsp release] |
| Space Inventor libparam | [`csh/2.10` at `d78da56e7cbc8b6be30d8890b6f01cdf80d0b9b9`][libparam release] |

Keep the two Space Inventor repositories clean so the build remains traceable
to the tested release pair. From this library's root, the bootstrap helper
creates and verifies the default `lib/libcsp` and `lib/libparam` layout:

```console
./scripts/bootstrap_dependencies.sh
```

Alternatively, keep those repositories in the consuming project and pass
their absolute paths as `FPRIMECSP_LIBCSP_SOURCE` and
`FPRIMECSP_LIBPARAM_SOURCE` when generating the build.

## 2. Register the library in the F Prime project

Add the standalone repository to `library_locations` in the custom
application's `settings.ini`. For example, if both F Prime and this library are
Git submodules under `lib/`:

```ini
[fprime]
framework_path: ./lib/fprime
project_root: .
library_locations: ./lib/fprime-csp-libparam
```

F Prime discovers the repository's `library.cmake` during generation. The
[F Prime settings guide][F Prime settings] describes `library_locations`; the
[library development guide][Develop an F Prime library] explains the manifest
mechanism. Do not include the repository through a user-specific absolute path
inside a component `CMakeLists.txt`.

SocketCAN is intentionally opt-in. On a Linux application that needs the
supplied adapter, generate with:

```console
fprime-util generate -DFPRIMECSP_ENABLE_SOCKETCAN=ON
```

If the consuming project owns the upstream checkouts, pass them explicitly:

```console
fprime-util generate \
  -DFPRIMECSP_ENABLE_SOCKETCAN=ON \
  -DFPRIMECSP_LIBCSP_SOURCE=/absolute/path/to/libcsp \
  -DFPRIMECSP_LIBPARAM_SOURCE=/absolute/path/to/libparam
```

The [F Prime CMake API][F Prime CMake API] and [external-library integration
guide][Integrate external libraries] provide background on targets and build
dependencies.

### Build-profile constraints

The bundled libparam configuration uses a dynamic descriptor list and needs
`<sys/queue.h>`; the pinned sources also contain Linux-oriented headers. Its
API does not depend on SocketCAN, but Linux is the supported target for this
bundled profile. Set `FPRIMECSP_LIBPARAM_CONFIG_DIR` to select another generated
`libparam.h`; an embedded application must still port and qualify the upstream
sources around that configuration.

When the consumer has already created a `csp` CMake target, it must expose
`CSP_USE_RDP=ON`. The router relies on RDP's bounded queue wake-up to observe a
stop request; CMake rejects an existing target that cannot establish that
contract.

## 3. Link the required modules

An F Prime component that uses libparam over Linux SocketCAN depends on all
three library targets:

```cmake
register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/CspAdapter.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/CspAdapter.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/CspAdapter.hpp"
    DEPENDS
        FprimeCsp_Runtime
        FprimeCsp_SocketCan
        FprimeCsp_Libparam
)
```

Leave out `FprimeCsp_SocketCan` when supplying another `Transport`
implementation. A CSP-only application can omit `FprimeCsp_Libparam` as well.
The external library provides CMake targets, not a component instance, so your
[F Prime topology][Build an F Prime topology] still instantiates and connects
the application-owned adapter.

## 4. Create a thin application adapter

The adapter owns the one process-global `Stack`, its transport, libparam
helpers, and all stable `param_t` descriptors and storage. The following
minimal skeleton shows the public API and required ordering; production code
should capture the `lastError()` of the subsystem whose call failed and
propagate it through application events and fault handling.

```cpp
#include "FprimeCsp/Libparam/Libparam.hpp"
#include "FprimeCsp/Runtime/Stack.hpp"
#include "FprimeCsp/SocketCan/SocketCanTransport.hpp"

class ProtocolAdapter final {
  public:
    explicit ProtocolAdapter(const FprimeCsp::SocketCanConfig& canConfig)
        : m_transport(canConfig), m_client(2) {}

    bool configure(const FprimeCsp::StackConfig& stackConfig,
                   param_t& descriptor,
                   bool serveParameters) {
        if (!m_stack.initialize(stackConfig, m_transport)) {
            m_lastError = m_stack.lastError();
            return false;
        }
        if (!m_registry.add(descriptor)) {
            m_lastError = m_registry.lastError();
            return false;
        }
        if (!m_stack.bindServiceHandler()) {
            m_lastError = m_stack.lastError();
            return false;
        }
        if (serveParameters && !m_server.bind(m_stack)) {
            m_lastError = m_server.lastError();
            return false;
        }
        m_lastError = 0;
        return true;
    }

    bool start() {
        if (!m_stack.startRouter()) {
            m_lastError = m_stack.lastError();
            return false;
        }
        return true;
    }

    bool stop() {
        m_stack.requestStop();
        if (!m_stack.join()) {
            m_lastError = m_stack.lastError();
            return false;
        }
        m_stack.shutdown();
        m_lastError = m_stack.lastError();
        return m_lastError == 0;
    }

    int ping(std::uint16_t peer, std::uint32_t timeoutMs) const {
        return m_stack.ping(peer, timeoutMs, 1);
    }

    int lastError() const {
        return m_lastError;
    }

    FprimeCsp::Client& client() {
        return m_client;
    }

  private:
    // Declaration order keeps the transport alive until after Stack teardown.
    FprimeCsp::SocketCanTransport m_transport;
    FprimeCsp::Stack m_stack;
    FprimeCsp::Registry m_registry;
    FprimeCsp::Server m_server;
    FprimeCsp::Client m_client;
    int m_lastError = 0;
};
```

`Stack::initialize()` retains a pointer to the supplied transport. The
transport must outlive `Stack::shutdown()`. A registered descriptor and every
name, node, timestamp, and value it references must remain stable until the
router has stopped and joined. The [extension guide](extending.md) shows a
complete `param_t` descriptor and the safe callback-to-component handoff.

## 5. Call it from an F Prime component

Network client calls such as `ping()`, `pullU32()`, and `pushU32()` are
**synchronous**. Invoke them from a context that may block for the configured
timeout, normally the custom adapter's active component thread. Never make a
blocking client call from the CSP router callback.

For example, an active component declaration such as
`async command PING(peer: U16)` generates a `PING_cmdHandler()`. The following
illustrative handler performs a CSP ping, publishes an F Prime telemetry value
or event, and returns exactly one command response:

```cpp
void CspAdapter::PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 peer) {
    const int elapsedMs = this->m_protocol.ping(peer, 1000);
    if (elapsedMs >= 0) {
        this->tlmWrite_LastPingMs(static_cast<U32>(elapsedMs));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // Stack::ping() returns the error; it does not update Stack::lastError().
    this->log_WARNING_HI_PingFailed(peer, elapsedMs);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
}
```

`PING_cmdHandler()`, `tlmWrite_LastPingMs()`, `log_WARNING_HI_PingFailed()`,
and `cmdResponse_out()` are generated from the component's FPP model; the exact
names follow your declarations. `Svc::CommandDispatcher` routes the ground
command to this handler. See [F Prime's autocoded functions][Autocoded
functions] for command, telemetry, event, and internal-interface APIs.

A long CSP timeout occupies that active component thread and can let later
async port messages accumulate. Choose timeout, queue depth, scheduling rate,
and overload telemetry together. For incoming CSP services, copy the request
in the router callback and use an FPP internal interface or bounded application
queue to execute it on the active component thread.

## 6. Map the lifetime to F Prime topology phases

Keep the protocol lifetime visible in the deployment instead of hiding it in a
component constructor or destructor. In an FPP-generated deployment these are
free topology functions and `Fpp.ToCpp.Phases` init specifiers, not methods on
an F Prime lifecycle class. The [topology guide][Build an F Prime topology] and
[FPP init-specifier reference][FPP init specifiers] explain the mechanism.

| Deployment phase | Integration work |
|---|---|
| Configure components | Construct the application adapter and transport, validate `StackConfig`, initialize the stack, create stable descriptors, and bind the standard, libparam, and application services. Do not emit output ports during this phase. |
| Start tasks | Start the CSP router before scheduling components that can issue client calls. |
| Run | Perform synchronous network calls from an active F Prime component thread. A CSP callback only validates and copies work into a bounded application queue. |
| Stop and free tasks | Stop timers and other producers, request router stop, and join the router task. |
| Terminal teardown | Call stack shutdown, close the transport, retain callback and descriptor state until no further libcsp/libparam call is possible, and exit. |

Do not try to restart the process-global stack in place. A deployment that
needs a fresh CSP lifetime should launch a new process.

## Linux SocketCAN notes

The library opens an interface that the host has already configured. CAN
bitrate, restart policy, queue length, interface state, cabling, and termination
belong to system or deployment setup. The `FprimeCsp_SocketCan` target is
optional so those Linux details do not leak into a target-specific transport.

A physical-loopback acceptance deployment should start two processes so each
endpoint has independent process-global state, exercise the complete CAN path,
and verify both CSP services and the intended libparam operations before the
integration is connected to flight hardware.

[Develop an F Prime library]: https://fprime.jpl.nasa.gov/latest/docs/how-to/develop-fprime-libraries/
[F Prime settings]: https://fprime.jpl.nasa.gov/latest/docs/user-manual/build-system/settings/
[F Prime CMake API]: https://fprime.jpl.nasa.gov/latest/docs/user-manual/build-system/cmake-api/
[Integrate external libraries]: https://fprime.jpl.nasa.gov/latest/docs/how-to/integrate-external-libraries/
[Build an F Prime topology]: https://fprime.jpl.nasa.gov/latest/docs/user-manual/framework/building-topology/
[Autocoded functions]: https://fprime.jpl.nasa.gov/latest/docs/user-manual/framework/autocoded-functions/
[FPP init specifiers]: https://nasa.github.io/fpp/fpp-users-guide.html#Defining-Component-Instances_Init-Specifiers
[Tested F Prime]: https://github.com/nasa/fprime/tree/35d4c0e881482adf372b8eee3eb56ad2dee74618
[libcsp release]: https://github.com/spaceinventor/libcsp/releases/tag/csh%2F2.10
[libparam release]: https://github.com/spaceinventor/libparam/releases/tag/csh%2F2.10
