#include "MorphThread.h"

namespace morphir {

MorphThread::MorphThread(FFTProvider& fft,
                         NUPCEngine&  engine,
                         std::size_t  fftSize,
                         int          updateIntervalMs)
    : juce::Thread("MorphIR Morph Thread")
    , fft(fft)
    , engine(engine)
    , morpher(fft, fftSize)
    , fftSize(fftSize)
    , updateIntervalMs(updateIntervalMs)
    , blendedIR(fftSize, 0.0f)
{
}

void MorphThread::setSlots(const IRSlot* a, const IRSlot* b)
{
    slotA = a;
    slotB = b;
}

void MorphThread::run()
{
    while (! threadShouldExit())
    {
        if (slotA != nullptr && slotB != nullptr
            && slotA->loaded && slotB->loaded
            && slotA->fft.size() == fftSize / 2 + 1
            && slotB->fft.size() == fftSize / 2 + 1)
        {
            const float t = morphPosition.load(std::memory_order_acquire);
            morpher.morph(*slotA, *slotB, t, blendedIR.data(), fftSize);

            // Push to NUPCEngine. If the audio thread hasn't consumed the
            // previous update, this returns false — we just sleep and retry.
            engine.loadIRToBack(blendedIR.data(),
                                std::min(blendedIR.size(), engine.getMaxIRSamples()));
        }

        wait(updateIntervalMs);
    }
}

} // namespace morphir
