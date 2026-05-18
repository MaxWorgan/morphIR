#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphIRProcessor.h"

class MorphIREditor : public juce::AudioProcessorEditor
{
public:
    explicit MorphIREditor(MorphIRProcessor&);
    ~MorphIREditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MorphIRProcessor& processor;

    juce::Label placeholderLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphIREditor)
};
