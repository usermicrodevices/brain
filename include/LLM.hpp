#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "OpenCodeDriver.hpp"
#include "OpenaiDriver.hpp"
#include "HttpClient.hpp"
#include "LLMClient.hpp"

class LLM {
public:
    // Provider names: "OpenCode", "OpenAI"
    LLM(const std::string& provider);
    ~LLM();

    // Send a prompt and return the answer
    std::string ask(const std::string& prompt);

    // OpenAI configuration
    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    void setMaxTokens(int tokens);
    void setTemperature(double temp);

    // OpenCode configuration
    void setUrl(const std::string& url);
    void setUsername(const std::string& user);
    void setPassword(const std::string& pass);
    void setAgent(const std::string& agent);
    void setThinkingEffort(const std::string& effort);
    void setWorkingDirectory(const std::string& dir);

    // Generic session management
    bool newSession();
    std::string getSessionId() const;
    void setSessionId(const std::string& id);

private:
    std::unique_ptr<LLMClient> driver_;
};
