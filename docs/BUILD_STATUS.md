# Build status (kept current during the unattended build)

## Environment
- Development container: Linux x86_64 (no macOS, no Xcode). Mac binaries are produced by the GitHub Actions
  workflow `.github/workflows/build.yml` on `macos-15` runners for the repository's own branch.
- Local commands: `./scripts/build-linux.sh` (Linux dev build), `./scripts/test.sh`.
- Mac one-command build: `./scripts/build-and-package-macos.sh universal` (or `arm64`).

## Completed
- Core DSP engine, all modes (see README coverage table); 32 headless tests passing.
- JUCE VST3 + Standalone build on Linux; standalone `--selftest` passing (demo audio, 16 presets, state roundtrip,
  user preset save/load, offline export, editor + screenshots).
- pluginval v1.0.4 strictness 5 on the Linux VST3: SUCCESS.
- Benchmarks (`pa_bench`), audio comparisons (`pa_render`), packaging/validation/install scripts, CI workflow.

## In progress / next actions
- Run the macOS CI job; fix any AppleClang/AU-specific issues it reveals; collect auval + pluginval reports.
- UI polish passes against the mockup at the documented sizes.

## Known limitations
- Live/Logic host tests, hardware live input and listening tests: not possible here (documented in the report).
- Signing: ad-hoc only (no Developer ID credentials).
