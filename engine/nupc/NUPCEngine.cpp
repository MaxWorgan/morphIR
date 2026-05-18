#include "NUPCEngine.h"
#include <cassert>
#include <cmath>
#include <algorithm>

namespace morphir {

static std::size_t nextPow2(std::size_t n)
{
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

NUPCEngine::NUPCEngine(FFTProvider& fft, std::size_t maxIRSamples, int blockSize)
    : fft(fft)
    , maxIRSamples(maxIRSamples)
    , blockSize(blockSize)
{
    assert(blockSize > 0 && (blockSize & (blockSize - 1)) == 0); // power of two
    buildSchedule();

    // Input delay line: large enough to feed the largest partition's FFT
    std::size_t maxFftSize = partitions.empty() ? 0 : partitions.back().fftSize;
    delayLine.assign(maxFftSize, 0.0f);

    outputBuf.assign(blockSize * 2, 0.0f);

    std::size_t maxScratch = maxFftSize;
    fftScratch.assign(maxScratch, 0.0f);
}

void NUPCEngine::buildSchedule()
{
    // NUPC schedule: groups of partitions with doubling sizes.
    // Each group covers ~16x the coverage of the previous group's total samples,
    // achieved by having 16 partitions before stepping up in size.
    const int partitionsPerLevel = 16;

    std::size_t covered = 0;
    std::size_t partSize = static_cast<std::size_t>(blockSize);

    while (covered < maxIRSamples)
    {
        int schedEvery = static_cast<int>(partSize / static_cast<std::size_t>(blockSize));

        for (int i = 0; i < partitionsPerLevel && covered < maxIRSamples; ++i)
        {
            Partition p;
            p.size        = partSize;
            p.fftSize     = partSize * 2; // linear convolution requires 2× zero-pad
            p.irOffset    = covered;
            p.scheduleEvery = schedEvery;
            p.schedulePhase = (i % schedEvery); // stagger to spread CPU load

            std::size_t bins = p.fftSize / 2 + 1;
            p.irFFT.assign(bins, {0.0f, 0.0f});
            p.inputFFT.assign(bins, {0.0f, 0.0f});
            p.accumFFT.assign(bins, {0.0f, 0.0f});
            p.overlapBuf.assign(p.size, 0.0f); // overlap from previous block

            partitions.push_back(std::move(p));
            covered += partSize;
        }

        partSize *= 8; // jump to next level
    }
}

void NUPCEngine::loadIR(const float* irData, std::size_t irLength)
{
    for (auto& p : partitions)
    {
        // Copy the relevant segment of the IR into the FFT scratch, zero-padded to fftSize
        std::vector<float> seg(p.fftSize, 0.0f);
        std::size_t segLen = std::min(p.size, irLength > p.irOffset ? irLength - p.irOffset : std::size_t(0));
        if (segLen > 0)
            std::copy(irData + p.irOffset, irData + p.irOffset + segLen, seg.data());

        fft.forward(seg.data(), p.irFFT.data(), p.fftSize);
    }
}

void NUPCEngine::processPartition(Partition& p, int currentBlock)
{
    // Only process this partition on its scheduled block
    if ((currentBlock % p.scheduleEvery) != p.schedulePhase)
        return;

    // Build the input segment: most recent p.size samples from the delay line
    // The delay line is circular; we need the last p.size samples ending at writePos
    const int dlen = static_cast<int>(delayLine.size());
    std::fill(fftScratch.begin(), fftScratch.begin() + static_cast<int>(p.fftSize), 0.0f);

    int srcEnd = writePos; // exclusive: writePos is the next write position
    for (int i = 0; i < static_cast<int>(p.size); ++i)
    {
        int srcIdx = (srcEnd - static_cast<int>(p.size) + i + dlen * 16) % dlen;
        fftScratch[i] = delayLine[srcIdx];
    }
    // Zero-pad the second half (already zeroed above)

    fft.forward(fftScratch.data(), p.inputFFT.data(), p.fftSize);

    // Complex multiply-accumulate: accumFFT += inputFFT * irFFT
    const std::size_t bins = p.fftSize / 2 + 1;
    for (std::size_t k = 0; k < bins; ++k)
        p.accumFFT[k] += p.inputFFT[k] * p.irFFT[k];

    // IFFT the accumulated result
    std::vector<float> ifftOut(p.fftSize, 0.0f);
    fft.inverse(p.accumFFT.data(), ifftOut.data(), p.fftSize);

    // Clear accumulator for next round
    std::fill(p.accumFFT.begin(), p.accumFFT.end(), std::complex<float>{0.0f, 0.0f});

    // Overlap-add: first p.size samples add to outputBuf, save second half as overlap
    for (int i = 0; i < static_cast<int>(p.size); ++i)
        outputBuf[i] += ifftOut[i] + p.overlapBuf[i];

    // Save the tail for the next scheduled block of this partition
    for (int i = 0; i < static_cast<int>(p.size); ++i)
        p.overlapBuf[i] = ifftOut[p.size + i];
}

void NUPCEngine::process(float* block, int numSamples)
{
    assert(numSamples == blockSize);

    // Write input block into circular delay line
    for (int i = 0; i < numSamples; ++i)
    {
        delayLine[writePos] = block[i];
        writePos = (writePos + 1) % static_cast<int>(delayLine.size());
    }

    // Clear output accumulator for this block
    std::fill(outputBuf.begin(), outputBuf.begin() + numSamples, 0.0f);

    // Process all partitions (each decides internally whether it's its turn)
    for (auto& p : partitions)
        processPartition(p, blockIndex);

    // Copy output
    for (int i = 0; i < numSamples; ++i)
        block[i] = outputBuf[i];

    ++blockIndex;
}

} // namespace morphir
