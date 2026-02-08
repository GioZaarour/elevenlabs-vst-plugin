#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

/**
 * PluginEditor provides the user interface:
 * - Main view with waveform display and controls
 * - Modal dialog for generation settings
 * - Settings panel for API key configuration
 */
class PluginEditor : public juce::AudioProcessorEditor,
                      public juce::Timer,
                      public juce::DragAndDropContainer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PluginProcessor& processor;

    // UI Colors
    struct Colors
    {
        static constexpr juce::uint32 background = 0xFF1A1A2E;
        static constexpr juce::uint32 surface = 0xFF16213E;
        static constexpr juce::uint32 primary = 0xFF0F3460;
        static constexpr juce::uint32 accent = 0xFFE94560;
        static constexpr juce::uint32 text = 0xFFEEEEEE;
        static constexpr juce::uint32 textMuted = 0xFF888888;
        static constexpr juce::uint32 success = 0xFF4CAF50;
        static constexpr juce::uint32 error = 0xFFE94560;
        static constexpr juce::uint32 waveform = 0xFF0F3460;
    };

    // Waveform display component with loading animation and drag support
    class WaveformDisplay : public juce::Component,
                            public juce::Timer
    {
    public:
        WaveformDisplay();
        ~WaveformDisplay() override;

        void setAudioBuffer(juce::AudioBuffer<float>* buffer);
        juce::AudioBuffer<float>* getAudioBuffer() const { return audioBuffer; }
        void setPlaybackPosition(double position);  // 0.0 - 1.0
        void setGenerating(bool generating);
        void setCachedFilePath(const juce::String& path) { cachedFilePath = path; }

        void paint(juce::Graphics&) override;
        void timerCallback() override;

        // Mouse handling for scrub and drag
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

        // Callbacks
        std::function<void(double)> onScrub;  // Called with position 0.0-1.0
        std::function<void()> onDragStarted;

    private:
        juce::AudioBuffer<float>* audioBuffer = nullptr;
        double playbackPos = 0.0;
        bool isGenerating = false;
        float animationPhase = 0.0f;
        juce::String cachedFilePath;

        // Drag detection
        juce::int64 mouseDownTime = 0;
        bool isDragging = false;
        static constexpr juce::int64 kDragThresholdMs = 300;
    };

    // History dropdown
    juce::ComboBox historyDropdown;
    void populateHistoryDropdown();
    void onHistorySelectionChanged();

    // Main controls
    juce::TextButton generateButton{"Generate New"};
    juce::TextButton playButton{"Play"};
    juce::TextButton stopButton{"Stop"};
    juce::ToggleButton loopButton{"Loop"};
    juce::TextButton settingsButton{"Settings"};

    WaveformDisplay waveformDisplay;
    juce::Label statusLabel;
    juce::Label errorLabel;

    // Generation dialog components
    class GenerationDialog : public juce::Component
    {
    public:
        GenerationDialog();

        void show(std::function<void(const juce::String& prompt,
                                       const juce::String& genre,
                                       int durationMs)> onGenerate,
                   std::function<void()> onCancel);
        void hide();
        bool isVisible() const { return visible; }

        void setDefaults(const juce::String& genre, int durationMs);

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        bool visible = false;

        juce::TextEditor promptEditor;
        juce::ComboBox genreCombo;
        juce::ComboBox durationCombo;
        juce::TextButton generateBtn{"Generate"};
        juce::TextButton cancelBtn{"Cancel"};

        std::function<void(const juce::String&, const juce::String&, int)> onGenerateCallback;
        std::function<void()> onCancelCallback;
    };

    // Settings dialog components
    class SettingsDialog : public juce::Component
    {
    public:
        SettingsDialog();

        void show(const juce::String& currentApiKey,
                   bool showAllSamples,
                   std::function<void(const juce::String& apiKey, bool showAllSamples)> onSave,
                   std::function<void()> onCancel,
                   std::function<void()> onClearCache);
        void hide();
        bool isVisible() const { return visible; }

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        bool visible = false;

        juce::Label apiKeyLabel{"", "API Key:"};
        juce::TextEditor apiKeyEditor;
        juce::Label historyScopeLabel{"", "Show samples from:"};
        juce::ComboBox historyScopeCombo;
        juce::TextButton clearCacheBtn{"Clear Cache"};
        juce::TextButton saveBtn{"Save"};
        juce::TextButton cancelBtn{"Cancel"};

        std::function<void(const juce::String&, bool)> onSaveCallback;
        std::function<void()> onCancelCallback;
        std::function<void()> onClearCacheCallback;
    };

    GenerationDialog generationDialog;
    SettingsDialog settingsDialog;

    // State
    bool isGenerating = false;
    juce::String currentStatus;
    juce::String currentError;
    juce::String currentHistoryId;
    juce::Array<AudioCacheManager::HistoryEntry> historyEntries;

    void showGenerationDialog();
    void showSettingsDialog();
    void updatePlaybackState();
    void handleGenerationComplete(bool success, const juce::String& errorMsg);
    void regenerateFromHistory(const AudioCacheManager::HistoryEntry& entry);

    // Missing audio state
    AudioCacheManager::HistoryEntry missingAudioEntry;
    bool hasMissingAudio = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
