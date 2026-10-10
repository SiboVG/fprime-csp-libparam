# fprime-csp-libparam contributor guidance

This is a reusable F Prime integration for Space Inventor libcsp and libparam.
Keep the production tree (`FprimeCsp/`, `Config/`, `cmake/`, `docs/`,
`scripts/`) free of mission- or bench-specific names; the repository contracts
enforce this.

Keep `docs/requirements-and-verification.md` synchronized with behavior and
tests. Add or update requirement-to-test evidence whenever an interface,
lifecycle phase, failure response, or validation rule changes, and never mark a
hardware row verified without a retained as-run record.

Before handing off a change, run and pass:

```bash
cmake -S tests/unit -B build-unit-tests -G Ninja && cmake --build build-unit-tests \
  && ctest --test-dir build-unit-tests --output-on-failure
# sanitizers and the 80% production line-coverage gate: see .github/workflows/ci.yml
python3 scripts/static_analysis.py      # cppcheck + clang-tidy, zero findings
./tests/run_repository_contracts.sh
mkdocs build --strict --clean          # in a venv with requirements-docs.txt
```

Static-analysis findings are fixed in the code. A suppression is allowed only
inline, naming the check and the reason (`// NOLINT(check-id): reason`, or
`// cppcheck-suppress id ; reason` on the line before); never file-level or
blanket, and keep them rare. If a finding exposes a real defect whose fix
changes behavior, fix it as a deliberate, tested behavior change and update the
requirements matrix. `scripts/static_analysis.py`, `scripts/install-cppcheck.sh`,
`scripts/clang-tidy`, and `.clang-tidy` (except `HeaderFilterRegex`) are vendored
byte-identical from the workspace reference; change them there, not here.

`FprimeCsp/ZephyrCan` compiles only under Zephyr; its static analysis runs in
the consuming Zephyr application's build, not in this repository's host gate.

## Test-first, from the requirement

Develop test-first, and derive tests from what the code must do, not from how it does it:

1. Add or update the requirement (ID, `shall` statement, rationale or parent, method) in this repository's requirements matrix.
2. Design test cases from the requirement and the interface contract (datasheet, ICD, FPP model) without reading the implementation: nominal values computed independently, boundaries, invalid inputs, injected dependency faults, state transitions.
3. Watch the tests fail, implement the minimum to make them pass, then refactor.
4. When coverage shows unexecuted code, add a requirement-derived test, remove dead code, add the missing requirement, or justify the code by analysis. Never add a test only to execute a line.
5. Name the requirement ID in each test, and list the test in the requirement's row.

Coverage target (ECSS category C, CubeSpec): 100% statement coverage, tested or justified, and at least 80% decision coverage. The full procedure is in the CubeSpec workspace `AGENTS.md` (`../../AGENTS.md` in the workspace layout).
