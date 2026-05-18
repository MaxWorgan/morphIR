#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../engine/fft/FFTProvider.h"
#include "../engine/ir/IRSlot.h"
#include "../engine/ir/IRLoader.h"
#include <vector>
#include <cmath>

static juce::File writeTempWav(const juce::AudioBuffer<float>& buf, double sampleRate)
{
    juce::TemporaryFile tmp(".wav");
    auto stream = tmp.getFile().createOutputStream();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.release(), sampleRate,
                            static_cast<unsigned int>(buf.getNumChannels()), 24, {}, 0));
    writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    writer.reset();
    // Copy to a permanent temp path so the TemporaryFile destructor doesn't delete it yet
    juce::File dest = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("morphir_test_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");
    tmp.getFile().copyFileTo(dest);
    return dest;
}

class IRLoaderTests : public juce::UnitTest
{
public:
    IRLoaderTests() : juce::UnitTest("IRLoader", "morphir") {}

    void runTest() override
    {
        auto fftProvider = morphir::FFTProvider::create();
        const std::size_t maxIR = 4096;
        const double targetRate = 48000.0;

        beginTest("Round-trip FFT: IFFT of stored FFT matches original");
        {
            const int len = 512;
            juce::AudioBuffer<float> buf(1, len);
            buf.clear();
            buf.setSample(0, 0, 1.0f); // Dirac

            juce::File f = writeTempWav(buf, targetRate);
            morphir::IRLoader loader(*fftProvider, maxIR, targetRate);
            auto result = loader.load(f);
            f.deleteFile();

            expect(result.error.isEmpty(), result.error);
            expect(result.left.loaded);

            // IFFT the stored FFT
            std::vector<float> recovered(maxIR, 0.0f);
            fftProvider->inverse(result.left.fft.data(), recovered.data(), maxIR);

            // Dirac at 0 should survive the round-trip
            expectWithinAbsoluteError(recovered[0], 1.0f, 1e-3f);
            for (int i = 1; i < 16; ++i)
                expectWithinAbsoluteError(recovered[i], 0.0f, 1e-3f);
        }

        beginTest("Mono file is duplicated to both slots");
        {
            const int len = 256;
            juce::AudioBuffer<float> buf(1, len);
            buf.clear();
            for (int i = 0; i < len; ++i)
                buf.setSample(0, i, static_cast<float>(i) / static_cast<float>(len));

            juce::File f = writeTempWav(buf, targetRate);
            morphir::IRLoader loader(*fftProvider, maxIR, targetRate);
            auto result = loader.load(f);
            f.deleteFile();

            expect(result.left.loaded && result.right.loaded);
            for (int i = 0; i < len; ++i)
                expectWithinAbsoluteError(result.left.samples[i], result.right.samples[i], 1e-6f);
        }

        beginTest("Stereo file splits into left and right correctly");
        {
            const int len = 256;
            juce::AudioBuffer<float> buf(2, len);
            buf.clear();
            buf.setSample(0, 0, 1.0f); // L: impulse at 0
            buf.setSample(1, 1, 1.0f); // R: impulse at 1

            juce::File f = writeTempWav(buf, targetRate);
            morphir::IRLoader loader(*fftProvider, maxIR, targetRate);
            auto result = loader.load(f);
            f.deleteFile();

            expect(result.left.loaded && result.right.loaded);
            expectWithinAbsoluteError(result.left.samples[0],  1.0f, 1e-3f);
            expectWithinAbsoluteError(result.left.samples[1],  0.0f, 1e-3f);
            expectWithinAbsoluteError(result.right.samples[0], 0.0f, 1e-3f);
            expectWithinAbsoluteError(result.right.samples[1], 1.0f, 1e-3f);
        }

        beginTest("originalLength matches file length");
        {
            const int len = 300;
            juce::AudioBuffer<float> buf(1, len);
            buf.clear();
            buf.setSample(0, 0, 1.0f);

            juce::File f = writeTempWav(buf, targetRate);
            morphir::IRLoader loader(*fftProvider, maxIR, targetRate);
            auto result = loader.load(f);
            f.deleteFile();

            expectEquals(static_cast<int>(result.left.originalLength), len);
        }

        beginTest("samples are zero-padded to maxIRSamples");
        {
            const int len = 128;
            juce::AudioBuffer<float> buf(1, len);
            buf.clear();
            buf.setSample(0, 0, 1.0f);

            juce::File f = writeTempWav(buf, targetRate);
            morphir::IRLoader loader(*fftProvider, maxIR, targetRate);
            auto result = loader.load(f);
            f.deleteFile();

            expectEquals(static_cast<int>(result.left.samples.size()), static_cast<int>(maxIR));
            for (std::size_t i = static_cast<std::size_t>(len); i < maxIR; ++i)
                expectWithinAbsoluteError(result.left.samples[i], 0.0f, 1e-9f);
        }

        beginTest("Missing file returns error");
        {
            juce::File missing("/tmp/morphir_nonexistent_file_xyz.wav");
            morphir::IRLoader loader(*fftProvider, maxIR, targetRate);
            auto result = loader.load(missing);

            expect(result.error.isNotEmpty());
            expect(!result.left.loaded);
            expect(!result.right.loaded);
        }
    }
};

static IRLoaderTests irLoaderTests;
