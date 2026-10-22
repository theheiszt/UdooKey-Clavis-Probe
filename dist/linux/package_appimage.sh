#!/usr/bin/env bash
# ==============================================================================
# UDOO KEY Workbench — Linux x86_64 AppImage Packaging Script
# Target Architecture: x86_64
# Primary Target Distribution: Ubuntu 24.04 LTS (Noble Numbat)
# Secondary Target Distributions: Debian 12 (Bookworm), Fedora 39/40
# Uses linuxdeploy and linuxdeploy-plugin-qt to bundle Qt 6 dependencies
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
APP_DIR="${PROJECT_ROOT}/udoo-key-app"
BUILD_DIR="${APP_DIR}/build-linux"
APPDIR="${BUILD_DIR}/AppDir"
OUTPUT_DIR="${PROJECT_ROOT}/dist/linux/output"

echo "=== Packaging UDOO KEY Workbench as AppImage ==="

if [ ! -f "${BUILD_DIR}/udoo-key-app" ]; then
    echo "Executable not found. Running build first..."
    "${SCRIPT_DIR}/build_linux.sh" Release
fi

mkdir -p "${OUTPUT_DIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}"

# Stage install tree into AppDir
DESTDIR="${APPDIR}" ninja -C "${BUILD_DIR}" install

# Download linuxdeploy and Qt plugin if not present
TOOLS_DIR="${BUILD_DIR}/tools"
mkdir -p "${TOOLS_DIR}"
cd "${TOOLS_DIR}"

if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
    echo "Downloading linuxdeploy..."
    wget -q -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x linuxdeploy-x86_64.AppImage
fi

if [ ! -f "linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -q -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
fi

# Ensure QMAKE is detectable
if command -v qmake6 &> /dev/null; then
    export QMAKE="$(command -v qmake6)"
elif command -v qmake &> /dev/null; then
    export QMAKE="$(command -v qmake)"
fi

# Bundle application dependencies and package AppImage
export OUTPUT="${OUTPUT_DIR}/UDOO_KEY_Workbench-x86_64.AppImage"

./linuxdeploy-x86_64.AppImage \
    --appdir "${APPDIR}" \
    -d "${SCRIPT_DIR}/udoo-key.desktop" \
    -i "${SCRIPT_DIR}/icons/udoo-key.svg" \
    --plugin qt \
    --output appimage

echo "=== AppImage Package Complete ==="
echo "Artifact: ${OUTPUT}"
