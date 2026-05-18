#include "IRSlotPanel.h"

IRSlotPanel::IRSlotPanel(MorphIRProcessor& proc,
                         MorphIRProcessor::SlotId slotId,
                         const juce::String& label)
    : processor(proc), slot(slotId)
{
    titleLabel.setText(label, juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    fileLabel.setText("(drop WAV or AIFF, or click Load)", juce::dontSendNotification);
    fileLabel.setJustificationType(juce::Justification::centred);
    fileLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(fileLabel);

    loadButton.setButtonText("Load...");
    loadButton.onClick = [this] { openFileChooser(); };
    addAndMakeVisible(loadButton);

    updateFromProcessor();
}

void IRSlotPanel::paint(juce::Graphics& g)
{
    g.setColour(highlightDrop ? juce::Colour(0xff4060a0) : juce::Colour(0xff2a2a3e));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 6.0f);
    g.setColour(juce::Colour(0xff4a4a5e));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 6.0f, 1.0f);
}

void IRSlotPanel::resized()
{
    auto r = getLocalBounds().reduced(10);
    titleLabel.setBounds(r.removeFromTop(28));
    r.removeFromTop(4);
    loadButton.setBounds(r.removeFromBottom(32).reduced(20, 0));
    r.removeFromBottom(6);
    fileLabel.setBounds(r);
}

bool IRSlotPanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (files.isEmpty()) return false;
    const auto ext = juce::File(files[0]).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff";
}

void IRSlotPanel::fileDragEnter(const juce::StringArray&, int, int) { highlightDrop = true; repaint(); }
void IRSlotPanel::fileDragExit(const juce::StringArray&)            { highlightDrop = false; repaint(); }

void IRSlotPanel::filesDropped(const juce::StringArray& files, int, int)
{
    highlightDrop = false;
    repaint();
    if (files.isEmpty()) return;
    attemptLoad(juce::File(files[0]));
}

void IRSlotPanel::updateFromProcessor()
{
    const auto path = processor.getSlotFilePath(slot);
    if (path.isEmpty())
        fileLabel.setText("(drop WAV or AIFF, or click Load)", juce::dontSendNotification);
    else
        fileLabel.setText(juce::File(path).getFileName(), juce::dontSendNotification);
}

void IRSlotPanel::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Select an IR file...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.aif;*.aiff");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file.existsAsFile())
            attemptLoad(file);
    });
}

void IRSlotPanel::attemptLoad(const juce::File& file)
{
    const auto err = processor.loadSlot(slot, file);
    if (err.isNotEmpty())
        fileLabel.setText("Error: " + err, juce::dontSendNotification);
    else
        updateFromProcessor();
}
