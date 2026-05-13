#!/usr/bin/env bash
# Configure the TrenchBroom fork for a typical Linux dev tree (GCC, Ninja, vcpkg manifest).
# Optional env:
#   PANDOC_PATH      — if pandoc is not on PATH (required by CMakeLists.txt)
#   QT6SVG_DIR       — directory containing Qt6SvgConfig.cmake (e.g. …/lib/x86_64-linux-gnu/cmake/Qt6Svg)
#   QT6WEBSOCKETS_DIR — directory containing Qt6WebSocketsConfig.cmake (same layout as Qt6Svg)
#   EDITOR_BUILD_DIR — CMake build directory (default: <repo>/editor/build)
#   NACHT_EDITOR_WITH_FREEUSD — 1/on (default) passes -DNACHT_EDITOR_WITH_FREEUSD=ON; 0/off disables FreeUSD fetch/link.
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
EDITOR_SRC="${REPO_ROOT}/editor"
BUILD_DIR="${EDITOR_BUILD_DIR:-${EDITOR_SRC}/build}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

CMAKE_EXTRA=()
if [[ -n "${PANDOC_PATH:-}" ]]; then
  CMAKE_EXTRA+=("-DPANDOC_PATH=${PANDOC_PATH}")
fi
if [[ -n "${QT6SVG_DIR:-}" ]]; then
  CMAKE_EXTRA+=("-DQt6Svg_DIR=${QT6SVG_DIR}")
fi
if [[ -n "${QT6WEBSOCKETS_DIR:-}" ]]; then
  CMAKE_EXTRA+=("-DQt6WebSockets_DIR=${QT6WEBSOCKETS_DIR}")
fi

# FreeUSD (.usd/.usda/.usdc) entity previews: default ON (matches editor/cmake/NachtFreeUsd.cmake).
: "${NACHT_EDITOR_WITH_FREEUSD:=1}"
case "${NACHT_EDITOR_WITH_FREEUSD}" in
  0|no|false|OFF|off)
    CMAKE_EXTRA+=("-DNACHT_EDITOR_WITH_FREEUSD=OFF") ;;
  *)
    CMAKE_EXTRA+=("-DNACHT_EDITOR_WITH_FREEUSD=ON") ;;
esac

echo "Configuring: ${EDITOR_SRC} -> ${BUILD_DIR}"
cmake -S "${EDITOR_SRC}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-g++}" \
  -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-gcc}" \
  "${CMAKE_EXTRA[@]}"

echo "Build: cmake --build \"${BUILD_DIR}\" --target TrenchBroom -j\$(nproc)"
