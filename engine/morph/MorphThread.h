#pragma once
#include <juce_core/juce_core.h>
#include "../fft/FFTProvider.h"
#include "../ir/IRSlot.h"
#include "../morph/SpectralMorpher.h"
#include "../nupc/NUPCEngine.h"
#include <atomic>
#include <vector>
#include <cstddef>

namespace morphir {

// Background thread that computes spectrally-blended IRs from slot pairs and
// feeds them into one or more NUPCEngines via their back buffers. All channels
// share the same morph position, so morphing is sample-coherent across them.
class MorphThread : public juce::Thread
{
public:
    struct Channel
    {
        NUPCEngine*   engine = nullptr;
        const IRSlot* slotA  = nullptr;
        const IRSlot* slotB  = nullptr;
    };

    MorphThread(FFTProvider& fft,
                std::size_t  fftSize,
                int          updateIntervalMs = 25);

    // Replace the channel list. Stop the thread, set channels, then start.
    void setChannels(std::vector<Channel> newChannels);

    void  setMorphPosition(float t) { morphPosition.store(t, std::memory_order_release); }
    float getMorphPosition() const  { return morphPosition.load(std::memory_order_acquire); }

    void run() override;

private:
    FFTProvider&        fft;
    SpectralMorpher     morpher;
    std::size_t         fftSize;
    int                 updateIntervalMs;

    std::vector<float>  blendedIR;
    std::atomic<float>  morphPosition { 0.0f };

    std::vector<Channel> channels;
};

} // namespace morphir
