#include "MorphIREditor.h"

MorphIREditor::MorphIREditor(MorphIRProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    slotPanelA = std::make_unique<IRSlotPanel>(processor, MorphIRProcessor::SlotId::A, "Slot A");
    slotPanelB = std::make_unique<IRSlotPanel>(processor, MorphIRProcessor::SlotId::B, "Slot B");
    addAndMakeVisible(slotPanelA.get());
    addAndMakeVisible(slotPanelB.get());

    styleKnob(morphKnob,      morphLabel,      "Morph");
    styleKnob(dryWetKnob,     dryWetLabel,     "Dry/Wet");
    styleKnob(outputGainKnob, outputGainLabel, "Gain (dB)");
    styleKnob(preDelayKnob,   preDelayLabel,   "Pre-delay (ms)");

    morphAttach      = std::make_unique<SliderAttachment>(processor.getAPVTS(), "morphPosition", morphKnob);
    dryWetAttach     = std::make_unique<SliderAttachment>(processor.getAPVTS(), "dryWet",        dryWetKnob);
    outputGainAttach = std::make_unique<SliderAttachment>(processor.getAPVTS(), "outputGain",    outputGainKnob);
    preDelayAttach   = std::make_unique<SliderAttachment>(processor.getAPVTS(), "preDelay",      preDelayKnob);

    setSize(640, 360);
    setResizable(true, true);
    setResizeLimits(560, 320, 1600, 1000);
}

MorphIREditor::~MorphIREditor() = default;

void MorphIREditor::styleKnob(juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff6090c0));
    addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colours::white);
    l.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(l);
}

void MorphIREditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("MorphIR", getLocalBounds().removeFromTop(36), juce::Justification::centred);
}

void MorphIREditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(36);
    auto bottom = r.removeFromBottom(110);

    auto slotsArea = r.removeFromTop(140);
    auto leftSlot  = slotsArea.removeFromLeft(slotsArea.getWidth() / 2);
    slotPanelA->setBounds(leftSlot.reduced(8));
    slotPanelB->setBounds(slotsArea.reduced(8));

    auto midRow = r.reduced(8, 4);
    const int knobSize = juce::jmin(midRow.getHeight(), 100);
    juce::Rectangle<int> morphArea(midRow.getCentreX() - knobSize / 2,
                                    midRow.getY(),
                                    knobSize, knobSize);
    morphKnob.setBounds(morphArea);
    morphLabel.setBounds(morphArea.getX(), morphArea.getBottom() - 4, knobSize, 16);

    const int n = 3;
    const int kw = bottom.getWidth() / n;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int idx)
    {
        juce::Rectangle<int> cell(bottom.getX() + idx * kw, bottom.getY(), kw, bottom.getHeight());
        cell.reduce(8, 4);
        const int kh = cell.getHeight() - 22;
        s.setBounds(cell.getX(), cell.getY(), cell.getWidth(), kh);
        l.setBounds(cell.getX(), cell.getBottom() - 18, cell.getWidth(), 16);
    };
    placeKnob(dryWetKnob,     dryWetLabel,     0);
    placeKnob(outputGainKnob, outputGainLabel, 1);
    placeKnob(preDelayKnob,   preDelayLabel,   2);
}
