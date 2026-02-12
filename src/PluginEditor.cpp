#include "PluginEditor.h"

//==============================================================================
// WaveformDisplay
//==============================================================================
PluginEditor::WaveformDisplay::WaveformDisplay()
{
}

PluginEditor::WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void PluginEditor::WaveformDisplay::setAudioBuffer(const juce::AudioBuffer<float>* buffer)
{
    if (buffer != nullptr && buffer->getNumSamples() > 0)
    {
        audioBufferCopy.makeCopyOf(*buffer);
        hasAudioData = true;
    }
    else
    {
        audioBufferCopy.setSize(0, 0);
        hasAudioData = false;
    }
    repaint();
}

void PluginEditor::WaveformDisplay::clearAudioBuffer()
{
    audioBufferCopy.setSize(0, 0);
    hasAudioData = false;
    repaint();
}

void PluginEditor::WaveformDisplay::setPlaybackPosition(double position)
{
    playbackPos = position;
    repaint();
}

void PluginEditor::WaveformDisplay::setGenerating(bool generating)
{
    if (isGenerating != generating)
    {
        isGenerating = generating;
        if (generating)
        {
            animationPhase = 0.0f;
            startTimerHz(60);  // 60fps for smooth animation
        }
        else
        {
            stopTimer();
        }
        repaint();
    }
}

void PluginEditor::WaveformDisplay::timerCallback()
{
    // Advance animation phase
    animationPhase += 0.02f;
    if (animationPhase > 1.0f)
        animationPhase = 0.0f;
    repaint();
}

void PluginEditor::WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    mouseDownTime = juce::Time::currentTimeMillis();
    isDragging = false;
}

void PluginEditor::WaveformDisplay::mouseUp(const juce::MouseEvent& event)
{
    if (!isDragging && hasAudioData)
    {
        // This was a click (scrub) - calculate position
        auto bounds = getLocalBounds().toFloat();
        float width = bounds.getWidth() - 20.0f;
        float clickX = static_cast<float>(event.x) - (bounds.getX() + 10.0f);
        double position = juce::jlimit(0.0, 1.0, static_cast<double>(clickX / width));

        if (onScrub)
            onScrub(position);
    }
    isDragging = false;
}

