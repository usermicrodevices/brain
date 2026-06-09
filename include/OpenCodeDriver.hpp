#pragma once

#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "LLMClient.hpp"
#include "HttpClient.hpp"

class OpenCodeDriver : public LLMClient {
public:
    OpenCodeDriver(HttpClient& http);
    ~OpenCodeDriver() override = default;

    bool newSession() override;
    std::string ask(const std::string& prompt) override;
    void setSessionId(const std::string& id) override;

    void setUrl(const std::string& url);
    void setUsername(const std::string& user);
    void setPassword(const std::string& pass);
    void setModel(const std::string& model);
    void setAgent(const std::string& agent);
    void setThinkingEffort(const std::string& effort);
    void setWorkingDirectory(const std::string& dir);

    std::string getSessionId() const;

private:
    HttpClient& http_;
    std::string baseUrl_;
    std::string username_;
    std::string password_;
    std::string sessionId_;
    std::string providerID_;
    std::string modelID_;
    std::string agent_;
    std::string thinkingEffort_;
    std::string workingDirectory_;

    std::string buildAuthHeader() const;
    bool switchProject();
};
