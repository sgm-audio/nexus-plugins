#pragma once
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_audio_formats/juce_audio_formats.h>

class MambaARAPlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    using ARAPlaybackRenderer::ARAPlaybackRenderer;

    void prepareToPlay (double sampleRateIn,
                        int maximumSamplesPerBlockIn,
                        int numChannelsIn,
                        juce::AudioProcessor::ProcessingPrecision,
                        juce::ARAPlaybackRenderer::AlwaysNonRealtime) override
    {
        sampleRate = sampleRateIn;
        numChannels = numChannelsIn;
        maximumSamplesPerBlock = maximumSamplesPerBlockIn;

        for (const auto playbackRegion : getPlaybackRegions())
        {
            auto audioSource = playbackRegion->getAudioModification()->getAudioSource();
            if (audioSourceReaders.find (audioSource) == audioSourceReaders.end())
                audioSourceReaders.emplace (audioSource, std::make_unique<juce::ARAAudioSourceReader> (audioSource));
        }
    }

    void releaseResources() override
    {
        audioSourceReaders.clear();
    }

    bool processBlock (juce::AudioBuffer<float>& buffer,
                       juce::AudioProcessor::Realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override
    {
        const auto numSamples = buffer.getNumSamples();
        const auto timeInSamples = positionInfo.getTimeInSamples().orFallback (0);
        const auto isPlaying = positionInfo.getIsPlaying();
        bool didRenderAnyRegion = false;

        if (isPlaying)
        {
            const auto blockRange = juce::Range<juce::int64>::withStartAndLength (timeInSamples, numSamples);

            for (const auto& playbackRegion : getPlaybackRegions())
            {
                const auto playbackSampleRange = playbackRegion->getSampleRange (
                    sampleRate, juce::ARAPlaybackRegion::IncludeHeadAndTail::no);
                auto renderRange = blockRange.getIntersectionWith (playbackSampleRange);

                if (renderRange.isEmpty())
                    continue;

                const auto modificationSampleRange = juce::Range<juce::int64> (
                    playbackRegion->getStartInAudioModificationSamples(),
                    playbackRegion->getEndInAudioModificationSamples());
                const auto modificationSampleOffset = modificationSampleRange.getStart() - playbackSampleRange.getStart();

                renderRange = renderRange.getIntersectionWith (
                    modificationSampleRange.movedToStartAt (playbackSampleRange.getStart()));

                if (renderRange.isEmpty())
                    continue;

                const auto audioSource = playbackRegion->getAudioModification()->getAudioSource();
                auto readerIt = audioSourceReaders.find (audioSource);

                if (std::make_tuple (audioSource->getChannelCount(), audioSource->getSampleRate())
                    != std::make_tuple (numChannels, sampleRate)
                    || readerIt == audioSourceReaders.end())
                    continue;

                auto& reader = readerIt->second;

                const int numSamplesToRead = static_cast<int> (renderRange.getLength());
                const int startInBuffer = static_cast<int> (renderRange.getStart() - blockRange.getStart());
                auto startInSource = renderRange.getStart() + modificationSampleOffset;

                std::vector<int*> intPtrs (static_cast<size_t> (numChannels));
                for (int c = 0; c < numChannels; ++c)
                    intPtrs[static_cast<size_t> (c)] = reinterpret_cast<int*> (buffer.getWritePointer (c));

                if (! reader->readSamples (intPtrs.data(), numChannels, startInBuffer, startInSource, numSamplesToRead))
                    continue;

                didRenderAnyRegion = true;
            }
        }

        if (! didRenderAnyRegion)
            buffer.clear();

        return true;
    }

    using ARAPlaybackRenderer::processBlock;

private:
    double sampleRate = 48000.0;
    int numChannels = 2;
    int maximumSamplesPerBlock = 128;
    std::map<juce::ARAAudioSource*, std::unique_ptr<juce::ARAAudioSourceReader>> audioSourceReaders;
};

class MambaARADocumentController final : public juce::ARADocumentControllerSpecialisation
{
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

protected:
    void willBeginEditing (juce::ARADocument*) override
    {
        processBlockLock.enterWrite();
    }

    void didEndEditing (juce::ARADocument*) override
    {
        processBlockLock.exitWrite();
    }

    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new MambaARAPlaybackRenderer (getDocumentController());
    }

    bool doRestoreObjectsFromStream (juce::ARAInputStream& input,
                                     const juce::ARARestoreObjectsFilter*) noexcept override
    {
        juce::ignoreUnused (input);
        return true;
    }

    bool doStoreObjectsToStream (juce::ARAOutputStream& output,
                                 const juce::ARAStoreObjectsFilter*) noexcept override
    {
        juce::ignoreUnused (output);
        return true;
    }

private:
    juce::ReadWriteLock processBlockLock;
};
