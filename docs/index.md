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

The bundled dependency profile is intended for Linux integration testing and
is not flight-qualified. Applications own their CSP addressing, parameter
schema, authorization, topology, scheduling, and hardware acceptance tests.
