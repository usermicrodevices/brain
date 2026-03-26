#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "LLMClient.hpp"
#include "HttpClient.hpp"

class DeepDriver : public LLMClient {
public:
    DeepDriver(HttpClient& http);
    ~DeepDriver() override = default;

    bool newSession() override;
    void setSessionId(const std::string& id) override;
    std::string ask(const std::string& prompt) override;

    // Additional configuration
    void setThinkingEnabled(bool enabled);
    void setSearchEnabled(bool enabled);
    void setVerbose(bool verbose);

private:
    HttpClient& http_;
    std::string sessionId_;
    int lastMessageId_;
    bool thinkingEnabled_;
    bool searchEnabled_;
    bool verbose_;

    // Fetch a new session ID from the server
    bool fetchNewSession();

    // Parse the streaming SSE response and extract the final text
    std::string parseStreamingResponse(const std::string& rawResponse);
};
