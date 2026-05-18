#pragma once
#include <cstddef>
#include <complex>
#include <memory>

namespace morphir {

// Forward and inverse FFT for real-valued signals.
// Size must be a power of two. Output of forward() has size/2 + 1 complex bins.
// Input to inverse() is size/2 + 1 complex bins, output is size real samples.
class FFTProvider
{
public:
    virtual ~FFTProvider() = default;

    // Forward real FFT: realIn[size] -> complexOut[size/2 + 1]
    virtual void forward(const float* realIn, std::complex<float>* complexOut, std::size_t size) = 0;

    // Inverse real FFT: complexIn[size/2 + 1] -> realOut[size] (normalised)
    virtual void inverse(const std::complex<float>* complexIn, float* realOut, std::size_t size) = 0;

    static std::unique_ptr<FFTProvider> create();
};

} // namespace morphir
