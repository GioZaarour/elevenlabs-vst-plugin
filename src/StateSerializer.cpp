#include "StateSerializer.h"

StateSerializer::StateSerializer()
{
    loadGlobalConfig();
}

juce::File StateSerializer::getConfigDirectory() const
{
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ElevenLabsVST");
#elif JUCE_WINDOWS
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ElevenLabsVST");
#else
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".elevenlabs-vst");
#endif
}

juce::File StateSerializer::getGlobalConfigFile() const
{
    return getConfigDirectory().getChildFile("config.json");
}

juce::File StateSerializer::getCacheDirectory(const juce::File& projectDir) const
{
    return projectDir.getChildFile("ElevenLabsCache");
}

void StateSerializer::loadGlobalConfig()
{
    juce::ScopedLock lock(configLock);

    auto configFile = getGlobalConfigFile();
    if (configFile.existsAsFile())
    {
        auto content = configFile.loadFileAsString();
        globalConfig = juce::JSON::parse(content);
    }
    else
    {
        globalConfig = juce::var(new juce::DynamicObject());
    }
}

void StateSerializer::saveGlobalConfig()
{
    juce::ScopedLock lock(configLock);

    auto configFile = getGlobalConfigFile();
    configFile.getParentDirectory().createDirectory();

    auto json = juce::JSON::toString(globalConfig, true);
    configFile.replaceWithText(json);
}

void StateSerializer::setApiKey(const juce::String& key)
{
    juce::ScopedLock lock(configLock);

    if (auto* obj = globalConfig.getDynamicObject())
    {
        obj->setProperty("apiKey", key);
        saveGlobalConfig();
    }
}

juce::String StateSerializer::getApiKey() const
{
    juce::ScopedLock lock(configLock);
    return globalConfig.getProperty("apiKey", "").toString();
}

void StateSerializer::setLastGenre(const juce::String& genre)
{
    juce::ScopedLock lock(configLock);

    if (auto* obj = globalConfig.getDynamicObject())
    {
        obj->setProperty("lastGenre", genre);
        saveGlobalConfig();
    }
}

juce::String StateSerializer::getLastGenre() const
{
    juce::ScopedLock lock(configLock);
    return globalConfig.getProperty("lastGenre", "pop").toString();
}

void StateSerializer::setLastDuration(int durationMs)
{
    juce::ScopedLock lock(configLock);

    if (auto* obj = globalConfig.getDynamicObject())
    {
        obj->setProperty("lastDurationMs", durationMs);
        saveGlobalConfig();
    }
}

int StateSerializer::getLastDuration() const
{
    juce::ScopedLock lock(configLock);
    return globalConfig.getProperty("lastDurationMs", 30000);
}

void StateSerializer::savePluginState(juce::MemoryBlock& destData, const PluginState& state)
{
    juce::DynamicObject::Ptr stateObj = new juce::DynamicObject();
    stateObj->setProperty("version", kCurrentStateVersion);
    stateObj->setProperty("prompt", state.prompt);
    stateObj->setProperty("genre", state.genre);
    stateObj->setProperty("durationMs", state.durationMs);
    stateObj->setProperty("cachedAudioPath", state.cachedAudioPath);
    stateObj->setProperty("generationId", state.generationId);

    auto json = juce::JSON::toString(juce::var(stateObj.get()), false);
    destData.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

StateSerializer::PluginState StateSerializer::loadPluginState(const void* data, int sizeInBytes)
{
    PluginState state;

    if (data == nullptr || sizeInBytes <= 0)
        return state;

    juce::String json(static_cast<const char*>(data), static_cast<size_t>(sizeInBytes));
    auto parsed = juce::JSON::parse(json);

    if (parsed.isObject())
    {
        state.prompt = parsed.getProperty("prompt", "").toString();
        state.genre = parsed.getProperty("genre", "").toString();
        state.durationMs = parsed.getProperty("durationMs", 30000);
        state.cachedAudioPath = parsed.getProperty("cachedAudioPath", "").toString();
        state.generationId = parsed.getProperty("generationId", "").toString();
    }

    return state;
}
