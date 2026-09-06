#!/usr/bin/env bash
set -Eeuo pipefail

readonly TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly CONSUMER_ROOT="$TEST_ROOT/smoke_consumer"
readonly CMAKE_EXECUTABLE="${CMAKE_COMMAND:-cmake}"

usage() {
    echo "Usage: run_runtime_smoke.sh FPRIME_FRAMEWORK_PATH LIBCSP_SOURCE [BUILD_DIR]"
    echo
    echo "Configure the checked-in F Prime consumer and build the reusable modules."
    echo "When BUILD_DIR is omitted, a temporary build directory is removed on exit."
}

fail() {
    echo "ERROR: $*" >&2
    exit 2
}

if (($# == 1)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi
if (($# < 2 || $# > 3)); then
    usage >&2
    exit 2
fi

command -v "$CMAKE_EXECUTABLE" >/dev/null || fail "CMake is required"
[[ -f "$1/cmake/FPrime.cmake" ]] || fail "not an F Prime source tree: $1"
[[ -f "$2/include/csp/csp.h" ]] || fail "not a libcsp source tree: $2"

fprime_source="$(cd "$1" && pwd -P)"
libcsp_source="$(cd "$2" && pwd -P)"
remove_build=false

if (($# == 3)); then
    mkdir -p "$3"
    build_root="$(cd "$3" && pwd -P)"
else
    temporary_parent="$(cd "${TMPDIR:-/tmp}" && pwd -P)"
    build_root="$(mktemp -d "$temporary_parent/fprimecsp-runtime-smoke.XXXXXX")"
    remove_build=true
fi

cleanup() {
    if $remove_build; then
        "$CMAKE_EXECUTABLE" -E remove_directory "$build_root"
    fi
}
trap cleanup EXIT

"$CMAKE_EXECUTABLE" \
    -S "$CONSUMER_ROOT" \
    -B "$build_root" \
    -DSMOKE_FPRIME_FRAMEWORK_PATH="$fprime_source" \
    -DSMOKE_LIBCSP_SOURCE="$libcsp_source"
"$CMAKE_EXECUTABLE" --build "$build_root" --target FprimeCsp_Runtime FprimeCsp_Libparam

echo "FprimeCsp Runtime and Libparam smoke build passed."
