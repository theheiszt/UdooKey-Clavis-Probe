#!/usr/bin/env bash
# ==============================================================================
# UDOO KEY Workbench — Native Linux Build Script
# Targets: Ubuntu 22.04/24.04 LTS, Debian 12/13, Fedora 39+, Arch Linux
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
APP_DIR="${PROJECT_ROOT}/udoo-key-app"
BUILD_DIR="${APP_DIR}/build-linux"

echo "=== Building UDOO KEY Workbench Native Linux Application ==="
echo "Project Root: ${PROJECT_ROOT}"
echo "Source Dir:   ${APP_DIR}"
echo "Build Dir:    ${BUILD_DIR}"

BUILD_TYPE="${1:-Release}"
PARALLEL_JOBS="$(nproc || echo 4)"

# Check for required tools
for cmd in cmake ninja; do
    if ! command -v "$cmd" &> /dev/null; then
        echo "Error: Required tool '$cmd' is not installed."
        echo "Install via: sudo apt-get install cmake ninja-build build-essential"
        exit 1
    fi
done

# Configure with CMake
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${APP_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="/usr"

# Compile application and test suite
ninja -j "${PARALLEL_JOBS}"

# Run CTest verification suite
echo "=== Running CTest Verification Suite ==="
ctest --output-on-failure

echo "=== Build and Tests Successful ==="
echo "Binary created at: ${BUILD_DIR}/udoo-key-app"
echo "To install system-wide: cd ${BUILD_DIR} && sudo ninja install"
