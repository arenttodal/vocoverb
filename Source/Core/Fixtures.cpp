#include "Fixtures.h"

#include "DspUtil.h"

namespace pa
{
const FixtureInfo& fixtureInfo (Fixture f) noexcept
{
    static const FixtureInfo infos[] = {
        { "Breath / Noise Phrase (synthetic)", "Filtered noise breaths with moving formants: broadband, unpitched." },
        { "Harmonic Tone (synthetic)", "A sung-like additive tone phrase with vibrato and vowel formants, then silence." },
        { "Plucked Sequence (synthetic)", "Karplus-Strong plucks, transient and decaying." },
        { "Sustained Bowed Texture (synthetic)", "Slow-attack sawtooth ensemble, two sustained notes." },
        { "Drum Pulse (synthetic)", "Synthesised kick, snare and hat at 120 BPM." },
    };
    const int i = std::clamp ((int) f, 0, 4);
    return infos[i];
}

namespace
{
struct Formant
{
    Biquad bp;
    void set (double sr, double f, double q) { bp.bandpass (sr, f, q); }
};

float env (double t, double a, double d, double len)
{
    if (t < 0 || t > len) return 0.0f;
    if (t < a) return (float) (t / a);
    if (t > len - d) return (float) std::max (0.0, (len - t) / d);
    return 1.0f;
}
} // namespace

void generateFixture (Fixture f, double sr, std::vector<float>& L, std::vector<float>& R)
{
    const double total = 7.0;
    const int n = (int) (total * sr);
    L.assign ((size_t) n, 0.0f);
    R.assign ((size_t) n, 0.0f);
    Rng rng (0xC0FFEEu + (uint32_t) f * 977u);

    switch (f)
    {
        case Fixture::BreathNoise:
        {
            Biquad f1, f2, f3;
            float pink = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                float e = 0.0f;
                for (int b = 0; b < 3; ++b) e += env (t - 0.15 - b * 1.05, 0.25, 0.45, 0.85);
                if ((i & 63) == 0)
                {
                    const double sweep = 0.5 + 0.5 * std::sin (t * 2.1);
                    f1.bandpass (sr, 500 + 500 * sweep, 2.0);
                    f2.bandpass (sr, 1400 + 900 * sweep, 2.5);
                    f3.bandpass (sr, 4200 + 1500 * sweep, 1.2);
                }
                const float w = rng.bi();
                pink = 0.97f * pink + 0.03f * w;
                const float x = 0.6f * w + 2.0f * pink;
                const float y = 1.6f * f1.process (x) + 1.2f * f2.process (x) + 0.7f * f3.process (x);
                L[(size_t) i] = y * e * 0.35f;
                R[(size_t) i] = y * e * 0.33f;
            }
            break;
        }
        case Fixture::HarmonicTone:
        {
            // melody G3 Bb3 C4 Bb3 (sung-like), 3.2 s
            const double notes[4] = { 55, 58, 60, 58 }, starts[4] = { 0.1, 0.9, 1.6, 2.4 }, lens[4] = { 0.8, 0.7, 0.8, 0.9 };
            Formant fa, fb, fc;
            fa.set (sr, 700, 4.0); fb.set (sr, 1150, 5.0); fc.set (sr, 2600, 6.0);
            double phase = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                double hz = 0.0; float e = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    const float ek = env (t - starts[k], 0.06, 0.12, lens[k]);
                    if (ek > 0.0f) { hz = 440.0 * std::exp2 ((notes[k] - 69.0) / 12.0); e = std::max (e, ek); }
                }
                if (hz <= 0.0) { L[(size_t) i] = R[(size_t) i] = 0.0f; continue; }
                hz *= 1.0 + 0.006 * std::sin (2.0 * kPiD * 5.3 * t);
                phase += hz / sr; phase -= std::floor (phase);
                float s = 0.0f;
                for (int h = 1; h <= 24; ++h)
                {
                    if (h * hz > sr * 0.45) break;
                    s += (float) (std::sin (2.0 * kPiD * phase * h) / h);
                }
                const float y = 0.3f * s + 2.2f * fa.bp.process (s) + 1.6f * fb.bp.process (s) + 0.8f * fc.bp.process (s);
                L[(size_t) i] = R[(size_t) i] = y * e * 0.22f;
            }
            break;
        }
        case Fixture::PluckedSequence:
        {
            const int seq[8] = { 60, 64, 67, 71, 72, 67, 64, 62 };
            std::vector<float> line;
            int pos = 0; int len = 1; float last = 0.0f;
            int noteIdx = -1;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                const int k = (int) std::floor ((t - 0.1) / 0.38);
                if (k >= 0 && k < 8 && k != noteIdx)
                {
                    noteIdx = k;
                    const double hz = 440.0 * std::exp2 ((seq[k] - 69.0) / 12.0);
                    len = std::max (2, (int) (sr / hz));
                    line.assign ((size_t) len, 0.0f);
                    for (auto& v : line) v = rng.bi();
                    pos = 0; last = 0.0f;
                }
                float y = 0.0f;
                if (! line.empty())
                {
                    const float cur = line[(size_t) pos];
                    const int nx = (pos + 1) % len;
                    const float v = 0.996f * 0.5f * (cur + line[(size_t) nx]);
                    line[(size_t) pos] = v;
                    y = cur;
                    pos = nx;
                    last = v;
                }
                if (t > 3.4) y *= (float) std::max (0.0, 1.0 - (t - 3.4) / 0.4);
                L[(size_t) i] = y * 0.4f;
                R[(size_t) i] = y * 0.38f;
            }
            (void) last;
            break;
        }
        case Fixture::BowedTexture:
        {
            const double hzA = 146.83, hzB = 220.0;
            double ph[6] {};
            const double det[3] = { 0.997, 1.0, 1.004 };
            Biquad lp; lp.lowpass (sr, 1800, 0.8);
            Biquad lp2; lp2.lowpass (sr, 1800, 0.8);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                const float e = env (t - 0.1, 0.9, 0.8, 3.6);
                float s = 0.0f;
                const double vib = 1.0 + 0.004 * std::sin (2.0 * kPiD * 4.7 * t);
                for (int k = 0; k < 3; ++k)
                {
                    ph[k] += hzA * det[k] * vib / sr; ph[k] -= std::floor (ph[k]);
                    ph[k + 3] += hzB * det[k] * vib / sr; ph[k + 3] -= std::floor (ph[k + 3]);
                    s += (float) (2.0 * ph[k] - 1.0) + (float) (2.0 * ph[k + 3] - 1.0);
                }
                const float y = lp2.process (lp.process (s)) * 0.12f * e;
                L[(size_t) i] = y;
                R[(size_t) i] = y * 0.95f;
            }
            break;
        }
        case Fixture::DrumPulse:
        default:
        {
            const double beat = 0.5;
            Biquad hp; hp.highpass (sr, 7000);
            Biquad bpS; bpS.bandpass (sr, 1800, 0.8);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr - 0.1;
                float y = 0.0f;
                if (t >= 0 && t < 4.0)
                {
                    const double tb = std::fmod (t, beat);
                    const int b = (int) (t / beat);
                    if (b % 2 == 0) // kick
                    {
                        const double f = 50.0 + 90.0 * std::exp (-tb * 30.0);
                        y += (float) (std::sin (2.0 * kPiD * f * tb) * std::exp (-tb * 9.0)) * 0.9f;
                    }
                    else // snare
                        y += bpS.process (rng.bi()) * (float) std::exp (-tb * 18.0) * 1.4f;
                    const double th = std::fmod (t, beat * 0.5);
                    y += hp.process (rng.bi()) * (float) std::exp (-th * 60.0) * 0.5f;
                }
                L[(size_t) i] = y * 0.45f;
                R[(size_t) i] = y * 0.43f;
            }
            break;
        }
    }
}
} // namespace pa
