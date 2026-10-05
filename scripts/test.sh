#!/usr/bin/env bash
# Core + offline-render tests and the standalone self-test (no audio device, no microphone).
source "$(dirname "$0")/common.sh"
BUILD_DIR="${1:-$(cat "${PA_ROOT}/out/last-build-dir" 2>/dev/null || echo "${PA_ROOT}/build")}"
REPORT_DIR="${PA_ROOT}/out/reports"; mkdir -p "${REPORT_DIR}"
TESTS="${BUILD_DIR}/pa_tests"; [[ -x "${TESTS}" ]] || TESTS="${BUILD_DIR}/Release/pa_tests"
[[ -x "${TESTS}" ]] || die "pa_tests not built in ${BUILD_DIR}"
log "Running core tests"
set +e
"${TESTS}" --report "${REPORT_DIR}/core-tests.txt"; core=$?
set -e
APP=""
if [[ "$(uname)" == "Darwin" ]]; then APP="${BUILD_DIR}/PlayableAmbience_artefacts/Release/Standalone/Playable Ambience.app/Contents/MacOS/Playable Ambience"
else APP="${BUILD_DIR}/PlayableAmbience_artefacts/Release/Standalone/Playable Ambience"; fi
selftest=0
if [[ -x "${APP}" ]]; then
  log "Running standalone self-test with screenshots"
  set +e
  if [[ "$(uname)" != "Darwin" ]] && have xvfb-run; then
    timeout 600 xvfb-run -a "${APP}" --selftest --screenshots "${PA_ROOT}/out/screenshots" --report "${REPORT_DIR}/standalone-selftest.txt"; selftest=$?
  else
    "${APP}" --selftest --screenshots "${PA_ROOT}/out/screenshots" --report "${REPORT_DIR}/standalone-selftest.txt"; selftest=$?
  fi
  set -e
else
  log "standalone binary not found; self-test skipped"
fi
RENDER="${BUILD_DIR}/pa_render"; [[ -x "${RENDER}" ]] || RENDER="${BUILD_DIR}/Release/pa_render"
if [[ -x "${RENDER}" ]]; then
  log "Rendering audio comparisons"
  "${RENDER}" --out "${PA_ROOT}/out/Audio-Examples" > "${REPORT_DIR}/render.txt" 2>&1 || core=1
fi
BENCH="${BUILD_DIR}/pa_bench"; [[ -x "${BENCH}" ]] || BENCH="${BUILD_DIR}/Release/pa_bench"
[[ -x "${BENCH}" ]] && "${BENCH}" --seconds 20 > "${REPORT_DIR}/benchmark.txt" 2>&1 || true
log "core tests exit ${core}, self-test exit ${selftest}"
[[ ${core} -eq 0 && ${selftest} -eq 0 ]]
