#pragma once
#include "NUPCEngine.h"
#include "../fft/FFTProvider.h"
#include "../ir/IRSlot.h"
#include "../morph/MorphThread.h"
#include <cstddef>

namespace morphir {

// Stereo convolver as two independent NUPCEngines (L and R) sharing one
// MorphThread and one morph position. Dual mono — not true 4-channel stereo.
class DualMonoConvolver
{
public:
    DualMonoConvolver(FFTProvider& fft, std::size_t maxIRSamples, int blockSize);
    ~DualMonoConvolver();

    // Set the slot pairs to morph between for each channel. The IRSlot objects
    // must outlive the convolver. Stops the morph thread, swaps slots, restarts.
    void setSlots(const IRSlot& leftA,  const IRSlot& leftB,
                  const IRSlot& rightA, const IRSlot& rightB);

    // Bind a single IR pair with no morphing (one slot loaded). Stops the
    // morph thread; the engines convolve this IR until setSlots is called.
    void setSingleSlot(const IRSlot& left, const IRSlot& right);

    // Atomic morph position [0, 1].
    void  setMorphPosition(float t) { morphThread.setMorphPosition(t); }
    float getMorphPosition() const  { return morphThread.getMorphPosition(); }

    // Process a stereo block in-place. Audio thread only.
    void process(float* leftChannel, float* rightChannel, int numSamples);

    void startMorphing();
    void stopMorphing();

private:
    static std::size_t nextPow2(std::size_t n);

    FFTProvider& fft;
    std::size_t  fftSize;

    NUPCEngine   leftEngine;
    NUPCEngine   rightEngine;
    MorphThread  morphThread;
};

} // namespace morphir
