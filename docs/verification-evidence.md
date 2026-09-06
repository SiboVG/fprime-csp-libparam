# Verification evidence

The repository uses several independent evidence levels. Unit tests,
sanitizers, coverage, static analysis, repository contracts, and a strict docs
build run on every applicable CI event. The integration-consumer job adds a
real libcsp/libparam exchange over Linux virtual CAN on trusted main-branch
pushes.

These automated checks are necessary, but they are not a substitute for a
hardware-in-the-loop run. A physical CAN requirement must not be marked
verified merely because a procedure exists or `vcan` passed.

## Automated evidence

| Layer | What it establishes | Limit |
|---|---|---|
| Host unit tests | Runtime ownership, ordered shutdown, retryable two-phase SocketCAN close, typed libparam validation, and error propagation against deterministic boundary fakes | Does not execute a real protocol stack or CAN controller |
| Repository contracts | Dependency pins, F Prime library registration, strict-warning policy, CI structure, documentation navigation, and hardware-status wording | Inspects repository structure rather than runtime behavior |
| Standalone F Prime consumer | F Prime discovers `library.cmake` and compiles Runtime and Libparam against the pinned sources on a clean consumer graph | Uses a header-only CSP target; does not compile libcsp, link an executable, instantiate a mission topology, or exercise SocketCAN |
| Sanitizers | Host unit paths run with address and undefined-behavior instrumentation | Cannot prove target-specific behavior |
| Coverage | The workflow enforces an 80% production line-coverage gate and retains XML results | Coverage is not proof of correct requirements or timing |
| Static analysis | `clang-tidy` analyzes the production Runtime, SocketCAN, Libparam, and bridge sources with warnings as errors | A configured job is evidence only after its workflow run succeeds |
| Linux `vcan` consumer | The downstream deployment exchanges ping and typed libparam pull, push, read-back, restoration, and teardown through the real pinned libraries | Virtual CAN does not exercise electrical hardware |
| Strict MkDocs build | All published pages, navigation, links, and diagrams render without warnings | Does not verify source behavior |

The table defines the evidence expected from CI. A local or uncommitted change
is not “Verified in CI” until its corresponding workflow run has completed
successfully.

## Evidence expected from a hardware run

The consuming application owns the hardware procedure and should retain:

1. requirement and procedure identifiers;
2. repository revisions and dependency revisions;
3. UTC start time, operator, and controlled asset identifiers;
4. CAN interface state and bitrate before execution;
5. the exact command line and complete console logs;
6. process exit status and any deviations; and
7. hashes for the evidence files.

Record a failure as a failure. Never edit a raw log to create a passing record;
write an assessed report that references the immutable raw evidence instead.
The downstream acceptance bench provides the executable capture wrapper and
the Kvaser and FlatSat procedures.

## Release interpretation

The [requirements matrix](requirements-and-verification.md) is authoritative.
An automated row may say “Verified in CI.” A physical row stays “Pending
hardware execution” until its retained as-run package has been reviewed. This
library remains integration software, not flight-qualified software, until the
mission adds its own target qualification and acceptance evidence.
