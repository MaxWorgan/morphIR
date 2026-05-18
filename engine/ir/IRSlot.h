#pragma once
#include <vector>
#include <complex>
#include <cstddef>

namespace morphir {

struct IRSlot
{
    std::vector<float>               samples;        // time-domain, zero-padded to maxIRSamples
    std::vector<std::complex<float>> fft;            // size = fftSize/2 + 1
    std::size_t                      originalLength = 0;
    double                           originalSampleRate = 0.0;
    bool                             loaded = false;
};

} // namespace morphir