void PluginEditor::WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDragging && hasAudioData && cachedFilePath.isNotEmpty())
    {
        juce::int64 elapsed = juce::Time::currentTimeMillis() - mouseDownTime;
        if (elapsed >= kDragThresholdMs)
        {
            // Initiate drag
            isDragging = true;

            juce::File audioFile(cachedFilePath);
            if (audioFile.existsAsFile())
            {
                juce::StringArray files;
                files.add(cachedFilePath);
                juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
            }
        }
    }
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

    // Loading animation during generation
    if (isGenerating)
    {
        // Draw horizontal gradient sweep
        float width = bounds.getWidth() - 20.0f;
        float gradientWidth = width * 0.3f;  // 30% of width
        float gradientX = bounds.getX() + 10.0f + (width - gradientWidth) * animationPhase;

        juce::ColourGradient gradient(
            juce::Colour(Colors::accent).withAlpha(0.0f),
            gradientX, bounds.getCentreY(),
            juce::Colour(Colors::accent).withAlpha(0.5f),
            gradientX + gradientWidth * 0.5f, bounds.getCentreY(),
            false);
        gradient.addColour(1.0, juce::Colour(Colors::accent).withAlpha(0.0f));

        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds.reduced(2), 6.0f);

        // Draw generating text
        g.setColour(juce::Colour(Colors::text));
        g.setFont(14.0f);
        g.drawText("Generating...", bounds, juce::Justification::centred);
        return;
    }

    if (!hasAudioData || audioBufferCopy.getNumSamples() == 0)
    {
        // No audio - show placeholder
        g.setColour(juce::Colour(Colors::textMuted));
        g.setFont(14.0f);
        g.drawText("No audio generated yet", bounds, juce::Justification::centred);
        return;
    }

    // Draw waveform
    const int numSamples = audioBufferCopy.getNumSamples();
    const int numChannels = audioBufferCopy.getNumChannels();
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
                float val = std::abs(audioBufferCopy.getSample(ch, sampleIndex + i));
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
                float val = std::abs(audioBufferCopy.getSample(ch, sampleIndex + i));
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

    historyScopeLabel.setColour(juce::Label::textColourId, juce::Colour(Colors::text));
    addAndMakeVisible(historyScopeLabel);

    historyScopeCombo.addItem("This Project", 1);
    historyScopeCombo.addItem("All Projects", 2);
    historyScopeCombo.setSelectedId(1);
    addAndMakeVisible(historyScopeCombo);

    clearCacheBtn.onClick = [this]()
    {
        if (onClearCacheCallback)
            onClearCacheCallback();
    };
    addAndMakeVisible(clearCacheBtn);

    saveBtn.onClick = [this]()
    {
        if (onSaveCallback)
        {
            bool showAll = (historyScopeCombo.getSelectedId() == 2);
            onSaveCallback(apiKeyEditor.getText(), showAll);
        }
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
    bool showAllSamples,
    std::function<void(const juce::String&, bool)> onSave,
    std::function<void()> onCancel,
    std::function<void()> onClearCache)
{
    apiKeyEditor.setText(currentApiKey, false);
    historyScopeCombo.setSelectedId(showAllSamples ? 2 : 1, juce::dontSendNotification);
    onSaveCallback = std::move(onSave);
    onCancelCallback = std::move(onCancel);
    onClearCacheCallback = std::move(onClearCache);
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
    auto dialogBounds = getLocalBounds().reduced(40, 80).toFloat();
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
    auto bounds = getLocalBounds().reduced(60, 100);

    bounds.removeFromTop(40);  // Title space

    apiKeyLabel.setBounds(bounds.removeFromTop(25));
    apiKeyEditor.setBounds(bounds.removeFromTop(35));

    bounds.removeFromTop(20);

    historyScopeLabel.setBounds(bounds.removeFromTop(25));
    historyScopeCombo.setBounds(bounds.removeFromTop(30));

    bounds.removeFromTop(20);

    clearCacheBtn.setBounds(bounds.removeFromTop(35).reduced(50, 0));

    bounds.removeFromTop(20);

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

    // History dropdown
    historyDropdown.setTextWhenNothingSelected("Select sample...");
    historyDropdown.onChange = [this]() { onHistorySelectionChanged(); };
    addAndMakeVisible(historyDropdown);
    populateHistoryDropdown();

    // Configure main controls
    generateButton.onClick = [this]()
    {
        if (hasMissingAudio)
        {
            regenerateFromHistory(missingAudioEntry);
        }
        else
        {
            showGenerationDialog();
        }
    };
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

    // Waveform display with scrub and drag support
    waveformDisplay.onScrub = [this](double position)
    {
        if (!processor.hasAudio())
            return;

        double length = processor.getAudioLength();
        if (length > 0)
        {
            processor.setPlaybackPosition(position * length);
            waveformDisplay.setPlaybackPosition(position);
        }
    };
    addAndMakeVisible(waveformDisplay);

    // Update waveform with cached file path if available
    juce::String cachedPath = processor.getCurrentCachedPath();
    if (cachedPath.isNotEmpty())
    {
        waveformDisplay.setCachedFilePath(cachedPath);
        // Select matching history entry
        for (int i = 0; i < historyEntries.size(); ++i)
        {
            if (historyEntries[i].audioFilePath == cachedPath)
            {
                historyDropdown.setSelectedId(i + 1, juce::dontSendNotification);
                currentHistoryId = historyEntries[i].id;
                break;
            }
        }
    }

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(Colors::textMuted));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    errorLabel.setColour(juce::Label::textColourId, juce::Colour(Colors::error));
    errorLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(errorLabel);

    addChildComponent(generationDialog);
    addChildComponent(settingsDialog);

    // Window sizing: min 400x350, default 500x450, resizable
    setResizable(true, true);
    setResizeLimits(400, 350, 1200, 900);
    setSize(500, 450);
    startTimerHz(30);
}

