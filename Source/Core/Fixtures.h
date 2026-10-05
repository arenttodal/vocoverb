// Procedurally generated, clearly synthetic audition material (no downloads, deterministic).
#pragma once

#include <string>
#include <vector>

namespace pa
{
enum class Fixture { BreathNoise = 0, HarmonicTone, PluckedSequence, BowedTexture, DrumPulse, Count };

struct FixtureInfo { const char* name; const char* description; };
const FixtureInfo& fixtureInfo (Fixture f) noexcept;

/** Generates a phrase followed by silence (so tails can be heard). Stereo, total length ~7 s. */
void generateFixture (Fixture f, double sampleRate, std::vector<float>& left, std::vector<float>& right);
} // namespace pa
