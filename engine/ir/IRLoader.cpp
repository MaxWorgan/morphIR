#include "IRLoader.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace morphir {

namespace {

constexpr double kSilenceEnergyThreshold = 1.0e-12;

std::size_t nextPow2(std::size_t n)
{
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// Returns the channel resampled to the target rate (or a straight copy when
// the rates already match).
std::vector<float> resampleToTarget(const float* samples, std::size_t count,
                                    double srcRate, double targetRate)
{
    if (std::abs(srcRate - targetRate) <= 0.5)
        return { samples, samples + count };

    const double ratio = targetRate / srcRate;
    const std::size_t outLen =
        static_cast<std::size_t>(std::ceil(static_cast<double>(count) * ratio));

    std::vector<float> resampled(outLen, 0.0f);
    juce::LagrangeInterpolator interp;
    interp.reset();
    const int produced = interp.process(1.0 / ratio, samples,
                                        resampled.data(), static_cast<int>(outLen));
    resampled.resize(static_cast<std::size_t>(produced));
    return resampled;
}

// Sum of squares over at most `limit` samples.
double energyOf(const std::vector<float>& samples, std::size_t limit)
{
    double energy = 0.0;
    const std::size_t n = std::min(samples.size(), limit);
    for (std::size_t i = 0; i < n; ++i)
        energy += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
    return energy;
}

} // namespace

IRLoader::IRLoader(FFTProvider& fft, std::size_t maxIRSamples, double targetSampleRate)
    : fft(fft)
    , maxIRSamples(maxIRSamples)
    , fftSize(nextPow2(maxIRSamples))
    , targetSampleRate(targetSampleRate)
{
    formatManager.registerBasicFormats(); // WAV + AIFF
}

IRSlot IRLoader::makeSlot(const std::vector<float>& resampled, double srcRate, float gain)
{
    IRSlot slot;
    slot.originalSampleRate = srcRate;
    slot.originalLength = std::min(resampled.size(), maxIRSamples);
    slot.samples.assign(maxIRSamples, 0.0f);

    std::transform(resampled.begin(),
                   resampled.begin() + static_cast<std::ptrdiff_t>(slot.originalLength),
                   slot.samples.begin(),
                   [gain](float s) { return s * gain; });

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

    // Mono files are duplicated to both channels.
    const float* srcL = buf.getReadPointer(0);
    const float* srcR = (numChannels == 1) ? srcL : buf.getReadPointer(1);
    const auto count  = static_cast<std::size_t>(numSamples);

    const auto left  = resampleToTarget(srcL, count, srcRate, targetSampleRate);
    const auto right = resampleToTarget(srcR, count, srcRate, targetSampleRate);

    // Energy-normalise so convolution is roughly unity gain: scale by
    // 1/sqrt(energy) of the louder channel, applied identically to both
    // channels so the stereo balance of the IR is preserved.
    const double energy = std::max(energyOf(left,  maxIRSamples),
                                   energyOf(right, maxIRSamples));
    if (energy <= kSilenceEnergyThreshold)
    {
        result.error = "IR file is silent: " + file.getFileName();
        return result;
    }
    const float gain = static_cast<float>(1.0 / std::sqrt(energy));

    result.left  = makeSlot(left,  srcRate, gain);
    result.right = makeSlot(right, srcRate, gain);

    return result;
}

} // namespace morphir
