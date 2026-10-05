#!/usr/bin/env bash
# Wrapper validation on macOS: installs ONLY this product into the per-user plugin folders, runs auval for both AU
# types and pluginval for VST3/AU, then checks architectures, linked libraries and signatures. Writes a report.
source "$(dirname "$0")/common.sh"
BUILD_DIR="${1:-$(cat "${PA_ROOT}/out/last-build-dir" 2>/dev/null)}"
[[ -n "${BUILD_DIR}" && -d "${BUILD_DIR}" ]] || die "no build dir"
[[ "$(uname)" == "Darwin" ]] || die "macOS only"
REPORT_DIR="${PA_ROOT}/out/reports"; mkdir -p "${REPORT_DIR}"
R="${REPORT_DIR}/validation-macos.md"
ART="${BUILD_DIR}/PlayableAmbience_artefacts/Release"
APP="${ART}/Standalone/Playable Ambience.app"
VST3="${ART}/VST3/Playable Ambience.vst3"
AUM="${ART}/AU/Playable Ambience.component"
AUA="${BUILD_DIR}/PlayableAmbienceAudio_artefacts/Release/AU/Playable Ambience Audio.component"
COMP_DIR="${HOME}/Library/Audio/Plug-Ins/Components"
VST3_DIR="${HOME}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${COMP_DIR}" "${VST3_DIR}"
status_line() { printf '| %s | %s | %s |\n' "$1" "$2" "$3" >> "${R}"; }
{
  echo "# macOS wrapper validation"
  echo
  echo "- Date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "- macOS: $(sw_vers -productVersion) ($(uname -m)), $(xcodebuild -version 2>/dev/null | head -1 || echo 'Xcode unknown')"
  echo "- Build dir: ${BUILD_DIR}"
  echo
  echo "| Check | Result | Detail |"
  echo "|---|---|---|"
} > "${R}"

# Install only this product into the user folders (needed for auval/pluginval AU discovery).
ditto "${AUM}" "${COMP_DIR}/Playable Ambience.component"
ditto "${AUA}" "${COMP_DIR}/Playable Ambience Audio.component"
ditto "${VST3}" "${VST3_DIR}/Playable Ambience.vst3"
if [[ "${PA_CI:-0}" == "1" ]]; then killall -9 AudioComponentRegistrar 2>/dev/null || true; sleep 2; fi

fails=0
run_check() { # name, logfile, command...
  local name="$1" logf="$2"; shift 2
  if "$@" > "${logf}" 2>&1; then status_line "${name}" "PASS" "$(basename "${logf}")"
  else local rc=$?; status_line "${name}" "FAIL" "$(basename "${logf}") (exit ${rc})"; fails=$((fails+1)); fi
}
run_check "auval -strict aumf PaAm Arnv (MIDI-controlled AU)" "${REPORT_DIR}/auval-aumf.txt" auval -strict -v aumf PaAm Arnv
run_check "auval -strict aufx PaAa Arnv (Audio AU companion)" "${REPORT_DIR}/auval-aufx.txt" auval -strict -v aufx PaAa Arnv

PV="${PA_ROOT}/.deps/pluginval.app/Contents/MacOS/pluginval"
if [[ ! -x "${PV}" ]]; then
  ( cd "${PA_ROOT}/.deps" && curl -sSL -o pv-mac.zip "https://github.com/Tracktion/pluginval/releases/download/${PA_PLUGINVAL_TAG}/pluginval_macOS.zip" && unzip -o -q pv-mac.zip ) || true
fi
if [[ -x "${PV}" ]]; then
  run_check "pluginval ${PA_PLUGINVAL_TAG} strictness 8: VST3" "${REPORT_DIR}/pluginval-vst3.txt" "${PV}" --strictness-level 8 --validate-in-process --timeout-ms 900000 "${VST3_DIR}/Playable Ambience.vst3"
  run_check "pluginval ${PA_PLUGINVAL_TAG} strictness 8: MIDI AU" "${REPORT_DIR}/pluginval-au.txt" "${PV}" --strictness-level 8 --validate-in-process --timeout-ms 900000 "${COMP_DIR}/Playable Ambience.component"
  run_check "pluginval ${PA_PLUGINVAL_TAG} strictness 8: Audio AU" "${REPORT_DIR}/pluginval-au-audio.txt" "${PV}" --strictness-level 8 --validate-in-process --timeout-ms 900000 "${COMP_DIR}/Playable Ambience Audio.component"
else
  status_line "pluginval" "NOT PERFORMED" "pluginval could not be downloaded"
fi

for b in "${APP}" "${VST3}" "${AUM}" "${AUA}"; do
  exe="$(find "${b}/Contents/MacOS" -type f -perm -u+x | head -1)"
  archs="$(lipo -archs "${exe}" 2>/dev/null || echo unknown)"
  libs="$(otool -L "${exe}" | tail -n +2 | awk '{print $1}' | grep -v -E '^/System/|^/usr/lib/' | tr '\n' ' ' || true)"
  if codesign --verify --strict "${b}" >/dev/null 2>&1; then sv="valid"; else sv="unsigned/invalid (packaging re-signs)"; fi
  status_line "$(basename "${b}") binary" "${archs}" "non-system libs: ${libs:-none}; build-tree signature ${sv}"
  [[ -z "${libs}" ]] || fails=$((fails+1))
done
{
  echo
  echo "Gatekeeper (spctl) is expected to reject ad-hoc signed local builds (not notarized):"
  echo '```'
  spctl --assess --type execute -vv "${APP}" 2>&1 || true
  echo '```'
  echo
  echo "Installed for validation (user folders only): ${COMP_DIR}/Playable Ambience.component, ${COMP_DIR}/Playable Ambience Audio.component, ${VST3_DIR}/Playable Ambience.vst3"
} >> "${R}"
log "validation report: ${R} (${fails} failures)"
[[ ${fails} -eq 0 ]]
