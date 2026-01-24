#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "StateSerializer.h"
#include "ApiClient.h"
#include "AudioCacheManager.h"
#include <atomic>
#include <memory>

/**
 * PluginProcessor handles the audio processing pipeline.
 * Uses atomic pointer swap for thread-safe buffer hot-swapping.
 */
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Generation control
    void startGeneration(const juce::String& prompt, const juce::String& genre, int durationMs);
    void cancelGeneration();
    bool isGenerating() const;

    // Playback control
    void setPlaying(bool shouldPlay);
    bool isPlaying() const;
    void setLooping(bool shouldLoop);
    bool isLooping() const;
    void setPlaybackPosition(double positionInSeconds);
    double getPlaybackPosition() const;
    double getAudioLength() const;

    // Audio buffer management
    void loadAudioFromCache(const juce::String& filePath);
    bool hasAudio() const;

    // Status and callbacks
    using StatusCallback = std::function<void(const juce::String&)>;
    using GenerationCompleteCallback = std::function<void(bool success, const juce::String& errorMsg)>;

    void setStatusCallback(StatusCallback callback);
    void setGenerationCompleteCallback(GenerationCompleteCallback callback);

    // Access to components
    StateSerializer& getStateSerializer() { return stateSerializer; }
    AudioCacheManager& getCacheManager() { return cacheManager; }

    // Current state
    juce::String getCurrentPrompt() const { return currentPrompt; }
    juce::String getCurrentGenre() const { return currentGenre; }
    int getCurrentDurationMs() const { return currentDurationMs; }

private:
    // Thread-safe audio buffer wrapper
    struct AudioBufferRef
    {
        std::unique_ptr<juce::AudioBuffer<float>> buffer;
        double sampleRate = 48000.0;

        AudioBufferRef() = default;
        AudioBufferRef(std::unique_ptr<juce::AudioBuffer<float>> b, double sr)
            : buffer(std::move(b)), sampleRate(sr) {}
    };

    // Components
    StateSerializer stateSerializer;
    ApiClient apiClient;
    AudioCacheManager cacheManager;

    // Audio buffer (accessed from audio thread)
    std::shared_ptr<AudioBufferRef> currentBuffer;
    std::atomic<AudioBufferRef*> pendingBuffer{nullptr};
    juce::SpinLock bufferLock;

    // Playback state
    std::atomic<bool> playing{false};
    std::atomic<bool> looping{true};
    std::atomic<juce::int64> playbackPosition{0};
    double currentSampleRate = 48000.0;

    // Generation state
    juce::String currentPrompt;
    juce::String currentGenre;
    int currentDurationMs = 30000;
    juce::String currentCachedPath;

    // Callbacks
    StatusCallback statusCallback;
    GenerationCompleteCallback generationCompleteCallback;
    juce::CriticalSection callbackLock;

    void swapInPendingBuffer();
    void notifyStatus(const juce::String& status);
    void notifyGenerationComplete(bool success, const juce::String& errorMsg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
