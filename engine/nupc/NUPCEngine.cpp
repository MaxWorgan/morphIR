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

    std::size_t maxFftSize = partitions.empty() ? 0 : partitions.back().fftSize;
    delayLine.assign(maxFftSize, 0.0f);
    outputBuf.assign(blockSize * 2, 0.0f);
    fftScratch.assign(maxFftSize, 0.0f);
}

void NUPCEngine::buildSchedule()
{
    const int partitionsPerLevel = 16;
    std::size_t covered = 0;
    std::size_t partSize = static_cast<std::size_t>(blockSize);

    while (covered < maxIRSamples)
    {
        int schedEvery = static_cast<int>(partSize / static_cast<std::size_t>(blockSize));

        for (int i = 0; i < partitionsPerLevel && covered < maxIRSamples; ++i)
        {
            Partition p;
            p.size          = partSize;
            p.fftSize       = partSize * 2;
            p.irOffset      = covered;
            p.scheduleEvery = schedEvery;
            p.schedulePhase = (i % schedEvery);

            std::size_t bins = p.fftSize / 2 + 1;
            p.irFFT[0].assign(bins, {0.0f, 0.0f});
            p.irFFT[1].assign(bins, {0.0f, 0.0f});
            p.inputFFT.assign(bins, {0.0f, 0.0f});
            p.accumFFT.assign(bins, {0.0f, 0.0f});
            p.overlapBuf.assign(p.size, 0.0f);

            partitions.push_back(std::move(p));
            covered += partSize;
        }

        partSize *= 8;
    }
}

void NUPCEngine::writeIRToBuffer(int bufferIdx, const float* irData, std::size_t irLength)
{
    std::vector<float> seg; // resized per-partition
    for (auto& p : partitions)
    {
        seg.assign(p.fftSize, 0.0f);
        std::size_t segLen = std::min(p.size,
            irLength > p.irOffset ? irLength - p.irOffset : std::size_t(0));
        if (segLen > 0)
            std::copy(irData + p.irOffset, irData + p.irOffset + segLen, seg.data());
        fft.forward(seg.data(), p.irFFT[bufferIdx].data(), p.fftSize);
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
    // If audio thread hasn't consumed the previous update, skip this cycle —
    // writing to back now could collide with the audio thread's pending swap.
    if (swapPending.load(std::memory_order_acquire))
        return false;

    const int currentActive = activeIdx.load(std::memory_order_acquire);
    const int backIdx       = 1 - currentActive;
    writeIRToBuffer(backIdx, irData, irLength);

    // Release: ensures writes above are visible to the audio thread before
    // it observes swapPending == true.
    swapPending.store(true, std::memory_order_release);
    return true;
}

void NUPCEngine::processPartition(Partition& p, int currentBlock, int readIdx)
{
    if ((currentBlock % p.scheduleEvery) != p.schedulePhase)
        return;

    const int dlen = static_cast<int>(delayLine.size());
    std::fill(fftScratch.begin(), fftScratch.begin() + static_cast<int>(p.fftSize), 0.0f);

    int srcEnd = writePos;
    for (int i = 0; i < static_cast<int>(p.size); ++i)
    {
        int srcIdx = (srcEnd - static_cast<int>(p.size) + i + dlen * 16) % dlen;
        fftScratch[i] = delayLine[srcIdx];
    }

    fft.forward(fftScratch.data(), p.inputFFT.data(), p.fftSize);

    const std::size_t bins = p.fftSize / 2 + 1;
    const auto& ir = p.irFFT[readIdx];
    for (std::size_t k = 0; k < bins; ++k)
        p.accumFFT[k] += p.inputFFT[k] * ir[k];

    // Reuse fftScratch as IFFT output buffer (already cleared above).
    fft.inverse(p.accumFFT.data(), fftScratch.data(), p.fftSize);

    std::fill(p.accumFFT.begin(), p.accumFFT.end(), std::complex<float>{0.0f, 0.0f});

    for (int i = 0; i < static_cast<int>(p.size); ++i)
        outputBuf[i] += fftScratch[i] + p.overlapBuf[i];

    for (int i = 0; i < static_cast<int>(p.size); ++i)
        p.overlapBuf[i] = fftScratch[p.size + i];
}

void NUPCEngine::process(float* block, int numSamples)
{
    assert(numSamples == blockSize);

    // Check for a pending IR swap and perform it atomically.
    if (swapPending.load(std::memory_order_acquire))
    {
        const int current = activeIdx.load(std::memory_order_relaxed);
        activeIdx.store(1 - current, std::memory_order_release);
        swapPending.store(false, std::memory_order_release);
    }
    const int readIdx = activeIdx.load(std::memory_order_acquire);

    for (int i = 0; i < numSamples; ++i)
    {
        delayLine[writePos] = block[i];
        writePos = (writePos + 1) % static_cast<int>(delayLine.size());
    }

    std::fill(outputBuf.begin(), outputBuf.begin() + numSamples, 0.0f);

    for (auto& p : partitions)
        processPartition(p, blockIndex, readIdx);

    for (int i = 0; i < numSamples; ++i)
        block[i] = outputBuf[i];

    ++blockIndex;
}

} // namespace morphir
