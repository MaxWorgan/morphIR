#include "MorphIRProcessor.h"
#include "MorphIREditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MorphIRProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "morphPosition", 1 },
        "Morph Position",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
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
        juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f),
        0.0f));

    return { params.begin(), params.end() };
}

MorphIRProcessor::MorphIRProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",   juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void MorphIRProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void MorphIRProcessor::releaseResources()
{
}

void MorphIRProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    // Passthrough until DSP engine is wired in
}

juce::AudioProcessorEditor* MorphIRProcessor::createEditor()
{
    return new MorphIREditor(*this);
}

void MorphIRProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MorphIRProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MorphIRProcessor();
}
