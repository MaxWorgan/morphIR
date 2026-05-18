#include <juce_core/juce_core.h>
#include "../engine/fft/FFTProvider.h"
#include "../engine/ir/IRSlot.h"
#include "../engine/morph/SpectralMorpher.h"
#include "../engine/morph/MorphThread.h"
#include "../engine/nupc/NUPCEngine.h"
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

class MorphThreadTests : public juce::UnitTest
{
public:
    MorphThreadTests() : juce::UnitTest("MorphThread", "morphir") {}

    void runTest() override
    {
        auto fft = morphir::FFTProvider::create();
        const std::size_t fftSize  = 2048;
        const int         blockSize = 64;

        beginTest("NUPCEngine: loadIRToBack triggers swap on next process()");
        {
            morphir::NUPCEngine engine(*fft, fftSize, blockSize);

            // Initial IR: all zeros → silence output
            std::vector<float> zeroIR(fftSize, 0.0f);
            engine.loadIR(zeroIR.data(), zeroIR.size());

            std::vector<float> block(blockSize, 0.0f);
            block[0] = 1.0f;
            engine.process(block.data(), blockSize);
            for (int i = 0; i < blockSize; ++i)
                expectWithinAbsoluteError(block[i], 0.0f, 1e-4f);

            // Push a Dirac IR to back buffer
            std::vector<float> diracIR(fftSize, 0.0f);
            diracIR[0] = 1.0f;
            const bool ok = engine.loadIRToBack(diracIR.data(), diracIR.size());
            expect(ok);

            // Drive an impulse through; the swap happens at the next process() call
            std::vector<float> block2(blockSize, 0.0f);
            block2[0] = 1.0f;
            engine.process(block2.data(), blockSize);
            // After swap, Dirac IR is active → impulse passes through somewhere in output
            float maxVal = 0.0f;
            for (int i = 0; i < blockSize; ++i)
                maxVal = std::max(maxVal, std::abs(block2[i]));
            expect(maxVal > 0.5f);
        }

        beginTest("NUPCEngine: loadIRToBack returns false while swap pending");
        {
            morphir::NUPCEngine engine(*fft, fftSize, blockSize);
            std::vector<float> ir(fftSize, 0.0f);
            ir[0] = 1.0f;
            engine.loadIR(ir.data(), ir.size());

            expect(engine.loadIRToBack(ir.data(), ir.size()));    // first ok
            expect(! engine.loadIRToBack(ir.data(), ir.size()));  // pending, second rejected

            // Audio thread consumes
            std::vector<float> block(blockSize, 0.0f);
            engine.process(block.data(), blockSize);

            // Now it can accept again
            expect(engine.loadIRToBack(ir.data(), ir.size()));
        }

        beginTest("MorphThread: feeds blended IR while sweeping position");
        {
            // Slot A: Dirac at 0 with amplitude 1
            std::vector<float> aTime(fftSize, 0.0f);
            aTime[0] = 1.0f;
            auto slotA = makeSlot(*fft, fftSize, aTime, 1);

            // Slot B: Dirac at 0 with amplitude 0.5
            std::vector<float> bTime(fftSize, 0.0f);
            bTime[0] = 0.5f;
            auto slotB = makeSlot(*fft, fftSize, bTime, 1);

            morphir::NUPCEngine engine(*fft, fftSize, blockSize);

            // Initial state: load slot A as starting IR
            engine.loadIR(slotA.samples.data(), slotA.originalLength);

            morphir::MorphThread morphThread(*fft, fftSize, /*updateMs=*/10);
            morphThread.setChannels({ { &engine, &slotA, &slotB } });
            morphThread.setMorphPosition(0.0f);
            morphThread.startThread();

            // Process audio for ~300ms while sweeping morph position
            const int totalBlocks = (48000 / blockSize) / 3; // ~333ms of audio
            float maxObserved = 0.0f;
            for (int b = 0; b < totalBlocks; ++b)
            {
                std::vector<float> block(blockSize, 0.0f);
                block[0] = (b == 0) ? 1.0f : 0.0f; // impulse at start

                engine.process(block.data(), blockSize);

                for (int i = 0; i < blockSize; ++i)
                {
                    expect(std::isfinite(block[i]), "output contains non-finite sample");
                    maxObserved = std::max(maxObserved, std::abs(block[i]));
                }

                // Sweep morph position across the run
                morphThread.setMorphPosition(static_cast<float>(b) / static_cast<float>(totalBlocks));

                juce::Thread::sleep(1); // let morph thread run between blocks
            }

            expect(maxObserved > 0.1f, "expected non-trivial output");
            expect(maxObserved < 10.0f, "output blew up - possible swap-during-process race");

            morphThread.stopThread(500);
            expect(! morphThread.isThreadRunning());
        }
    }
};

static MorphThreadTests morphThreadTests;
