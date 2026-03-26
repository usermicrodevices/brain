#include "DeepDriver.hpp"

using json = nlohmann::json;

DeepDriver::DeepDriver(HttpClient& http)
    : http_(http), sessionId_(""), lastMessageId_(0), thinkingEnabled_(true), searchEnabled_(true), verbose_(false) {}

bool DeepDriver::newSession() {
    return fetchNewSession();
}

void DeepDriver::setSessionId(const std::string& id) {
    sessionId_ = id;
    lastMessageId_ = 0;
}

void DeepDriver::setThinkingEnabled(bool enabled) { thinkingEnabled_ = enabled; }
void DeepDriver::setSearchEnabled(bool enabled) { searchEnabled_ = enabled; }
void DeepDriver::setVerbose(bool verbose) { verbose_ = verbose; }

std::string DeepDriver::ask(const std::string& prompt) {
    if (sessionId_.empty()) {
        if (!fetchNewSession())
            return "";
    }

    json payload;
    payload["chat_session_id"] = sessionId_;
    payload["parent_message_id"] = lastMessageId_;
    payload["prompt"] = prompt;
    payload["ref_file_ids"] = json::array();
    payload["thinking_enabled"] = thinkingEnabled_;
    payload["search_enabled"] = searchEnabled_;
    payload["preempt"] = false;

    std::string response = http_.postJson("https://chat.deepseek.com/api/chat", payload.dump());
    if (response.empty()) {
        std::cerr << "[DeepDriver] No response from server\n";
        return "";
    }

    std::string answer = parseStreamingResponse(response);
    if (answer.empty()) {
        std::cerr << "[DeepDriver] Failed to parse answer\n";
    }

    // Increment message ID – in a real implementation you'd extract the actual ID.
    lastMessageId_++;
    return answer;
}

bool DeepDriver::fetchNewSession() {
    http_.addHeader("Content-Type: application/json");
    std::string response = http_.get("https://chat.deepseek.com/api/chat/new");
    if (response.empty()) {
        std::cerr << "[DeepDriver] Failed to fetch new session\n";
        return false;
    }

    try {
        json j = json::parse(response);
        if (j.contains("chat_session_id")) {
            sessionId_ = j["chat_session_id"].get<std::string>();
            lastMessageId_ = 0;
            if (verbose_)
                std::cout << "[DeepDriver] New session ID: " << sessionId_ << "\n";
            return true;
        } else {
            std::cerr << "[DeepDriver] Response missing chat_session_id: " << response << "\n";
            return false;
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[DeepDriver] JSON parse error in session response: " << e.what() << "\n";
        return false;
    }
}

std::string DeepDriver::parseStreamingResponse(const std::string& rawResponse) {
    std::stringstream ss(rawResponse);
    std::string line;
    std::string answer;
    bool isMessageComplete = false;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("data: ", 0) == 0) {
            std::string jsonStr = line.substr(6);
            if (jsonStr.empty())
                continue;

            try {
                json chunk = json::parse(jsonStr);
                std::string type = chunk.value("type", "");

                if (type == "text") {
                    if (chunk.contains("data"))
                        answer += chunk["data"].get<std::string>();
                } else if (type == "message") {
                    if (chunk.contains("data") && chunk["data"].is_string()) {
                        answer = chunk["data"].get<std::string>();
                        isMessageComplete = true;
                        break;
                    }
                } else if (type == "error") {
                    std::cerr << "[DeepDriver] Error in stream: " << chunk.dump() << "\n";
                    return "";
                }
            } catch (const json::parse_error& e) {
                if (verbose_)
                    std::cerr << "[DeepDriver] Failed to parse chunk: " << jsonStr << "\n";
            }
        }
    }

    if (!isMessageComplete && answer.empty())
        std::cerr << "[DeepDriver] No answer found in stream\n";
    return answer;
}
