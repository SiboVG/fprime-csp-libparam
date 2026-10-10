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
| Static analysis | cppcheck and `clang-tidy` analyze the production Runtime, SocketCAN, Libparam, and bridge sources and their headers; any finding fails the job | Host build only; the Zephyr CAN adapter is analyzed by its Zephyr consumer. A configured job is evidence only after its workflow run succeeds |
| Linux `vcan` consumer | The downstream deployment exchanges ping and typed libparam pull, push, read-back, restoration, and teardown through the real pinned libraries | Virtual CAN does not exercise electrical hardware |
| Strict MkDocs build | All published pages, navigation, links, and diagrams render without warnings | Does not verify source behavior |

The table defines the evidence expected from CI. A local or uncommitted change
is not “Verified in CI” until its corresponding workflow run has completed
successfully.

## Static analysis gate

`scripts/static_analysis.py` is the single entry point used locally, by the
VS Code task, and by the `static-analysis` CI job. It configures the host unit
build (`build-static`, from `tests/unit`) with a compilation database, keeps
only the first-party production translation units listed in
`static-analysis.toml`, and runs both analyzers on exactly that set:

| Tool | Pinned version | Rule set |
|---|---|---|
| cppcheck | 2.22.0 | `warning`, `style`, `performance`, and `portability` with `--library=posix` and `--check-level=exhaustive` |
| clang-tidy | 22.1.8 | `.clang-tidy`: `clang-analyzer-*`, `bugprone-*`, `cert-*`, `misc-*`, `performance-*`, `portability-*`, with every warning promoted to an error; the few disabled checks are listed with their rationale in that file |

The gate passes only with zero findings. It analyzes `FprimeCsp/Runtime`,
`FprimeCsp/SocketCan`, `FprimeCsp/Libparam` (including the C bridge), their
headers, and `Config/`. Test code, the boundary fakes, and the vendored
`lib/libcsp` and `lib/libparam` sources are excluded, and ordinary findings in
their headers are not counted. Analysis-integrity errors (for example an
`#error`, syntax error, or unknown macro that makes cppcheck skip a translation
unit) count wherever they occur, so no production file can pass unanalyzed. The host build compiles SocketCAN against the boundary fakes, so the
same gate runs on Linux and macOS. CI enforces the exact tool versions; a local
run with a different version prints a warning.

Run it locally with:

```bash
python3 scripts/static_analysis.py                 # configure, analyze, report
python3 scripts/static_analysis.py --no-configure  # reuse build-static
python3 scripts/static_analysis.py --report static-analysis-report.txt
```

`brew install cppcheck` (or `sh scripts/install-cppcheck.sh`, which builds the
pinned release into `~/.local/cppcheck-2.22.0`; set `CPPCHECK` to its binary)
provides cppcheck. clang-tidy comes from the pinned PyPI wheel through `uvx`
or `pipx install clang-tidy==22.1.8`. In VS Code, the checked-in
`.vscode/settings.json` points the C/C++ extension at `build-static` and at
`scripts/clang-tidy`, the pinned clang-tidy wrapper, so editor diagnostics match
the gate; the *Static analysis (cppcheck + clang-tidy)* task runs the full gate
and lists findings in the Problems panel.

Fix findings in the code. A suppression is allowed only when the finding is a
false positive or the rule cannot apply, must be inline on (or immediately
before) the affected line, must name the check, and must state the reason:

```c
/* NOLINTNEXTLINE(performance-enum-size): shared C/C++ header; ... */
// cppcheck-suppress knownConditionTrueFalse ; reason
```

File-level or blanket suppressions are not accepted. The current tree has one
such suppression: the C-compatible enumeration in `libparam_bridge.h`, because
C17 cannot fix an enumeration's underlying type.

`FprimeCsp/ZephyrCan/ZephyrCanTransport.cpp` compiles only under Zephyr, so it
is not part of the host build. It is analyzed with the same pinned tools and
rule set by the consuming Zephyr application's static-analysis build (the
TE0715 Zephyr bench, which compiles the adapter against the real Zephyr and
libcsp headers). Its header is covered by this repository's `.clang-tidy`
header filter in that build.

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
