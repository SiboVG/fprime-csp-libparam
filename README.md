# fprime-csp-libparam

`fprime-csp-libparam` is a reusable F Prime library for Space Inventor libcsp
and libparam. It owns the common stack lifecycle, transport boundary, parameter
registry, parameter server, and typed client operations. Applications retain
their own CSP node/port assignments, parameter schema, commands, telemetry,
events, authorization, and fault policy.

The package is intentionally small: it provides reusable mechanics, not a
mission command framework. Each consuming project remains responsible for its
own executable acceptance deployment and hardware tests.

## Tested dependency set

| Dependency | Tested revision |
|---|---|
| F Prime | `35d4c0e881482adf372b8eee3eb56ad2dee74618` |
| Space Inventor libcsp | tag `csh/2.10`, commit `5a95e5c48826c3602bf4a34aa5d60e2af9e4a004` |
| Space Inventor libparam | tag `csh/2.10`, commit `d78da56e7cbc8b6be30d8890b6f01cdf80d0b9b9` |

libcsp and libparam are a matched `csh/2.10` pair. Other revisions may work,
but are not supported until they pass the same unit and CAN acceptance tests.
The source trees are supplied explicitly; the build never downloads or modifies
them. Run `scripts/bootstrap_dependencies.sh` to place the matched pair under
this library's `lib/` directory, or set both source options to existing pinned
checkouts owned by the consuming project.

## Use it from an F Prime project

Place the library somewhere stable relative to the consuming project and add
it to `settings.ini`. A conventional Git submodule under the consumer's `lib/`
directory uses:

```ini
[fprime]
framework_path: ./lib/fprime
project_root: .
library_locations: ./lib/fprime-csp-libparam
```

If the consuming project already owns the pinned checkouts, set their paths and
the optional transport in its top-level CMake before F Prime setup, or pass the
same options through `default_cmake_options`:

```ini
default_cmake_options: FPRIMECSP_LIBCSP_SOURCE=/path/to/libcsp FPRIMECSP_LIBPARAM_SOURCE=/path/to/libparam FPRIMECSP_ENABLE_SOCKETCAN=ON
```

Use absolute paths when the generator's working directory is uncertain. A
Linux application must enable SocketCAN; it is `OFF` by default. Then regenerate
the F Prime build:

```bash
fprime-util generate
fprime-util build
```

Link only the modules the application uses:

| F Prime target | Provides |
|---|---|
| `FprimeCsp_Runtime` | CSP initialization, abstract transport attachment, service binding, router lifecycle, and ping |
| `FprimeCsp_SocketCan` | Optional Linux `SocketCanTransport` implementation |
| `FprimeCsp_Libparam` | Parameter `Registry`, port-10 `Server`, and typed `Client` pull/push operations |

For example, an application adapter using all three lists those targets in its
`CMakeLists.txt` dependencies. Set `FPRIMECSP_ENABLE_SOCKETCAN=OFF` on a
non-Linux target and provide another implementation of the runtime `Transport`
interface. The runtime and libparam APIs are independent of SocketCAN, while
the SocketCAN module is Linux-specific and requires the host
SocketCAN/libsocketcan dependencies.

The bundled libparam CMake configuration currently enables its dynamic
descriptor list, which requires `<sys/queue.h>`, and the pinned sources contain
other Linux-oriented headers. Linux is the supported build target for this
first release. `FPRIMECSP_LIBPARAM_CONFIG_DIR` can select another generated
`libparam.h` profile, but an embedded target must still port and qualify the
upstream sources before treating the module as flight code.

If a consumer supplies an existing `csp` CMake target, it must also declare
`CSP_USE_RDP=ON`. This gives `csp_route_work()` a bounded wake-up interval so
the F Prime router task can stop and join. Configuration fails instead of
accepting a target whose shutdown could block forever.

## Repository checks and documentation

The repository-owned contracts use only Python's standard library and CMake:

```bash
./tests/run_repository_contracts.sh
```

Serve the documentation locally with:

```bash
python3 -m venv .venv-docs
source .venv-docs/bin/activate
python -m pip install -r requirements-docs.txt
mkdocs serve
```

Use `mkdocs build --strict --clean` for the same documentation check run by
continuous integration.

The Pages workflow publishes to
[sibovg.github.io/fprime-csp-libparam](https://sibovg.github.io/fprime-csp-libparam/)
after a repository maintainer enables GitHub Pages with GitHub Actions as its
source. On GitHub.com, a Pages site is public even when its source repository is
private unless private Pages visibility is provided and configured through
GitHub Enterprise.

## Public API

- `Stack::initialize(StackConfig, Transport&)` claims the process-wide CSP
  state and attaches one transport. `bindServiceHandler()` enables libcsp's
  standard services; `bindCallback()` adds an application callback on a
  specific port. `Stack` retains a pointer to that `Transport`, so the transport
  object must remain alive through `Stack::shutdown()`.
- `Stack::startRouter()`, `requestStop()`, `join()`, and `shutdown()` provide
  the ordered task lifetime. `ping()` is the basic synchronous CSP probe.
- `SocketCanTransport(SocketCanConfig)` attaches an already configured Linux
  CAN interface. `Stack` alone invokes the transport's private open/close
  lifecycle.
- `Registry::add(param_t&)` registers an application-owned descriptor;
  `Server::bind(Stack&)` exposes the complete registry on port 10.
- `Client(protocolVersion)` supplies synchronous `pullU32()` and `pushU32()`
  for application-owned remote descriptors. Protocol version 2 is the default.

## Ownership model

libcsp routing state, port bindings, and libparam's parameter list are
process-global. A deployment must enforce **one stack owner per process**:

1. create stable application-owned parameter descriptors and storage;
2. initialize the `Stack` with a transport that remains alive through stack
   shutdown;
3. add descriptors to `Registry` and bind the libparam `Server` and any custom
   services;
4. start the router;
5. perform client work from an F Prime component thread; and
6. request stop and join the router, then release the transport and descriptors
   only as part of terminal teardown immediately before process exit.

Do not attempt two independent stacks or an in-process hot restart. A CSP
service callback runs on the router task: validate and copy its payload into a
bounded queue, then let an F Prime component thread perform application work
and emit ports, telemetry, or events. Once callback targets or descriptors are
released, do not call libcsp or libparam again in that process.

## What belongs in the application

The library deliberately does not define:

- CSP node addresses, network prefixes, mission ports, or routing policy;
- parameter IDs, types, storage, limits, persistence, or write authorization;
- CSP command payloads or their application behavior;
- FPP commands, telemetry, events, component instances, or topology; or
- retries, pass/fail criteria, safe-mode behavior, or fault response.

See [architecture and boundaries](docs/architecture.md), [integration](docs/integration.md),
and [adding parameters and services](docs/extending.md). Consumers should keep
their transport, peer, and mission-policy acceptance tests in their own
repositories.

## Scope and safety

This initial library supports the extracted `uint32` libparam client path and a
single CSP stack lifetime. It does not provide authentication, authorization,
parameter discovery, redundant routing, or flight qualification. Treat all
wire contracts and write access as application safety responsibilities.

## License

This integration code is licensed under the [Apache License 2.0](LICENSE).
F Prime, libcsp, and libparam remain under their respective upstream licenses.
See [third-party notices](THIRD_PARTY_NOTICES.md) for the tested revisions and
license identifiers.
