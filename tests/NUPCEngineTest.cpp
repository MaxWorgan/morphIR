#include <juce_core/juce_core.h>
#include "../engine/fft/FFTProvider.h"
#include "../engine/nupc/NUPCEngine.h"
#include <vector>
#include <cmath>
#include <thread>
#include <atomic>

class NUPCEngineTests : public juce::UnitTest
{
public:
    NUPCEngineTests() : juce::UnitTest("NUPCEngine", "morphir") {}

    void runTest() override
    {
        const int blockSize = 64;
        const int sampleRate = 48000;
        auto fftProvider = morphir::FFTProvider::create();

        beginTest("Silence in, silence out");
        {
            morphir::NUPCEngine engine(*fftProvider, 1024, blockSize);

            // Load identity IR
            std::vector<float> ir(1024, 0.0f);
            ir[0] = 1.0f;
            engine.loadIR(ir.data(), ir.size());

            std::vector<float> block(blockSize, 0.0f);
            for (int b = 0; b < 10; ++b)
            {
                engine.process(block.data(), blockSize);
                for (int i = 0; i < blockSize; ++i)
                    expectWithinAbsoluteError(block[i], 0.0f, 1e-4f);
            }
        }

        beginTest("Known short IR: impulse response matches IR");
        {
            // IR = [1.0, 0.5, 0.25, 0.125, 0, 0, ...]
            // Feed a unit impulse. Collect enough output to see all 4 IR samples.
            const std::size_t irLen = 512;
            std::vector<float> ir(irLen, 0.0f);
            ir[0] = 1.0f;
            ir[1] = 0.5f;
            ir[2] = 0.25f;
            ir[3] = 0.125f;

            morphir::NUPCEngine engine(*fftProvider, irLen, blockSize);
            engine.loadIR(ir.data(), irLen);

            // Feed one block with a unit impulse at position 0
            std::vector<float> inputBlock(blockSize, 0.0f);
            inputBlock[0] = 1.0f;

            // Collect output over several blocks
            std::vector<float> output;
            engine.process(inputBlock.data(), blockSize);
            for (int i = 0; i < blockSize; ++i)
                output.push_back(inputBlock[i]);

            // Process silent blocks to flush the tail
            std::vector<float> silentBlock(blockSize, 0.0f);
            for (int b = 0; b < 4; ++b)
            {
                std::fill(silentBlock.begin(), silentBlock.end(), 0.0f);
                engine.process(silentBlock.data(), blockSize);
                for (int i = 0; i < blockSize; ++i)
                    output.push_back(silentBlock[i]);
            }

            // Find the peak (where the impulse response arrives)
            float maxVal = 0.0f;
            int maxIdx = 0;
            for (int i = 0; i < static_cast<int>(output.size()); ++i)
            {
                if (std::abs(output[i]) > maxVal)
                {
                    maxVal = std::abs(output[i]);
                    maxIdx = i;
                }
            }

            // The impulse response should appear in the output
            expectWithinAbsoluteError(output[maxIdx],     1.0f,  0.1f);
            expectWithinAbsoluteError(output[maxIdx + 1], 0.5f,  0.1f);
            expectWithinAbsoluteError(output[maxIdx + 2], 0.25f, 0.1f);
        }

        beginTest("Long IR spanning multiple partition levels matches direct convolution");
        {
            // IR length 3000 with blockSize 64 engages level-0 (16 x 64 = 1024)
            // and level-1 (512-sample) partitions.
            const std::size_t maxIR = 4096;
            const std::size_t irLen = 3000;
            juce::Random rng(42);

            std::vector<float> ir(irLen);
            for (auto& s : ir)
                s = (rng.nextFloat() * 2.0f - 1.0f) / 50.0f;

            const int numBlocks = 80;
            std::vector<float> input(static_cast<std::size_t>(numBlocks * blockSize));
            for (auto& s : input)
                s = rng.nextFloat() * 2.0f - 1.0f;

            // Direct convolution reference, double-precision accumulation.
            std::vector<float> ref(input.size(), 0.0f);
            for (std::size_t n = 0; n < ref.size(); ++n)
            {
                double acc = 0.0;
                const std::size_t kMax = std::min(irLen - 1, n);
                for (std::size_t k = 0; k <= kMax; ++k)
                    acc += static_cast<double>(ir[k]) * static_cast<double>(input[n - k]);
                ref[n] = static_cast<float>(acc);
            }

            morphir::NUPCEngine engine(*fftProvider, maxIR, blockSize);
            engine.loadIR(ir.data(), irLen);

            std::vector<float> out(input);
            for (int b = 0; b < numBlocks; ++b)
                engine.process(out.data() + b * blockSize, blockSize);

            float maxRef = 0.0f, maxErr = 0.0f;
            for (std::size_t n = 0; n < ref.size(); ++n)
            {
                maxRef = std::max(maxRef, std::abs(ref[n]));
                maxErr = std::max(maxErr, std::abs(out[n] - ref[n]));
            }
            expect(maxErr <= 1e-3f * maxRef,
                   "max error " + juce::String(maxErr) + " vs max ref " + juce::String(maxRef));
        }

        beginTest("Concurrent reloads of an identical IR leave output unchanged");
        {
            // Hammering loadIRToBack with the *same* IR data from another
            // thread must be output-invariant: both double buffers always
            // hold identical spectra, so swaps cannot change the result.
            // Any deviation means shared state is being corrupted.
            const std::size_t maxIR = 4096;
            const std::size_t irLen = 3000;
            juce::Random rng(7);

            std::vector<float> ir(irLen);
            for (auto& s : ir)
                s = (rng.nextFloat() * 2.0f - 1.0f) / 50.0f;

            const int numBlocks = 1500;
            std::vector<float> input(static_cast<std::size_t>(numBlocks * blockSize));
            for (auto& s : input)
                s = rng.nextFloat() * 2.0f - 1.0f;

            // Reference: same engine config, no concurrent activity.
            std::vector<float> ref(input);
            {
                morphir::NUPCEngine engine(*fftProvider, maxIR, blockSize);
                engine.loadIR(ir.data(), irLen);
                for (int b = 0; b < numBlocks; ++b)
                    engine.process(ref.data() + b * blockSize, blockSize);
            }

            morphir::NUPCEngine engine(*fftProvider, maxIR, blockSize);
            engine.loadIR(ir.data(), irLen);

            std::atomic<bool> stop { false };
            std::thread hammer([&]
            {
                while (! stop.load(std::memory_order_acquire))
                    engine.loadIRToBack(ir.data(), irLen);
            });

            std::vector<float> out(input);
            for (int b = 0; b < numBlocks; ++b)
                engine.process(out.data() + b * blockSize, blockSize);

            stop.store(true, std::memory_order_release);
            hammer.join();

            float maxDiff = 0.0f;
            for (std::size_t n = 0; n < out.size(); ++n)
                maxDiff = std::max(maxDiff, std::abs(out[n] - ref[n]));
            expect(maxDiff <= 1e-6f,
                   "output deviated by " + juce::String(maxDiff) + " under concurrent reloads");
        }

        beginTest("Engine constructs without crashing for large IR");
        {
            // 30s at 48kHz
            const std::size_t largeIR = static_cast<std::size_t>(sampleRate) * 30;
            morphir::NUPCEngine engine(*fftProvider, largeIR, blockSize);

            std::vector<float> ir(1024, 0.0f);
            ir[0] = 1.0f;
            engine.loadIR(ir.data(), ir.size());

            std::vector<float> block(blockSize, 0.0f);
            block[0] = 1.0f;
            engine.process(block.data(), blockSize);
            expect(true); // no crash or allocation
        }
    }
};

static NUPCEngineTests nupcEngineTests;
