#!/usr/bin/env bash
# Removes only the Playable Ambience binaries installed per-user. User presets are kept.
set -euo pipefail
declare -a DST=( "${HOME:?}/Applications/Playable Ambience.app"
                 "${HOME:?}/Library/Audio/Plug-Ins/VST3/Playable Ambience.vst3"
                 "${HOME:?}/Library/Audio/Plug-Ins/Components/Playable Ambience.component"
                 "${HOME:?}/Library/Audio/Plug-Ins/Components/Playable Ambience Audio.component" )
bundle_id() { /usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$1/Contents/Info.plist" 2>/dev/null || echo ""; }
for d in "${DST[@]}"; do
  if [[ -d "${d}" ]]; then
    case "$(bundle_id "${d}")" in
      com.arn.playableambience*) rm -rf "${d:?}"; echo "Removed ${d}";;
      *) echo "Skipped ${d}: not a Playable Ambience bundle";;
    esac
  fi
done
echo "Presets kept in ${HOME}/Library/Application Support/Playable Ambience/Presets (delete manually if wanted)."
echo "Rescan plug-ins in your DAW."
