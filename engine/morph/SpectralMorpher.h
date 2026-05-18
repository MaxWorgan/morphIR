#pragma once
#include "../fft/FFTProvider.h"
#include "../ir/IRSlot.h"
#include <vector>
#include <complex>
#include <cstddef>

namespace morphir {

// Blends two IRSlots in the frequency domain and produces a time-domain IR.
//
// Algorithm:
//   - Interpolate FFT magnitudes element-wise: |H| = lerp(|A|, |B|, t)
//   - Retain phase from the dominant slot (t < 0.5 → A's phase, else B's)
//   - IFFT to time domain
//   - Output length is linearly interpolated: lerp(A.originalLength, B.originalLength, t)
//   - Region beyond the blended length is zeroed
//
// Stateless w.r.t. outputs: identical inputs always produce identical outputs.
// Scratch buffers are reused across calls but never affect the result.
//
// Not safe to call concurrently on the same instance.
class SpectralMorpher
{
public:
    SpectralMorpher(FFTProvider& fft, std::size_t fftSize);

    // Compute the blended IR. Writes exactly outputSize samples to output[].
    // outputSize must equal the fftSize passed to the constructor.
    void morph(const IRSlot& a,
               const IRSlot& b,
               float position,
               float* output,
               std::size_t outputSize);

private:
    FFTProvider&                     fft;
    std::size_t                      fftSize;
    std::vector<std::complex<float>> scratchSpectrum;
};

} // namespace morphir
