#include "DualMonoConvolver.h"

namespace morphir {

std::size_t DualMonoConvolver::nextPow2(std::size_t n)
{
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

DualMonoConvolver::DualMonoConvolver(FFTProvider& fft,
                                     std::size_t maxIRSamples,
                                     int blockSize)
    : fft(fft)
    , fftSize(nextPow2(maxIRSamples))
    , leftEngine(fft, maxIRSamples, blockSize)
    , rightEngine(fft, maxIRSamples, blockSize)
    , morphThread(fft, fftSize)
{
}

DualMonoConvolver::~DualMonoConvolver()
{
    stopMorphing();
}

void DualMonoConvolver::setSlots(const IRSlot& leftA,  const IRSlot& leftB,
                                  const IRSlot& rightA, const IRSlot& rightB)
{
    const bool wasRunning = morphThread.isThreadRunning();
    if (wasRunning)
        morphThread.stopThread(500);

    // Seed both engines with the current morph position's blended IR so that
    // process() produces sensible output from the very first block, before the
    // morph thread has had a chance to write to back buffers.
    leftEngine.loadIR(leftA.samples.data(),  leftA.originalLength);
    rightEngine.loadIR(rightA.samples.data(), rightA.originalLength);

    morphThread.setChannels({
        { &leftEngine,  &leftA,  &leftB  },
        { &rightEngine, &rightA, &rightB }
    });

    if (wasRunning)
        morphThread.startThread();
}

void DualMonoConvolver::setSingleSlot(const IRSlot& left, const IRSlot& right)
{
    if (morphThread.isThreadRunning())
        morphThread.stopThread(500);

    // Detach the morph thread from any previous slots so a later
    // startMorphing() can't read stale IRSlot pointers.
    morphThread.setChannels({});

    leftEngine.loadIR(left.samples.data(),  left.originalLength);
    rightEngine.loadIR(right.samples.data(), right.originalLength);
}

void DualMonoConvolver::process(float* leftChannel, float* rightChannel, int numSamples)
{
    leftEngine.process(leftChannel, numSamples);
    rightEngine.process(rightChannel, numSamples);
}

void DualMonoConvolver::startMorphing()
{
    if (! morphThread.isThreadRunning())
        morphThread.startThread();
}

void DualMonoConvolver::stopMorphing()
{
    if (morphThread.isThreadRunning())
        morphThread.stopThread(500);
}

} // namespace morphir