PluginEditor::~PluginEditor()
{
    // Stop waveform animation timer before child components are destroyed
    waveformDisplay.stopTimer();
    stopTimer();
    processor.setStatusCallback(nullptr);
    processor.setGenerationCompleteCallback(nullptr);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colors::background));

    // Scale based on window size
    const float scale = juce::jmin(getWidth() / 500.0f, getHeight() / 450.0f);
    const int headerHeight = static_cast<int>(50 * scale);

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(headerHeight).toFloat();
    g.setColour(juce::Colour(Colors::surface));
    g.fillRect(headerBounds);

    g.setColour(juce::Colour(Colors::text));
    g.setFont(20.0f * scale);
    g.drawText("ElevenLabs Music Generator", headerBounds,
                juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
    const float scale = juce::jmin(bounds.getWidth() / 500.0f, bounds.getHeight() / 450.0f);

    // Header - proportional height
    const int headerHeight = static_cast<int>(50 * scale);
    bounds.removeFromTop(headerHeight);

    // Main content with proportional padding
    const int padding = static_cast<int>(20 * scale);
    bounds = bounds.reduced(padding);

    // History dropdown - fixed height, proportional spacing
    const int dropdownHeight = static_cast<int>(28 * scale);
    historyDropdown.setBounds(bounds.removeFromTop(dropdownHeight));
    bounds.removeFromTop(static_cast<int>(10 * scale));

    // Waveform display - takes available space minus controls
    const int labelHeight = static_cast<int>(25 * scale);
    const int buttonHeight = static_cast<int>(35 * scale);
    const int bottomSpace = labelHeight * 2 + buttonHeight + static_cast<int>(35 * scale);
    int waveformHeight = bounds.getHeight() - bottomSpace;
    waveformHeight = juce::jmax(waveformHeight, static_cast<int>(100 * scale));
    waveformDisplay.setBounds(bounds.removeFromTop(waveformHeight));

    bounds.removeFromTop(static_cast<int>(15 * scale));

    // Status labels
    statusLabel.setBounds(bounds.removeFromTop(labelHeight));
    errorLabel.setBounds(bounds.removeFromTop(labelHeight));

    bounds.removeFromTop(static_cast<int>(10 * scale));

    // Control buttons - proportional sizing
    auto buttonRow = bounds.removeFromTop(buttonHeight);
    const int buttonWidth = static_cast<int>(80 * scale);
    const int generateWidth = static_cast<int>(100 * scale);
    const int spacing = static_cast<int>(10 * scale);

    generateButton.setBounds(buttonRow.removeFromLeft(generateWidth));
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

    // Update waveform if buffer has changed (version-based to avoid redundant copies)
    int currentVersion = processor.getBufferVersion();
    if (currentVersion != lastBufferVersion)
    {
        lastBufferVersion = currentVersion;
        juce::AudioBuffer<float> tempBuffer;
        if (processor.copyAudioBufferTo(tempBuffer))
            waveformDisplay.setAudioBuffer(&tempBuffer);
        else
            waveformDisplay.clearAudioBuffer();
    }

    // Update playback position
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
    bool wasGenerating = isGenerating;
    isGenerating = processor.isGenerating();

    // Trigger animation repaint during generation
    if (isGenerating)
        waveformDisplay.setGenerating(true);
    else if (wasGenerating && !isGenerating)
        waveformDisplay.setGenerating(false);

    generateButton.setEnabled(!isGenerating);

    if (isGenerating)
    {
        generateButton.setButtonText("Generating...");
    }
    else if (hasMissingAudio)
    {
        generateButton.setButtonText("Regenerate");
    }
    else
    {
        generateButton.setButtonText("Generate New");
    }
}

void PluginEditor::populateHistoryDropdown()
{
    historyDropdown.clear();

    // Get history based on scope setting
    bool showAll = processor.getStateSerializer().getShowAllSamples();
    if (showAll)
    {
        historyEntries = processor.getCacheManager().getHistory();
    }
    else
    {
        juce::String projectUuid = processor.getInstanceUuid();
        historyEntries = processor.getCacheManager().getHistoryForProject(projectUuid);
    }

    // Sort by timestamp descending (most recent first)
    std::sort(historyEntries.begin(), historyEntries.end(),
              [](const AudioCacheManager::HistoryEntry& a, const AudioCacheManager::HistoryEntry& b)
              {
                  return a.timestamp > b.timestamp;
              });

    for (int i = 0; i < historyEntries.size(); ++i)
    {
        const auto& entry = historyEntries[i];

        // Format: "Genre (Duration) - Date"
        int durationSec = entry.durationMs / 1000;
        juce::String dateStr = entry.timestamp.toString(true, false);  // Date only

        juce::String itemText = entry.genre + " (" + juce::String(durationSec) + "s) - " + dateStr;
        historyDropdown.addItem(itemText, i + 1);  // IDs start at 1
    }
}

