#include "MorphThread.h"

namespace morphir {

MorphThread::MorphThread(FFTProvider& fft,
                         std::size_t  fftSize,
                         int          updateIntervalMs)
    : juce::Thread("MorphIR Morph Thread")
    , fft(fft)
    , morpher(fft, fftSize)
    , fftSize(fftSize)
    , updateIntervalMs(updateIntervalMs)
    , blendedIR(fftSize, 0.0f)
{
}

void MorphThread::setChannels(std::vector<Channel> newChannels)
{
    channels = std::move(newChannels);
}

void MorphThread::run()
{
    while (! threadShouldExit())
    {
        const float t = morphPosition.load(std::memory_order_acquire);

        for (auto& ch : channels)
        {
            if (ch.engine == nullptr || ch.slotA == nullptr || ch.slotB == nullptr)
                continue;
            if (! ch.slotA->loaded || ! ch.slotB->loaded)
                continue;
            if (ch.slotA->fft.size() != fftSize / 2 + 1
                || ch.slotB->fft.size() != fftSize / 2 + 1)
                continue;

            morpher.morph(*ch.slotA, *ch.slotB, t, blendedIR.data(), fftSize);
            ch.engine->loadIRToBack(blendedIR.data(),
                                    std::min(blendedIR.size(), ch.engine->getMaxIRSamples()));
        }

        wait(updateIntervalMs);
    }
}

} // namespace morphir
