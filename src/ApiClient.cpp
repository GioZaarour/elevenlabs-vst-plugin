#include "ApiClient.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

class ApiClient::RequestThread : public juce::Thread
{
public:
    RequestThread(ApiClient& owner)
        : juce::Thread("ElevenLabs API Thread"), owner(owner)
    {
    }

    void startRequest(const juce::String& apiKey,
                      const GenerationRequest& request,
                      CompletionCallback onComplete,
                      ProgressCallback onProgress)
    {
        this->apiKey = apiKey;
        this->request = request;
        this->onComplete = std::move(onComplete);
        this->onProgress = std::move(onProgress);

        startThread();
    }

    void run() override
    {
        owner.requestInProgress.store(true);

        GenerationResult result;
        int retryDelay = kInitialRetryDelayMs;

        for (int attempt = 0; attempt <= kMaxRetries && !threadShouldExit(); ++attempt)
        {
            if (attempt > 0)
            {
                reportProgress(juce::String("Retrying (attempt ") + juce::String(attempt + 1) + ")...");
                juce::Thread::sleep(retryDelay);
                retryDelay *= 2;  // Exponential backoff

                if (threadShouldExit())
                    break;
            }

            result = makeRequest();

            if (result.success || !shouldRetry(result))
                break;
        }

        owner.requestInProgress.store(false);

        // Deliver result on message thread
        auto callback = onComplete;
        auto finalResult = result;

        juce::MessageManager::callAsync([callback, finalResult]()
        {
            if (callback)
                callback(finalResult);
        });
    }

private:
    GenerationResult makeRequest()
    {
        GenerationResult result;

        reportProgress("Connecting to ElevenLabs API...");

        httplib::Client client("https://api.elevenlabs.io");
        client.set_connection_timeout(kConnectionTimeoutSec);
        client.set_read_timeout(kReadTimeoutSec);

        // Build request body
        juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
        bodyObj->setProperty("prompt", buildPrompt());
        bodyObj->setProperty("duration_seconds", request.durationMs / 1000.0);

        auto bodyJson = juce::JSON::toString(juce::var(bodyObj.get()), false);

        // Set headers
        httplib::Headers headers = {
            {"xi-api-key", apiKey.toStdString()},
            {"Content-Type", "application/json"}
        };

        reportProgress("Generating music...");

        auto response = client.Post("/v1/music", headers,
                                     bodyJson.toStdString(),
                                     "application/json");

        if (threadShouldExit())
        {
            result.success = false;
            result.errorMessage = "Request cancelled";
            return result;
        }

        if (!response)
        {
            result.success = false;
            result.errorMessage = "Network error: Could not connect to API";
            result.httpStatusCode = 0;
            return result;
        }

        result.httpStatusCode = response->status;

        if (response->status == 200)
        {
            result.success = true;
            result.audioData.append(response->body.data(), response->body.size());
            reportProgress("Generation complete!");
        }
        else if (response->status == 429)
        {
            result.success = false;
            result.wasRateLimited = true;
            result.errorMessage = "Rate limited. Please wait before trying again.";

            // Check for Retry-After header
            if (response->has_header("Retry-After"))
            {
                result.retryAfterSeconds = std::stoi(response->get_header_value("Retry-After"));
                owner.rateLimitResetTime.store(
                    static_cast<int>(juce::Time::currentTimeMillis() / 1000) + result.retryAfterSeconds);
            }
        }
        else if (response->status == 401)
        {
            result.success = false;
            result.errorMessage = "Invalid API key. Please check your settings.";
        }
        else if (response->status == 400)
        {
            result.success = false;
            result.errorMessage = "Invalid request: " + parseErrorMessage(response->body);
        }
        else
        {
            result.success = false;
            result.errorMessage = "API error (HTTP " + juce::String(response->status) + "): "
                                  + parseErrorMessage(response->body);
        }

        return result;
    }

    juce::String buildPrompt() const
    {
        juce::String fullPrompt = request.prompt;

        if (request.genre.isNotEmpty())
        {
            fullPrompt = request.genre + " style: " + fullPrompt;
        }

        return fullPrompt;
    }

    juce::String parseErrorMessage(const std::string& body) const
    {
        auto parsed = juce::JSON::parse(juce::String(body));
        if (parsed.isObject())
        {
            auto detail = parsed.getProperty("detail", juce::var());
            if (detail.isString())
                return detail.toString();

            auto message = parsed.getProperty("message", juce::var());
            if (message.isString())
                return message.toString();
        }
        return "Unknown error";
    }

    bool shouldRetry(const GenerationResult& result) const
    {
        // Retry on network errors and 5xx server errors
        // Don't retry on 4xx client errors (except rate limiting which is handled separately)
        if (result.httpStatusCode == 0)
            return true;  // Network error

        if (result.httpStatusCode >= 500 && result.httpStatusCode < 600)
            return true;  // Server error

        return false;
    }

    void reportProgress(const juce::String& status)
    {
        if (onProgress)
        {
            auto callback = onProgress;
            juce::MessageManager::callAsync([callback, status]()
            {
                callback(status);
            });
        }
    }

    ApiClient& owner;
    juce::String apiKey;
    GenerationRequest request;
    CompletionCallback onComplete;
    ProgressCallback onProgress;

    static constexpr int kConnectionTimeoutSec = 30;
    static constexpr int kReadTimeoutSec = 120;
    static constexpr int kMaxRetries = 3;
    static constexpr int kInitialRetryDelayMs = 1000;
};

ApiClient::ApiClient()
{
}

ApiClient::~ApiClient()
{
    cancelRequest();

    if (requestThread != nullptr)
    {
        requestThread->stopThread(5000);
    }
}

void ApiClient::generateMusic(const juce::String& apiKey,
                               const GenerationRequest& request,
                               CompletionCallback onComplete,
                               ProgressCallback onProgress)
{
    // Cancel any existing request
    cancelRequest();

    if (requestThread != nullptr)
    {
        requestThread->stopThread(1000);
    }

    cancelFlag.store(false);
    requestThread = std::make_unique<RequestThread>(*this);
    requestThread->startRequest(apiKey, request, std::move(onComplete), std::move(onProgress));
}

void ApiClient::cancelRequest()
{
    cancelFlag.store(true);

    if (requestThread != nullptr && requestThread->isThreadRunning())
    {
        requestThread->signalThreadShouldExit();
    }
}

bool ApiClient::isRequestInProgress() const
{
    return requestInProgress.load();
}

int ApiClient::getSecondsUntilRateLimitReset() const
{
    int resetTime = rateLimitResetTime.load();
    if (resetTime == 0)
        return 0;

    int currentTime = static_cast<int>(juce::Time::currentTimeMillis() / 1000);
    int remaining = resetTime - currentTime;

    return remaining > 0 ? remaining : 0;
}
