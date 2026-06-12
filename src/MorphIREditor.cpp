#include "MorphIREditor.h"

namespace pal = morphir::ui::palette;

MorphIREditor::MorphIREditor(MorphIRProcessor& p)
    : AudioProcessorEditor(&p), morphProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    slotPanelA = std::make_unique<IRSlotPanel>(morphProcessor, MorphIRProcessor::SlotId::A, "IR Slot A");
    slotPanelB = std::make_unique<IRSlotPanel>(morphProcessor, MorphIRProcessor::SlotId::B, "IR Slot B");
    addAndMakeVisible(slotPanelA.get());
    addAndMakeVisible(slotPanelB.get());

    styleKnob(morphKnob,      morphLabel,      "MORPH");
    styleKnob(dryWetKnob,     dryWetLabel,     "DRY / WET");
    styleKnob(outputGainKnob, outputGainLabel, "GAIN (dB)");
    styleKnob(preDelayKnob,   preDelayLabel,   "PRE-DELAY (ms)");

    morphKnob.getProperties().set("isMorphKnob", true);

    morphAttach      = std::make_unique<SliderAttachment>(morphProcessor.getAPVTS(), "morphPosition", morphKnob);
    dryWetAttach     = std::make_unique<SliderAttachment>(morphProcessor.getAPVTS(), "dryWet",        dryWetKnob);
    outputGainAttach = std::make_unique<SliderAttachment>(morphProcessor.getAPVTS(), "outputGain",    outputGainKnob);
    preDelayAttach   = std::make_unique<SliderAttachment>(morphProcessor.getAPVTS(), "preDelay",      preDelayKnob);

    setSize(680, 460);
    setResizable(true, true);
    setResizeLimits(600, 420, 1600, 1000);
}

MorphIREditor::~MorphIREditor()
{
    setLookAndFeel(nullptr);
}

void MorphIREditor::styleKnob(juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    addAndMakeVisible(s);
    // Members construct before setLookAndFeel runs, so the text box has
    // already snapshotted the default colours; force it to re-read ours.
    s.sendLookAndFeelChange();

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colour(pal::textDim));
    l.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold"))
                  .withExtraKerningFactor(0.12f));
    addAndMakeVisible(l);
}

void MorphIREditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(pal::backgroundTop), 0.0f, 0.0f,
                                           juce::Colour(pal::backgroundBottom), 0.0f,
                                           bounds.getBottom(), false));
    g.fillRect(bounds);

    // Header: centred wordmark, version tag at right, hairline below.
    const auto header = getLocalBounds().removeFromTop(kHeaderHeight);

    const auto titleFont = juce::Font(juce::FontOptions(20.0f).withStyle("Bold"))
                               .withExtraKerningFactor(0.14f);
    juce::AttributedString wordmark;
    wordmark.append("MORPH", titleFont, juce::Colour(pal::textPrimary));
    wordmark.append("IR", titleFont, juce::Colour(pal::accent));
    wordmark.setJustification(juce::Justification::centred);
    wordmark.draw(g, header.toFloat());

    g.setColour(juce::Colour(pal::textDim).withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("v" JucePlugin_VersionString, header.reduced(16, 0),
               juce::Justification::centredRight);

    g.setColour(juce::Colour(pal::panelBorder));
    g.fillRect(0, header.getBottom() - 1, getWidth(), 1);

    // Hairline separating the morph section from the output controls.
    if (bottomRowSeparatorY > 0)
    {
        g.setColour(juce::Colour(pal::panelBorder).withAlpha(0.7f));
        g.fillRect(16, bottomRowSeparatorY, getWidth() - 32, 1);
    }
}

void MorphIREditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(kHeaderHeight);

    auto bottom = r.removeFromBottom(kBottomRowHeight);
    bottomRowSeparatorY = bottom.getY();

    auto slotsArea = r.removeFromTop(kSlotRowHeight).reduced(8, 0);
    slotsArea.removeFromTop(12);
    auto leftSlot = slotsArea.removeFromLeft(slotsArea.getWidth() / 2);
    slotPanelA->setBounds(leftSlot.withTrimmedRight(5));
    slotPanelB->setBounds(slotsArea.withTrimmedLeft(5));

    // Hero morph knob, centred in the remaining middle area.
    auto morphArea = r.reduced(8, 6);
    const int labelHeight = 16;
    const int knobSize = juce::jmin(morphArea.getHeight() - labelHeight,
                                    juce::jmin(morphArea.getWidth(), 150));
    juce::Rectangle<int> knobBounds(morphArea.getCentreX() - knobSize / 2,
                                    morphArea.getY()
                                        + (morphArea.getHeight() - knobSize - labelHeight) / 2,
                                    knobSize, knobSize);
    morphKnob.setBounds(knobBounds);
    morphLabel.setBounds(knobBounds.getX(), knobBounds.getBottom(), knobSize, labelHeight);

    // Output controls in a three-column row.
    const int columnWidth = bottom.getWidth() / 3;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int column)
    {
        auto cell = juce::Rectangle<int>(bottom.getX() + column * columnWidth,
                                         bottom.getY(), columnWidth, bottom.getHeight())
                        .reduced(8, 6);
        l.setBounds(cell.removeFromBottom(labelHeight));
        s.setBounds(cell);
    };
    placeKnob(dryWetKnob,     dryWetLabel,     0);
    placeKnob(outputGainKnob, outputGainLabel, 1);
    placeKnob(preDelayKnob,   preDelayLabel,   2);
}
