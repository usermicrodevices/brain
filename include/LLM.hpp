#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "DeepDriver.hpp"
#include "OpenaiDriver.hpp"
#include "HttpClient.hpp"
#include "LLMClient.hpp"

class LLM {
public:
    // Provider names: "DeepSeek", "OpenAI"
    LLM(const std::string& provider);
    ~LLM();

    // Send a prompt and return the answer
    std::string ask(const std::string& prompt);

    // Configuration methods (forwarded to the underlying driver)
    void setApiKey(const std::string& key);          // for OpenAI
    void setModel(const std::string& model);         // for OpenAI
    void setMaxTokens(int tokens);                   // for OpenAI
    void setTemperature(double temp);                // for OpenAI

    // DeepSeek specific
    void setSessionId(const std::string& id);
    void setThinkingEnabled(bool enabled);
    void setSearchEnabled(bool enabled);
    void setVerbose(bool verbose);

    // Generic session management
    bool newSession();

private:
    std::unique_ptr<LLMClient> driver_;
};
