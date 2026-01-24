#include "PluginEditor.h"

//==============================================================================
// WaveformDisplay
//==============================================================================
void PluginEditor::WaveformDisplay::setAudioBuffer(juce::AudioBuffer<float>* buffer)
{
    audioBuffer = buffer;
    repaint();
}

void PluginEditor::WaveformDisplay::setPlaybackPosition(double position)
{
    playbackPos = position;
    repaint();
}

void PluginEditor::WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(Colors::surface));
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(juce::Colour(Colors::primary));
    g.drawRoundedRectangle(bounds.reduced(1), 8.0f, 2.0f);

    if (audioBuffer == nullptr || audioBuffer->getNumSamples() == 0)
    {
        // No audio - show placeholder
        g.setColour(juce::Colour(Colors::textMuted));
        g.setFont(14.0f);
        g.drawText("No audio generated yet", bounds, juce::Justification::centred);
        return;
    }

    // Draw waveform
    const int numSamples = audioBuffer->getNumSamples();
    const int numChannels = audioBuffer->getNumChannels();
    const float width = bounds.getWidth() - 20;
    const float height = bounds.getHeight() - 20;
    const float centerY = bounds.getCentreY();

    g.setColour(juce::Colour(Colors::waveform));

    juce::Path waveformPath;
    bool pathStarted = false;

    const int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));

    for (int x = 0; x < static_cast<int>(width); ++x)
    {
        const int sampleIndex = x * samplesPerPixel;
        if (sampleIndex >= numSamples)
            break;

        // Get max absolute value in this pixel's sample range
        float maxVal = 0.0f;
        for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float val = std::abs(audioBuffer->getSample(ch, sampleIndex + i));
                maxVal = juce::jmax(maxVal, val);
            }
        }

        const float y = centerY - maxVal * (height * 0.4f);

        if (!pathStarted)
        {
            waveformPath.startNewSubPath(bounds.getX() + 10 + x, y);
            pathStarted = true;
        }
        else
        {
            waveformPath.lineTo(bounds.getX() + 10 + x, y);
        }
    }

    // Mirror for bottom half
    for (int x = static_cast<int>(width) - 1; x >= 0; --x)
    {
        const int sampleIndex = x * samplesPerPixel;
        if (sampleIndex >= numSamples)
            continue;

        float maxVal = 0.0f;
        for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float val = std::abs(audioBuffer->getSample(ch, sampleIndex + i));
                maxVal = juce::jmax(maxVal, val);
            }
        }

        const float y = centerY + maxVal * (height * 0.4f);
        waveformPath.lineTo(bounds.getX() + 10 + x, y);
    }

    waveformPath.closeSubPath();
    g.setColour(juce::Colour(Colors::accent).withAlpha(0.6f));
    g.fillPath(waveformPath);

    g.setColour(juce::Colour(Colors::accent));
    g.strokePath(waveformPath, juce::PathStrokeType(1.5f));

    // Draw playback position
    if (playbackPos > 0.0 && playbackPos < 1.0)
    {
        const float posX = bounds.getX() + 10 + static_cast<float>(playbackPos * width);
        g.setColour(juce::Colour(Colors::text));
        g.drawLine(posX, bounds.getY() + 10, posX, bounds.getBottom() - 10, 2.0f);
    }
}

