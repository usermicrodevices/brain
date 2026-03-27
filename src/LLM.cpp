#include "LLM.hpp"

LLM::LLM(const std::string& provider) {
    // HttpClient is shared across all drivers – we create a single instance.
    static HttpClient http;  // one global HTTP client for all LLMs
    if (provider == "DeepSeek") {
        driver_ = std::make_unique<DeepDriver>(http);
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
    else throw std::runtime_error("setApiKey only available for OpenAI");
}

void LLM::setModel(const std::string& model) {
    auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get());
    if (openai) openai->setModel(model);
    else throw std::runtime_error("setModel only available for OpenAI");
}

void LLM::setMaxTokens(int tokens) {
    auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get());
    if (openai) openai->setMaxTokens(tokens);
    else throw std::runtime_error("setMaxTokens only available for OpenAI");
}

void LLM::setTemperature(double temp) {
    auto* openai = dynamic_cast<OpenaiDriver*>(driver_.get());
    if (openai) openai->setTemperature(temp);
    else throw std::runtime_error("setTemperature only available for OpenAI");
}

void LLM::setSessionId(const std::string& id) {
    if (driver_->getName() == "DeepSeek"){
        auto* deep = dynamic_cast<DeepDriver*>(driver_.get());
        if (deep) deep->setSessionId(id);
        else throw std::runtime_error("setSessionId dynamic_cast DeepSeek");
    }
}

void LLM::setThinkingEnabled(bool enabled) {
    if (driver_->getName() == "DeepSeek"){
        auto* deep = dynamic_cast<DeepDriver*>(driver_.get());
        if (deep) deep->setThinkingEnabled(enabled);
        else throw std::runtime_error("setThinkingEnabled dynamic_cast DeepSeek");
    }
}

void LLM::setSearchEnabled(bool enabled) {
    if (driver_->getName() == "DeepSeek"){
        auto* deep = dynamic_cast<DeepDriver*>(driver_.get());
        if (deep) deep->setSearchEnabled(enabled);
        else throw std::runtime_error("setSearchEnabled dynamic_cast DeepSeek");
    }
}

void LLM::setVerbose(bool verbose) {
    if (driver_->getName() == "DeepSeek"){
        auto* deep = dynamic_cast<DeepDriver*>(driver_.get());
        if (deep) deep->setVerbose(verbose);
        else throw std::runtime_error("setVerbose dynamic_cast DeepSeek");
    }
}

bool LLM::newSession() {
    return driver_->newSession();
}

std::string LLM::getSessionId() const {
    if (driver_->getName() == "DeepSeek"){
        auto* deep = dynamic_cast<DeepDriver*>(driver_.get());
        if (deep) return deep->getSessionId();
    }
    return "";
}
