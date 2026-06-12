#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MorphIRProcessor.h"

class IRSlotPanel : public juce::Component,
                    public juce::FileDragAndDropTarget,
                    private juce::ChangeListener
{
public:
    IRSlotPanel(MorphIRProcessor& proc, MorphIRProcessor::SlotId slotId, const juce::String& label);
    ~IRSlotPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

    void updateFromProcessor();

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void openFileChooser();
    void attemptLoad(const juce::File& file);
    void paintWaveform(juce::Graphics& g);
    void paintEmptyState(juce::Graphics& g);

    MorphIRProcessor&        processor;
    MorphIRProcessor::SlotId slot;
    juce::String             title;
    juce::Colour             accent;

    juce::Label      fileLabel;
    juce::TextButton loadButton;

    juce::AudioFormatManager  formatManager;
    juce::AudioThumbnailCache thumbnailCache { 2 };
    juce::AudioThumbnail      thumbnail { 256, formatManager, thumbnailCache };

    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> waveArea;

    bool highlightDrop = false;
    bool hovered       = false;
    bool hasIR         = false;

    std::unique_ptr<juce::FileChooser> chooser;
};
