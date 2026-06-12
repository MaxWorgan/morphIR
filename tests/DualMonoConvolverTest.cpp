#include <juce_core/juce_core.h>
#include "../engine/fft/FFTProvider.h"
#include "../engine/ir/IRSlot.h"
#include "../engine/nupc/DualMonoConvolver.h"
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

class DualMonoConvolverTests : public juce::UnitTest
{
public:
    DualMonoConvolverTests() : juce::UnitTest("DualMonoConvolver", "morphir") {}

    void runTest() override
    {
        auto fft = morphir::FFTProvider::create();
        const std::size_t maxIRSamples = 2048; // already a power of 2
        const std::size_t fftSize      = 2048;
        const int         blockSize    = 64;

        // Two IR sets with distinct characteristics so L and R can be distinguished.
        std::vector<float> diracOne(fftSize, 0.0f);   diracOne[0] = 1.0f;
        std::vector<float> diracHalf(fftSize, 0.0f);  diracHalf[0] = 0.5f;
        std::vector<float> diracQuarter(fftSize, 0.0f); diracQuarter[0] = 0.25f;
        std::vector<float> diracEighth(fftSize, 0.0f);  diracEighth[0] = 0.125f;

        auto slotLeftA  = makeSlot(*fft, fftSize, diracOne,     1);
        auto slotLeftB  = makeSlot(*fft, fftSize, diracHalf,    1);
        auto slotRightA = makeSlot(*fft, fftSize, diracQuarter, 1);
        auto slotRightB = makeSlot(*fft, fftSize, diracEighth,  1);

        beginTest("No crosstalk between L and R");
        {
            morphir::DualMonoConvolver conv(*fft, maxIRSamples, blockSize);
            conv.setSlots(slotLeftA, slotLeftB, slotRightA, slotRightB);
            conv.setMorphPosition(0.0f);
            // Don't start the morph thread — we want a static IR for this test.

            std::vector<float> L(blockSize, 0.0f);
            std::vector<float> R(blockSize, 0.0f);
            L[0] = 1.0f;  // impulse into L only
            // R remains silent

            conv.process(L.data(), R.data(), blockSize);

            // L should have non-trivial output (slotLeftA = Dirac 1.0)
            float lMax = 0.0f;
            for (int i = 0; i < blockSize; ++i)
                lMax = std::max(lMax, std::abs(L[i]));
            expect(lMax > 0.5f, "L output should reflect L impulse through L's IR");

            // R should be silent — no input was fed to R, so no convolution leakage
            for (int i = 0; i < blockSize; ++i)
                expectWithinAbsoluteError(R[i], 0.0f, 1e-4f);
        }

        beginTest("Symmetric input + symmetric IRs produce identical L/R output");
        {
            morphir::DualMonoConvolver conv(*fft, maxIRSamples, blockSize);
            // Same slot pair on both channels (mono IR setup)
            conv.setSlots(slotLeftA, slotLeftB, slotLeftA, slotLeftB);
            conv.setMorphPosition(0.0f);

            std::vector<float> L(blockSize, 0.0f);
            std::vector<float> R(blockSize, 0.0f);
            L[0] = 1.0f;
            R[0] = 1.0f;

            conv.process(L.data(), R.data(), blockSize);

            for (int i = 0; i < blockSize; ++i)
                expectWithinAbsoluteError(L[i], R[i], 1e-5f);
        }

        beginTest("Different IRs on L and R produce different outputs");
        {
            morphir::DualMonoConvolver conv(*fft, maxIRSamples, blockSize);
            conv.setSlots(slotLeftA, slotLeftB, slotRightA, slotRightB);
            conv.setMorphPosition(0.0f);

            std::vector<float> L(blockSize, 0.0f);
            std::vector<float> R(blockSize, 0.0f);
            L[0] = 1.0f;
            R[0] = 1.0f;

            conv.process(L.data(), R.data(), blockSize);

            // L's IR has amplitude 1.0; R's IR has amplitude 0.25 → outputs should differ
            float diff = 0.0f;
            for (int i = 0; i < blockSize; ++i)
                diff += std::abs(L[i] - R[i]);
            expect(diff > 0.1f, "L and R outputs should differ when their IRs differ");
        }

        beginTest("Single slot pair convolves without the morph thread");
        {
            morphir::DualMonoConvolver conv(*fft, maxIRSamples, blockSize);
            conv.setSingleSlot(slotLeftA, slotRightA); // dirac 1.0 / dirac 0.25

            std::vector<float> L(blockSize, 0.0f);
            std::vector<float> R(blockSize, 0.0f);
            L[0] = 1.0f;
            R[0] = 1.0f;

            conv.process(L.data(), R.data(), blockSize);

            expectWithinAbsoluteError(L[0], 1.0f,  1e-4f);
            expectWithinAbsoluteError(R[0], 0.25f, 1e-4f);
        }

        beginTest("Morph thread updates both channels within one period");
        {
            morphir::DualMonoConvolver conv(*fft, maxIRSamples, blockSize);
            conv.setSlots(slotLeftA, slotLeftB, slotRightA, slotRightB);
            conv.setMorphPosition(0.0f);
            conv.startMorphing();

            // Run audio while sweeping position; verify outputs stay finite and
            // both channels remain non-trivial throughout.
            const int totalBlocks = (48000 / blockSize) / 5; // ~200ms
            float lMaxOverall = 0.0f, rMaxOverall = 0.0f;

            for (int b = 0; b < totalBlocks; ++b)
            {
                std::vector<float> L(blockSize, 0.0f), R(blockSize, 0.0f);
                L[0] = (b % 4 == 0) ? 1.0f : 0.0f;
                R[0] = (b % 4 == 0) ? 1.0f : 0.0f;

                conv.process(L.data(), R.data(), blockSize);

                for (int i = 0; i < blockSize; ++i)
                {
                    expect(std::isfinite(L[i]) && std::isfinite(R[i]));
                    lMaxOverall = std::max(lMaxOverall, std::abs(L[i]));
                    rMaxOverall = std::max(rMaxOverall, std::abs(R[i]));
                }

                conv.setMorphPosition(static_cast<float>(b) / static_cast<float>(totalBlocks));
                juce::Thread::sleep(1);
            }

            expect(lMaxOverall > 0.05f, "L should produce audible output during sweep");
            expect(rMaxOverall > 0.01f, "R should produce audible output during sweep");
            expect(lMaxOverall < 10.0f && rMaxOverall < 10.0f, "outputs should not blow up");

            conv.stopMorphing();
        }
    }
};

static DualMonoConvolverTests dualMonoConvolverTests;
