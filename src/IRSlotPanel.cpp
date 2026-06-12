#include "IRSlotPanel.h"
#include "MorphLookAndFeel.h"

namespace pal = morphir::ui::palette;

IRSlotPanel::IRSlotPanel(MorphIRProcessor& proc,
                         MorphIRProcessor::SlotId slotId,
                         const juce::String& label)
    : processor(proc), slot(slotId), title(label.toUpperCase())
{
    accent = juce::Colour(slot == MorphIRProcessor::SlotId::A ? pal::slotA : pal::slotB);

    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    fileLabel.setJustificationType(juce::Justification::centred);
    fileLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    fileLabel.setColour(juce::Label::textColourId, juce::Colour(pal::textDim));
    fileLabel.setInterceptsMouseClicks(false, false);
    fileLabel.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(fileLabel);

    loadButton.setButtonText("Load");
    loadButton.onClick = [this] { openFileChooser(); };
    addAndMakeVisible(loadButton);

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    updateFromProcessor();
}

IRSlotPanel::~IRSlotPanel()
{
    thumbnail.removeChangeListener(this);
}

void IRSlotPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.5f);
    constexpr float cornerRadius = 8.0f;

    g.setColour(juce::Colour(pal::panel));
    g.fillRoundedRectangle(bounds, cornerRadius);

    if (highlightDrop)
    {
        g.setColour(accent.withAlpha(0.12f));
        g.fillRoundedRectangle(bounds, cornerRadius);
    }

    const auto border = highlightDrop ? accent
                      : hovered       ? juce::Colour(pal::panelBorder).brighter(0.4f)
                                      : juce::Colour(pal::panelBorder);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, cornerRadius, highlightDrop ? 1.5f : 1.0f);

    // Header: accent dot + letter-spaced slot title.
    g.setColour(accent);
    g.fillEllipse((float) headerArea.getX(),
                  (float) headerArea.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour(juce::Colour(pal::textPrimary));
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold"))
                  .withExtraKerningFactor(0.15f));
    g.drawText(title, headerArea.withTrimmedLeft(14),
               juce::Justification::centredLeft);

    if (hasIR && thumbnail.getTotalLength() > 0.0)
        paintWaveform(g);
    else
        paintEmptyState(g);
}

void IRSlotPanel::paintWaveform(juce::Graphics& g)
{
    g.setColour(accent.withAlpha(0.55f));
    thumbnail.drawChannels(g, waveArea, 0.0, thumbnail.getTotalLength(), 0.9f);
}

void IRSlotPanel::paintEmptyState(juce::Graphics& g)
{
    juce::Path outline;
    outline.addRoundedRectangle(waveArea.toFloat().reduced(1.0f), 6.0f);

    juce::Path dashed;
    const float dashes[] = { 4.0f, 4.0f };
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
    g.setColour(juce::Colour(pal::textDim).withAlpha(0.45f));
    g.fillPath(dashed);

    g.setColour(juce::Colour(pal::textDim));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawFittedText("Drop WAV / AIFF here\nor click to browse",
                     waveArea.reduced(8), juce::Justification::centred, 2);
}

void IRSlotPanel::resized()
{
    auto r = getLocalBounds().reduced(12, 10);

    auto header = r.removeFromTop(20);
    loadButton.setBounds(header.removeFromRight(56));
    headerArea = header;

    fileLabel.setBounds(r.removeFromBottom(18));
    r.removeFromTop(8);
    r.removeFromBottom(4);
    waveArea = r;
}

void IRSlotPanel::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mouseWasDraggedSinceMouseDown())
        openFileChooser();
}

void IRSlotPanel::mouseEnter(const juce::MouseEvent&) { hovered = true;  repaint(); }
void IRSlotPanel::mouseExit(const juce::MouseEvent&)  { hovered = false; repaint(); }

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

void IRSlotPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}

void IRSlotPanel::updateFromProcessor()
{
    const auto path = processor.getSlotFilePath(slot);

    if (path.isEmpty())
    {
        hasIR = false;
        thumbnail.setSource(nullptr);
        fileLabel.setText("No IR loaded", juce::dontSendNotification);
        fileLabel.setColour(juce::Label::textColourId, juce::Colour(pal::textDim));
    }
    else
    {
        hasIR = true;
        const juce::File file(path);
        thumbnail.setSource(new juce::FileInputSource(file));
        fileLabel.setText(file.getFileName(), juce::dontSendNotification);
        fileLabel.setColour(juce::Label::textColourId, juce::Colour(pal::textPrimary));
    }

    repaint();
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
    {
        fileLabel.setText("Error: " + err, juce::dontSendNotification);
        fileLabel.setColour(juce::Label::textColourId, juce::Colour(pal::error));
    }
    else
    {
        updateFromProcessor();
    }
}
