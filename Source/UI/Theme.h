// Design tokens (approximate, reference-inspired; see docs/DECISIONS.md) and font access.
#pragma once

#include <JuceHeader.h>

namespace pa::theme
{
inline const juce::Colour shell { 0xffeae5da };
inline const juce::Colour shellTop { 0xfff0ebe1 };
inline const juce::Colour shellBottom { 0xffe2dccf };
inline const juce::Colour cardTop { 0xfff6f1e8 };
inline const juce::Colour cardBottom { 0xffe9e2d5 };
inline const juce::Colour text { 0xff17201f };
inline const juce::Colour textMuted { 0xff5b5a52 };   // >= 4.5:1 on card colours
inline const juce::Colour textDisabled { 0xff8d897e };
inline const juce::Colour display { 0xff1b2524 };
inline const juce::Colour displayTop { 0xff222b29 };
inline const juce::Colour displayGrid { 0x1fe8e2d4 };
inline const juce::Colour displayLabel { 0xffb9b4a6 };
inline const juce::Colour accent { 0xfff45a1e };
inline const juce::Colour accentTop { 0xffff6c2c };
inline const juce::Colour accentBottom { 0xffe64b16 };
inline const juce::Colour amber { 0xffffb27a };
inline const juce::Colour ivoryTrace { 0xfff3ead9 };
inline const juce::Colour border { 0xffc9c1b2 };
inline const juce::Colour borderDark { 0xff9f988a };
inline const juce::Colour knobFaceTop { 0xfffbf8f2 };
inline const juce::Colour knobFaceBottom { 0xffe6dfd2 };
inline const juce::Colour valueBox { 0xffefe9de };
inline constexpr float cardRadius = 12.0f;

/** Embedded Inter (SIL OFL 1.1). weight: 0 regular, 1 medium, 2 semibold, 3 bold. */
juce::Font font (float height, int weight = 0);
/** Uppercase, letter-spaced subtitle font. */
juce::Font spaced (float height, float tracking = 0.32f, int weight = 0);
} // namespace pa::theme
