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

// Background thread that continuously computes a spectrally-blended IR
// from two slots and feeds it into the NUPCEngine via its back buffer.
// One MorphThread instance drives one NUPCEngine (single channel).
class MorphThread : public juce::Thread
{
public:
    MorphThread(FFTProvider& fft,
                NUPCEngine&  engine,
                std::size_t  fftSize,
                int          updateIntervalMs = 25);

    // Set the IR slots to morph between. Must not be called while the thread
    // is running; stop the thread, set slots, then start.
    void setSlots(const IRSlot* a, const IRSlot* b);

    // Atomic morph position [0.0, 1.0]. Safe to write from any thread.
    void setMorphPosition(float t) { morphPosition.store(t, std::memory_order_release); }
    float getMorphPosition() const { return morphPosition.load(std::memory_order_acquire); }

    void run() override;

private:
    FFTProvider&        fft;
    NUPCEngine&         engine;
    SpectralMorpher     morpher;
    std::size_t         fftSize;
    int                 updateIntervalMs;

    std::vector<float>  blendedIR;
    std::atomic<float>  morphPosition { 0.0f };

    const IRSlot* slotA = nullptr;
    const IRSlot* slotB = nullptr;
};

} // namespace morphir
