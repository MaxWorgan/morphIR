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

    // Load IR into both front and back buffers. Not safe to call concurrently
    // with process() or loadIRToBack().
    void loadIR(const float* irData, std::size_t irLength);

    // Load IR into the back buffer and signal a swap. Returns false if a
    // previous swap is still pending. Called from the morph thread.
    bool loadIRToBack(const float* irData, std::size_t irLength);

    // Process one block in-place. Audio thread only. Never allocates.
    void process(float* block, int numSamples);

    std::size_t getMaxIRSamples() const { return maxIRSamples; }

private:
    struct Partition
    {
        std::size_t size;          // P
        std::size_t fftSize;       // 2P (linear convolution)
        std::size_t irOffset;      // start of this partition in the IR
        int         scheduleEvery; // run every N blocks
        int         schedulePhase; // run when blockIndex % scheduleEvery == phase

        // Double-buffered IR FFT.
        std::vector<std::complex<float>> irFFT[2];

        // Per-partition overlap (saved second half of last IFFT for this partition).
        std::vector<float> overlap;
    };

    void buildSchedule();
    void writeIRToBuffer(int bufferIdx, const float* irData, std::size_t irLength);
    void processPartition(Partition& p, int currentBlock, int readIdx);

    FFTProvider& fft;
    std::size_t  maxIRSamples;
    int          blockSize;

    std::vector<Partition> partitions;

    // Circular input history. Sized so the largest partition's lookback window
    // fits without aliasing.
    std::vector<float> delayLine;
    int                writePos   = 0;
    int                blockIndex = 0;

    // Future-output ring: contributions from large partitions span multiple
    // blocks. Sized to hold the largest partition's output footprint.
    std::vector<float> outputBuf;

    // Scratch buffers reused by every partition (sized to max fftSize).
    // Audio thread only — see irScratch for the IR-writing paths.
    std::vector<float>               fftInScratch;
    std::vector<float>               fftOutScratch;
    std::vector<std::complex<float>> inputSpectrum;
    std::vector<std::complex<float>> productSpectrum;

    // Dedicated scratch for writeIRToBuffer. The morph thread calls
    // loadIRToBack concurrently with process(), so it must never share
    // scratch memory with the audio thread.
    std::vector<float>               irScratch;

    std::atomic<int>  activeIdx   { 0 };
    std::atomic<bool> swapPending { false };
};

} // namespace morphir
