#include <juce_core/juce_core.h>
#include "../engine/util/BlockAdapter.h"
#include <vector>
#include <numeric>

class BlockAdapterTests : public juce::UnitTest
{
public:
    BlockAdapterTests() : juce::UnitTest("BlockAdapter", "morphir") {}

    void runTest() override
    {
        constexpr int B = 64;

        beginTest("Identity processing delays by exactly blockSize, any chunking");
        {
            morphir::BlockAdapter adapter(B, 2);

            const int total = 1000;
            std::vector<float> left(total), right(total);
            for (int i = 0; i < total; ++i)
            {
                left[i]  = static_cast<float>(i + 1);
                right[i] = -static_cast<float>(i + 1);
            }

            // Feed in deliberately irregular chunk sizes.
            const int chunks[] = { 1, 7, 13, 64, 3, 100, 31, 64, 64, 5 };
            int pos = 0, chunkIdx = 0;
            while (pos < total)
            {
                const int n = std::min(chunks[chunkIdx % 10], total - pos);
                float* chans[2] = { left.data() + pos, right.data() + pos };
                adapter.process(chans, n, [](float* const*, int) {});
                pos += n;
                ++chunkIdx;
            }

            // First B samples are silence (the latency), then the input
            // sequence resumes intact.
            for (int i = 0; i < B; ++i)
                expectWithinAbsoluteError(left[i], 0.0f, 1e-9f);
            for (int i = B; i < total; ++i)
            {
                expectWithinAbsoluteError(left[i],  static_cast<float>(i - B + 1), 1e-9f);
                expectWithinAbsoluteError(right[i], -static_cast<float>(i - B + 1), 1e-9f);
            }
        }

        beginTest("Callback always receives exactly blockSize samples");
        {
            morphir::BlockAdapter adapter(B, 1);

            std::vector<float> data(B * 5, 1.0f);
            int calls = 0;
            bool sizesOk = true;

            // Feed one sample at a time — the worst case.
            for (int i = 0; i < static_cast<int>(data.size()); ++i)
            {
                float* chans[1] = { data.data() + i };
                adapter.process(chans, 1, [&](float* const*, int n)
                {
                    ++calls;
                    sizesOk = sizesOk && (n == B);
                });
            }

            expectEquals(calls, 5);
            expect(sizesOk);
        }

        beginTest("Processing is applied to the delayed signal");
        {
            morphir::BlockAdapter adapter(B, 1);

            std::vector<float> data(B * 3, 1.0f);
            float* chans[1] = { data.data() };
            adapter.process(chans, B * 3, [](float* const* ch, int n)
            {
                for (int i = 0; i < n; ++i)
                    ch[0][i] *= 2.0f;
            });

            for (int i = 0; i < B; ++i)
                expectWithinAbsoluteError(data[i], 0.0f, 1e-9f);
            for (int i = B; i < B * 3; ++i)
                expectWithinAbsoluteError(data[i], 2.0f, 1e-9f);
        }

        beginTest("reset clears buffered state");
        {
            morphir::BlockAdapter adapter(B, 1);

            std::vector<float> data(B, 1.0f);
            float* chans[1] = { data.data() };
            adapter.process(chans, B, [](float* const*, int) {});

            adapter.reset();

            // After reset, the next block must come out silent — no leftovers
            // from the pre-reset input.
            std::vector<float> next(B, 0.5f);
            float* chans2[1] = { next.data() };
            adapter.process(chans2, B, [](float* const*, int) {});
            for (int i = 0; i < B; ++i)
                expectWithinAbsoluteError(next[i], 0.0f, 1e-9f);
        }
    }
};

static BlockAdapterTests blockAdapterTests;
