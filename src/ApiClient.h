#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <atomic>
#include <memory>

/**
 * ApiClient handles communication with the ElevenLabs Music API.
 * All network operations run on a background thread.
 * Results are delivered via MessageManager::callAsync to the message thread.
 */
class ApiClient
{
public:
    ApiClient();
    ~ApiClient();

    // Request parameters
    struct GenerationRequest
    {
        juce::String prompt;
        int durationMs = 30000;
        juce::String genre;  // Optional genre hint
    };

    // Result types
    struct GenerationResult
    {
        bool success = false;
        juce::MemoryBlock audioData;  // MP3 binary data
        juce::String errorMessage;
        int httpStatusCode = 0;
        bool wasRateLimited = false;
        int retryAfterSeconds = 0;
    };

    // Callback type - called on message thread
    using CompletionCallback = std::function<void(const GenerationResult&)>;
    using ProgressCallback = std::function<void(const juce::String& status)>;

    // Start a generation request (non-blocking)
    void generateMusic(const juce::String& apiKey,
                       const GenerationRequest& request,
                       CompletionCallback onComplete,
                       ProgressCallback onProgress = nullptr);

    // Cancel any in-progress request
    void cancelRequest();

    // Check if a request is currently in progress
    bool isRequestInProgress() const;

    // Rate limit info
    int getSecondsUntilRateLimitReset() const;

private:
    class RequestThread;
    std::unique_ptr<RequestThread> requestThread;

    std::atomic<bool> cancelFlag{false};
    std::atomic<bool> requestInProgress{false};
    std::atomic<int> rateLimitResetTime{0};

    static constexpr int kMaxRetries = 3;
    static constexpr int kInitialRetryDelayMs = 1000;
    static constexpr int kConnectionTimeoutSec = 30;
    static constexpr int kReadTimeoutSec = 120;  // Generation can take time

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApiClient)
};
