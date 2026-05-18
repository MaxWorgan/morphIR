#pragma once
#include "../fft/FFTProvider.h"
#include <complex>
#include <vector>
#include <atomic>
#include <cstddef>

namespace morphir {

class NUPCEngine
{
public:
    NUPCEngine(FFTProvider& fft, std::size_t maxIRSamples, int blockSize);

    // Load a new IR into both front and back buffers. Must not be called
    // concurrently with process() or loadIRToBack(). Use for initial setup.
    void loadIR(const float* irData, std::size_t irLength);

    // Load an IR into the back buffer and signal a swap. Called from the
    // morph thread. Returns false if a previous swap is still pending
    // (audio thread hasn't consumed it yet); caller should retry later.
    bool loadIRToBack(const float* irData, std::size_t irLength);

    // Process one block in-place. Audio thread only. Never allocates.
    void process(float* block, int numSamples);

    std::size_t getMaxIRSamples() const { return maxIRSamples; }

private:
    struct Partition
    {
        std::size_t size;
        std::size_t fftSize;
        std::size_t irOffset;
        int         scheduleEvery;
        int         schedulePhase;

        // Double-buffered IR FFT. Audio thread reads [activeIdx];
        // morph thread writes [1 - activeIdx].
        std::vector<std::complex<float>> irFFT[2];

        std::vector<std::complex<float>> inputFFT;
        std::vector<std::complex<float>> accumFFT;
        std::vector<float>               overlapBuf;
    };

    void buildSchedule();
    void writeIRToBuffer(int bufferIdx, const float* irData, std::size_t irLength);
    void processPartition(Partition& p, int currentBlock, int readIdx);

    FFTProvider&              fft;
    std::size_t               maxIRSamples;
    int                       blockSize;

    std::vector<Partition>    partitions;
    std::vector<float>        delayLine;
    std::vector<float>        outputBuf;
    int                       writePos   = 0;
    int                       blockIndex = 0;

    std::vector<float>        fftScratch;

    std::atomic<int>          activeIdx   { 0 };
    std::atomic<bool>         swapPending { false };
};

} // namespace morphir
