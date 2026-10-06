#!/usr/bin/env bash
# Release build of all macOS products. Usage: build-macos.sh [--arch universal|arm64|x86_64] [--build-dir DIR]
source "$(dirname "$0")/common.sh"
ARCH="universal"; BUILD_DIR=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch) ARCH="$2"; shift 2;;
    --build-dir) BUILD_DIR="$2"; shift 2;;
    *) die "unknown option $1";;
  esac
done
[[ "$(uname)" == "Darwin" ]] || die "build-macos.sh must run on macOS (use scripts/build-linux.sh elsewhere)"
case "${ARCH}" in
  universal) ARCHS="arm64;x86_64";;
  arm64|x86_64) ARCHS="${ARCH}";;
  *) die "bad --arch ${ARCH}";;
esac
BUILD_DIR="${BUILD_DIR:-${PA_ROOT}/build-macos-${ARCH}}"
"${PA_ROOT}/scripts/bootstrap.sh"
GEN="Unix Makefiles"; have ninja && GEN="Ninja"
log "Configuring (${ARCHS}, ${GEN}) in ${BUILD_DIR}"
cmake -S "${PA_ROOT}" -B "${BUILD_DIR}" -G "${GEN}" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DPA_JUCE_DIR="${PA_ROOT}/.deps/JUCE" 2>&1 | tee "${PA_LOG_DIR}/configure-${ARCH}.log"
log "Building"
cmake --build "${BUILD_DIR}" --config Release --parallel "$(sysctl -n hw.ncpu)" \
  --target PlayableAmbience_All PlayableAmbienceAudio_All pa_tests pa_render pa_bench pa_hostcheck 2>&1 | tee "${PA_LOG_DIR}/build-${ARCH}.log"
ART="${BUILD_DIR}/PlayableAmbience_artefacts/Release"
for p in "${ART}/Standalone/Playable Ambience.app" "${ART}/VST3/Playable Ambience.vst3" "${ART}/AU/Playable Ambience.component" \
         "${BUILD_DIR}/PlayableAmbienceAudio_artefacts/Release/AU/Playable Ambience Audio.component"; do
  [[ -d "$p" ]] || die "missing build product: $p"
  log "built: $p"
done
echo "${BUILD_DIR}" > "${PA_ROOT}/out/last-build-dir"
log "build OK (${ARCH})"
