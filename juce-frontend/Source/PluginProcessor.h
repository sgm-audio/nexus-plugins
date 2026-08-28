#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include "MambaCompressorDSP.h"
#include "MambaARADocumentController.h"
#include <atomic>

namespace ParamIDs {
static constexpr const char* threshold = "threshold";
static constexpr const char* ratio = "ratio";
static constexpr const char* knee = "knee";
static constexpr const char* attack = "attack";
static constexpr const char* release_ = "release";
static constexpr const char* makeup = "makeup";
static constexpr const char* lookahead = "lookahead";
static constexpr const char* mix = "mix";
}

class MambaCompressorProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorARAExtension
{
public:
    MambaCompressorProcessor();
    ~MambaCompressorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
            && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;
        return true;
    }

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mamba Compressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }

    double getTailLengthSeconds() const override
    {
        double tail;
        if (getTailLengthSecondsForARA (tail))
            return tail;
        if (pLookahead == nullptr) return 0.0;
        return static_cast<double>(pLookahead->load(std::memory_order_relaxed)) / 1000.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> gainReductionDb{0.0f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    CompressorDSP dsp;

    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pKnee = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMakeup = nullptr;
    std::atomic<float>* pLookahead = nullptr;
    std::atomic<float>* pMix = nullptr;

    float lastLookaheadMs = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MambaCompressorProcessor)
};
