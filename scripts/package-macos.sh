#!/usr/bin/env bash
# Assembles ad-hoc signed release products + docs into a versioned ZIP (and a DMG when hdiutil works),
# plus a source archive. Usage: package-macos.sh [--arch-label universal|arm64|x86_64] [build-dir]
source "$(dirname "$0")/common.sh"
LABEL="universal"
if [[ "${1:-}" == "--arch-label" ]]; then LABEL="$2"; shift 2; fi
BUILD_DIR="${1:-$(cat "${PA_ROOT}/out/last-build-dir" 2>/dev/null)}"
[[ "$(uname)" == "Darwin" ]] || die "macOS only"
[[ -d "${BUILD_DIR}" ]] || die "no build dir"
ART="${BUILD_DIR}/PlayableAmbience_artefacts/Release"
NAME="Playable-Ambience-${PA_VERSION}-macOS-${LABEL}-local-build"
DIST="${PA_ROOT}/dist"
STAGE_ROOT="${DIST}/stage"
STAGE="${STAGE_ROOT}/${NAME}"
rm -rf "${STAGE_ROOT:?}"
mkdir -p "${STAGE}/Applications" "${STAGE}/Plugins/VST3" "${STAGE}/Plugins/AU" "${STAGE}/Presets" "${STAGE}/THIRD-PARTY-NOTICES"
ditto "${ART}/Standalone/Playable Ambience.app" "${STAGE}/Applications/Playable Ambience.app"
ditto "${ART}/VST3/Playable Ambience.vst3" "${STAGE}/Plugins/VST3/Playable Ambience.vst3"
ditto "${ART}/AU/Playable Ambience.component" "${STAGE}/Plugins/AU/Playable Ambience.component"
ditto "${BUILD_DIR}/PlayableAmbienceAudio_artefacts/Release/AU/Playable Ambience Audio.component" "${STAGE}/Plugins/AU/Playable Ambience Audio.component"

# Ad-hoc signing, inside out. No Developer ID credentials => local developer build, NOT notarized.
ENT="${PA_ROOT}/Resources/macOS/PlayableAmbience.entitlements"
sign() { codesign --force --timestamp=none --options runtime --sign - "$@"; }
for b in "${STAGE}/Plugins/VST3/Playable Ambience.vst3" "${STAGE}/Plugins/AU/Playable Ambience.component" "${STAGE}/Plugins/AU/Playable Ambience Audio.component"; do
  find "${b}/Contents" -type f \( -name "*.dylib" -o -name "*.so" \) -print0 | while IFS= read -r -d '' f; do sign "$f"; done
  sign "${b}"
done
sign --entitlements "${ENT}" "${STAGE}/Applications/Playable Ambience.app"
for b in "${STAGE}/Applications/Playable Ambience.app" "${STAGE}/Plugins/VST3/Playable Ambience.vst3" "${STAGE}/Plugins/AU/"*.component; do
  codesign --verify --deep --strict "${b}" || die "signature verification failed: ${b}"
done

