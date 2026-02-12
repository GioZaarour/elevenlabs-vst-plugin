#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>

/**
 * AudioCacheManager handles:
 * - Caching generated audio to disk
 * - Converting MP3 to WAV format
 * - Tracking generation history
 * - Loading cached audio for playback
 */
class AudioCacheManager
{
public:
    AudioCacheManager();
    ~AudioCacheManager() = default;

    // History entry for tracking generations
    struct HistoryEntry
    {
        juce::String id;
        juce::String prompt;
        juce::String genre;
        int durationMs = 0;
        juce::String audioFilePath;
        juce::Time timestamp;
        juce::String projectUuid;  // UUID of the plugin instance that created this
    };

    // Save MP3 data to cache, converting to WAV
    // Returns the path to the cached WAV file, or empty string on failure
    juce::String cacheAudio(const juce::MemoryBlock& mp3Data,
                             const juce::String& prompt,
                             const juce::String& genre,
                             int durationMs,
                             const juce::String& projectUuid = {});

    // Load cached audio as an AudioBuffer
    std::unique_ptr<juce::AudioBuffer<float>> loadCachedAudio(const juce::String& filePath,
                                                               double targetSampleRate);

    // Get the audio format reader for streaming (caller owns the reader)
    std::unique_ptr<juce::AudioFormatReader> getAudioReader(const juce::String& filePath);

    // History management
    juce::Array<HistoryEntry> getHistory() const;
    juce::Array<HistoryEntry> getHistoryForProject(const juce::String& projectUuid) const;
    void clearHistory();
    void removeHistoryEntry(const juce::String& id);

    // Cache directory management
    void setCacheDirectory(const juce::File& directory);
    juce::File getCacheDirectory() const;
    void clearCache();

    // Utility
    juce::int64 getCacheSizeBytes() const;

private:
    juce::String generateUniqueId() const;
    void loadHistory();
    void saveHistory();
    juce::File getHistoryFile() const;

    juce::AudioFormatManager formatManager;
    juce::File cacheDirectory;
    juce::Array<HistoryEntry> history;
    juce::CriticalSection historyLock;
    juce::InterProcessLock historyFileLock{"ElevenLabsVST_HistoryLock"};

    static constexpr double kDefaultSampleRate = 48000.0;
    static constexpr int kDefaultBitDepth = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioCacheManager)
};
