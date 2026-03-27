#pragma once

#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "LLMClient.hpp"
#include "HttpClient.hpp"

class OpenaiDriver : public LLMClient {
public:
    OpenaiDriver(HttpClient& http);
    ~OpenaiDriver() override = default;

    // OpenAI doesn't require session creation (stateless API)
    bool newSession() override { return true; }

    void setSessionId(const std::string& id) override { /* ignored */ }

    std::string ask(const std::string& prompt) override;

    void setApiKey(const std::string& key);
    void setModel(const std::string& model);
    void setMaxTokens(int maxTokens);
    void setTemperature(double temperature);

private:
    HttpClient& http_;
    std::string apiKey_;
    std::string model_;
    int maxTokens_;
    double temperature_;
};
