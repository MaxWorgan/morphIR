#pragma once
#include "IRSlot.h"
#include "../fft/FFTProvider.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace morphir {

class IRLoader
{
public:
    struct Result
    {
        IRSlot       left;
        IRSlot       right;
        juce::String error; // empty on success
    };

    IRLoader(FFTProvider& fft, std::size_t maxIRSamples, double targetSampleRate);

    // Loads a WAV or AIFF file synchronously. Never call from the audio thread.
    Result load(const juce::File& file);

private:
    IRSlot makeSlot(const std::vector<float>& resampled, double srcRate, float gain);

    FFTProvider&            fft;
    std::size_t             maxIRSamples;
    std::size_t             fftSize;
    double                  targetSampleRate;
    juce::AudioFormatManager formatManager;
};

} // namespace morphir
