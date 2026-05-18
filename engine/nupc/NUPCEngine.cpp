#include "NUPCEngine.h"
#include <cassert>
#include <cmath>
#include <algorithm>

namespace morphir {

NUPCEngine::NUPCEngine(FFTProvider& fft, std::size_t maxIRSamples, int blockSize)
    : fft(fft)
    , maxIRSamples(maxIRSamples)
    , blockSize(blockSize)
{
    assert(blockSize > 0 && (blockSize & (blockSize - 1)) == 0);
    buildSchedule();

    std::size_t maxP        = 0;
    std::size_t maxFftSize  = 0;
    std::size_t maxLookback = 0; // largest (irOffset + size) needed in delayLine

    for (auto& p : partitions)
    {
        maxP        = std::max(maxP,        p.size);
        maxFftSize  = std::max(maxFftSize,  p.fftSize);
        maxLookback = std::max(maxLookback, p.irOffset + p.size);
    }

    // Delay line must hold the full lookback window for the deepest partition,
    // plus the current block so reading and writing don't alias.
    delayLine.assign(maxLookback + static_cast<std::size_t>(blockSize), 0.0f);

    // Output ring: large partitions write maxP samples that get dispensed over
    // maxP/blockSize blocks. Add one block of headroom for the shift step.
    outputBuf.assign(maxP + static_cast<std::size_t>(blockSize), 0.0f);

    fftInScratch.assign(maxFftSize, 0.0f);
    fftOutScratch.assign(maxFftSize, 0.0f);
    inputSpectrum.assign(maxFftSize / 2 + 1, {0.0f, 0.0f});
    productSpectrum.assign(maxFftSize / 2 + 1, {0.0f, 0.0f});
}

void NUPCEngine::buildSchedule()
{
    const int   partitionsPerLevel = 16;
    std::size_t covered  = 0;
    std::size_t partSize = static_cast<std::size_t>(blockSize);

    while (covered < maxIRSamples)
    {
        const int schedEvery = static_cast<int>(partSize / static_cast<std::size_t>(blockSize));

        for (int i = 0; i < partitionsPerLevel && covered < maxIRSamples; ++i)
        {
            Partition p;
            p.size          = partSize;
            p.fftSize       = partSize * 2;
            p.irOffset      = covered;
            p.scheduleEvery = schedEvery;
            p.schedulePhase = (i % schedEvery);

            const std::size_t bins = p.fftSize / 2 + 1;
            p.irFFT[0].assign(bins, {0.0f, 0.0f});
            p.irFFT[1].assign(bins, {0.0f, 0.0f});
            p.overlap.assign(p.size, 0.0f);

            partitions.push_back(std::move(p));
            covered += partSize;
        }

        partSize *= 8;
    }
}

void NUPCEngine::writeIRToBuffer(int bufferIdx, const float* irData, std::size_t irLength)
{
    for (auto& p : partitions)
    {
        std::fill(fftInScratch.begin(), fftInScratch.begin() + static_cast<long>(p.fftSize), 0.0f);

        const std::size_t segLen = std::min(p.size,
            irLength > p.irOffset ? irLength - p.irOffset : std::size_t(0));
        if (segLen > 0)
            std::copy(irData + p.irOffset,
                      irData + p.irOffset + segLen,
                      fftInScratch.begin());

        fft.forward(fftInScratch.data(), p.irFFT[bufferIdx].data(), p.fftSize);
    }
}

void NUPCEngine::loadIR(const float* irData, std::size_t irLength)
{
    writeIRToBuffer(0, irData, irLength);
    writeIRToBuffer(1, irData, irLength);
    activeIdx.store(0, std::memory_order_release);
    swapPending.store(false, std::memory_order_release);
}

bool NUPCEngine::loadIRToBack(const float* irData, std::size_t irLength)
{
    if (swapPending.load(std::memory_order_acquire))
        return false;

    const int currentActive = activeIdx.load(std::memory_order_acquire);
    const int backIdx       = 1 - currentActive;
    writeIRToBuffer(backIdx, irData, irLength);

    swapPending.store(true, std::memory_order_release);
    return true;
}

void NUPCEngine::processPartition(Partition& p, int currentBlock, int readIdx)
{
    if ((currentBlock % p.scheduleEvery) != p.schedulePhase)
        return;

    const int dlen      = static_cast<int>(delayLine.size());
    const int safeBase  = dlen * 16; // keep modulus positive
    const int P         = static_cast<int>(p.size);
    const int F         = static_cast<int>(p.fftSize);
    const int O         = static_cast<int>(p.irOffset);

    // Build input window: P samples covering input times [n - O - P + 1, n - O].
    // delayLine[writePos - 1] is the most recently written sample (time n).
    // So we want indices [writePos - O - P, writePos - O - 1].
    for (int i = 0; i < P; ++i)
    {
        const int srcIdx = ((writePos - O - P + i) + safeBase) % dlen;
        fftInScratch[i] = delayLine[srcIdx];
    }
    // Zero-pad the second half (for linear convolution).
    std::fill(fftInScratch.begin() + P, fftInScratch.begin() + F, 0.0f);

    fft.forward(fftInScratch.data(), inputSpectrum.data(), p.fftSize);

    const std::size_t bins = p.fftSize / 2 + 1;
    const auto& ir = p.irFFT[readIdx];
    for (std::size_t k = 0; k < bins; ++k)
        productSpectrum[k] = inputSpectrum[k] * ir[k];

    fft.inverse(productSpectrum.data(), fftOutScratch.data(), p.fftSize);

    // Overlap-add into the future-output ring. Partition contributes P samples
    // starting at the current block position (outputBuf[0..P-1]).
    for (int i = 0; i < P; ++i)
        outputBuf[i] += fftOutScratch[i] + p.overlap[i];

    // Save the IFFT tail as overlap for this partition's next scheduled run.
    for (int i = 0; i < P; ++i)
        p.overlap[i] = fftOutScratch[P + i];
}

void NUPCEngine::process(float* block, int numSamples)
{
    assert(numSamples == blockSize);

    if (swapPending.load(std::memory_order_acquire))
    {
        const int current = activeIdx.load(std::memory_order_relaxed);
        activeIdx.store(1 - current, std::memory_order_release);
        swapPending.store(false, std::memory_order_release);
    }
    const int readIdx = activeIdx.load(std::memory_order_acquire);

    // Write input into the delay line first so partition 0 (offset 0) can see
    // the just-arrived block.
    const int dlen = static_cast<int>(delayLine.size());
    for (int i = 0; i < numSamples; ++i)
    {
        delayLine[writePos] = block[i];
        writePos = (writePos + 1) % dlen;
    }

    // Shift the output ring left by one block (dispense the current block,
    // making room at the tail for future contributions).
    const int outLen = static_cast<int>(outputBuf.size());
    std::copy(outputBuf.begin() + numSamples, outputBuf.end(), outputBuf.begin());
    std::fill(outputBuf.begin() + (outLen - numSamples), outputBuf.end(), 0.0f);

    // Accumulate partition contributions into outputBuf[0..maxP-1].
    for (auto& p : partitions)
        processPartition(p, blockIndex, readIdx);

    // Emit the current block's output.
    for (int i = 0; i < numSamples; ++i)
        block[i] = outputBuf[i];

    ++blockIndex;
}

} // namespace morphir
