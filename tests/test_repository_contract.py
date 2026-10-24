"""Standalone repository contracts for fprime-csp-libparam.

These tests intentionally use only the Python standard library, CMake, and the
files in this repository.  They must remain runnable after this directory is
cloned on its own, without the CubeSpec acceptance deployment beside it.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


LIBRARY_ROOT = Path(__file__).resolve().parents[1]
TEST_ROOT = LIBRARY_ROOT / "tests"
RUNNER = TEST_ROOT / "run_repository_contracts.sh"
SMOKE_ROOT = TEST_ROOT / "smoke_consumer"
SMOKE_RUNNER = TEST_ROOT / "run_runtime_smoke.sh"
PAGES_WORKFLOW = LIBRARY_ROOT / ".github" / "workflows" / "pages.yml"

LIBCSP_COMMIT = "5a95e5c48826c3602bf4a34aa5d60e2af9e4a004"
LIBPARAM_COMMIT = "d78da56e7cbc8b6be30d8890b6f01cdf80d0b9b9"
FPRIME_COMMIT = "35d4c0e881482adf372b8eee3eb56ad2dee74618"
RELEASE_TAG = "csh/2.10"

MODULES = {
    "Runtime": ("Stack.cpp", "Stack.hpp"),
    "SocketCan": ("SocketCanTransport.cpp", "SocketCanTransport.hpp"),
    "Udp": ("UdpTransport.cpp", "UdpTransport.hpp", "UdpSockets.cpp"),
    "Libparam": (
        "Libparam.cpp",
        "Libparam.hpp",
        "libparam_bridge.c",
        "libparam_bridge.h",
    ),
}


# Byte-identical copies of the shared static-analysis tooling. Their upstream
# reference names the workspace that maintains them; they are tooling, not a
# runtime or build dependency of the library.
VENDORED_STATIC_ANALYSIS_FILES = frozenset(
    LIBRARY_ROOT / "scripts" / name
    for name in ("static_analysis.py", "install-cppcheck.sh", "clang-tidy")
)


def read(relative_path: str) -> str:
    path = LIBRARY_ROOT / relative_path
    if not path.is_file():
        raise AssertionError(f"required repository file is missing: {relative_path}")
    return path.read_text(encoding="utf-8")


def production_files() -> list[Path]:
    """Return the files that must be independent of the acceptance project."""

    roots = [
        LIBRARY_ROOT / "Config",
        LIBRARY_ROOT / "FprimeCsp",
        LIBRARY_ROOT / "cmake",
        LIBRARY_ROOT / "docs",
        LIBRARY_ROOT / "scripts",
    ]
    files = [
        LIBRARY_ROOT / "library.cmake",
        LIBRARY_ROOT / "README.md",
        LIBRARY_ROOT / "LICENSE",
    ]
    for root in roots:
        files.extend(
            path
            for path in root.rglob("*")
            if path.is_file() and path not in VENDORED_STATIC_ANALYSIS_FILES
        )
    return sorted(files)


class StandaloneRunnerContract(unittest.TestCase):
    def test_repository_contracts_have_an_executable_standalone_runner(self) -> None:
        self.assertTrue(
            RUNNER.is_file(),
            "add tests/run_repository_contracts.sh so a standalone clone can run its contracts",
        )


class RuntimeSmokeConsumerContract(unittest.TestCase):
    def test_runtime_smoke_consumer_is_checked_in_and_executable(self) -> None:
        for filename in ("CMakeLists.txt", "settings.ini", "csp_autoconfig.h.in"):
            self.assertTrue(
                (SMOKE_ROOT / filename).is_file(),
                f"tests/smoke_consumer/{filename} is missing",
            )
        self.assertTrue(SMOKE_RUNNER.is_file(), "tests/run_runtime_smoke.sh is missing")
        self.assertTrue(
            os.access(SMOKE_RUNNER, os.X_OK),
            "tests/run_runtime_smoke.sh must be executable",
        )

    def test_runtime_smoke_consumer_discovers_the_manifest_and_builds_runtime(self) -> None:
        cmake_path = SMOKE_ROOT / "CMakeLists.txt"
        self.assertTrue(cmake_path.is_file(), "runtime smoke CMakeLists.txt is missing")
        self.assertTrue(SMOKE_RUNNER.is_file(), "runtime smoke runner is missing")
        cmake = cmake_path.read_text(encoding="utf-8")
        runner = SMOKE_RUNNER.read_text(encoding="utf-8")

        for token in (
            "SMOKE_FPRIME_FRAMEWORK_PATH",
            "SMOKE_LIBCSP_SOURCE",
            "FPRIME_LIBRARY_LOCATIONS",
            "library.cmake",
            "if(NOT TARGET FprimeCsp_Runtime)",
            "if(NOT TARGET FprimeCsp_Libparam)",
        ):
            self.assertIn(token, cmake)
        self.assertNotIn("add_library(fprimecsp_libparam_c INTERFACE)", cmake)
        self.assertIn("--target FprimeCsp_Runtime FprimeCsp_Libparam", runner)
        libparam_cmake = read("FprimeCsp/Libparam/CMakeLists.txt")
        self.assertIn(
            "target_link_libraries(${FPRIME_CURRENT_MODULE} PUBLIC ${FPRIMECSP_LIBPARAM_C_TARGET})",
            libparam_cmake,
        )
        self.assertIn("C_STANDARD 11", libparam_cmake)
        self.assertIn("$<$<COMPILE_LANGUAGE:C>:-std=c11>", libparam_cmake)

    def test_runtime_smoke_consumer_has_no_mission_specific_paths(self) -> None:
        self.assertTrue(SMOKE_RUNNER.is_file(), "runtime smoke runner is missing")
        files = [SMOKE_RUNNER]
        if SMOKE_ROOT.is_dir():
            files.extend(path for path in SMOKE_ROOT.rglob("*") if path.is_file())
        text = "\n".join(path.read_text(encoding="utf-8") for path in files)
        self.assertNotIn("cubespec", text.casefold())
        self.assertNotIn("/Users/", text)

    def test_default_build_directory_uses_a_canonical_temporary_parent(self) -> None:
        self.assertTrue(SMOKE_RUNNER.is_file(), "runtime smoke runner is missing")
        runner = SMOKE_RUNNER.read_text(encoding="utf-8")
        canonicalize = 'temporary_parent="$(cd "${TMPDIR:-/tmp}" && pwd -P)"'
        create = 'mktemp -d "$temporary_parent/fprimecsp-runtime-smoke.XXXXXX"'
        self.assertIn(canonicalize, runner)
        self.assertIn(create, runner)
        self.assertLess(runner.index(canonicalize), runner.index(create))
        self.assertTrue(
            os.access(RUNNER, os.X_OK),
            "tests/run_repository_contracts.sh must be executable",
        )

    def test_ci_builds_the_standalone_runtime_and_libparam_consumer(self) -> None:
        workflow = read(".github/workflows/ci.yml")
        for token in (
            "standalone-consumer:",
            "repository: nasa/fprime",
            FPRIME_COMMIT,
            "./scripts/bootstrap_dependencies.sh",
            "./tests/run_runtime_smoke.sh",
        ):
            self.assertIn(token, workflow)


class RepositoryIndependenceContract(unittest.TestCase):
    def test_production_tree_has_no_cubespec_or_bench_dependency(self) -> None:
        forbidden = (
            "CubeSpec",
            "CspLibparamFprimeTest",
            "BenchController",
            "bench_u32",
        )
        violations: list[str] = []
        for path in production_files():
            text = path.read_text(encoding="utf-8", errors="replace")
            for token in forbidden:
                if token in text:
                    violations.append(f"{path.relative_to(LIBRARY_ROOT)}: {token}")
        self.assertEqual(
            [],
            violations,
            "the reusable repository must not depend on mission-specific names",
        )

    def test_dependency_configuration_is_library_relative(self) -> None:
        dependencies = read("cmake/Dependencies.cmake")
        self.assertIn("FPRIMECSP_LIBRARY_ROOT", dependencies)
        self.assertIn('${FPRIMECSP_LIBRARY_ROOT}/lib/libcsp', dependencies)
        self.assertIn('${FPRIMECSP_LIBRARY_ROOT}/lib/libparam', dependencies)
        self.assertNotIn("FPRIME_PROJECT_ROOT", dependencies)
        self.assertNotIn("CMAKE_SOURCE_DIR", dependencies)

    def test_dependency_checks_reject_modified_source_trees(self) -> None:
        dependencies = read("cmake/Dependencies.cmake")
        bootstrap = read("scripts/bootstrap_dependencies.sh")
        self.assertIn("status --porcelain", dependencies)
        self.assertIn("status --porcelain", bootstrap)


class ReleaseContract(unittest.TestCase):
    def test_third_party_dependency_licenses_are_identified(self) -> None:
        notices = read("THIRD_PARTY_NOTICES.md")
        self.assertIn("F Prime", notices)
        self.assertIn("Apache License 2.0", notices)
        self.assertIn("libcsp", notices)
        self.assertIn("libparam", notices)
        self.assertIn("MIT License", notices)

    def test_cmake_and_bootstrap_pin_the_exact_csh_2_10_pair(self) -> None:
        dependencies = read("cmake/Dependencies.cmake")
        bootstrap = read("scripts/bootstrap_dependencies.sh")

        self.assertRegex(
            dependencies,
            rf'set\(FPRIMECSP_LIBCSP_COMMIT\s+"{LIBCSP_COMMIT}"\)',
        )
        self.assertRegex(
            dependencies,
            rf'set\(FPRIMECSP_LIBPARAM_COMMIT\s+"{LIBPARAM_COMMIT}"\)',
        )
        for token in (LIBCSP_COMMIT, LIBPARAM_COMMIT, RELEASE_TAG):
            self.assertIn(token, bootstrap)

    def test_library_does_not_pin_the_consumers_f_prime_checkout(self) -> None:
        build_configuration = "\n".join(
            (read("library.cmake"), read("cmake/Dependencies.cmake"))
        )
        self.assertNotRegex(build_configuration, r"FPRIME_(?:COMMIT|REVISION|TAG)")
        self.assertNotRegex(build_configuration, r"framework_path")

    def test_cmake_uses_native_csh_2_10_targets_and_full_libparam_sources(self) -> None:
        dependencies = read("cmake/Dependencies.cmake")
        self.assertIn("if(NOT TARGET csp)", dependencies)
        self.assertNotIn("if(NOT TARGET libcsp)", dependencies)
        self.assertNotIn("csp_v2_0_compat", dependencies)
        for source in (
            "src/vmem/vmem_client.c",
            "src/vmem/vmem_crc32.c",
            "src/vmem/vmem_server.c",
            "src/vmem/vmem_block.c",
            "src/objstore/objstore.c",
        ):
            self.assertIn(source, dependencies)

    def test_third_party_headers_are_system_includes_for_strict_consumers(self) -> None:
        dependencies = read("cmake/Dependencies.cmake")
        self.assertIn("INTERFACE_SYSTEM_INCLUDE_DIRECTORIES", dependencies)
        self.assertRegex(
            dependencies,
            r"target_include_directories\(\s*\$\{FPRIMECSP_LIBPARAM_C_TARGET\}\s+SYSTEM\s+PUBLIC",
        )
        self.assertIn("cmake/compat/darwin", dependencies)
        self.assertTrue((LIBRARY_ROOT / "cmake/compat/darwin/malloc.h").is_file())
        self.assertTrue((LIBRARY_ROOT / "cmake/compat/darwin/endian.h").is_file())

    def test_libparam_bridge_port_tracks_upstream(self) -> None:
        bridge = read("FprimeCsp/Libparam/libparam_bridge.c")
        self.assertIn(
            "_Static_assert(FPRIMECSP_PARAM_SERVER_PORT == PARAM_PORT_SERVER,",
            bridge,
        )


class FPrimePackageContract(unittest.TestCase):
    def test_manifest_registers_each_namespaced_module(self) -> None:
        manifest = read("library.cmake")
        self.assertIn('include("${CMAKE_CURRENT_LIST_DIR}/cmake/Dependencies.cmake")', manifest)
        for module in MODULES:
            self.assertIn(
                f'add_fprime_subdirectory("${{CMAKE_CURRENT_LIST_DIR}}/FprimeCsp/{module}")',
                manifest,
            )

    def test_module_sources_and_f_prime_registrations_are_self_contained(self) -> None:
        for module, expected_files in MODULES.items():
            module_root = LIBRARY_ROOT / "FprimeCsp" / module
            for filename in expected_files:
                self.assertTrue(
                    (module_root / filename).is_file(),
                    f"FprimeCsp/{module}/{filename} is missing",
                )
            cmake = read(f"FprimeCsp/{module}/CMakeLists.txt")
            self.assertIn("register_fprime_library(", cmake)
            self.assertIn("${FPRIMECSP_LIBRARY_ROOT}", cmake)

    def test_socketcan_is_an_opt_in_module(self) -> None:
        dependencies = read("cmake/Dependencies.cmake")
        socketcan = read("FprimeCsp/SocketCan/CMakeLists.txt")
        self.assertRegex(
            dependencies,
            r"option\(\s*FPRIMECSP_ENABLE_SOCKETCAN[\s\S]*?OFF\s*\)",
        )
        self.assertIn("if(NOT FPRIMECSP_ENABLE_SOCKETCAN)", socketcan)

    def test_udp_is_an_opt_in_module_built_by_the_smoke_consumer(self) -> None:
        # FCL-UDP-007: off by default; the standalone consumer builds it on Linux
        dependencies = read("cmake/Dependencies.cmake")
        udp = read("FprimeCsp/Udp/CMakeLists.txt")
        self.assertRegex(dependencies, r"option\(\s*FPRIMECSP_ENABLE_UDP[\s\S]*?OFF\s*\)")
        self.assertIn("if(NOT FPRIMECSP_ENABLE_UDP)", udp)
        self.assertIn("FPRIMECSP_HAVE_UDP=1", udp)
        smoke = (SMOKE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('set(FPRIMECSP_ENABLE_UDP ON CACHE BOOL "" FORCE)', smoke)
        self.assertIn("if(NOT TARGET FprimeCsp_Udp)", smoke)
        self.assertIn("FprimeCsp_Udp", SMOKE_RUNNER.read_text(encoding="utf-8"))


class RuntimeSafetyContract(unittest.TestCase):
    def test_public_api_is_composable_and_policy_free(self) -> None:
        stack = read("FprimeCsp/Runtime/Stack.hpp")
        socketcan = read("FprimeCsp/SocketCan/SocketCanTransport.hpp")
        libparam = read("FprimeCsp/Libparam/Libparam.hpp")

        for token in (
            "namespace FprimeCsp",
            "struct StackConfig",
            "class Transport",
            "class Stack",
            "bindServiceHandler",
            "startRouter",
            "requestStop",
            "join",
            "ping",
        ):
            self.assertIn(token, stack)
        for token in (
            "struct SocketCanConfig",
            "class SocketCanTransport",
            "public Transport",
        ):
            self.assertIn(token, socketcan)
        for token in (
            "class Registry",
            "class Server",
            "class Client",
            "pullU32",
            "pushU32",
        ):
            self.assertIn(token, libparam)

    def test_runtime_uses_csh_2_10_lifecycle_and_crc32_ping(self) -> None:
        stack = read("FprimeCsp/Runtime/Stack.cpp")
        socketcan = read("FprimeCsp/SocketCan/SocketCanTransport.cpp")
        combined = stack + socketcan
        for obsolete in ("csp_conf.address", "csp_rtable_set", "csp_free_resources"):
            self.assertNotIn(obsolete, combined)
        self.assertIn("m_interface->is_default = 1", socketcan)
        self.assertIn(
            "csp_ping(peerNode, timeoutMs, payloadBytes, CSP_O_CRC32)",
            stack,
        )

    def test_addressing_is_validated_before_global_state_is_claimed(self) -> None:
        stack = read("FprimeCsp/Runtime/Stack.cpp")
        for token in (
            "ERROR_INVALID_LOCAL_NODE",
            "ERROR_INVALID_NETWORK_PREFIX",
            "addressBits",
            "subnetHostMask",
        ):
            self.assertIn(token, stack)
        self.assertLess(
            stack.index("subnetHostMask"),
            stack.index("processStackClaimed.compare_exchange_strong"),
        )

    def test_socketcan_removes_interface_before_freeing_driver_context(self) -> None:
        socketcan = read("FprimeCsp/SocketCan/SocketCanTransport.cpp")
        self.assertLess(
            socketcan.index("csp_can_remove_interface"),
            socketcan.index("csp_can_socketcan_stop"),
        )

    def test_shutdown_reports_partial_failure_and_preserves_transport_order(self) -> None:
        header = read("FprimeCsp/Runtime/Stack.hpp")
        source = read("FprimeCsp/Runtime/Stack.cpp")
        self.assertIn("enum class ShutdownStatus", header)
        self.assertIn("ROUTER_JOIN_FAILED", header)
        self.assertIn("TRANSPORT_CLOSE_FAILED", header)
        self.assertLess(source.index("this->join()"), source.index("m_transport->close()"))
        self.assertIn("std::terminate()", source)

    def test_production_modules_enable_strict_compiler_warnings(self) -> None:
        manifest = read("library.cmake")
        self.assertIn("fprimecsp_enable_strict_warnings", manifest)
        self.assertIn("-Werror", manifest)
        for module in MODULES:
            self.assertIn(
                "fprimecsp_enable_strict_warnings(${FPRIME_CURRENT_MODULE})",
                read(f"FprimeCsp/{module}/CMakeLists.txt"),
            )


class DocumentationContract(unittest.TestCase):
    def test_repository_owns_its_documentation_interface(self) -> None:
        mkdocs = read("mkdocs.yml")
        requirements = read("requirements-docs.txt")
        index = read("docs/index.md")
        gitignore = read(".gitignore")

        self.assertIn("strict: true", mkdocs)
        for page in (
            "index.md",
            "architecture.md",
            "integration.md",
            "extending.md",
            "requirements-and-verification.md",
            "verification-evidence.md",
        ):
            self.assertIn(page, mkdocs)
        self.assertIn("mkdocs-material", requirements)
        self.assertIn("fprime-csp-libparam", index)
        self.assertIn("site/", gitignore)

    def test_integration_guide_matches_supported_host_profiles(self) -> None:
        integration = read("docs/integration.md")
        normalized = " ".join(integration.split())
        self.assertIn(
            "The default dependency build supports Linux",
            normalized,
        )
        self.assertIn("SocketCAN remains Linux-only", normalized)
        self.assertNotIn(
            "Linux is the supported target for this bundled profile",
            normalized,
        )

    def test_docs_summarize_current_runtime_portability_and_assurance(self) -> None:
        readme = " ".join(read("README.md").split())
        index = " ".join(read("docs/index.md").split())
        evidence = " ".join(read("docs/verification-evidence.md").split())

        for token in (
            "ShutdownStatus",
            "two-phase SocketCAN",
            "pullInteger",
            "Linux and Darwin",
            "SocketCAN remains Linux-only",
        ):
            with self.subTest(index_token=token):
                self.assertIn(token, index)

        for token in ("standalone consumer", "coverage", "static analysis"):
            with self.subTest(readme_token=token):
                self.assertIn(token, readme)

        for token in (
            "Standalone F Prime consumer",
            "80% production line-coverage gate",
            "clang-tidy",
            "Linux `vcan`",
            "Pending hardware execution",
        ):
            with self.subTest(evidence_token=token):
                self.assertIn(token, evidence)

    def test_github_pages_publishes_the_strict_mkdocs_artifact(self) -> None:
        self.assertTrue(
            PAGES_WORKFLOW.is_file(),
            ".github/workflows/pages.yml must publish the standalone documentation",
        )
        workflow = PAGES_WORKFLOW.read_text(encoding="utf-8")
        mkdocs = read("mkdocs.yml")

        self.assertIn(
            "site_url: https://sibovg.github.io/fprime-csp-libparam/",
            mkdocs,
        )
        self.assertIn(
            "on:\n  workflow_run:\n    workflows:\n      - Repository checks\n"
            "    types:\n      - completed\n    branches:\n      - main\n"
            "  workflow_dispatch:",
            workflow,
        )
        self.assertIn(
            "permissions:\n  contents: read\n  pages: write\n  id-token: write",
            workflow,
        )
        self.assertIn(
            "concurrency:\n  group: pages\n  cancel-in-progress: false",
            workflow,
        )

        for action in (
            "actions/checkout@v7",
            "actions/setup-python@v7",
            "actions/configure-pages@v6",
            "actions/upload-pages-artifact@v5",
            "actions/deploy-pages@v5",
        ):
            self.assertIn(action, workflow)

        self.assertIn("python -m mkdocs build --strict --clean", workflow)
        self.assertIn("path: ./site", workflow)
        self.assertIn("name: github-pages", workflow)
        self.assertIn("url: ${{ steps.deployment.outputs.page_url }}", workflow)

        permission_block = re.search(
            r"(?m)^permissions:\n((?:^  [a-z-]+: [a-z]+\n?)+)",
            workflow,
        )
        self.assertIsNotNone(permission_block)
        self.assertEqual(
            {
                "contents: read",
                "pages: write",
                "id-token: write",
            },
            {
                line.strip()
                for line in permission_block.group(1).splitlines()
                if line.strip()
            },
            "the Pages workflow must not grant unrelated token permissions",
        )

    def test_documentation_site_offers_light_and_dark_palettes(self) -> None:
        mkdocs = read("mkdocs.yml")

        for token in (
            'media: "(prefers-color-scheme: light)"',
            "scheme: default",
            "material/brightness-7",
            "Switch to dark mode",
            'media: "(prefers-color-scheme: dark)"',
            "scheme: slate",
            "material/brightness-4",
            "Switch to light mode",
        ):
            self.assertIn(token, mkdocs)

    def test_standalone_readme_links_the_library_guides(self) -> None:
        readme = read("README.md")
        self.assertIn("library_locations", readme)
        for guide in ("architecture.md", "integration.md", "extending.md"):
            self.assertIn(f"docs/{guide}", readme)
        for target in (
            "FprimeCsp_Runtime",
            "FprimeCsp_SocketCan",
            "FprimeCsp_Libparam",
        ):
            self.assertIn(target, readme)

    def test_guides_cover_ownership_integration_and_extension(self) -> None:
        architecture = read("docs/architecture.md").lower()
        integration = read("docs/integration.md")
        extending = read("docs/extending.md").lower()

        self.assertIn("one stack owner per process", architecture)
        self.assertIn("library_locations", integration)
        self.assertIn("FPRIMECSP_ENABLE_SOCKETCAN", integration)
        self.assertIn("parameter", extending)
        self.assertIn("service", extending)

    def test_requirements_matrix_is_explicit_and_hardware_status_is_honest(self) -> None:
        matrix = read("docs/requirements-and-verification.md")
        for requirement_id in (
            "FCL-ARCH-001",
            "FCL-RUN-001",
            "FCL-RUN-002",
            "FCL-RUN-003",
            "FCL-CSP-001",
            "FCL-CAN-001",
            "FCL-PAR-001",
            "FCL-LEG-001",
            "FCL-INT-001",
            "FCL-HIL-001",
        ):
            self.assertIn(requirement_id, matrix)
        self.assertIn("Pending hardware execution", matrix)

    def test_verification_guide_separates_ci_from_hardware_evidence(self) -> None:
        guide = " ".join(read("docs/verification-evidence.md").lower().split())
        self.assertIn("unit tests", guide)
        self.assertIn("sanitizers", guide)
        self.assertIn("coverage", guide)
        self.assertIn("static analysis", guide)
        self.assertIn("hardware-in-the-loop", guide)
        self.assertIn("must not be marked verified", guide)

    def test_ci_has_coverage_and_static_analysis_gates(self) -> None:
        workflow = read(".github/workflows/ci.yml")
        for token in (
            "coverage:",
            "gcovr",
            "-DFPRIMECSP_ENABLE_COVERAGE=ON",
            "--fail-under-line 80",
            "static-analysis:",
            "python3 scripts/static_analysis.py --build host",
            "scripts/install-cppcheck.sh",
            "cppcheck",
            "clang-tidy",
        ):
            self.assertIn(token, workflow)
        self.assertIn(
            "option(FPRIMECSP_ENABLE_COVERAGE",
            read("tests/unit/CMakeLists.txt"),
        )

    def test_static_analysis_gate_covers_production_sources_with_zero_findings(
        self,
    ) -> None:
        config = read("static-analysis.toml")
        tidy = read(".clang-tidy")
        runner = read("scripts/static_analysis.py")
        for token in (
            'name = "host"',
            '"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"',
            '"FprimeCsp/**"',
            '"tests/**"',
            '"lib/*"',
            'local_builds = ["host"]',
        ):
            with self.subTest(config_token=token):
                self.assertIn(token, config)
        self.assertIn("WarningsAsErrors: '*'", tidy)
        self.assertIn("HeaderFilterRegex: '(^|/)(FprimeCsp|Config)/'", tidy)
        self.assertIn('CPPCHECK_VERSION = "2.22.0"', runner)
        self.assertIn('CLANG_TIDY_VERSION = "22.1.8"', runner)
        self.assertIn("--enable=warning,style,performance,portability", runner)
        for name in ("install-cppcheck.sh", "clang-tidy", "static_analysis.py"):
            with self.subTest(executable=name):
                self.assertTrue(os.access(LIBRARY_ROOT / "scripts" / name, os.X_OK))

    def test_editor_runs_the_same_static_analysis_as_ci(self) -> None:
        settings = read(".vscode/settings.json")
        tasks = read(".vscode/tasks.json")
        self.assertIn("build-static/compile_commands.json", settings)
        self.assertIn("scripts/clang-tidy", settings)
        self.assertIn("python3 scripts/static_analysis.py", tasks)
        self.assertIn("ms-vscode.cpptools", read(".vscode/extensions.json"))

    def test_architecture_is_the_authoritative_protocol_and_f_prime_explanation(
        self,
    ) -> None:
        architecture = read("docs/architecture.md")

        for heading in (
            "## CSP at a high level",
            "## libparam at a high level",
            "## How F Prime normally handles commands and telemetry",
            "## How this library joins the two worlds",
            "## Per-process architecture",
            "## Runtime lifecycle",
            "## Protocol and configuration contract",
        ):
            self.assertIn(heading, architecture)

        for fact in (
            "CSP node",
            "destination port",
            "parameter descriptor",
            "CSP port 10",
            "FPP",
            "command dispatcher",
            "telemetry",
            "csp_route_work()",
            "CSP v2",
            "libparam protocol version 2",
            "CSP_O_CRC32",
        ):
            self.assertIn(fact, architecture)

        self.assertGreaterEqual(architecture.count("```mermaid"), 3)
        for diagram in re.findall(r"```mermaid\n(.*?)```", architecture, re.DOTALL):
            self.assertNotIn(r"\n", diagram)

    def test_sequence_diagram_labels_are_wrapped_for_document_width(self) -> None:
        architecture = read("docs/architecture.md")
        sequence_diagrams = [
            diagram
            for diagram in re.findall(r"```mermaid\n(.*?)```", architecture, re.DOTALL)
            if diagram.lstrip().startswith("sequenceDiagram")
        ]

        self.assertTrue(sequence_diagrams)
        for diagram in sequence_diagrams:
            self.assertNotRegex(
                diagram,
                r"(?m)^\s*(\w+)->>\1:",
                "Mermaid centers self-message labels on the actor lifeline",
            )
            participants = re.findall(
                r"^\s*participant\s+\S+\s+as\s+(.+)$", diagram, re.MULTILINE
            )
            messages = re.findall(
                r"^\s*\S+\s*-+>>\s*\S+\s*:\s*(.+)$", diagram, re.MULTILINE
            )

            for label in participants:
                for line in re.split(r"<br\s*/?>", label):
                    with self.subTest(participant_line=line):
                        self.assertLessEqual(len(line), 22)
            for label in messages:
                for line in re.split(r"<br\s*/?>", label):
                    with self.subTest(message_line=line):
                        self.assertLessEqual(len(line), 30)

    def test_overview_diagram_avoids_subgraph_heading_collisions(self) -> None:
        architecture = read("docs/architecture.md")
        diagrams = re.findall(r"```mermaid\n(.*?)```", architecture, re.DOTALL)

        self.assertTrue(diagrams)
        self.assertNotIn(
            "subgraph ",
            diagrams[0],
            "cross-path arrows otherwise run through centered subgraph headings",
        )

    def test_mermaid_diagrams_defer_colors_to_the_active_theme(self) -> None:
        architecture = read("docs/architecture.md")
        diagrams = re.findall(r"```mermaid\n(.*?)```", architecture, re.DOTALL)

        self.assertTrue(diagrams)
        for diagram in diagrams:
            self.assertNotRegex(
                diagram,
                r"(?i)\b(?:fill|stroke|color)\s*:",
                "fixed Mermaid colors do not adapt to Material light/dark palettes",
            )

    def test_flowchart_node_lines_are_short_enough_for_narrow_pages(self) -> None:
        architecture = read("docs/architecture.md")
        diagrams = re.findall(r"```mermaid\n(.*?)```", architecture, re.DOTALL)

        for diagram in diagrams:
            labels = re.findall(
                r'\b[A-Za-z_][A-Za-z0-9_]*\[(?:"([^"]*)"|([^\]]*))\]',
                diagram,
            )
            for quoted, plain in labels:
                for line in re.split(r"<br\s*/?>", quoted or plain):
                    with self.subTest(label_line=line):
                        self.assertLessEqual(len(line.strip()), 22)

    def test_architecture_names_and_links_the_relevant_f_prime_classes(self) -> None:
        architecture = read("docs/architecture.md")

        for class_name in (
            "Svc::CommandDispatcher",
            "Fw::ActiveComponentBase",
            "Svc::TlmChan",
            "Svc::EventManager",
            "Svc::FprimeDeframer",
            "Svc::FprimeRouter",
            "Svc::ActiveRateGroup",
        ):
            self.assertIn(class_name, architecture)

        for official_reference in (
            "https://fprime.jpl.nasa.gov/latest/Svc/CmdDispatcher/docs/sdd/",
            "https://fprime.jpl.nasa.gov/latest/Svc/TlmChan/docs/sdd/",
            "https://fprime.jpl.nasa.gov/latest/docs/user-manual/framework/autocoded-functions/",
            "https://github.com/nasa/fprime/blob/35d4c0e881482adf372b8eee3eb56ad2dee74618/",
        ):
            self.assertIn(official_reference, architecture)

    def test_integration_is_a_custom_application_walkthrough_with_examples(self) -> None:
        integration = read("docs/integration.md")

        self.assertIn("custom F Prime application", integration)
        for fence in ("```ini", "```cmake", "```cpp"):
            self.assertIn(fence, integration)
        for api_call in (
            "m_stack.initialize",
            "m_registry.add",
            "m_server.bind",
            "m_stack.startRouter",
            "m_stack.requestStop",
            "m_stack.join",
            "m_stack.shutdown",
        ):
            self.assertIn(api_call, integration)
        for official_reference in (
            "https://fprime.jpl.nasa.gov/latest/docs/how-to/develop-fprime-libraries/",
            "https://fprime.jpl.nasa.gov/latest/docs/user-manual/build-system/settings/",
            "https://fprime.jpl.nasa.gov/latest/docs/user-manual/framework/building-topology/",
            "https://fprime.jpl.nasa.gov/latest/docs/user-manual/framework/autocoded-functions/",
        ):
            self.assertIn(official_reference, integration)
        self.assertIn("active component thread", integration)
        self.assertIn("synchronous", integration)
        self.assertIn("m_registry.lastError()", integration)
        self.assertIn("m_server.lastError()", integration)
        self.assertIn("PingFailed(peer, elapsedMs)", integration)
        self.assertNotIn("m_protocol.lastError()", integration)

    def test_extension_guide_owns_reusable_parameter_and_service_mechanics(
        self,
    ) -> None:
        extending = read("docs/extending.md")

        for heading in (
            "## Decide whether a change belongs in the library",
            "## Add a libparam parameter",
            "## Handle an incoming CSP service",
            "## Hand work to an F Prime component thread",
            "## Lifecycle and packet-ownership rules",
            "## Verification checklist",
        ):
            self.assertIn(heading, extending)

        for fact in (
            "FprimeCsp/Libparam/Libparam.hpp",
            "FprimeCsp/Runtime/Stack.hpp",
            "param_t",
            "Stack::bindCallback",
            "csp_packet_t",
            "csp_sendto_reply",
            "CSP_O_CRC32",
            "active component thread",
        ):
            self.assertIn(fact, extending)

        self.assertNotIn("CubeSpec", extending)

    def test_docs_do_not_confuse_parameter_requests_with_list_download(self) -> None:
        architecture = read("docs/architecture.md")
        extending = read("docs/extending.md")

        self.assertIn("services read and write requests", architecture)
        self.assertIn("services requests for all registered descriptors", extending)
        self.assertNotIn("serves the shared list on **CSP port 10**", architecture)
        self.assertNotIn("exposes the complete registered list on CSP port 10", extending)

    def test_high_level_diagram_stacks_the_two_paths_for_readability(self) -> None:
        architecture = read("docs/architecture.md")
        overview = re.findall(
            r"```mermaid\n(.*?)```", architecture, re.DOTALL
        )[0]

        self.assertTrue(overview.startswith("flowchart TB\n"))
        self.assertIn('GROUND["F PRIME CONTROL +<br/>OBSERVABILITY', overview)
        self.assertIn('API["CSP + LIBPARAM<br/>PACKET PATH', overview)
        self.assertLess(overview.index("GROUND["), overview.index("API["))


class BoundedShutdownContract(unittest.TestCase):
    def test_rdp_off_configuration_is_rejected_with_actionable_diagnostic(self) -> None:
        source = TEST_ROOT / "rdp_off"
        self.assertTrue((source / "CMakeLists.txt").is_file())

        with tempfile.TemporaryDirectory(prefix="fprimecsp-rdp-off-") as build_dir:
            result = subprocess.run(
                ["cmake", "-S", str(source), "-B", build_dir],
                cwd=LIBRARY_ROOT,
                check=False,
                capture_output=True,
                text=True,
            )

        output = f"{result.stdout}\n{result.stderr}"
        self.assertNotEqual(0, result.returncode, output)
        self.assertIn(
            "CSP_USE_RDP must be ON for bounded FprimeCsp router shutdown",
            output,
        )
        self.assertNotIn("libcsp is missing", output)
        self.assertNotIn("libparam is missing", output)


class ContinuousIntegrationContract(unittest.TestCase):
    def test_github_workflow_runs_cpp_unit_tests_contracts_and_strict_docs(self) -> None:
        workflow = read(".github/workflows/ci.yml")

        for token in (
            "ubuntu-24.04",
            "cmake -S tests/unit -B build-unit-tests -G Ninja",
            "cmake --build build-unit-tests",
            "ctest --test-dir build-unit-tests --output-on-failure",
            "build-unit-tests-sanitized",
            "-fsanitize=address,undefined",
            "ctest --test-dir build-unit-tests-sanitized --output-on-failure",
            "./tests/run_repository_contracts.sh",
            "mkdocs build --strict --clean",
        ):
            self.assertIn(token, workflow)

    def test_pages_waits_for_successful_ci_and_validates_manual_runs(self) -> None:
        workflow = read(".github/workflows/pages.yml")

        for token in (
            "workflow_run:",
            "workflows:\n      - Repository checks",
            "types:\n      - completed",
            "branches:\n      - main",
            "workflow_dispatch:",
            "github.event.workflow_run.head_sha || github.sha",
            "CI_CONCLUSION: ${{ github.event.workflow_run.conclusion }}",
            "CI_EVENT: ${{ github.event.workflow_run.event }}",
            "CI_REPOSITORY: ${{ github.event.workflow_run.head_repository.full_name }}",
            'test "$CI_CONCLUSION" = "success"',
            'test "$CI_EVENT" = "push"',
            'test "$CI_REPOSITORY" = "$GITHUB_REPOSITORY"',
            "cmake -S tests/unit -B build-unit-tests -G Ninja",
            "ctest --test-dir build-unit-tests --output-on-failure",
            "build-unit-tests-sanitized",
            "-fsanitize=address,undefined",
            "./tests/run_repository_contracts.sh",
            "needs: validation",
        ):
            self.assertIn(token, workflow)

        self.assertNotIn("on:\n  push:", workflow)

    def test_library_ci_runs_the_real_private_bench_consumer(self) -> None:
        workflow = read(".github/workflows/ci.yml")

        for token in (
            "integration-consumer:",
            "if: github.event_name == 'push' && github.ref == 'refs/heads/main'",
            "repository: SiboVG/CspLibparamFprimeTest",
            "ssh-key: ${{ secrets.CSPBENCH_DEPLOY_KEY }}",
            "path: CspLibparamFprimeTest",
            "path: fprime-csp-libparam",
            "persist-credentials: false",
            "./scripts/bootstrap_dependencies.sh",
            ".venv/bin/fprime-util generate",
            '-DPYTHON="$PWD/.venv/bin/python"',
            ".venv/bin/fprime-util build",
            "CSP_BENCH_BIN=",
            'ldd_output="$(ldd "$CSP_BENCH_BIN")"',
            'grep -F -q -- "not found"',
            "sudo ./scripts/setup_vcan.sh up cspvcan0",
            "id: setup-vcan",
            "./scripts/run_vcan_type_matrix.sh",
            './scripts/run_vcan_type_matrix.sh "$CSP_BENCH_BIN"',
            "--interface cspvcan0",
            'export CUBESPEC_LOG_DIR="$PWD/integration-logs"',
            "name: consumer-vcan-integration-logs",
            "path: CspLibparamFprimeTest/integration-logs",
            "sudo ./scripts/setup_vcan.sh down cspvcan0",
            "if: always() && steps.setup-vcan.outcome == 'success'",
        ):
            self.assertIn(token, workflow)

        self.assertNotRegex(workflow, r"gh[opsu]_[A-Za-z0-9_]+")
        self.assertNotIn("sudo ./scripts/run_vcan_integration.sh", workflow)
        self.assertNotIn("continue-on-error: true", workflow)

    def test_real_consumer_build_puts_venv_tools_on_path(self) -> None:
        workflow = read(".github/workflows/ci.yml")

        for command in (
            ".venv/bin/fprime-util generate",
            ".venv/bin/fprime-util build",
        ):
            with self.subTest(command=command):
                self.assertRegex(
                    workflow,
                    rf'export PATH="\$PWD/\.venv/bin:\$PATH" &&\s+{re.escape(command)}',
                )

    def test_consumer_vcan_job_installs_the_running_kernels_module_and_fails_closed(
        self,
    ) -> None:
        workflow = read(".github/workflows/ci.yml")
        setup_command = "sudo ./scripts/setup_vcan.sh up cspvcan0"

        for token in (
            'kernel_release="$(uname -r)"',
            'extra_package="linux-modules-extra-${kernel_release}"',
            'apt-cache policy "$extra_package"',
            'candidate="$(apt-cache policy "$extra_package" |',
            'if [[ -z "$candidate" || "$candidate" == "(none)" ]]; then',
            'echo "::error::No vcan kernel-module package for running kernel',
            'ls -la "/lib/modules/${kernel_release}" || true',
            "exit 1",
            'sudo apt-get install -y "$extra_package"',
            'sudo depmod -a "$kernel_release"',
            "sudo modprobe vcan",
            "modinfo -F filename vcan",
            "test -d /sys/module/vcan",
        ):
            self.assertIn(token, workflow)

        self.assertRegex(
            workflow,
            rf"(?s)sudo modprobe vcan.*{re.escape(setup_command)}",
        )
        self.assertNotIn("sudo modprobe vcan || true", workflow)
        self.assertNotIn("continue-on-error: true", workflow)


if __name__ == "__main__":
    unittest.main()
