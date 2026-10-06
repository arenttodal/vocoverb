#!/usr/bin/env bash
# Builds the standalone (Linux/X11 or macOS), runs the self-test capture and compares with the reference.
# Usage: tools/visual/capture.sh [build-dir] [out-dir]
set -euo pipefail
B="${1:-build}"; O="${2:-out/visual}"
cmake --build "$B" --target PlayableAmbience_Standalone --parallel >/dev/null
rm -rf "$O/shots"; mkdir -p "$O/shots"
APP="$B/PlayableAmbience_artefacts/Release/Standalone/Playable Ambience"
[[ -d "$APP.app" ]] && APP="$APP.app/Contents/MacOS/Playable Ambience"
if command -v xvfb-run >/dev/null && [[ "$(uname)" != Darwin ]]; then xvfb-run -a "$APP" --selftest --screenshots "$O/shots" > "$O/shots/selftest.txt"
else "$APP" --selftest --screenshots "$O/shots" > "$O/shots/selftest.txt"; fi
python3 tools/visual/compare.py references/playable-ambience-approved-gui.png "$O/shots/reference-fixture-1536x1024.png" "$O/compare" 2>/dev/null