//==============================================================================
// GenerationDialog
//==============================================================================
PluginEditor::GenerationDialog::GenerationDialog()
{
    promptEditor.setMultiLine(true);
    promptEditor.setReturnKeyStartsNewLine(true);
    promptEditor.setTextToShowWhenEmpty("Describe the music you want to generate...",
                                          juce::Colour(Colors::textMuted));
    addAndMakeVisible(promptEditor);

    genreCombo.addItem("Pop", 1);
    genreCombo.addItem("Rock", 2);
    genreCombo.addItem("Electronic", 3);
    genreCombo.addItem("Hip Hop", 4);
    genreCombo.addItem("Jazz", 5);
    genreCombo.addItem("Classical", 6);
    genreCombo.addItem("Ambient", 7);
    genreCombo.addItem("Folk", 8);
    genreCombo.setSelectedId(1);
    addAndMakeVisible(genreCombo);

    durationCombo.addItem("15 seconds", 15000);
    durationCombo.addItem("30 seconds", 30000);
    durationCombo.addItem("60 seconds", 60000);
    durationCombo.addItem("90 seconds", 90000);
    durationCombo.addItem("120 seconds", 120000);
    durationCombo.setSelectedId(30000);
    addAndMakeVisible(durationCombo);

    generateBtn.onClick = [this]()
    {
        if (onGenerateCallback)
        {
            juce::String genre = genreCombo.getText();
            int duration = durationCombo.getSelectedId();
            onGenerateCallback(promptEditor.getText(), genre, duration);
        }
        hide();
    };
    addAndMakeVisible(generateBtn);

    cancelBtn.onClick = [this]()
    {
        if (onCancelCallback)
            onCancelCallback();
        hide();
    };
    addAndMakeVisible(cancelBtn);

    setVisible(false);
}

void PluginEditor::GenerationDialog::show(
    std::function<void(const juce::String&, const juce::String&, int)> onGenerate,
    std::function<void()> onCancel)
{
    onGenerateCallback = std::move(onGenerate);
    onCancelCallback = std::move(onCancel);
    visible = true;
    setVisible(true);
    promptEditor.grabKeyboardFocus();
}

void PluginEditor::GenerationDialog::hide()
{
    visible = false;
    setVisible(false);
}

void PluginEditor::GenerationDialog::setDefaults(const juce::String& genre, int durationMs)
{
    // Find and select matching genre
    for (int i = 0; i < genreCombo.getNumItems(); ++i)
    {
        if (genreCombo.getItemText(i).equalsIgnoreCase(genre))
        {
            genreCombo.setSelectedItemIndex(i);
            break;
        }
    }

    // Select matching duration
    durationCombo.setSelectedId(durationMs, juce::dontSendNotification);
}

void PluginEditor::GenerationDialog::paint(juce::Graphics& g)
{
    // Semi-transparent overlay
    g.fillAll(juce::Colours::black.withAlpha(0.7f));

    // Dialog background
    auto dialogBounds = getLocalBounds().reduced(40).toFloat();
    g.setColour(juce::Colour(Colors::surface));
    g.fillRoundedRectangle(dialogBounds, 12.0f);

    g.setColour(juce::Colour(Colors::primary));
    g.drawRoundedRectangle(dialogBounds.reduced(1), 12.0f, 2.0f);

    // Title
    g.setColour(juce::Colour(Colors::text));
    g.setFont(18.0f);
    g.drawText("Generate Music", dialogBounds.removeFromTop(50),
                juce::Justification::centred);
}

void PluginEditor::GenerationDialog::resized()
{
    auto bounds = getLocalBounds().reduced(60);

    bounds.removeFromTop(40);  // Title space

    auto promptBounds = bounds.removeFromTop(100);
    promptEditor.setBounds(promptBounds);

    bounds.removeFromTop(20);

    auto rowBounds = bounds.removeFromTop(30);
    genreCombo.setBounds(rowBounds.removeFromLeft(rowBounds.getWidth() / 2 - 10));
    rowBounds.removeFromLeft(20);
    durationCombo.setBounds(rowBounds);

    bounds.removeFromTop(30);

    auto buttonBounds = bounds.removeFromTop(40);
    const int buttonWidth = 100;
    const int spacing = 20;
    const int totalWidth = buttonWidth * 2 + spacing;
    buttonBounds = buttonBounds.withSizeKeepingCentre(totalWidth, 40);

    cancelBtn.setBounds(buttonBounds.removeFromLeft(buttonWidth));
    buttonBounds.removeFromLeft(spacing);
    generateBtn.setBounds(buttonBounds);
}

