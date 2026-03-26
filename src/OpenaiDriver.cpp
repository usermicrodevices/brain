#include "OpenaiDriver.hpp"

using json = nlohmann::json;

OpenaiDriver::OpenaiDriver(HttpClient& http)
    : http_(http), apiKey_(""), model_("gpt-3.5-turbo"), maxTokens_(500), temperature_(0.7) {}

void OpenaiDriver::setApiKey(const std::string& key) { apiKey_ = key; }
void OpenaiDriver::setModel(const std::string& model) { model_ = model; }
void OpenaiDriver::setMaxTokens(int maxTokens) { maxTokens_ = maxTokens; }
void OpenaiDriver::setTemperature(double temperature) { temperature_ = temperature; }

std::string OpenaiDriver::ask(const std::string& prompt) {
    if (apiKey_.empty()) {
        std::cerr << "[OpenaiDriver] API key not set\n";
        return "";
    }

    json payload;
    payload["model"] = model_;
    payload["messages"] = json::array({ {{"role", "user"}, {"content", prompt}} });
    payload["max_tokens"] = maxTokens_;
    payload["temperature"] = temperature_;
    payload["stream"] = false;

    // Add Authorization header
    http_.addHeader("Authorization: Bearer " + apiKey_);

    std::string response = http_.postJson("https://api.openai.com/v1/chat/completions", payload.dump());

    try {
        json resp = json::parse(response);
        if (resp.contains("choices") && resp["choices"].is_array() && !resp["choices"].empty()) {
            auto& choice = resp["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content"))
                return choice["message"]["content"].get<std::string>();
        }
        std::cerr << "[OpenaiDriver] Unexpected response: " << response << "\n";
        return "";
    } catch (const json::parse_error& e) {
        std::cerr << "[OpenaiDriver] JSON parse error: " << e.what() << "\n";
        return "";
    }
}
