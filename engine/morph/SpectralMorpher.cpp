#include "SpectralMorpher.h"
#include <cassert>
#include <cmath>
#include <algorithm>

namespace morphir {

SpectralMorpher::SpectralMorpher(FFTProvider& fft, std::size_t fftSize)
    : fft(fft)
    , fftSize(fftSize)
    , scratchSpectrum(fftSize / 2 + 1)
{
    assert(fftSize >= 2 && (fftSize & (fftSize - 1)) == 0); // power of two
}

void SpectralMorpher::morph(const IRSlot& a,
                            const IRSlot& b,
                            float position,
                            float* output,
                            std::size_t outputSize)
{
    assert(outputSize == fftSize);
    assert(a.fft.size() == fftSize / 2 + 1);
    assert(b.fft.size() == fftSize / 2 + 1);

    const float t = std::clamp(position, 0.0f, 1.0f);
    const std::size_t bins = fftSize / 2 + 1;

    // Magnitude interpolation; phase taken from the dominant slot.
    const bool useAPhase = (t < 0.5f);

    for (std::size_t k = 0; k < bins; ++k)
    {
        const float magA = std::abs(a.fft[k]);
        const float magB = std::abs(b.fft[k]);
        const float mag  = magA + t * (magB - magA);

        const std::complex<float>& phaseSrc = useAPhase ? a.fft[k] : b.fft[k];
        const float refMag = std::abs(phaseSrc);

        if (refMag > 1e-20f)
        {
            // Scale phaseSrc to have magnitude `mag` while preserving its phase
            const float scale = mag / refMag;
            scratchSpectrum[k] = phaseSrc * scale;
        }
        else
        {
            // Dominant slot has zero magnitude at this bin — fall back to the other,
            // or to a real-valued result if both are zero.
            const std::complex<float>& fallback = useAPhase ? b.fft[k] : a.fft[k];
            const float fbMag = std::abs(fallback);
            if (fbMag > 1e-20f)
                scratchSpectrum[k] = fallback * (mag / fbMag);
            else
                scratchSpectrum[k] = { mag, 0.0f };
        }
    }

    fft.inverse(scratchSpectrum.data(), output, fftSize);

    // Length interpolation: zero out tail beyond the blended length.
    const float lenA = static_cast<float>(a.originalLength);
    const float lenB = static_cast<float>(b.originalLength);
    const std::size_t blendedLen = static_cast<std::size_t>(std::round(lenA + t * (lenB - lenA)));

    if (blendedLen < outputSize)
        std::fill(output + blendedLen, output + outputSize, 0.0f);
}

} // namespace morphir
