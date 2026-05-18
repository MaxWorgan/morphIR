#include "MorphIREditor.h"

MorphIREditor::MorphIREditor(MorphIRProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    placeholderLabel.setText("MorphIR", juce::dontSendNotification);
    placeholderLabel.setJustificationType(juce::Justification::centred);
    placeholderLabel.setFont(juce::FontOptions(24.0f));
    addAndMakeVisible(placeholderLabel);

    setSize(600, 300);
    setResizable(true, true);
}

void MorphIREditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
}

void MorphIREditor::resized()
{
    placeholderLabel.setBounds(getLocalBounds());
}
