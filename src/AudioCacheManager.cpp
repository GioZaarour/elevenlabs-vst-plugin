#include "AudioCacheManager.h"

AudioCacheManager::AudioCacheManager()
{
    formatManager.registerBasicFormats();

    // Set default cache directory
    cacheDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("ElevenLabsVST")
                         .getChildFile("Cache");
    cacheDirectory.createDirectory();

    loadHistory();
}

void AudioCacheManager::setCacheDirectory(const juce::File& directory)
{
    cacheDirectory = directory;
    cacheDirectory.createDirectory();
    loadHistory();
}

juce::File AudioCacheManager::getCacheDirectory() const
{
    return cacheDirectory;
}

juce::String AudioCacheManager::generateUniqueId() const
{
    return juce::Uuid().toString();
}

juce::String AudioCacheManager::cacheAudio(const juce::MemoryBlock& mp3Data,
                                            const juce::String& prompt,
                                            const juce::String& genre,
                                            int durationMs,
                                            const juce::String& projectUuid)
{
    // Create a memory input stream from the MP3 data
    auto inputStream = std::make_unique<juce::MemoryInputStream>(mp3Data, false);

    // Try to create an audio reader for the MP3 data
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(std::move(inputStream)));

    if (reader == nullptr)
    {
        DBG("Failed to create audio reader for MP3 data");
        return {};
    }

    // Generate unique ID and file path
    juce::String id = generateUniqueId();
    juce::File wavFile = cacheDirectory.getChildFile(id + ".wav");

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(wavFile.createOutputStream());

    if (outputStream == nullptr)
    {
        DBG("Failed to create output stream for WAV file");
        return {};
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(),
                                   kDefaultSampleRate,
                                   reader->numChannels,
                                   kDefaultBitDepth,
                                   {},
                                   0));

    if (writer == nullptr)
    {
        DBG("Failed to create WAV writer");
        return {};
    }

    outputStream.release();  // Writer now owns the stream

    // If sample rates differ, we need to resample
    if (std::abs(reader->sampleRate - kDefaultSampleRate) > 0.1)
    {
        // Read entire audio into buffer
        juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),
                                         static_cast<int>(reader->lengthInSamples));
        reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        // Simple resampling using JUCE's interpolator
        double ratio = reader->sampleRate / kDefaultSampleRate;
        int newLength = static_cast<int>(reader->lengthInSamples / ratio);

        juce::AudioBuffer<float> resampledBuffer(static_cast<int>(reader->numChannels), newLength);

        for (int channel = 0; channel < static_cast<int>(reader->numChannels); ++channel)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process(ratio,
                                  buffer.getReadPointer(channel),
                                  resampledBuffer.getWritePointer(channel),
                                  newLength);
        }

        writer->writeFromAudioSampleBuffer(resampledBuffer, 0, newLength);
    }
    else
    {
        // Direct copy - same sample rate
        writer->writeFromAudioReader(*reader, 0, reader->lengthInSamples);
    }

    writer.reset();  // Flush and close

    // Add to history
    HistoryEntry entry;
    entry.id = id;
    entry.prompt = prompt;
    entry.genre = genre;
    entry.durationMs = durationMs;
    entry.audioFilePath = wavFile.getFullPathName();
    entry.timestamp = juce::Time::getCurrentTime();
    entry.projectUuid = projectUuid;

    {
        juce::ScopedLock lock(historyLock);
        history.add(entry);
        saveHistory();
    }

    return wavFile.getFullPathName();
}

std::unique_ptr<juce::AudioBuffer<float>> AudioCacheManager::loadCachedAudio(
    const juce::String& filePath,
    double targetSampleRate)
{
    juce::File audioFile(filePath);
    if (!audioFile.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(audioFile));

    if (reader == nullptr)
        return nullptr;

    // Read the audio
    auto buffer = std::make_unique<juce::AudioBuffer<float>>(
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples));

    reader->read(buffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    // Resample if necessary
    if (std::abs(reader->sampleRate - targetSampleRate) > 0.1)
    {
        double ratio = reader->sampleRate / targetSampleRate;
        int newLength = static_cast<int>(reader->lengthInSamples / ratio);

        auto resampledBuffer = std::make_unique<juce::AudioBuffer<float>>(
            buffer->getNumChannels(), newLength);

        for (int channel = 0; channel < buffer->getNumChannels(); ++channel)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process(ratio,
                                  buffer->getReadPointer(channel),
                                  resampledBuffer->getWritePointer(channel),
                                  newLength);
        }

        return resampledBuffer;
    }

    return buffer;
}

