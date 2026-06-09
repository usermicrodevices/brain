#include "OpenCodeDriver.hpp"

using json = nlohmann::json;

OpenCodeDriver::OpenCodeDriver(HttpClient& http)
    : http_(http),
      baseUrl_("http://127.0.0.1:4096"),
      username_("opencode"),
      password_(""),
      providerID_(""),
      modelID_(""),
      agent_("build"),
      thinkingEffort_(""),
      workingDirectory_(".")
{
    setName("OpenCode");
}

void OpenCodeDriver::setUrl(const std::string& url) { baseUrl_ = url; }
void OpenCodeDriver::setUsername(const std::string& user) { username_ = user; }
void OpenCodeDriver::setPassword(const std::string& pass) { password_ = pass; }
void OpenCodeDriver::setAgent(const std::string& agent) { agent_ = agent; }
void OpenCodeDriver::setThinkingEffort(const std::string& effort) { thinkingEffort_ = effort; }
void OpenCodeDriver::setWorkingDirectory(const std::string& dir) { workingDirectory_ = dir; }

void OpenCodeDriver::setModel(const std::string& model) {
    size_t slash = model.find('/');
    if (slash != std::string::npos) {
        providerID_ = model.substr(0, slash);
        modelID_ = model.substr(slash + 1);
    } else {
        providerID_ = "";
        modelID_ = model;
    }
}

void OpenCodeDriver::setSessionId(const std::string& id) { sessionId_ = id; }
std::string OpenCodeDriver::getSessionId() const { return sessionId_; }

std::string OpenCodeDriver::buildAuthHeader() const {
    if (username_.empty()) return "";
    std::string credentials = username_ + ":" + password_;
    // Base64 encode
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : credentials) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    while (encoded.size() % 4) encoded.push_back('=');
    return "Basic " + encoded;
}

bool OpenCodeDriver::switchProject() {
    http_.addHeader("Content-Type: application/json");
    std::string auth = buildAuthHeader();
    if (!auth.empty()) http_.addHeader("Authorization: " + auth);

    json body;
    body["directory"] = workingDirectory_;

    std::string response = http_.postJson(baseUrl_ + "/config", body.dump());
    return !response.empty();
}

bool OpenCodeDriver::newSession() {
    switchProject();

    http_.addHeader("Content-Type: application/json");
    std::string auth = buildAuthHeader();
    if (!auth.empty()) http_.addHeader("Authorization: " + auth);

    json body;
    body["title"] = "Brain Analysis";

    std::string response = http_.postJson(baseUrl_ + "/session", body.dump());
    if (response.empty()) {
        std::cerr << "[OpenCodeDriver] Failed to create session\n";
        return false;
    }

    try {
        json j = json::parse(response);
        if (j.contains("id")) {
            sessionId_ = j["id"].get<std::string>();
            return true;
        }
        std::cerr << "[OpenCodeDriver] Response missing id: " << response << "\n";
        return false;
    } catch (const json::parse_error& e) {
        std::cerr << "[OpenCodeDriver] JSON parse error: " << e.what() << "\n";
        return false;
    }
}

std::string OpenCodeDriver::ask(const std::string& prompt) {
    if (sessionId_.empty()) {
        if (!newSession()) return "";
    }

    http_.addHeader("Content-Type: application/json");
    std::string auth = buildAuthHeader();
    if (!auth.empty()) http_.addHeader("Authorization: " + auth);

    json payload;
    payload["parts"] = json::array({ {{"type", "text"}, {"text", prompt}} });
    if (!providerID_.empty() && !modelID_.empty()) {
        payload["model"] = { {"providerID", providerID_}, {"modelID", modelID_} };
    }
    if (!agent_.empty()) {
        payload["agent"] = agent_;
    }
    if (!thinkingEffort_.empty()) {
        payload["system"] = "Thinking effort: " + thinkingEffort_;
    }

    std::string url = baseUrl_ + "/session/" + sessionId_ + "/message";
    std::string response = http_.postJson(url, payload.dump());
    if (response.empty()) {
        std::cerr << "[OpenCodeDriver] No response from server\n";
        return "";
    }

    try {
        json j = json::parse(response);
        std::string result;
        if (j.contains("parts") && j["parts"].is_array()) {
            for (const auto& part : j["parts"]) {
                if (part.contains("type") && part["type"] == "text") {
                    result += part.value("text", "");
                }
            }
        }
        if (result.empty()) {
            std::cerr << "[OpenCodeDriver] No text in response: " << response << "\n";
        }
        return result;
    } catch (const json::parse_error& e) {
        std::cerr << "[OpenCodeDriver] JSON parse error: " << e.what() << "\n";
        return "";
    }
}
