#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ─── Gain Reduction Meter ───────────────────────────────────────────────────
class GRMeter : public juce::Component {
public:
    void setGainReduction(float grDb) // grDb ≤ 0
    {
        currentGr = grDb;
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(b, 3.0f);

        constexpr float kRange = 30.0f;
        const float frac = std::clamp(-currentGr / kRange, 0.0f, 1.0f);
        const float fillH = b.getHeight() * frac;

        if (fillH > 1.0f) {
            auto fill = b.removeFromBottom(fillH);
            juce::ColourGradient grad(
                juce::Colour(0xff33dd33), fill.getX(), fill.getBottom(),  // green at bottom
                juce::Colour(0xffdd3333), fill.getX(), b.getY(),          // red at top
                false);
            grad.addColour(0.4, juce::Colour(0xffddaa00)); // yellow mid
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fill, 2.0f);
        }

        // Tick marks at -6, -12, -18, -24 dB
        g.setColour(juce::Colour(0xff444444));
        auto bounds = getLocalBounds();
        for (float db : {-6.0f, -12.0f, -18.0f, -24.0f}) {
            int y = bounds.getBottom() - static_cast<int>(bounds.getHeight() * (-db / kRange));
            g.drawHorizontalLine(y, (float)bounds.getX() + 2, (float)bounds.getRight() - 2);
        }
    }

private:
    float currentGr = 0.0f;
};

// ─── Custom LookAndFeel ──────────────────────────────────────────────────────
class MambaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MambaLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId,     juce::Colour(0xffaaaaaa));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
        setColour(juce::Slider::textBoxOutlineColourId,  juce::Colour(0x00000000));
        setColour(juce::Label::textColourId,             juce::Colour(0xffaaaaaa));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override
    {
        const float r  = std::min(width, height) * 0.5f - 6.0f;
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // Track
        {
            juce::Path track;
            track.addCentredArc(cx, cy, r, r, 0.0f, startAngle, endAngle, true);
            g.setColour(juce::Colour(0xff333333));
            g.strokePath(track, juce::PathStrokeType(3.0f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Value arc
        {
            juce::Path arc;
            arc.addCentredArc(cx, cy, r, r, 0.0f, startAngle, angle, true);
            g.setColour(juce::Colour(0xff7a44ff));
            g.strokePath(arc, juce::PathStrokeType(3.0f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Body
        g.setColour(juce::Colour(0xff262626));
        g.fillEllipse(cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f);

        // Pointer line
        const float px = cx + (r * 0.55f) * std::sin(angle);
        const float py = cy - (r * 0.55f) * std::cos(angle);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawLine(cx, cy, px, py, 2.0f);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        g.drawText(label.getText(), label.getLocalBounds(),
                   juce::Justification::centred, false);
    }
};

// ─── Editor ─────────────────────────────────────────────────────────────────
class MambaCompressorEditor : public juce::AudioProcessorEditor,
                              public juce::AudioProcessorEditorARAExtension,
                              private juce::Timer {
public:
      explicit MambaCompressorEditor(MambaCompressorProcessor&);
    ~MambaCompressorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MambaCompressorProcessor& processor;
    MambaLookAndFeel laf;

    // 8 knobs + attachments
    juce::Slider knobs[8];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachments[8];
    juce::Label  knobLabels[8];

    // GR meter
    GRMeter  grMeter;
    juce::Label grLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MambaCompressorEditor)
};
