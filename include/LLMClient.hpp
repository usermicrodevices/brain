#pragma once

#include <string>

class LLMClient {
public:
    virtual ~LLMClient() = default;

    // Start a new conversation session (if needed)
    virtual bool newSession() = 0;

    // Send a prompt and return the answer
    virtual std::string ask(const std::string& prompt) = 0;

    // Set an existing session ID (if applicable)
    virtual void setSessionId(const std::string& id) = 0;
};
