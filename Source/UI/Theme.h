// Design tokens sampled from references/playable-ambience-approved-gui.png (see design/reference/measurements.json)
// and font access. The editor is laid out on one canonical 1536 x 1024 canvas (theme::kCanvasW/H) and scaled uniformly.
#pragma once

#include <JuceHeader.h>

namespace pa::theme
{
inline constexpr int kCanvasW = 1536, kCanvasH = 1024;

// surfaces
inline const juce::Colour backdrop { 0xffebe6de };      // outside the shell
inline const juce::Colour shell { 0xffefe9e0 };
inline const juce::Colour shellTop { 0xfff5efe7 };
inline const juce::Colour shellBottom { 0xffe2d8cb };
inline const juce::Colour cardTop { 0xfff4efe7 };
inline const juce::Colour cardBottom { 0xffe7ddcf };
inline const juce::Colour cardEdgeLight { 0xfffffcf6 };
inline const juce::Colour cardEdgeDark { 0x1f5a4630 };  // lower lip (translucent warm)
inline const juce::Colour shadow { 0x2e3a2614 };        // warm translucent shadow
// text
inline const juce::Colour text { 0xff182221 };
inline const juce::Colour textMuted { 0xff77756f };
inline const juce::Colour textLabel { 0xff2b2f2c };
inline const juce::Colour textDisabled { 0xff9a968b };
// dark wells
inline const juce::Colour display { 0xff192221 };
inline const juce::Colour displayTop { 0xff212b2b };
inline const juce::Colour displayGrid { 0x16e8e2d4 };
inline const juce::Colour displayLabel { 0xffd8d2c6 };
// accents
inline const juce::Colour accent { 0xfff45a1e };
inline const juce::Colour accentTop { 0xffff672b };
inline const juce::Colour accentBottom { 0xffe64713 };
inline const juce::Colour marker { 0xfff75a1c };
inline const juce::Colour amber { 0xffffb27a };
inline const juce::Colour ivoryTrace { 0xfffff1de };
inline const juce::Colour traceOrange { 0xffff7a32 };
inline const juce::Colour traceHalo { 0xffff4e12 };
// controls
inline const juce::Colour border { 0xffcfc6b7 };
inline const juce::Colour borderDark { 0xffa49c8e };
inline const juce::Colour fieldTop { 0xfff3eee6 };
inline const juce::Colour fieldBottom { 0xffe9e2d6 };
inline const juce::Colour knobFaceTop { 0xfff8f4ee };
inline const juce::Colour knobFaceBottom { 0xffdcd3c5 };
inline const juce::Colour valueBox { 0xffefe9de };
inline constexpr float cardRadius = 15.0f;

/** Embedded Inter (SIL OFL 1.1). weight: 0 regular, 1 medium, 2 semibold, 3 bold, 4 extrabold, 5 light. */
juce::Font font (float height, int weight = 0);
/** Font whose capital letters are `capPx` tall (Inter cap height = 0.727 em). */
juce::Font capFont (float capPx, int weight = 0, float tracking = 0.0f);
/** Uppercase, letter-spaced subtitle font. */
juce::Font spaced (float height, float tracking = 0.32f, int weight = 0);
} // namespace pa::theme
