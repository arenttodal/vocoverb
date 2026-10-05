#!/usr/bin/env bash
# One command: bootstrap -> build -> test -> validate -> package -> summary.  Usage: build-and-package-macos.sh [universal|arm64|x86_64]
source "$(dirname "$0")/common.sh"
ARCH="${1:-universal}"
S="${PA_ROOT}/scripts"
if ! "${S}/build-macos.sh" --arch "${ARCH}"; then
  log "build for ${ARCH} failed; trying the native architecture so a working release is preserved"
  ARCH="$(uname -m)"
  "${S}/build-macos.sh" --arch "${ARCH}"
fi
BD="$(cat "${PA_ROOT}/out/last-build-dir")"
set +e
"${S}/test.sh" "${BD}"; T=$?
"${S}/validate-macos.sh" "${BD}"; V=$?
python3 "${S}/make-validation-report.py" --platform macOS || true
"${S}/package-macos.sh" --arch-label "${ARCH}" "${BD}"; P=$?
set -e
echo
log "SUMMARY  arch=${ARCH}  tests=$([[ $T -eq 0 ]] && echo PASS || echo FAIL)  validation=$([[ $V -eq 0 ]] && echo PASS || echo FAIL)  package=$([[ $P -eq 0 ]] && echo OK || echo FAIL)"
ls -la "${PA_ROOT}/dist" 2>/dev/null || true
[[ $P -eq 0 ]]