std::unique_ptr<juce::AudioFormatReader> AudioCacheManager::getAudioReader(const juce::String& filePath)
{
    juce::File audioFile(filePath);
    if (!audioFile.existsAsFile())
        return nullptr;

    return std::unique_ptr<juce::AudioFormatReader>(
        formatManager.createReaderFor(audioFile));
}

juce::Array<AudioCacheManager::HistoryEntry> AudioCacheManager::getHistory() const
{
    juce::ScopedLock lock(historyLock);
    return history;
}

juce::Array<AudioCacheManager::HistoryEntry> AudioCacheManager::getHistoryForProject(
    const juce::String& projectUuid) const
{
    juce::ScopedLock lock(historyLock);

    if (projectUuid.isEmpty())
        return history;

    juce::Array<HistoryEntry> filtered;
    for (const auto& entry : history)
    {
        if (entry.projectUuid == projectUuid)
            filtered.add(entry);
    }
    return filtered;
}

void AudioCacheManager::clearHistory()
{
    juce::ScopedLock lock(historyLock);
    history.clear();
    saveHistory();
}

void AudioCacheManager::removeHistoryEntry(const juce::String& id)
{
    juce::ScopedLock lock(historyLock);

    for (int i = 0; i < history.size(); ++i)
    {
        if (history[i].id == id)
        {
            // Delete the audio file
            juce::File audioFile(history[i].audioFilePath);
            audioFile.deleteFile();

            history.remove(i);
            saveHistory();
            break;
        }
    }
}

void AudioCacheManager::clearCache()
{
    juce::ScopedLock lock(historyLock);

    // Delete all cached audio files
    for (const auto& entry : history)
    {
        juce::File audioFile(entry.audioFilePath);
        audioFile.deleteFile();
    }

    history.clear();
    saveHistory();
}

juce::int64 AudioCacheManager::getCacheSizeBytes() const
{
    juce::int64 totalSize = 0;

    juce::ScopedLock lock(historyLock);
    for (const auto& entry : history)
    {
        juce::File audioFile(entry.audioFilePath);
        if (audioFile.existsAsFile())
            totalSize += audioFile.getSize();
    }

    return totalSize;
}

juce::File AudioCacheManager::getHistoryFile() const
{
    return cacheDirectory.getChildFile("history.json");
}

void AudioCacheManager::loadHistory()
{
    juce::ScopedLock lock(historyLock);
    history.clear();

    auto historyFile = getHistoryFile();
    if (!historyFile.existsAsFile())
        return;

    // Acquire inter-process lock for file-level safety across plugin instances
    if (!historyFileLock.enter(2000))
    {
        DBG("Failed to acquire history file lock for reading");
        return;
    }

    auto content = historyFile.loadFileAsString();
    historyFileLock.exit();

    auto parsed = juce::JSON::parse(content);

    if (auto* arr = parsed.getArray())
    {
        for (const auto& item : *arr)
        {
            HistoryEntry entry;
            entry.id = item.getProperty("id", "").toString();
            entry.prompt = item.getProperty("prompt", "").toString();
            entry.genre = item.getProperty("genre", "").toString();
            entry.durationMs = item.getProperty("durationMs", 0);
            entry.audioFilePath = item.getProperty("audioFilePath", "").toString();
            entry.timestamp = juce::Time(static_cast<juce::int64>(
                item.getProperty("timestamp", 0)));
            entry.projectUuid = item.getProperty("projectUuid", "").toString();

            // Only add if the audio file still exists
            if (juce::File(entry.audioFilePath).existsAsFile())
                history.add(entry);
        }
    }
}

void AudioCacheManager::saveHistory()
{
    juce::Array<juce::var> arr;

    for (const auto& entry : history)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("id", entry.id);
        obj->setProperty("prompt", entry.prompt);
        obj->setProperty("genre", entry.genre);
        obj->setProperty("durationMs", entry.durationMs);
        obj->setProperty("audioFilePath", entry.audioFilePath);
        obj->setProperty("timestamp", entry.timestamp.toMilliseconds());
        obj->setProperty("projectUuid", entry.projectUuid);

        arr.add(juce::var(obj.get()));
    }

    auto json = juce::JSON::toString(juce::var(arr), true);

    // Acquire inter-process lock for file-level safety across plugin instances
    if (historyFileLock.enter(2000))
    {
        // Atomic write: write to temp file, then rename
        auto tempFile = getHistoryFile().getSiblingFile(".history.json.tmp");
        if (tempFile.replaceWithText(json))
            tempFile.moveFileTo(getHistoryFile());
        historyFileLock.exit();
    }
    else
    {
        DBG("Failed to acquire history file lock for writing");
    }
}
