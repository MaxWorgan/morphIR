#include <juce_core/juce_core.h>
#include "../engine/fft/FFTProvider.h"
#include <cmath>
#include <vector>
#include <complex>

class FFTProviderTests : public juce::UnitTest
{
public:
    FFTProviderTests() : juce::UnitTest("FFTProvider", "morphir") {}

    void runTest() override
    {
        auto fft = morphir::FFTProvider::create();

        beginTest("Round-trip: impulse at position 0");
        {
            const std::size_t size = 1024;
            std::vector<float> input(size, 0.0f);
            input[0] = 1.0f;

            std::vector<std::complex<float>> spectrum(size / 2 + 1);
            fft->forward(input.data(), spectrum.data(), size);

            std::vector<float> output(size, 0.0f);
            fft->inverse(spectrum.data(), output.data(), size);

            for (std::size_t i = 0; i < size; ++i)
                expectWithinAbsoluteError(output[i], input[i], 1e-5f);
        }

        beginTest("Round-trip: sine wave");
        {
            const std::size_t size = 2048;
            std::vector<float> input(size);
            for (std::size_t i = 0; i < size; ++i)
                input[i] = std::sin(2.0f * juce::MathConstants<float>::pi * 10.0f * static_cast<float>(i) / static_cast<float>(size));

            std::vector<std::complex<float>> spectrum(size / 2 + 1);
            fft->forward(input.data(), spectrum.data(), size);

            std::vector<float> output(size);
            fft->inverse(spectrum.data(), output.data(), size);

            for (std::size_t i = 0; i < size; ++i)
                expectWithinAbsoluteError(output[i], input[i], 1e-5f);
        }

        beginTest("Round-trip: large buffer (65536 samples)");
        {
            const std::size_t size = 65536;
            std::vector<float> input(size, 0.0f);
            input[0] = 1.0f;
            input[100] = 0.5f;

            std::vector<std::complex<float>> spectrum(size / 2 + 1);
            fft->forward(input.data(), spectrum.data(), size);

            std::vector<float> output(size);
            fft->inverse(spectrum.data(), output.data(), size);

            for (std::size_t i = 0; i < size; ++i)
                expectWithinAbsoluteError(output[i], input[i], 1e-4f);
        }

        beginTest("DC bin is real-valued");
        {
            const std::size_t size = 512;
            std::vector<float> input(size, 1.0f); // constant signal = pure DC
            std::vector<std::complex<float>> spectrum(size / 2 + 1);
            fft->forward(input.data(), spectrum.data(), size);
            expectWithinAbsoluteError(spectrum[0].imag(), 0.0f, 1e-5f);
        }

        beginTest("FFTSetup is cached (second call with same size doesn't crash)");
        {
            const std::size_t size = 1024;
            std::vector<float> input(size, 0.0f);
            input[0] = 1.0f;
            std::vector<std::complex<float>> spectrum(size / 2 + 1);
            fft->forward(input.data(), spectrum.data(), size);
            fft->forward(input.data(), spectrum.data(), size); // reuse setup
            expect(true); // no crash
        }
    }
};

static FFTProviderTests fftProviderTests;
