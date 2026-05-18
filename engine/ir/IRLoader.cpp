#include "IRLoader.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace morphir {

static std::size_t nextPow2(std::size_t n)
{
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

IRLoader::IRLoader(FFTProvider& fft, std::size_t maxIRSamples, double targetSampleRate)
    : fft(fft)
    , maxIRSamples(maxIRSamples)
    , fftSize(nextPow2(maxIRSamples))
    , targetSampleRate(targetSampleRate)
{
    formatManager.registerBasicFormats(); // WAV + AIFF
}

IRSlot IRLoader::makeSlot(const float* samples, std::size_t count, double srcRate)
{
    IRSlot slot;
    slot.originalSampleRate = srcRate;

    std::vector<float> resampled;

    if (std::abs(srcRate - targetSampleRate) > 0.5)
    {
        // Resample using JUCE's LagrangeInterpolator
        const double ratio = targetSampleRate / srcRate;
        const std::size_t outLen = static_cast<std::size_t>(std::ceil(static_cast<double>(count) * ratio));
        resampled.resize(outLen, 0.0f);

        juce::LagrangeInterpolator interp;
        interp.reset();
        int produced = interp.process(1.0 / ratio, samples, resampled.data(), static_cast<int>(outLen));
        resampled.resize(static_cast<std::size_t>(produced));

        slot.originalLength = std::min(resampled.size(), maxIRSamples);
        slot.samples.assign(maxIRSamples, 0.0f);
        std::copy(resampled.begin(), resampled.begin() + static_cast<std::ptrdiff_t>(slot.originalLength),
                  slot.samples.begin());
    }
    else
    {
        slot.originalLength = std::min(count, maxIRSamples);
        slot.samples.assign(maxIRSamples, 0.0f);
        std::copy(samples, samples + slot.originalLength, slot.samples.begin());
    }

    // Pre-compute FFT (zero-padded to fftSize)
    std::vector<float> padded(fftSize, 0.0f);
    std::copy(slot.samples.begin(), slot.samples.end(), padded.begin());

    slot.fft.resize(fftSize / 2 + 1);
    fft.forward(padded.data(), slot.fft.data(), fftSize);

    slot.loaded = true;
    return slot;
}

IRLoader::Result IRLoader::load(const juce::File& file)
{
    Result result;

    if (!file.existsAsFile())
    {
        result.error = "File not found: " + file.getFullPathName();
        return result;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        result.error = "Unsupported format or unreadable file: " + file.getFullPathName();
        return result;
    }

    const int numChannels = static_cast<int>(reader->numChannels);
    const int numSamples  = static_cast<int>(reader->lengthInSamples);
    const double srcRate  = reader->sampleRate;

    juce::AudioBuffer<float> buf(std::max(numChannels, 2), numSamples);
    buf.clear();
    reader->read(&buf, 0, numSamples, 0, true, true);

    if (numChannels == 1)
    {
        // Mono: duplicate to both channels
        result.left  = makeSlot(buf.getReadPointer(0), static_cast<std::size_t>(numSamples), srcRate);
        result.right = makeSlot(buf.getReadPointer(0), static_cast<std::size_t>(numSamples), srcRate);
    }
    else
    {
        result.left  = makeSlot(buf.getReadPointer(0), static_cast<std::size_t>(numSamples), srcRate);
        result.right = makeSlot(buf.getReadPointer(1), static_cast<std::size_t>(numSamples), srcRate);
    }

    return result;
}

} // namespace morphir
