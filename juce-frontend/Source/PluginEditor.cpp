#include "PluginEditor.h"

static constexpr int kWidth  = 620;
static constexpr int kHeight = 268;
static constexpr int kHeaderH = 36;
static constexpr int kMeterW  = 72;

// knobs[0..7] map to these param IDs in order
static const char* kParamIDs[8] = {
    ParamIDs::threshold,
    ParamIDs::ratio,
    ParamIDs::knee,
    ParamIDs::attack,
    ParamIDs::release_,
    ParamIDs::makeup,
    ParamIDs::lookahead,
    ParamIDs::mix,
};

static const char* kKnobNames[8] = {
    "Threshold", "Ratio", "Knee", "Attack",
    "Release", "Makeup", "Lookahead", "Mix",
};

MambaCompressorEditor::MambaCompressorEditor(MambaCompressorProcessor& p)
    : AudioProcessorEditor(&p),
      AudioProcessorEditorARAExtension(&p),
      processor(p)
{
    setLookAndFeel(&laf);
    setResizable(true, false);
    setSize(kWidth, kHeight);

    for (int i = 0; i < 8; ++i) {
        knobs[i].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        addAndMakeVisible(knobs[i]);

        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, kParamIDs[i], knobs[i]);

        knobLabels[i].setText(kKnobNames[i], juce::dontSendNotification);
        knobLabels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobLabels[i]);
    }

    grLabel.setText("GR", juce::dontSendNotification);
    grLabel.setJustificationType(juce::Justification::centred);
    grLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    grLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    addAndMakeVisible(grLabel);
    addAndMakeVisible(grMeter);

    startTimerHz(30);
}

MambaCompressorEditor::~MambaCompressorEditor()
{
    setLookAndFeel(nullptr);
}

void MambaCompressorEditor::timerCallback()
{
    grMeter.setGainReduction(processor.gainReductionDb.load(std::memory_order_relaxed));
    grMeter.repaint();
}

void MambaCompressorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Header bar
    auto header = getLocalBounds().removeFromTop(kHeaderH).toFloat();
    g.setColour(juce::Colour(0xff141414));
    g.fillRect(header);

    // Title
    g.setColour(juce::Colour(0xff7a44ff));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(15.0f).withStyle("Bold")));
    g.drawText("MAMBA COMPRESSOR", header, juce::Justification::centred);

    // Thin purple accent line under header
    g.setColour(juce::Colour(0xff7a44ff).withAlpha(0.5f));
    g.drawHorizontalLine(kHeaderH, 0.0f, (float)getWidth());

    // Separator between knobs and meter
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawVerticalLine(getWidth() - kMeterW - 1, (float)kHeaderH, (float)getHeight());

    // GR label area background
    g.setColour(juce::Colour(0xff191919));
    g.fillRect(getWidth() - kMeterW, kHeaderH, kMeterW, getHeight() - kHeaderH);
}

void MambaCompressorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kHeaderH);

    // Meter column on the right
    auto meterCol = area.removeFromRight(kMeterW).reduced(6, 4);
    grLabel.setBounds(meterCol.removeFromTop(16));
    grMeter.setBounds(meterCol);

    // Knob grid: 4 columns x 2 rows
    const int knobW = area.getWidth() / 4;
    const int knobH = area.getHeight() / 2;

    for (int row = 0; row < 2; ++row) {
        auto rowArea = (row == 0) ? area.removeFromTop(knobH) : area;
        for (int col = 0; col < 4; ++col) {
            int idx = row * 4 + col;
            auto cell = rowArea.removeFromLeft(knobW);
            knobLabels[idx].setBounds(cell.removeFromTop(17).reduced(2, 0));
            knobs[idx].setBounds(cell.reduced(4, 0));
        }
    }
}
