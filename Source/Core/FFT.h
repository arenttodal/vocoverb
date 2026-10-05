// Iterative radix-2 complex FFT (project-owned). Tables are built in prepare(); transforms never allocate.
#pragma once

#include <complex>
#include <vector>

namespace pa
{
class FFT
{
public:
    using cfloat = std::complex<float>;

    /** Allocates tables for sizes up to maxSize (power of two). */
    void prepare (int maxSize);
    /** In-place forward (sign -1) transform of size n (power of two <= maxSize). Unnormalised. */
    void forward (cfloat* data, int n) const noexcept { transform (data, n, false); }
    /** In-place inverse transform, unnormalised (caller divides by n). */
    void inverse (cfloat* data, int n) const noexcept { transform (data, n, true); }
    int maxSize() const noexcept { return maxN; }

private:
    void transform (cfloat* data, int n, bool inverse) const noexcept;
    int maxN = 0;
    std::vector<cfloat> twiddle; // e^{-2 pi i k / maxN}, k < maxN/2
};
} // namespace pa
