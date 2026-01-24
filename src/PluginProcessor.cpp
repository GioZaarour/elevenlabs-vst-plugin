#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

PluginProcessor::~PluginProcessor()
{
    // Clean up any pending buffer
    delete pendingBuffer.exchange(nullptr);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Check for pending buffer swap (non-blocking)
    swapInPendingBuffer();

    buffer.clear();

    // Get current buffer reference
    auto bufRef = currentBuffer;
    if (bufRef == nullptr || bufRef->buffer == nullptr || !playing.load())
        return;

    auto& audioData = *bufRef->buffer;
    const int numChannels = juce::jmin(buffer.getNumChannels(), audioData.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    const juce::int64 totalSamples = audioData.getNumSamples();

    if (totalSamples == 0)
        return;

    // Calculate sample rate ratio for playback speed adjustment
    const double ratio = bufRef->sampleRate / currentSampleRate;

    juce::int64 pos = playbackPosition.load();
    int samplesWritten = 0;

    while (samplesWritten < numSamples)
    {
        // Adjust position for sample rate difference
        juce::int64 sourcePos = static_cast<juce::int64>(pos * ratio);

        if (sourcePos >= totalSamples)
        {
            if (looping.load())
            {
                pos = 0;
                sourcePos = 0;
            }
            else
            {
                playing.store(false);
                break;
            }
        }

        const int sourceSamplesAvailable = static_cast<int>(totalSamples - sourcePos);
        const int destSamplesNeeded = numSamples - samplesWritten;
        const int samplesToCopy = juce::jmin(sourceSamplesAvailable, destSamplesNeeded);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            buffer.copyFrom(channel, samplesWritten,
                             audioData.getReadPointer(channel, static_cast<int>(sourcePos)),
                             samplesToCopy);
        }

        samplesWritten += samplesToCopy;
        pos += static_cast<juce::int64>(samplesToCopy / ratio);
    }

    playbackPosition.store(pos);
}

void PluginProcessor::swapInPendingBuffer()
{
    auto* pending = pendingBuffer.exchange(nullptr);
    if (pending != nullptr)
    {
        juce::SpinLock::ScopedTryLockType lock(bufferLock);
        if (lock.isLocked())
        {
            currentBuffer = std::shared_ptr<AudioBufferRef>(pending);
            playbackPosition.store(0);
        }
        else
        {
            // Put it back if we couldn't lock
            auto* expected = static_cast<AudioBufferRef*>(nullptr);
            if (!pendingBuffer.compare_exchange_strong(expected, pending))
            {
                // Another buffer was already set, delete this one
                delete pending;
            }
        }
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    StateSerializer::PluginState state;
    state.prompt = currentPrompt;
    state.genre = currentGenre;
    state.durationMs = currentDurationMs;
    state.cachedAudioPath = currentCachedPath;

    stateSerializer.savePluginState(destData, state);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = stateSerializer.loadPluginState(data, sizeInBytes);

    currentPrompt = state.prompt;
    currentGenre = state.genre;
    currentDurationMs = state.durationMs;
    currentCachedPath = state.cachedAudioPath;

    // Try to load cached audio
    if (currentCachedPath.isNotEmpty())
    {
        loadAudioFromCache(currentCachedPath);
    }
}

void PluginProcessor::startGeneration(const juce::String& prompt, const juce::String& genre, int durationMs)
{
    currentPrompt = prompt;
    currentGenre = genre;
    currentDurationMs = durationMs;

    // Save preferences
    stateSerializer.setLastGenre(genre);
    stateSerializer.setLastDuration(durationMs);

    ApiClient::GenerationRequest request;
    request.prompt = prompt;
    request.genre = genre;
    request.durationMs = durationMs;

    juce::String apiKey = stateSerializer.getApiKey();

    apiClient.generateMusic(
        apiKey,
        request,
        [this, prompt, genre, durationMs](const ApiClient::GenerationResult& result)
        {
            if (result.success)
            {
                notifyStatus("Processing audio...");

                // Cache the audio
                juce::String cachedPath = cacheManager.cacheAudio(
                    result.audioData, prompt, genre, durationMs);

                if (cachedPath.isNotEmpty())
                {
                    currentCachedPath = cachedPath;
                    loadAudioFromCache(cachedPath);
                    notifyGenerationComplete(true, {});
                }
                else
                {
                    notifyGenerationComplete(false, "Failed to process audio");
                }
            }
            else
            {
                notifyGenerationComplete(false, result.errorMessage);
            }
        },
        [this](const juce::String& status)
        {
            notifyStatus(status);
        });
}

void PluginProcessor::cancelGeneration()
{
    apiClient.cancelRequest();
}

bool PluginProcessor::isGenerating() const
{
    return apiClient.isRequestInProgress();
}

void PluginProcessor::setPlaying(bool shouldPlay)
{
    playing.store(shouldPlay);
}

bool PluginProcessor::isPlaying() const
{
    return playing.load();
}

void PluginProcessor::setLooping(bool shouldLoop)
{
    looping.store(shouldLoop);
}

bool PluginProcessor::isLooping() const
{
    return looping.load();
}

void PluginProcessor::setPlaybackPosition(double positionInSeconds)
{
    juce::int64 samplePos = static_cast<juce::int64>(positionInSeconds * currentSampleRate);
    playbackPosition.store(samplePos);
}

double PluginProcessor::getPlaybackPosition() const
{
    return static_cast<double>(playbackPosition.load()) / currentSampleRate;
}

double PluginProcessor::getAudioLength() const
{
    auto bufRef = currentBuffer;
    if (bufRef == nullptr || bufRef->buffer == nullptr)
        return 0.0;

    return static_cast<double>(bufRef->buffer->getNumSamples()) / bufRef->sampleRate;
}

void PluginProcessor::loadAudioFromCache(const juce::String& filePath)
{
    auto buffer = cacheManager.loadCachedAudio(filePath, currentSampleRate);

    if (buffer != nullptr)
    {
        auto* newBuffer = new AudioBufferRef(std::move(buffer), currentSampleRate);

        // Swap in the new buffer atomically
        auto* old = pendingBuffer.exchange(newBuffer);
        delete old;  // Delete any previous pending buffer

        currentCachedPath = filePath;
    }
}

bool PluginProcessor::hasAudio() const
{
    return currentBuffer != nullptr && currentBuffer->buffer != nullptr;
}

void PluginProcessor::setStatusCallback(StatusCallback callback)
{
    juce::ScopedLock lock(callbackLock);
    statusCallback = std::move(callback);
}

void PluginProcessor::setGenerationCompleteCallback(GenerationCompleteCallback callback)
{
    juce::ScopedLock lock(callbackLock);
    generationCompleteCallback = std::move(callback);
}

void PluginProcessor::notifyStatus(const juce::String& status)
{
    juce::ScopedLock lock(callbackLock);
    if (statusCallback)
        statusCallback(status);
}

void PluginProcessor::notifyGenerationComplete(bool success, const juce::String& errorMsg)
{
    juce::ScopedLock lock(callbackLock);
    if (generationCompleteCallback)
        generationCompleteCallback(success, errorMsg);
}

// Plugin factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
