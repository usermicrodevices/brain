#include "LLM.hpp"

LLM::LLM(const std::string& provider) {
    static HttpClient http;
    if (provider == "OpenCode") {
        driver_ = std::make_unique<OpenCodeDriver>(http);
    } else if (provider == "OpenAI") {
        driver_ = std::make_unique<OpenaiDriver>(http);
    } else {
        throw std::runtime_error("Unknown LLM provider: " + provider);
    }
}

LLM::~LLM() = default;

std::string LLM::ask(const std::string& prompt) {
    return driver_->ask(prompt);
}

void LLM::setApiKey(const std::string& key) {
    auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get());
    if (openai) openai->setApiKey(key);
}

void LLM::setModel(const std::string& model) {
    if (auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get())) {
        openai->setModel(model);
    } else if (auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get())) {
        oc->setModel(model);
    }
}

void LLM::setMaxTokens(int tokens) {
    auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get());
    if (openai) openai->setMaxTokens(tokens);
}

void LLM::setTemperature(double temp) {
    auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get());
    if (openai) openai->setTemperature(temp);
}

void LLM::setUrl(const std::string& url) {
    auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get());
    if (oc) oc->setUrl(url);
}

void LLM::setUsername(const std::string& user) {
    auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get());
    if (oc) oc->setUsername(user);
}

void LLM::setPassword(const std::string& pass) {
    auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get());
    if (oc) oc->setPassword(pass);
}

void LLM::setAgent(const std::string& agent) {
    auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get());
    if (oc) oc->setAgent(agent);
}

void LLM::setThinkingEffort(const std::string& effort) {
    auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get());
    if (oc) oc->setThinkingEffort(effort);
}

void LLM::setWorkingDirectory(const std::string& dir) {
    auto* oc = dynamic_cast<OpenCodeDriver*>(driver_.get());
    if (oc) oc->setWorkingDirectory(dir);
}

bool LLM::newSession() {
    return driver_->newSession();
}

std::string LLM::getSessionId() const {
    auto* oc = dynamic_cast<const OpenCodeDriver*>(driver_.get());
    if (oc) return oc->getSessionId();
    return "";
}

void LLM::setSessionId(const std::string& id) {
    driver_->setSessionId(id);
}
