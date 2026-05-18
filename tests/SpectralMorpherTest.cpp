#include <juce_core/juce_core.h>
#include "../engine/fft/FFTProvider.h"
#include "../engine/ir/IRSlot.h"
#include "../engine/morph/SpectralMorpher.h"
#include <vector>
#include <cmath>

static morphir::IRSlot makeSlot(morphir::FFTProvider& fft,
                                 std::size_t fftSize,
                                 const std::vector<float>& timeDomain,
                                 std::size_t originalLength)
{
    morphir::IRSlot slot;
    slot.samples.assign(fftSize, 0.0f);
    std::copy(timeDomain.begin(),
              timeDomain.begin() + std::min(timeDomain.size(), fftSize),
              slot.samples.begin());
    slot.fft.resize(fftSize / 2 + 1);
    fft.forward(slot.samples.data(), slot.fft.data(), fftSize);
    slot.originalLength = originalLength;
    slot.loaded = true;
    return slot;
}

class SpectralMorpherTests : public juce::UnitTest
{
public:
    SpectralMorpherTests() : juce::UnitTest("SpectralMorpher", "morphir") {}

    void runTest() override
    {
        auto fft = morphir::FFTProvider::create();
        const std::size_t fftSize = 1024;

        // Slot A: Dirac at position 0 (impulse → flat spectrum, magnitude 1.0)
        std::vector<float> aTime(fftSize, 0.0f);
        aTime[0] = 1.0f;
        auto slotA = makeSlot(*fft, fftSize, aTime, 1);

        // Slot B: scaled Dirac at position 0 (impulse → flat spectrum, magnitude 2.0)
        std::vector<float> bTime(fftSize, 0.0f);
        bTime[0] = 2.0f;
        auto slotB = makeSlot(*fft, fftSize, bTime, 1);

        morphir::SpectralMorpher morpher(*fft, fftSize);

        beginTest("Position 0.0 reproduces slot A");
        {
            std::vector<float> out(fftSize);
            morpher.morph(slotA, slotB, 0.0f, out.data(), fftSize);
            expectWithinAbsoluteError(out[0], 1.0f, 1e-3f);
            for (std::size_t i = 1; i < 16; ++i)
                expectWithinAbsoluteError(out[i], 0.0f, 1e-3f);
        }

        beginTest("Position 1.0 reproduces slot B");
        {
            std::vector<float> out(fftSize);
            morpher.morph(slotA, slotB, 1.0f, out.data(), fftSize);
            expectWithinAbsoluteError(out[0], 2.0f, 1e-3f);
            for (std::size_t i = 1; i < 16; ++i)
                expectWithinAbsoluteError(out[i], 0.0f, 1e-3f);
        }

        beginTest("Position 0.5 gives midpoint magnitude for Dirac inputs");
        {
            std::vector<float> out(fftSize);
            morpher.morph(slotA, slotB, 0.5f, out.data(), fftSize);
            // Midpoint of 1.0 and 2.0 → 1.5 at sample 0
            expectWithinAbsoluteError(out[0], 1.5f, 1e-3f);
        }

        beginTest("Spectral magnitudes are midway at position 0.5");
        {
            // Build two IRs with non-trivial spectral content
            std::vector<float> aData(fftSize, 0.0f);
            std::vector<float> bData(fftSize, 0.0f);
            for (std::size_t i = 0; i < 64; ++i)
            {
                aData[i] = std::sin(2.0f * juce::MathConstants<float>::pi * 5.0f * static_cast<float>(i) / 64.0f);
                bData[i] = std::sin(2.0f * juce::MathConstants<float>::pi * 5.0f * static_cast<float>(i) / 64.0f) * 3.0f;
            }
            auto sA = makeSlot(*fft, fftSize, aData, 64);
            auto sB = makeSlot(*fft, fftSize, bData, 64);

            std::vector<float> out(fftSize);
            morpher.morph(sA, sB, 0.5f, out.data(), fftSize);

            // Take FFT of output, compare magnitudes to expected midpoint
            std::vector<std::complex<float>> outSpec(fftSize / 2 + 1);
            fft->forward(out.data(), outSpec.data(), fftSize);

            for (std::size_t k = 0; k < fftSize / 2 + 1; ++k)
            {
                const float expectedMag = 0.5f * (std::abs(sA.fft[k]) + std::abs(sB.fft[k]));
                expectWithinAbsoluteError(std::abs(outSpec[k]), expectedMag, 1e-3f);
            }
        }

        beginTest("Blended length is linearly interpolated");
        {
            std::vector<float> shortData(fftSize, 0.5f); // pretend originalLength = 100
            std::vector<float> longData(fftSize, 0.5f);  // pretend originalLength = 500
            auto sShort = makeSlot(*fft, fftSize, shortData, 100);
            auto sLong  = makeSlot(*fft, fftSize, longData, 500);

            std::vector<float> out(fftSize);
            morpher.morph(sShort, sLong, 0.5f, out.data(), fftSize);
            // Expected blended length: lerp(100, 500, 0.5) = 300
            // Sample at 250 should be non-zero (within blended region);
            // sample at 350 should be zero (beyond blended length)
            expect(std::abs(out[250]) > 1e-3f);
            for (std::size_t i = 301; i < fftSize; ++i)
                expectWithinAbsoluteError(out[i], 0.0f, 1e-6f);
        }

        beginTest("Identical IRs produce identical output regardless of position");
        {
            std::vector<float> data(fftSize, 0.0f);
            data[0] = 0.7f;
            data[5] = -0.3f;
            auto s = makeSlot(*fft, fftSize, data, 32);

            std::vector<float> out0(fftSize), out05(fftSize), out1(fftSize);
            morpher.morph(s, s, 0.0f, out0.data(), fftSize);
            morpher.morph(s, s, 0.5f, out05.data(), fftSize);
            morpher.morph(s, s, 1.0f, out1.data(), fftSize);

            for (std::size_t i = 0; i < 32; ++i)
            {
                expectWithinAbsoluteError(out0[i],  out05[i], 1e-4f);
                expectWithinAbsoluteError(out05[i], out1[i],  1e-4f);
            }
        }

        beginTest("Stateless: repeated calls produce identical results");
        {
            std::vector<float> out1(fftSize), out2(fftSize);
            morpher.morph(slotA, slotB, 0.3f, out1.data(), fftSize);
            morpher.morph(slotA, slotB, 0.3f, out2.data(), fftSize);
            for (std::size_t i = 0; i < fftSize; ++i)
                expectWithinAbsoluteError(out1[i], out2[i], 1e-9f);
        }
    }
};

static SpectralMorpherTests spectralMorpherTests;
