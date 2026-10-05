#include "Analysis.h"

#include "FFT.h"

#include <cmath>
#include <complex>

namespace pa
{
double rms (const float* x, int n) noexcept
{
    if (n <= 0) return 0.0;
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / n);
}

double peakAbs (const float* x, int n) noexcept
{
    double m = 0.0;
    for (int i = 0; i < n; ++i) m = std::max (m, (double) std::abs (x[i]));
    return m;
}

double toneAmplitude (const float* x, int n, double f, double sr) noexcept
{
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (n - 1));
        const double a = 2.0 * 3.14159265358979 * f * i / sr;
        re += w * x[i] * std::cos (a);
        im -= w * x[i] * std::sin (a);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

void magnitudeSpectrum (const float* x, int n, std::vector<float>& mag)
{
    int N = 1;
    while (N < n) N <<= 1;
    FFT fft; fft.prepare (N);
    std::vector<std::complex<float>> buf ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const float v = i < n ? x[i] : 0.0f;
        const float w = i < n ? (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / std::max (1, n - 1))) : 0.0f;
        buf[(size_t) i] = { v * w, 0.0f };
    }
    fft.forward (buf.data(), N);
    mag.resize ((size_t) (N / 2 + 1));
    for (int k = 0; k <= N / 2; ++k) mag[(size_t) k] = std::abs (buf[(size_t) k]);
}

double harmonicEnergyFraction (const float* x, int n, double f0, double sr, int harmonics, double cents)
{
    std::vector<float> mag;
    magnitudeSpectrum (x, n, mag);
    const int N = (int) (mag.size() - 1) * 2;
    const double binHz = sr / N;
    double total = 0.0, inBand = 0.0;
    std::vector<char> mark (mag.size(), 0);
    for (int h = 1; h <= harmonics; ++h)
    {
        const double f = f0 * h;
        const double lo = f * std::exp2 (-cents / 1200.0), hi = f * std::exp2 (cents / 1200.0);
        const int klo = std::max (0, (int) std::floor (lo / binHz) - 1), khi = std::min ((int) mag.size() - 1, (int) std::ceil (hi / binHz) + 1);
        for (int k = klo; k <= khi; ++k) mark[(size_t) k] = 1;
    }
    for (size_t k = 1; k < mag.size(); ++k)
    {
        if (k * binHz > std::min (sr * 0.45, f0 * (harmonics + 0.5))) break;
        const double e = (double) mag[k] * mag[k];
        total += e;
        if (mark[k]) inBand += e;
    }
    return total > 0.0 ? inBand / total : 0.0;
}

double dominantFrequency (const float* x, int n, double sr, double fLo, double fHi)
{
    std::vector<float> mag;
    magnitudeSpectrum (x, n, mag);
    const int N = (int) (mag.size() - 1) * 2;
    const double binHz = sr / N;
    int best = 1; float bv = -1.0f;
    for (int k = std::max (1, (int) (fLo / binHz)); k < (int) mag.size() - 1 && k * binHz <= fHi; ++k)
        if (mag[(size_t) k] > bv) { bv = mag[(size_t) k]; best = k; }
    // parabolic interpolation
    const double a = mag[(size_t) best - 1], b = mag[(size_t) best], c = mag[(size_t) best + 1];
    const double d = (a - 2 * b + c) != 0.0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0.0;
    return (best + d) * binHz;
}
} // namespace pa
