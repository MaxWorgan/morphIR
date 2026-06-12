#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace morphir {

// Adapts arbitrary-sized audio callbacks to a processor that requires
// fixed-size blocks (e.g. NUPCEngine). Input is accumulated until a full
// block is available, which is then run through the supplied callback;
// output is dispensed from the previously processed block. This adds
// exactly blockSize samples of latency, independent of how the host
// chops up its callbacks. All memory is allocated up front; process()
// is real-time safe.
class BlockAdapter
{
public:
    BlockAdapter(int blockSizeIn, int numChannelsIn)
        : blockSize(blockSizeIn)
        , numChannels(numChannelsIn)
        , inBuf(static_cast<std::size_t>(numChannelsIn),
                std::vector<float>(static_cast<std::size_t>(blockSizeIn), 0.0f))
        , outBuf(static_cast<std::size_t>(numChannelsIn),
                 std::vector<float>(static_cast<std::size_t>(blockSizeIn), 0.0f))
        , channelPtrs(static_cast<std::size_t>(numChannelsIn), nullptr)
    {
    }

    // Processes numSamples in-place across `channels`. processBlock is
    // invoked with exactly blockSize samples per channel, zero or more
    // times, and must process them in-place.
    template <typename Fn>
    void process(float* const* channels, int numSamples, Fn&& processBlock)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto c = static_cast<std::size_t>(ch);
                const auto f = static_cast<std::size_t>(fill);
                inBuf[c][f] = channels[ch][i];
                channels[ch][i] = outBuf[c][f];
            }

            if (++fill == blockSize)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const auto c = static_cast<std::size_t>(ch);
                    std::copy(inBuf[c].begin(), inBuf[c].end(), outBuf[c].begin());
                    channelPtrs[c] = outBuf[c].data();
                }
                processBlock(const_cast<float* const*>(channelPtrs.data()), blockSize);
                fill = 0;
            }
        }
    }

    void reset()
    {
        fill = 0;
        for (auto& ch : inBuf)  std::fill(ch.begin(), ch.end(), 0.0f);
        for (auto& ch : outBuf) std::fill(ch.begin(), ch.end(), 0.0f);
    }

    int latencySamples() const { return blockSize; }

private:
    int blockSize;
    int numChannels;
    int fill = 0;
    std::vector<std::vector<float>> inBuf;
    std::vector<std::vector<float>> outBuf;
    std::vector<float*> channelPtrs;
};

} // namespace morphir
