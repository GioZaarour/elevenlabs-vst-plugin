#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

/**
 * StateSerializer handles persistent storage for:
 * - Global application settings (API key, preferences)
 * - Per-instance plugin state (for DAW project save/restore)
 */
class StateSerializer
{
public:
    StateSerializer();
    ~StateSerializer() = default;

    // Global settings (stored in Application Support)
    void setApiKey(const juce::String& key);
    juce::String getApiKey() const;

    void setLastGenre(const juce::String& genre);
    juce::String getLastGenre() const;

    void setLastDuration(int durationMs);
    int getLastDuration() const;

    // Per-instance state (for DAW project serialization)
    struct PluginState
    {
        juce::String prompt;
        juce::String genre;
        int durationMs = 30000;
        juce::String cachedAudioPath;  // Path to cached audio file
        juce::String generationId;     // For history tracking
        juce::String instanceUuid;     // Unique ID for this plugin instance (for history scoping)
    };

    // History scope setting (global)
    void setShowAllSamples(bool showAll);
    bool getShowAllSamples() const;

    void savePluginState(juce::MemoryBlock& destData, const PluginState& state);
    PluginState loadPluginState(const void* data, int sizeInBytes);

    // Utility
    juce::File getConfigDirectory() const;
    juce::File getCacheDirectory(const juce::File& projectDir) const;

private:
    void loadGlobalConfig();
    void saveGlobalConfig();

    juce::File getGlobalConfigFile() const;

    juce::CriticalSection configLock;
    juce::var globalConfig;

    static constexpr int kCurrentStateVersion = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateSerializer)
};
