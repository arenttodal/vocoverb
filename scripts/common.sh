#!/usr/bin/env bash
# Shared helpers for Playable Ambience build scripts (sourced, not executed).
set -euo pipefail
PA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PA_VERSION="0.1.0"
PA_JUCE_TAG="8.0.15"
PA_JUCE_COMMIT="91ad83ae34a81e0833b1a2b0866f54846370ae53"
PA_PLUGINVAL_TAG="v1.0.4"
PA_LOG_DIR="${PA_ROOT}/out/logs"
mkdir -p "${PA_LOG_DIR}"
log() { printf '[playable-ambience] %s\n' "$*"; }
die() { printf '[playable-ambience] ERROR: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }
