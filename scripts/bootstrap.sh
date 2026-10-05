#!/usr/bin/env bash
# Inspect the toolchain and fetch the pinned JUCE revision into .deps/JUCE (no prompts, no design choices).
source "$(dirname "$0")/common.sh"
log "Toolchain:"
for t in cmake git clang++ g++ ninja xcodebuild codesign hdiutil ditto lipo auval; do
  if have "$t"; then printf '  %-11s %s\n' "$t" "$(command -v "$t")"; else printf '  %-11s (not found)\n' "$t"; fi
done
have cmake || die "cmake is required (>= 3.22). On macOS: 'brew install cmake' or https://cmake.org/download/"
have git || die "git is required"
if [[ "$(uname)" == "Darwin" ]]; then
  xcode-select -p >/dev/null 2>&1 || die "Xcode command line tools missing. Run: xcode-select --install (requires your approval)."
  log "SDK: $(xcrun --show-sdk-path 2>/dev/null || echo unknown)"
fi
JUCE_DIR="${PA_ROOT}/.deps/JUCE"
if [[ -d "${JUCE_DIR}/.git" ]]; then
  have_commit="$(git -C "${JUCE_DIR}" rev-parse HEAD)"
  [[ "${have_commit}" == "${PA_JUCE_COMMIT}" ]] || die "JUCE checkout at ${have_commit}, expected ${PA_JUCE_COMMIT}. Remove .deps/JUCE and re-run."
  log "JUCE ${PA_JUCE_TAG} already present (${have_commit})"
else
  mkdir -p "${PA_ROOT}/.deps"
  log "Cloning JUCE ${PA_JUCE_TAG}..."
  for i in 1 2 3 4; do
    git clone -q --depth 1 --branch "${PA_JUCE_TAG}" https://github.com/juce-framework/JUCE.git "${JUCE_DIR}" && break
    sleep $((2 ** i))
  done
  [[ -d "${JUCE_DIR}" ]] || die "Could not clone JUCE (network?). Supply a local copy at .deps/JUCE (commit ${PA_JUCE_COMMIT})."
  got="$(git -C "${JUCE_DIR}" rev-parse HEAD)"
  [[ "${got}" == "${PA_JUCE_COMMIT}" ]] || die "Tag ${PA_JUCE_TAG} resolved to ${got}, expected ${PA_JUCE_COMMIT}"
fi
log "bootstrap OK"
