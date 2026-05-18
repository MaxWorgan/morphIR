#pragma once
#include "FFTProvider.h"
#include <Accelerate/Accelerate.h>
#include <unordered_map>
#include <mutex>

namespace morphir {

class VDSPFFTProvider final : public FFTProvider
{
public:
    VDSPFFTProvider() = default;
    ~VDSPFFTProvider() override;

    void forward(const float* realIn, std::complex<float>* complexOut, std::size_t size) override;
    void inverse(const std::complex<float>* complexIn, float* realOut, std::size_t size) override;

private:
    FFTSetup getSetup(std::size_t size);

    std::unordered_map<std::size_t, FFTSetup> setupCache;
    std::mutex cacheMutex;
};

} // namespace morphir