# Content
RENDER="${BUILD_DIR}/pa_render"
"${RENDER}" --export-presets "${STAGE}/Presets" > /dev/null
if [[ -d "${PA_ROOT}/out/Audio-Examples" ]]; then ditto "${PA_ROOT}/out/Audio-Examples" "${STAGE}/Audio-Examples"
else "${RENDER}" --out "${STAGE}/Audio-Examples" > /dev/null; fi
cp "${PA_ROOT}/docs/Quick-Start.md" "${STAGE}/Quick-Start.md"
cp "${PA_ROOT}/docs/Feedback-Template.md" "${STAGE}/Feedback-Template.md"
if [[ -f "${PA_ROOT}/out/reports/Validation-Report.md" ]]; then cp "${PA_ROOT}/out/reports/Validation-Report.md" "${STAGE}/Validation-Report.md"
else cp "${PA_ROOT}/docs/Validation-Report.md" "${STAGE}/Validation-Report.md"; fi
mkdir -p "${STAGE}/Validation"
cp "${PA_ROOT}/out/reports/"*.txt "${PA_ROOT}/out/reports/"*.md "${STAGE}/Validation/" 2>/dev/null || true
if [[ -d "${PA_ROOT}/out/screenshots" ]]; then ditto "${PA_ROOT}/out/screenshots" "${STAGE}/Validation/screenshots"; fi
cp "${PA_ROOT}/THIRD-PARTY-NOTICES/"* "${STAGE}/THIRD-PARTY-NOTICES/"
cp "${PA_ROOT}/scripts/install-user.command" "${STAGE}/Install Playable Ambience.command"
cp "${PA_ROOT}/scripts/uninstall-user.command" "${STAGE}/Uninstall Playable Ambience.command"
chmod +x "${STAGE}/"*.command
APP_EXE="${STAGE}/Applications/Playable Ambience.app/Contents/MacOS/Playable Ambience"
cat > "${STAGE}/Build-Info.json" <<JSON
{
  "product": "Playable Ambience",
  "version": "${PA_VERSION}",
  "git": "$(git -C "${PA_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)",
  "buildTimeUTC": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "architectures": "$(lipo -archs "${APP_EXE}")",
  "deploymentTarget": "12.0",
  "builtOn": "macOS $(sw_vers -productVersion) $(uname -m)",
  "compiler": "$(clang --version | head -1)",
  "dependencies": { "JUCE": "${PA_JUCE_TAG} ${PA_JUCE_COMMIT}", "Inter font": "4.1 (SIL OFL 1.1)", "pluginval (validation only)": "${PA_PLUGINVAL_TAG}" },
  "signing": "ad-hoc (local developer build, not notarized)",
  "products": ["Playable Ambience.app", "Playable Ambience.vst3", "Playable Ambience.component (aumf PaAm Arnv)", "Playable Ambience Audio.component (aufx PaAa Arnv)"]
}
JSON
( cd "${STAGE}" && find . -type f ! -name SHA256SUMS.txt -print0 | sort -z | xargs -0 shasum -a 256 > SHA256SUMS.txt )

mkdir -p "${DIST}"
rm -f "${DIST:?}/${NAME:?}.zip" "${DIST:?}/${NAME:?}.dmg"
ditto -c -k --sequesterRsrc --keepParent "${STAGE}" "${DIST}/${NAME}.zip"
log "zip: ${DIST}/${NAME}.zip"
if have hdiutil; then
  if hdiutil create -quiet -volname "Playable Ambience ${PA_VERSION}" -srcfolder "${STAGE}" -ov -format UDZO "${DIST}/${NAME}.dmg"; then
    log "dmg: ${DIST}/${NAME}.dmg"
  else
    log "DMG creation failed (ZIP remains the deliverable)"
  fi
fi
git -C "${PA_ROOT}" archive --format=zip --prefix="Playable-Ambience-${PA_VERSION}-source/" -o "${DIST}/Playable-Ambience-${PA_VERSION}-source.zip" HEAD
( cd "${DIST}" && shasum -a 256 ./*.zip ./*.dmg 2>/dev/null > "SHA256SUMS-${LABEL}.txt" || true )

# Verify the extracted archive, not just the build tree.
CHECK="${DIST}/verify"
rm -rf "${CHECK:?}"
mkdir -p "${CHECK}"
ditto -x -k "${DIST}/${NAME}.zip" "${CHECK}"
for b in "${CHECK}/${NAME}/Applications/Playable Ambience.app" "${CHECK}/${NAME}/Plugins/VST3/Playable Ambience.vst3" "${CHECK}/${NAME}/Plugins/AU/"*.component; do
  codesign --verify --deep --strict "${b}" || die "extracted bundle signature invalid: ${b}"
done
"${CHECK}/${NAME}/Applications/Playable Ambience.app/Contents/MacOS/Playable Ambience" --selftest --report "${PA_ROOT}/out/reports/extracted-selftest.txt" \
  || die "extracted app self-test failed"
rm -rf "${CHECK:?}"
log "package OK"
