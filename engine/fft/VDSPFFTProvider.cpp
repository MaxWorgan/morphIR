#include "VDSPFFTProvider.h"
#include "FFTProvider.h"
#include <cassert>
#include <cmath>
#include <vector>

namespace morphir {

// Returns the log2 of size; size must be a power of two.
static vDSP_Length log2n(std::size_t size)
{
    return static_cast<vDSP_Length>(std::round(std::log2(static_cast<double>(size))));
}

VDSPFFTProvider::~VDSPFFTProvider()
{
    for (auto& [size, setup] : setupCache)
        vDSP_destroy_fftsetup(setup);
}

FFTSetup VDSPFFTProvider::getSetup(std::size_t size)
{
    std::lock_guard lock(cacheMutex);
    auto it = setupCache.find(size);
    if (it != setupCache.end())
        return it->second;

    FFTSetup setup = vDSP_create_fftsetup(log2n(size), FFT_RADIX2);
    assert(setup != nullptr);
    setupCache[size] = setup;
    return setup;
}

void VDSPFFTProvider::forward(const float* realIn,
                               std::complex<float>* complexOut,
                               std::size_t size)
{
    assert(size >= 2 && (size & (size - 1)) == 0); // power of two

    FFTSetup setup = getSetup(size);

    // vDSP uses split-complex format: separate real and imaginary arrays
    std::vector<float> re(size / 2), im(size / 2);
    DSPSplitComplex split { re.data(), im.data() };

    // Pack real input into split-complex (interleaved → split)
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(realIn), 2, &split, 1, size / 2);

    vDSP_fft_zrip(setup, &split, 1, log2n(size), FFT_FORWARD);

    // vDSP packs the Nyquist bin into im[0]; unpack to standard half-spectrum layout
    const float scale = 0.5f;
    const std::size_t bins = size / 2 + 1;

    complexOut[0]    = { re[0] * scale, 0.0f };           // DC, real only
    complexOut[size / 2] = { im[0] * scale, 0.0f };       // Nyquist, real only

    for (std::size_t k = 1; k < size / 2; ++k)
        complexOut[k] = { re[k] * scale, im[k] * scale };

    (void)bins;
}

void VDSPFFTProvider::inverse(const std::complex<float>* complexIn,
                               float* realOut,
                               std::size_t size)
{
    assert(size >= 2 && (size & (size - 1)) == 0);

    FFTSetup setup = getSetup(size);

    std::vector<float> re(size / 2), im(size / 2);
    DSPSplitComplex split { re.data(), im.data() };

    // Pack half-spectrum back into vDSP split-complex format
    re[0] = complexIn[0].real();
    im[0] = complexIn[size / 2].real(); // Nyquist packed into im[0]

    for (std::size_t k = 1; k < size / 2; ++k)
    {
        re[k] = complexIn[k].real();
        im[k] = complexIn[k].imag();
    }

    vDSP_fft_zrip(setup, &split, 1, log2n(size), FFT_INVERSE);

    // Unpack split-complex back to real interleaved, then normalise
    vDSP_ztoc(&split, 1, reinterpret_cast<DSPComplex*>(realOut), 2, size / 2);

    const float norm = 1.0f / static_cast<float>(size);
    vDSP_vsmul(realOut, 1, &norm, realOut, 1, size);
}

std::unique_ptr<FFTProvider> FFTProvider::create()
{
    return std::make_unique<VDSPFFTProvider>();
}

} // namespace morphir
