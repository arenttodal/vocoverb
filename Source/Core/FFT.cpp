#include "FFT.h"

#include <cmath>
#include <utility>

namespace pa
{
void FFT::prepare (int maxSize)
{
    int n = 1;
    while (n < maxSize) n <<= 1;
    maxN = n;
    // Per-stage twiddles stored contiguously: for stage length len, entries [len/2 - 1, len - 1) hold e^{-2 pi i k / len}.
    twiddle.assign ((size_t) std::max (1, n), cfloat (1.0f, 0.0f));
    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1;
        for (int k = 0; k < half; ++k)
        {
            const double a = -2.0 * 3.14159265358979323846 * k / len;
            twiddle[(size_t) (half - 1 + k)] = cfloat ((float) std::cos (a), (float) std::sin (a));
        }
    }
}

void FFT::transform (cfloat* x, int n, bool inv) const noexcept
{
    if (n < 2 || n > maxN) return;
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (x[i], x[j]);
    }
    float* d = reinterpret_cast<float*> (x);
    // stage len = 2 (twiddle 1)
    for (int i = 0; i < n; i += 2)
    {
        const float ar = d[2 * i], ai = d[2 * i + 1], br = d[2 * i + 2], bi = d[2 * i + 3];
        d[2 * i] = ar + br; d[2 * i + 1] = ai + bi;
        d[2 * i + 2] = ar - br; d[2 * i + 3] = ai - bi;
    }
    const float sgn = inv ? -1.0f : 1.0f;
    for (int len = 4; len <= n; len <<= 1)
    {
        const int half = len >> 1;
        const float* tw = reinterpret_cast<const float*> (twiddle.data() + (half - 1));
        for (int i = 0; i < n; i += len)
        {
            float* a = d + 2 * i;
            float* b = d + 2 * (i + half);
            for (int k = 0; k < half; ++k)
            {
                const float wr = tw[2 * k], wi = sgn * tw[2 * k + 1];
                const float br = b[2 * k], bi = b[2 * k + 1];
                const float vr = br * wr - bi * wi, vi = br * wi + bi * wr;
                const float ur = a[2 * k], ui = a[2 * k + 1];
                a[2 * k] = ur + vr; a[2 * k + 1] = ui + vi;
                b[2 * k] = ur - vr; b[2 * k + 1] = ui - vi;
            }
        }
    }
}
} // namespace pa
