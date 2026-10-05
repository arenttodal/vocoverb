#!/usr/bin/env bash
# Developer build on Linux (VST3 + standalone + tests). Not a macOS release.
source "$(dirname "$0")/common.sh"
BUILD_DIR="${1:-${PA_ROOT}/build}"
"${PA_ROOT}/scripts/bootstrap.sh"
GEN="Unix Makefiles"; have ninja && GEN="Ninja"
cmake -S "${PA_ROOT}" -B "${BUILD_DIR}" -G "${GEN}" -DCMAKE_BUILD_TYPE=Release -DPA_JUCE_DIR="${PA_ROOT}/.deps/JUCE"
cmake --build "${BUILD_DIR}" --parallel
echo "${BUILD_DIR}" > "${PA_ROOT}/out/last-build-dir"
