#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace morphir::ui
{

// Shared colour palette for the plugin UI.
namespace palette
{
    inline constexpr juce::uint32 backgroundTop    = 0xff1c1c28;
    inline constexpr juce::uint32 backgroundBottom = 0xff121219;
    inline constexpr juce::uint32 panel            = 0xff20202d;
    inline constexpr juce::uint32 panelBorder      = 0xff33333f;
    inline constexpr juce::uint32 textPrimary      = 0xffe8e8f0;
    inline constexpr juce::uint32 textDim          = 0xff8d8d9e;
    inline constexpr juce::uint32 slotA            = 0xffe3a857; // warm amber
    inline constexpr juce::uint32 slotB            = 0xff57c7c2; // cool teal
    inline constexpr juce::uint32 accent           = 0xffd9a05b;
    inline constexpr juce::uint32 utilityKnob      = 0xff8d96ab; // desaturated steel
    inline constexpr juce::uint32 knobTrack        = 0xff2d2d3a;
    inline constexpr juce::uint32 error            = 0xffe07a7a;
}

// Custom look-and-feel: flat-dark theme, arc-style rotary knobs, soft
// rounded buttons. The morph knob (component property "isMorphKnob")
// blends its accent colour from slot A amber to slot B teal as it turns.
class MorphLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MorphLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    juce::Label* createSliderTextBox(juce::Slider&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphLookAndFeel)
};

} // namespace morphir::ui
