#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../engine/fft/FFTProvider.h"
#include "../engine/ir/IRSlot.h"
#include "../engine/ir/IRLoader.h"
#include "../engine/nupc/DualMonoConvolver.h"
#include "../engine/util/BlockAdapter.h"

class MorphIRProcessor : public juce::AudioProcessor
{
public:
    enum class SlotId { A = 0, B = 1 };

    struct LoadedBundle
    {
        morphir::IRSlot left;
        morphir::IRSlot right;
        juce::String filePath;
    };

    MorphIRProcessor();
    ~MorphIRProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Synchronous IR load. Safe to call from the message thread; blocks while
    // the file is decoded and FFT'd (typically tens of ms for short IRs, up to
    // hundreds for a 30s IR). Returns the error string from IRLoader (empty
    // on success). Stops and restarts the morph thread around the swap.
    juce::String loadSlot(SlotId slot, const juce::File& file);

    // Path of the file currently loaded in the slot, or empty if none.
    juce::String getSlotFilePath(SlotId slot) const;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void rebuildEngine(double sampleRate, int blockSize);
    bool slotsReady() const;    // both slots loaded — morphing available
    bool anySlotReady() const;  // at least one slot loaded — convolution available
    void rebindConvolverSlots();

    juce::AudioProcessorValueTreeState apvts;

    std::unique_ptr<morphir::FFTProvider>       fftProvider;
    std::unique_ptr<morphir::IRLoader>          irLoader;
    std::unique_ptr<morphir::DualMonoConvolver> convolver;

    LoadedBundle slotA;
    LoadedBundle slotB;
    juce::CriticalSection slotLock; // guards loadSlot / rebuild

    double currentSampleRate = 0.0;
    int    currentBlockSize  = 0;
    std::size_t currentMaxIRSamples = 0;

    static constexpr float kMaxPreDelayMs = 500.0f;

    // Pre-delay ring buffers, one per channel.
    juce::AudioBuffer<float> preDelayBuffer;
    int preDelaySize = 0;
    int preDelayWritePos = 0;

    // Pre-allocated copy buffer for the dry signal.
    juce::AudioBuffer<float> dryScratch;

    // Buffers host callbacks of any size into fixed engine-sized blocks for
    // the convolver (adds one engine block of latency, reported to the host).
    std::unique_ptr<morphir::BlockAdapter> blockAdapter;

    // Delays the dry signal by the same latency so dry and wet stay aligned.
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayPos = 0;

    std::atomic<float>* morphPositionParam = nullptr;
    std::atomic<float>* dryWetParam        = nullptr;
    std::atomic<float>* outputGainParam    = nullptr;
    std::atomic<float>* preDelayParam      = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphIRProcessor)
};
