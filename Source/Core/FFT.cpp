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
    twiddle.resize ((size_t) (n / 2));
    for (int k = 0; k < n / 2; ++k)
    {
        const double a = -2.0 * 3.14159265358979323846 * k / n;
        twiddle[(size_t) k] = cfloat ((float) std::cos (a), (float) std::sin (a));
    }
}

void FFT::transform (cfloat* x, int n, bool inv) const noexcept
{
    if (n < 2 || n > maxN) return;
    // bit reversal
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (x[i], x[j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1;
        const int stride = maxN / len;
        for (int i = 0; i < n; i += len)
        {
            for (int k = 0; k < half; ++k)
            {
                const cfloat w = twiddle[(size_t) (k * stride)];
                const float wr = w.real(), wi = inv ? -w.imag() : w.imag();
                const cfloat u = x[i + k];
                const cfloat b = x[i + k + half];
                const cfloat v (b.real() * wr - b.imag() * wi, b.real() * wi + b.imag() * wr);
                x[i + k] = u + v;
                x[i + k + half] = u - v;
            }
        }
    }
}
} // namespace pa
