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
                      public juce::Timer
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

    // Waveform display component
    class WaveformDisplay : public juce::Component
    {
    public:
        void setAudioBuffer(juce::AudioBuffer<float>* buffer);
        void setPlaybackPosition(double position);  // 0.0 - 1.0
        void paint(juce::Graphics&) override;

    private:
        juce::AudioBuffer<float>* audioBuffer = nullptr;
        double playbackPos = 0.0;
    };

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
                   std::function<void(const juce::String& apiKey)> onSave,
                   std::function<void()> onCancel);
        void hide();
        bool isVisible() const { return visible; }

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        bool visible = false;

        juce::Label apiKeyLabel{"", "API Key:"};
        juce::TextEditor apiKeyEditor;
        juce::TextButton saveBtn{"Save"};
        juce::TextButton cancelBtn{"Cancel"};

        std::function<void(const juce::String&)> onSaveCallback;
        std::function<void()> onCancelCallback;
    };

    GenerationDialog generationDialog;
    SettingsDialog settingsDialog;

    // State
    bool isGenerating = false;
    juce::String currentStatus;
    juce::String currentError;

    void showGenerationDialog();
    void showSettingsDialog();
    void updatePlaybackState();
    void handleGenerationComplete(bool success, const juce::String& errorMsg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