//==============================================================================
// SettingsDialog
//==============================================================================
PluginEditor::SettingsDialog::SettingsDialog()
{
    apiKeyLabel.setColour(juce::Label::textColourId, juce::Colour(Colors::text));
    addAndMakeVisible(apiKeyLabel);

    apiKeyEditor.setPasswordCharacter('*');
    apiKeyEditor.setTextToShowWhenEmpty("Enter your ElevenLabs API key",
                                          juce::Colour(Colors::textMuted));
    addAndMakeVisible(apiKeyEditor);

    saveBtn.onClick = [this]()
    {
        if (onSaveCallback)
            onSaveCallback(apiKeyEditor.getText());
        hide();
    };
    addAndMakeVisible(saveBtn);

    cancelBtn.onClick = [this]()
    {
        if (onCancelCallback)
            onCancelCallback();
        hide();
    };
    addAndMakeVisible(cancelBtn);

    setVisible(false);
}

void PluginEditor::SettingsDialog::show(
    const juce::String& currentApiKey,
    std::function<void(const juce::String&)> onSave,
    std::function<void()> onCancel)
{
    apiKeyEditor.setText(currentApiKey, false);
    onSaveCallback = std::move(onSave);
    onCancelCallback = std::move(onCancel);
    visible = true;
    setVisible(true);
    apiKeyEditor.grabKeyboardFocus();
}

void PluginEditor::SettingsDialog::hide()
{
    visible = false;
    setVisible(false);
}

void PluginEditor::SettingsDialog::paint(juce::Graphics& g)
{
    // Semi-transparent overlay
    g.fillAll(juce::Colours::black.withAlpha(0.7f));

    // Dialog background
    auto dialogBounds = getLocalBounds().reduced(60, 120).toFloat();
    g.setColour(juce::Colour(Colors::surface));
    g.fillRoundedRectangle(dialogBounds, 12.0f);

    g.setColour(juce::Colour(Colors::primary));
    g.drawRoundedRectangle(dialogBounds.reduced(1), 12.0f, 2.0f);

    // Title
    g.setColour(juce::Colour(Colors::text));
    g.setFont(18.0f);
    g.drawText("Settings", dialogBounds.removeFromTop(50),
                juce::Justification::centred);
}

void PluginEditor::SettingsDialog::resized()
{
    auto bounds = getLocalBounds().reduced(80, 140);

    bounds.removeFromTop(40);  // Title space

    apiKeyLabel.setBounds(bounds.removeFromTop(25));
    apiKeyEditor.setBounds(bounds.removeFromTop(35));

    bounds.removeFromTop(30);

    auto buttonBounds = bounds.removeFromTop(40);
    const int buttonWidth = 100;
    const int spacing = 20;
    const int totalWidth = buttonWidth * 2 + spacing;
    buttonBounds = buttonBounds.withSizeKeepingCentre(totalWidth, 40);

    cancelBtn.setBounds(buttonBounds.removeFromLeft(buttonWidth));
    buttonBounds.removeFromLeft(spacing);
    saveBtn.setBounds(buttonBounds);
}

//==============================================================================
// PluginEditor
//==============================================================================
PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Set up callbacks
    processor.setStatusCallback([this](const juce::String& status)
    {
        currentStatus = status;
        statusLabel.setText(status, juce::dontSendNotification);
    });

    processor.setGenerationCompleteCallback([this](bool success, const juce::String& errorMsg)
    {
        handleGenerationComplete(success, errorMsg);
    });

    // Configure main controls
    generateButton.onClick = [this]() { showGenerationDialog(); };
    addAndMakeVisible(generateButton);

    playButton.onClick = [this]()
    {
        processor.setPlaying(true);
    };
    addAndMakeVisible(playButton);

    stopButton.onClick = [this]()
    {
        processor.setPlaying(false);
    };
    addAndMakeVisible(stopButton);

    loopButton.setToggleState(processor.isLooping(), juce::dontSendNotification);
    loopButton.onClick = [this]()
    {
        processor.setLooping(loopButton.getToggleState());
    };
    addAndMakeVisible(loopButton);

    settingsButton.onClick = [this]() { showSettingsDialog(); };
    addAndMakeVisible(settingsButton);

    addAndMakeVisible(waveformDisplay);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(Colors::textMuted));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    errorLabel.setColour(juce::Label::textColourId, juce::Colour(Colors::error));
    errorLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(errorLabel);

    addChildComponent(generationDialog);
    addChildComponent(settingsDialog);

    setSize(500, 350);
    startTimerHz(30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    processor.setStatusCallback(nullptr);
    processor.setGenerationCompleteCallback(nullptr);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colors::background));

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(50).toFloat();
    g.setColour(juce::Colour(Colors::surface));
    g.fillRect(headerBounds);

    g.setColour(juce::Colour(Colors::text));
    g.setFont(20.0f);
    g.drawText("ElevenLabs Music Generator", headerBounds,
                juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header
    bounds.removeFromTop(50);

    // Main content with padding
    bounds = bounds.reduced(20);

    // Waveform display
    waveformDisplay.setBounds(bounds.removeFromTop(150));

    bounds.removeFromTop(15);

    // Status labels
    statusLabel.setBounds(bounds.removeFromTop(25));
    errorLabel.setBounds(bounds.removeFromTop(25));

    bounds.removeFromTop(10);

    // Control buttons
    auto buttonRow = bounds.removeFromTop(35);
    const int buttonWidth = 80;
    const int spacing = 10;

    generateButton.setBounds(buttonRow.removeFromLeft(100));
    buttonRow.removeFromLeft(spacing);

    playButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(spacing);

    stopButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(spacing);

    loopButton.setBounds(buttonRow.removeFromLeft(buttonWidth));

    settingsButton.setBounds(buttonRow.removeFromRight(buttonWidth));

    // Dialogs cover entire component
    generationDialog.setBounds(getLocalBounds());
    settingsDialog.setBounds(getLocalBounds());
}

