#pragma once
#include "../fft/FFTProvider.h"
#include <complex>
#include <vector>
#include <cstddef>

namespace morphir {

class NUPCEngine
{
public:
    NUPCEngine(FFTProvider& fft, std::size_t maxIRSamples, int blockSize);

    // Load a new IR. Must not be called concurrently with process().
    void loadIR(const float* irData, std::size_t irLength);

    // Process one block in-place. Audio thread only. Never allocates.
    void process(float* block, int numSamples);

private:
    struct Partition
    {
        std::size_t size;           // partition size in samples
        std::size_t fftSize;        // 2 * size (linear convolution)
        std::size_t irOffset;       // offset into IR in samples
        int         scheduleEvery;  // process every N blocks
        int         schedulePhase;  // which block phase triggers this partition

        std::vector<std::complex<float>> irFFT;      // pre-computed IR partition FFT
        std::vector<std::complex<float>> inputFFT;   // scratch: FFT of current input segment
        std::vector<std::complex<float>> accumFFT;   // accumulated output spectrum
        std::vector<float>               overlapBuf; // overlap-add tail
    };

    void buildSchedule();
    void processPartition(Partition& p, int blockIndex);

    FFTProvider&              fft;
    std::size_t               maxIRSamples;
    int                       blockSize;

    std::vector<Partition>    partitions;
    std::vector<float>        delayLine;   // circular input history
    std::vector<float>        outputBuf;   // overlap-add output accumulator
    int                       writePos  = 0;
    int                       blockIndex = 0;

    // Scratch buffer for FFT input (reused every block, pre-allocated)
    std::vector<float>        fftScratch;
};

} // namespace morphir
