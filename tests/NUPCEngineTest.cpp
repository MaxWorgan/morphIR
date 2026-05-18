#include <juce_core/juce_core.h>
#include "../engine/fft/FFTProvider.h"
#include "../engine/nupc/NUPCEngine.h"
#include <vector>
#include <cmath>

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