void PluginEditor::timerCallback()
{
    updatePlaybackState();

    // Update waveform if we have audio
    if (processor.hasAudio())
    {
        double length = processor.getAudioLength();
        if (length > 0)
        {
            double pos = processor.getPlaybackPosition() / length;
            waveformDisplay.setPlaybackPosition(pos);
        }
    }

    // Update generating state
    isGenerating = processor.isGenerating();
    generateButton.setEnabled(!isGenerating);

    if (isGenerating)
    {
        generateButton.setButtonText("Generating...");
    }
    else
    {
        generateButton.setButtonText("Generate New");
    }
}

void PluginEditor::showGenerationDialog()
{
    // Check if API key is set
    juce::String apiKey = processor.getStateSerializer().getApiKey();
    if (apiKey.isEmpty())
    {
        errorLabel.setText("Please set your API key in Settings first", juce::dontSendNotification);
        showSettingsDialog();
        return;
    }

    errorLabel.setText("", juce::dontSendNotification);

    // Set defaults from last use
    juce::String lastGenre = processor.getStateSerializer().getLastGenre();
    int lastDuration = processor.getStateSerializer().getLastDuration();
    generationDialog.setDefaults(lastGenre, lastDuration);

    generationDialog.show(
        [this](const juce::String& prompt, const juce::String& genre, int durationMs)
        {
            if (prompt.isNotEmpty())
            {
                processor.startGeneration(prompt, genre, durationMs);
            }
        },
        []() { /* cancelled */ });
}

void PluginEditor::showSettingsDialog()
{
    juce::String currentKey = processor.getStateSerializer().getApiKey();

    settingsDialog.show(
        currentKey,
        [this](const juce::String& apiKey)
        {
            processor.getStateSerializer().setApiKey(apiKey);
            errorLabel.setText("", juce::dontSendNotification);
        },
        []() { /* cancelled */ });
}

void PluginEditor::updatePlaybackState()
{
    bool hasAudio = processor.hasAudio();
    bool isPlaying = processor.isPlaying();

    playButton.setEnabled(hasAudio && !isPlaying);
    stopButton.setEnabled(hasAudio && isPlaying);
    loopButton.setEnabled(hasAudio);
}

void PluginEditor::handleGenerationComplete(bool success, const juce::String& errorMsg)
{
    isGenerating = false;

    if (success)
    {
        statusLabel.setText("Ready to play", juce::dontSendNotification);
        errorLabel.setText("", juce::dontSendNotification);

        // Update waveform with new audio
        // Note: We'd need to expose the buffer properly for this
        // For now, the timer callback will update the playback position
    }
    else
    {
        statusLabel.setText("Generation failed", juce::dontSendNotification);
        errorLabel.setText(errorMsg, juce::dontSendNotification);
    }

    repaint();
}
