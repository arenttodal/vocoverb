# Third-party notices — Playable Ambience 0.1.0 (evaluation build)

| Component | Version / revision | Licence | Use |
|---|---|---|---|
| JUCE framework | 8.0.15 (commit 91ad83ae34a81e0833b1a2b0866f54846370ae53) | Dual: AGPLv3 or commercial JUCE 8 licence (see JUCE-LICENSE.md) | Plugin wrappers, GUI, audio/MIDI devices, file I/O |
| Steinberg VST3 SDK (bundled in JUCE) | as shipped with JUCE 8.0.15 | MIT (VST3_SDK-LICENSE.txt) | VST3 wrapper |
| Apple AudioUnitSDK (bundled in JUCE) | as shipped with JUCE 8.0.15 | Apache 2.0 (AudioUnitSDK-LICENSE.txt) | AU wrappers |
| zlib, libpng, FLAC, SheenBidi (bundled in JUCE) | as shipped with JUCE 8.0.15 | see the respective licence files | compression, PNG screenshots, FLAC import, text layout |
| Inter typeface | 4.1 (Regular, Medium, SemiBold, Bold) | SIL Open Font License 1.1 (Inter-OFL-LICENSE.txt) | Embedded UI font |
| Tracktion pluginval | v1.0.4 | GPLv3 | Validation tool only; downloaded at validation time, never bundled |

All DSP (vocoders, resonator, shifters, delays, reverbs, FFT) is original project code. The Plate follows the
published plate-reverb topology described by Jon Dattorro, "Effect Design, Part 1: Reverberator and Other
Filters", J. Audio Eng. Soc. 45(9), 1997; no third-party reverb code is used. The Wash FDN follows public FDN
research (e.g. Schlecht & Habets, "On Lossless Feedback Delay Networks", IEEE TSP 2017). No Eventide, Chase Bliss,
Zynaptiq, BLEASS, Ableton or Kinotone code or assets are used.

**Licensing basis of this evaluation build.** No commercial JUCE licence configuration was found or used. This
local evaluation build is prepared under JUCE's AGPLv3 option; the complete corresponding source is provided in
the source archive. Commercial redistribution would require either AGPLv3 compliance or a commercial JUCE licence
— that decision is outside this build task.
