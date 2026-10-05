#!/usr/bin/env bash
# Playable Ambience 0.1.0 per-user installer. Installs ONLY this product; no sudo; existing copies of this same
# product are moved to a timestamped backup; other vendors' plugins and your presets are never touched.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TS="$(date +%Y%m%d-%H%M%S)"
BACKUP="${HOME}/Library/Application Support/Playable Ambience/Backups/${TS}"
APP_DIR="${HOME}/Applications"
VST3_DIR="${HOME}/Library/Audio/Plug-Ins/VST3"
COMP_DIR="${HOME}/Library/Audio/Plug-Ins/Components"

declare -a SRC=( "${HERE}/Applications/Playable Ambience.app"
                 "${HERE}/Plugins/VST3/Playable Ambience.vst3"
                 "${HERE}/Plugins/AU/Playable Ambience.component"
                 "${HERE}/Plugins/AU/Playable Ambience Audio.component" )
declare -a DST=( "${APP_DIR}/Playable Ambience.app"
                 "${VST3_DIR}/Playable Ambience.vst3"
                 "${COMP_DIR}/Playable Ambience.component"
                 "${COMP_DIR}/Playable Ambience Audio.component" )

bundle_id() { /usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$1/Contents/Info.plist" 2>/dev/null || echo ""; }

echo "Playable Ambience installer (per-user, no administrator rights needed)"
for i in "${!SRC[@]}"; do
  s="${SRC[$i]}"
  [[ -d "${s}" ]] || { echo "Missing in package: ${s}"; exit 1; }
  case "$(bundle_id "${s}")" in com.arn.playableambience*) ;; *) echo "Unexpected bundle identifier in ${s}"; exit 1;; esac
  codesign --verify --strict "${s}" 2>/dev/null || echo "Note: ${s##*/} signature could not be verified (local ad-hoc build)."
done
mkdir -p "${APP_DIR}" "${VST3_DIR}" "${COMP_DIR}"
for i in "${!SRC[@]}"; do
  s="${SRC[$i]}"; d="${DST[$i]}"
  if [[ -e "${d}" ]]; then
    case "$(bundle_id "${d}")" in
      com.arn.playableambience*) mkdir -p "${BACKUP}"; mv "${d}" "${BACKUP}/"; echo "Backed up previous ${d##*/} to ${BACKUP}";;
      *) echo "Refusing to replace ${d}: it is not a Playable Ambience bundle."; exit 1;;
    esac
  fi
  tmp="${d}.installing-${TS}"
  ditto "${s}" "${tmp}"
  mv "${tmp}" "${d}"
  echo "Installed ${d}"
done

echo
echo "These are local, ad-hoc signed developer builds (not notarized)."
read -r -p "Remove the download quarantine flag from the four Playable Ambience bundles just installed (only these)? [y/N] " ans || ans="n"
if [[ "${ans}" == "y" || "${ans}" == "Y" ]]; then
  for d in "${DST[@]}"; do xattr -dr com.apple.quarantine "${d}" 2>/dev/null || true; done
  echo "Quarantine flag removed from the Playable Ambience bundles."
else
  echo "Left unchanged. If macOS blocks the app: Control-click it in ~/Applications > Open, or System Settings > Privacy & Security > Open Anyway."
fi
cat <<EOF

Installed:
  ${APP_DIR}/Playable Ambience.app
  ${VST3_DIR}/Playable Ambience.vst3
  ${COMP_DIR}/Playable Ambience.component        (MIDI-controlled AU, aumf PaAm Arnv)
  ${COMP_DIR}/Playable Ambience Audio.component  (Audio AU, aufx PaAa Arnv)

Next:
  - Standalone: open ~/Applications/Playable Ambience.app and press Play.
  - Ableton Live: Preferences > Plug-Ins > enable "Use VST3 Plug-in Custom Folder" or system folders, then Rescan.
  - Logic Pro: restart Logic (it rescans AUs on launch); use Plug-in Manager if a component is marked failed.
Your presets in ~/Library/Application Support/Playable Ambience/Presets are untouched.
EOF
