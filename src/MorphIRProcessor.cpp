#include "MorphIRProcessor.h"
#include "MorphIREditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MorphIRProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "morphPosition", 1 },
        "Morph Position",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "dryWet", 1 },
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputGain", 1 },
        "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "preDelay", 1 },
        "Pre-delay",
        juce::NormalisableRange<float>(0.0f, kMaxPreDelayMs, 1.0f),
        0.0f));

    return { params.begin(), params.end() };
}

MorphIRProcessor::MorphIRProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",   juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "Parameters", createParameterLayout())
    , fftProvider(morphir::FFTProvider::create())
{
    morphPositionParam = apvts.getRawParameterValue("morphPosition");
    dryWetParam        = apvts.getRawParameterValue("dryWet");
    outputGainParam    = apvts.getRawParameterValue("outputGain");
    preDelayParam      = apvts.getRawParameterValue("preDelay");
}

MorphIRProcessor::~MorphIRProcessor()
{
    if (convolver != nullptr)
        convolver->stopMorphing();
}

void MorphIRProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Round block size up to a power of two for NUPC.
    int blockSizePow2 = 1;
    while (blockSizePow2 < samplesPerBlock)
        blockSizePow2 <<= 1;

    rebuildEngine(sampleRate, blockSizePow2);

    preDelaySize = static_cast<int>((kMaxPreDelayMs / 1000.0) * sampleRate) + 1;
    preDelayBuffer.setSize(2, preDelaySize, false, true, true);
    preDelayBuffer.clear();
    preDelayWritePos = 0;

    dryScratch.setSize(2, samplesPerBlock, false, true, true);
}

void MorphIRProcessor::releaseResources()
{
    if (convolver != nullptr)
        convolver->stopMorphing();
}

void MorphIRProcessor::rebuildEngine(double sampleRate, int blockSize)
{
    const juce::ScopedLock sl(slotLock);

    if (convolver != nullptr)
        convolver->stopMorphing();

    currentSampleRate    = sampleRate;
    currentBlockSize     = blockSize;
    currentMaxIRSamples  = static_cast<std::size_t>(sampleRate * 30.0); // 30 second cap

    irLoader  = std::make_unique<morphir::IRLoader>(*fftProvider, currentMaxIRSamples, sampleRate);
    convolver = std::make_unique<morphir::DualMonoConvolver>(*fftProvider, currentMaxIRSamples, blockSize);

    if (slotsReady())
    {
        rebindConvolverSlots();
        convolver->startMorphing();
    }
}

bool MorphIRProcessor::slotsReady() const
{
    return slotA.left.loaded && slotA.right.loaded
        && slotB.left.loaded && slotB.right.loaded;
}

void MorphIRProcessor::rebindConvolverSlots()
{
    if (convolver == nullptr) return;
    convolver->setSlots(slotA.left, slotB.left, slotA.right, slotB.right);
}

juce::String MorphIRProcessor::loadSlot(SlotId slot, const juce::File& file)
{
    const juce::ScopedLock sl(slotLock);

    if (irLoader == nullptr)
        return "Engine not initialised — call prepareToPlay first";

    auto result = irLoader->load(file);
    if (result.error.isNotEmpty())
        return result.error;

    LoadedBundle bundle;
    bundle.left     = std::move(result.left);
    bundle.right    = std::move(result.right);
    bundle.filePath = file.getFullPathName();

    if (convolver != nullptr)
        convolver->stopMorphing();

    if (slot == SlotId::A)
        slotA = std::move(bundle);
    else
        slotB = std::move(bundle);

    if (slotsReady())
    {
        rebindConvolverSlots();
        convolver->startMorphing();
    }

    return {};
}

juce::String MorphIRProcessor::getSlotFilePath(SlotId slot) const
{
    return (slot == SlotId::A) ? slotA.filePath : slotB.filePath;
}

void MorphIRProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    if (numChannels < 2 || numSamples == 0)
        return;

    // Snapshot dry signal for later mix.
    for (int ch = 0; ch < 2; ++ch)
        std::copy(buffer.getReadPointer(ch),
                  buffer.getReadPointer(ch) + numSamples,
                  dryScratch.getWritePointer(ch));

    // Pre-delay: simple per-channel ring buffer.
    const float preDelayMs = preDelayParam->load();
    const int   delaySamples = juce::jlimit(0, preDelaySize - 1,
        static_cast<int>((preDelayMs / 1000.0f) * static_cast<float>(currentSampleRate)));

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
            preDelayBuffer.setSample(ch, preDelayWritePos, buffer.getSample(ch, i));

        const int readPos = (preDelayWritePos - delaySamples + preDelaySize * 4) % preDelaySize;
        for (int ch = 0; ch < 2; ++ch)
            buffer.setSample(ch, i, preDelayBuffer.getSample(ch, readPos));

        preDelayWritePos = (preDelayWritePos + 1) % preDelaySize;
    }

    // Convolve (in place). If slots aren't fully loaded, silence the wet path.
    if (convolver != nullptr && slotsReady() && numSamples == currentBlockSize)
    {
        convolver->setMorphPosition(morphPositionParam->load());
        convolver->process(buffer.getWritePointer(0),
                           buffer.getWritePointer(1),
                           numSamples);
    }
    else
    {
        buffer.clear();
    }

    // Dry/wet mix.
    const float wet = dryWetParam->load();
    const float dry = 1.0f - wet;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* w = buffer.getWritePointer(ch);
        const auto* d = dryScratch.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            w[i] = d[i] * dry + w[i] * wet;
    }

    // Output gain.
    const float gainLin = juce::Decibels::decibelsToGain(outputGainParam->load());
    buffer.applyGain(gainLin);
}

juce::AudioProcessorEditor* MorphIRProcessor::createEditor()
{
    return new MorphIREditor(*this);
}

void MorphIRProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Attach IR file paths as child elements.
    state.setProperty("slotA_path", slotA.filePath, nullptr);
    state.setProperty("slotB_path", slotB.filePath, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void MorphIRProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) return;
    if (! xml->hasTagName(apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);

    const juce::String pathA = tree.getProperty("slotA_path", juce::String{}).toString();
    const juce::String pathB = tree.getProperty("slotB_path", juce::String{}).toString();

    if (pathA.isNotEmpty())
        loadSlot(SlotId::A, juce::File(pathA));
    if (pathB.isNotEmpty())
        loadSlot(SlotId::B, juce::File(pathB));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MorphIRProcessor();
}
