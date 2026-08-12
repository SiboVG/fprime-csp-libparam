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
RELEASE_TAG = "csh/2.10"

MODULES = {
    "Runtime": ("Stack.cpp", "Stack.hpp"),
    "SocketCan": ("SocketCanTransport.cpp", "SocketCanTransport.hpp"),
    "Libparam": (
        "Libparam.cpp",
        "Libparam.hpp",
        "libparam_bridge.c",
        "libparam_bridge.h",
    ),
}


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
        files.extend(path for path in root.rglob("*") if path.is_file())
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
        ):
            self.assertIn(token, cmake)
        self.assertIn("--target FprimeCsp_Runtime", runner)

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


class DocumentationContract(unittest.TestCase):
    def test_repository_owns_its_documentation_interface(self) -> None:
        mkdocs = read("mkdocs.yml")
        requirements = read("requirements-docs.txt")
        index = read("docs/index.md")
        gitignore = read(".gitignore")

        self.assertIn("strict: true", mkdocs)
        for page in ("index.md", "architecture.md", "integration.md", "extending.md"):
            self.assertIn(page, mkdocs)
        self.assertIn("mkdocs-material", requirements)
        self.assertIn("fprime-csp-libparam", index)
        self.assertIn("site/", gitignore)

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
            "on:\n  push:\n    branches:\n      - main\n  workflow_dispatch:",
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

        self.assertIn(
            'flowchart TB\n  subgraph GroundPath["F Prime control and observability"]',
            architecture,
        )
        self.assertGreaterEqual(architecture.count("direction LR"), 2)


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
    def test_github_workflow_runs_standalone_contracts_and_strict_docs(self) -> None:
        workflow = read(".github/workflows/ci.yml")

        self.assertIn("ubuntu-24.04", workflow)
        self.assertIn("./tests/run_repository_contracts.sh", workflow)
        self.assertIn("mkdocs build --strict --clean", workflow)


if __name__ == "__main__":
    unittest.main()