void PluginEditor::onHistorySelectionChanged()
{
    int selectedIdx = historyDropdown.getSelectedId() - 1;
    if (selectedIdx >= 0 && selectedIdx < historyEntries.size())
    {
        // Copy by value to guard against array mutation during use
        auto entry = historyEntries[selectedIdx];

        // Check if audio file exists
        juce::File audioFile(entry.audioFilePath);
        if (!audioFile.existsAsFile())
        {
            // Audio missing - store entry for potential regeneration
            missingAudioEntry = entry;
            hasMissingAudio = true;
            errorLabel.setText("Sample missing. Regenerate?", juce::dontSendNotification);
            statusLabel.setText("", juce::dontSendNotification);
            generateButton.setButtonText("Regenerate");
            waveformDisplay.clearAudioBuffer();
            return;
        }

        // Clear missing audio state
        hasMissingAudio = false;
        generateButton.setButtonText("Generate New");

        // Stop playback before loading new audio
        processor.setPlaying(false);

        // Load the cached audio
        processor.loadAudioFromCache(entry.audioFilePath);

        // Update waveform with a safe copy of the audio buffer
        waveformDisplay.setCachedFilePath(entry.audioFilePath);
        {
            juce::AudioBuffer<float> tempBuffer;
            if (processor.copyAudioBufferTo(tempBuffer))
                waveformDisplay.setAudioBuffer(&tempBuffer);
            else
                waveformDisplay.clearAudioBuffer();
        }
        waveformDisplay.setPlaybackPosition(0.0);
        lastBufferVersion = processor.getBufferVersion();

        currentHistoryId = entry.id;

        statusLabel.setText("Ready to play", juce::dontSendNotification);
        errorLabel.setText("", juce::dontSendNotification);
    }
}

void PluginEditor::regenerateFromHistory(const AudioCacheManager::HistoryEntry& entry)
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
    hasMissingAudio = false;
    generateButton.setButtonText("Generating...");

    // Use the exact same parameters from the history entry
    processor.startGeneration(entry.prompt, entry.genre, entry.durationMs);
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
    bool showAll = processor.getStateSerializer().getShowAllSamples();

    settingsDialog.show(
        currentKey,
        showAll,
        [this](const juce::String& apiKey, bool showAllSamples)
        {
            processor.getStateSerializer().setApiKey(apiKey);
            processor.getStateSerializer().setShowAllSamples(showAllSamples);
            errorLabel.setText("", juce::dontSendNotification);
            // Refresh history dropdown with new scope
            populateHistoryDropdown();
        },
        []() { /* cancelled */ },
        [this]()
        {
            // Clear cache callback
            processor.getCacheManager().clearCache();
            populateHistoryDropdown();
            waveformDisplay.clearAudioBuffer();
            statusLabel.setText("Cache cleared", juce::dontSendNotification);
        });
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
    hasMissingAudio = false;
    generateButton.setButtonText("Generate New");

    if (success)
    {
        statusLabel.setText("Ready to play", juce::dontSendNotification);
        errorLabel.setText("", juce::dontSendNotification);

        // Update waveform with a safe copy of the new audio buffer
        juce::String cachedPath = processor.getCurrentCachedPath();
        waveformDisplay.setCachedFilePath(cachedPath);
        {
            juce::AudioBuffer<float> tempBuffer;
            if (processor.copyAudioBufferTo(tempBuffer))
                waveformDisplay.setAudioBuffer(&tempBuffer);
            else
                waveformDisplay.clearAudioBuffer();
        }
        waveformDisplay.setPlaybackPosition(0.0);
        lastBufferVersion = processor.getBufferVersion();

        // Refresh history dropdown and select the new entry
        populateHistoryDropdown();

        // Find and select the new entry (should be first since sorted by timestamp desc)
        if (!historyEntries.isEmpty())
        {
            for (int i = 0; i < historyEntries.size(); ++i)
            {
                if (historyEntries[i].audioFilePath == cachedPath)
                {
                    historyDropdown.setSelectedId(i + 1, juce::dontSendNotification);
                    currentHistoryId = historyEntries[i].id;
                    break;
                }
            }
        }
    }
    else
    {
        statusLabel.setText("Generation failed", juce::dontSendNotification);
        errorLabel.setText(errorMsg, juce::dontSendNotification);
    }

    repaint();
}
