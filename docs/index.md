# fprime-csp-libparam

`fprime-csp-libparam` is an external F Prime library for integrating the Space
Inventor `csh/2.10` libcsp and libparam release pair. CSP transports packets
between addressed nodes; libparam carries typed parameter requests over CSP;
and the consuming F Prime application decides when to make those requests and
how to expose their results as commands, telemetry, or events.

The library supplies the reusable middle layer: one CSP stack lifetime, an
abstract transport boundary, an optional Linux SocketCAN transport, service
binding, and libparam registry/server/client wrappers. It deliberately does not
define an FPP component, parameter schema, command protocol, topology, or
mission policy.

Start with:

- [Architecture and ownership boundaries](architecture.md) explains CSP,
  libparam, normal F Prime command/telemetry flow, threads, and lifetime.
- [F Prime project integration](integration.md)
- [Adding parameters and CSP services](extending.md)
- [Requirements and verification](requirements-and-verification.md)
- [Verification evidence](verification-evidence.md)

## Current implementation state

- Ordered teardown returns `ShutdownStatus`; failed router join or transport
  close remains retryable, and incomplete destruction is process-fatal.
- The two-phase SocketCAN close unregisters the interface before stopping its
  driver and does not repeat a completed phase on retry.
- `pullInteger` and `pushInteger` validate protocol version, descriptor node,
  type, storage, alignment, array index, timeout conversion, and write range
  before starting I/O.
- Runtime and Libparam host builds are supported on Linux and Darwin.
  SocketCAN remains Linux-only; embedded targets still require an upstream
  libparam port and target qualification.
- CI separates unit tests, sanitizers, a standalone F Prime consumer, coverage,
  static analysis, strict documentation, and downstream Linux `vcan` evidence.

The integration remains unqualified for flight. Applications own their CSP
addressing, parameter schema, authorization, topology, scheduling, and
hardware acceptance tests.
