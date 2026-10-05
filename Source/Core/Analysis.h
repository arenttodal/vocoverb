// Offline analysis helpers used by tests, renders and the UI harmony display (never on the audio thread).
#pragma once

#include <vector>

namespace pa
{
double rms (const float* x, int n) noexcept;
double peakAbs (const float* x, int n) noexcept;
/** Hann-windowed single-frequency amplitude estimate (Goertzel). */
double toneAmplitude (const float* x, int n, double freq, double sampleRate) noexcept;
/** Energy within +-cents of freq and of its first `harmonics` multiples, relative to total energy (0..1). */
double harmonicEnergyFraction (const float* x, int n, double f0, double sampleRate, int harmonics, double cents);
/** Magnitude spectrum (Hann window, power of two size). */
void magnitudeSpectrum (const float* x, int n, std::vector<float>& mag);
/** Frequency of the strongest spectral peak in [fLo, fHi]. */
double dominantFrequency (const float* x, int n, double sampleRate, double fLo, double fHi);
} // namespace pa
