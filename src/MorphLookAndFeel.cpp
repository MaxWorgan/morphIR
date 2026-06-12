#include "MorphLookAndFeel.h"

namespace morphir::ui
{

MorphLookAndFeel::MorphLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId,   juce::Colour(palette::utilityKnob));
    setColour(juce::Slider::textBoxTextColourId,        juce::Colour(palette::textDim));
    setColour(juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId,   juce::Colour(palette::accent).withAlpha(0.35f));

    setColour(juce::Label::textColourId,                juce::Colour(palette::textPrimary));

    setColour(juce::TextButton::buttonColourId,         juce::Colour(0xff2a2a38));
    setColour(juce::TextButton::textColourOffId,        juce::Colour(palette::textPrimary));
    setColour(juce::TextButton::textColourOnId,         juce::Colour(palette::textPrimary));

    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(palette::accent));
    setColour(juce::CaretComponent::caretColourId,      juce::Colour(palette::accent));
}

void MorphLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y,
                                               (float) width, (float) height).reduced(6.0f);
    const auto centre = bounds.getCentre();
    const float radius       = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float arcThickness = juce::jmax(2.5f, radius * 0.085f);
    const float arcRadius    = radius - arcThickness * 0.5f;
    const float angle        = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const bool isMorph = static_cast<bool>(slider.getProperties()["isMorphKnob"]);
    const auto colourA = juce::Colour(palette::slotA);
    const auto colourB = juce::Colour(palette::slotB);
    const auto fill    = isMorph ? colourA.interpolatedWith(colourB, sliderPos)
                                 : slider.findColour(juce::Slider::rotarySliderFillColourId);

    // Background track arc.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(palette::knobTrack));
    g.strokePath(track, juce::PathStrokeType(arcThickness,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Value arc.
    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, angle, true);
        g.setColour(fill);
        g.strokePath(value, juce::PathStrokeType(arcThickness,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Knob body with a subtle top-light gradient.
    const float bodyRadius = arcRadius - arcThickness * 1.8f;
    const auto bodyBounds  = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f)
                                 .withCentre(centre);
    juce::ColourGradient bodyGradient(juce::Colour(0xff34343f), bodyBounds.getTopLeft(),
                                      juce::Colour(0xff1d1d26), bodyBounds.getBottomLeft(), false);
    g.setGradientFill(bodyGradient);
    g.fillEllipse(bodyBounds);
    g.setColour(juce::Colour(palette::panelBorder));
    g.drawEllipse(bodyBounds, 1.0f);

    // Pointer.
    const float pointerThickness = juce::jmax(2.0f, bodyRadius * 0.1f);
    const float pointerLength    = bodyRadius * 0.42f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f,
                                -bodyRadius + pointerThickness,
                                pointerThickness, pointerLength,
                                pointerThickness * 0.5f);
    g.setColour(fill);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));

    // The morph knob shows its endpoints: A (amber) lower-left, B (teal) lower-right.
    if (isMorph)
    {
        const float labelSize = juce::jmax(11.0f, radius * 0.2f);
        g.setFont(juce::Font(juce::FontOptions(labelSize).withStyle("Bold")));

        const auto labelOffsetX = radius * 0.56f;
        const auto labelOffsetY = radius * 0.82f;
        const juce::Rectangle<float> aBox(centre.x - labelOffsetX - labelSize,
                                          centre.y + labelOffsetY - labelSize * 0.5f,
                                          labelSize * 2.0f, labelSize);
        const juce::Rectangle<float> bBox(centre.x + labelOffsetX - labelSize,
                                          centre.y + labelOffsetY - labelSize * 0.5f,
                                          labelSize * 2.0f, labelSize);

        g.setColour(colourA.withAlpha(0.45f + 0.55f * (1.0f - sliderPos)));
        g.drawText("A", aBox, juce::Justification::centred);
        g.setColour(colourB.withAlpha(0.45f + 0.55f * sliderPos));
        g.drawText("B", bBox, juce::Justification::centred);
    }
}

void MorphLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    constexpr float cornerRadius = 5.0f;

    auto base = juce::Colour(0xff2a2a38);
    if (shouldDrawButtonAsDown)
        base = juce::Colour(0xff3a3a4a);
    else if (shouldDrawButtonAsHighlighted)
        base = juce::Colour(0xff32323f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, cornerRadius);

    const auto outline = (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
                             ? juce::Colour(palette::accent).withAlpha(0.7f)
                             : juce::Colour(palette::panelBorder);
    g.setColour(outline);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
}

juce::Font MorphLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jmin(13.0f, (float) buttonHeight * 0.6f)));
}

juce::Label* MorphLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::Font(juce::FontOptions(12.0f)));
    label->setColour(juce::Label::textColourId, juce::Colour(palette::textDim));
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineWhenEditingColourId,
                     juce::Colour(palette::accent).withAlpha(0.6f));
    return label;
}

} // namespace morphir::ui
