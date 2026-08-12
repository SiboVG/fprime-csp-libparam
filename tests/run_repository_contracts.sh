#!/usr/bin/env bash
set -Eeuo pipefail

readonly TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PYTHON_EXECUTABLE="${PYTHON:-python3}"

command -v "$PYTHON_EXECUTABLE" >/dev/null || {
    echo "ERROR: Python 3 is required to run repository contracts" >&2
    exit 2
}
command -v cmake >/dev/null || {
    echo "ERROR: CMake is required to run the RDP shutdown contract" >&2
    exit 2
}

exec "$PYTHON_EXECUTABLE" -m unittest discover \
    -s "$TEST_ROOT" \
    -p 'test_*.py' \
    -v
