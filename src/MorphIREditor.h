#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphIRProcessor.h"
#include "IRSlotPanel.h"

class MorphIREditor : public juce::AudioProcessorEditor
{
public:
    explicit MorphIREditor(MorphIRProcessor&);
    ~MorphIREditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MorphIRProcessor& processor;

    std::unique_ptr<IRSlotPanel> slotPanelA;
    std::unique_ptr<IRSlotPanel> slotPanelB;

    juce::Slider morphKnob;
    juce::Slider dryWetKnob;
    juce::Slider outputGainKnob;
    juce::Slider preDelayKnob;

    juce::Label morphLabel;
    juce::Label dryWetLabel;
    juce::Label outputGainLabel;
    juce::Label preDelayLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> morphAttach;
    std::unique_ptr<SliderAttachment> dryWetAttach;
    std::unique_ptr<SliderAttachment> outputGainAttach;
    std::unique_ptr<SliderAttachment> preDelayAttach;

    void styleKnob(juce::Slider& s, juce::Label& l, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphIREditor)
};
