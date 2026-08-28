#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MambaCompressorProcessor::createLayout()
{
    using Attr = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::threshold, 1}, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -18.0f,
        Attr{}.withLabel("dB")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::ratio, 1}, "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
        Attr{}.withLabel(":1")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::knee, 1}, "Knee",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 3.0f,
        Attr{}.withLabel("dB")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::attack, 1}, "Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.4f), 5.0f,
        Attr{}.withLabel("ms")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::release_, 1}, "Release",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.4f), 100.0f,
        Attr{}.withLabel("ms")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::makeup, 1}, "Makeup",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f,
        Attr{}.withLabel("dB")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::lookahead, 1}, "Lookahead",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 5.0f,
        Attr{}.withLabel("ms")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::mix, 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f,
        Attr{}.withLabel("%")));

    return {p.begin(), p.end()};
}

MambaCompressorProcessor::MambaCompressorProcessor()
: AudioProcessor(BusesProperties()
    .withInput ("Input", juce::AudioChannelSet::stereo(), true)
    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "MambaCompressor", createLayout())
{}

void MambaCompressorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate);

    pThreshold = apvts.getRawParameterValue(ParamIDs::threshold);
    pRatio = apvts.getRawParameterValue(ParamIDs::ratio);
    pKnee = apvts.getRawParameterValue(ParamIDs::knee);
    pAttack = apvts.getRawParameterValue(ParamIDs::attack);
    pRelease = apvts.getRawParameterValue(ParamIDs::release_);
    pMakeup = apvts.getRawParameterValue(ParamIDs::makeup);
    pLookahead = apvts.getRawParameterValue(ParamIDs::lookahead);
    pMix = apvts.getRawParameterValue(ParamIDs::mix);

    lastLookaheadMs = -1.0f;

    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
}

void MambaCompressorProcessor::releaseResources()
{
    releaseResourcesForARA();
}

void MambaCompressorProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto* audioPlayHead = getPlayHead();

    if (processBlockForARA (buffer, isRealtime(), audioPlayHead))
        return;

    dsp.threshold_db = static_cast<double>(pThreshold->load(std::memory_order_relaxed));
    dsp.ratio = static_cast<double>(pRatio ->load(std::memory_order_relaxed));
    dsp.knee_db = static_cast<double>(pKnee ->load(std::memory_order_relaxed));
    dsp.attack_ms = static_cast<double>(pAttack ->load(std::memory_order_relaxed));
    dsp.release_ms = static_cast<double>(pRelease ->load(std::memory_order_relaxed));
    dsp.makeup_db = static_cast<double>(pMakeup ->load(std::memory_order_relaxed));
    dsp.lookahead_ms = static_cast<double>(pLookahead->load(std::memory_order_relaxed));
    dsp.mix = static_cast<double>(pMix ->load(std::memory_order_relaxed));

    dsp.update_coeffs();

    const float currentLookahead = pLookahead->load(std::memory_order_relaxed);
    if (std::abs(currentLookahead - lastLookaheadMs) > 0.01f) {
        setLatencySamples(static_cast<int>(dsp.lookahead_samples));
        lastLookaheadMs = currentLookahead;
    }

    const int numCh = std::min(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    float* ch[2] = {nullptr, nullptr};
    for (int c = 0; c < numCh; ++c)
        ch[c] = buffer.getWritePointer(c);

    dsp.process(ch, ch, static_cast<uint32_t>(numCh),
                static_cast<uint32_t>(numSamples));

    gainReductionDb.store(static_cast<float>(dsp.env_db), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MambaCompressorProcessor::createEditor()
{
    return new MambaCompressorEditor(*this);
}

void MambaCompressorProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void MambaCompressorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MambaCompressorProcessor();
}

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<MambaARADocumentController>();
}
