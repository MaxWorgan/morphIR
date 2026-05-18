#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphIRProcessor.h"

class IRSlotPanel : public juce::Component,
                    public juce::FileDragAndDropTarget
{
public:
    IRSlotPanel(MorphIRProcessor& proc, MorphIRProcessor::SlotId slotId, const juce::String& label);

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

    void updateFromProcessor();

private:
    void openFileChooser();
    void attemptLoad(const juce::File& file);

    MorphIRProcessor&                  processor;
    MorphIRProcessor::SlotId           slot;
    juce::Label                        titleLabel;
    juce::Label                        fileLabel;
    juce::TextButton                   loadButton;
    bool                               highlightDrop = false;
    std::unique_ptr<juce::FileChooser> chooser;
};
