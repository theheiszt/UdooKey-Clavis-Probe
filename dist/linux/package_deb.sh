#!/usr/bin/env bash
# ==============================================================================
# UDOO KEY Workbench — Debian (.deb) Packaging Script via CPack
# Generates native .deb package for Ubuntu 22.04/24.04 and Debian 12/13
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
APP_DIR="${PROJECT_ROOT}/udoo-key-app"
BUILD_DIR="${APP_DIR}/build-linux"
OUTPUT_DIR="${PROJECT_ROOT}/dist/linux/output"

echo "=== Generating Debian (.deb) Package via CPack ==="

if [ ! -f "${BUILD_DIR}/udoo-key-app" ]; then
    echo "Executable not found. Running build first..."
    "${SCRIPT_DIR}/build_linux.sh" Release
fi

mkdir -p "${OUTPUT_DIR}"
cd "${BUILD_DIR}"

# Run CPack to generate DEB package
cpack -G DEB

# Move generated .deb packages to output directory
mv *.deb "${OUTPUT_DIR}/" 2>/dev/null || true

echo "=== Debian Package Complete ==="
ls -lh "${OUTPUT_DIR}"/*.deb
