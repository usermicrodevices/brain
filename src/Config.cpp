#include "Config.hpp"

static bool createDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return true;
        else
            return false;
    }
    return mkdir(path.c_str(), 0755) == 0;
}

void Config::ensureDirectoryExists() const {
    size_t lastSlash = filePath_.find_last_of('/');
    if (lastSlash == std::string::npos)
        return;
    std::string dir = filePath_.substr(0, lastSlash);
    if (dir.empty())
        return;
    if (!createDirectory(dir)) {
        std::cerr << "[Config] Failed to create directory: " << dir << std::endl;
    }
}

Config::Config(const std::string& filePath) : filePath_(filePath) {
    ensureDirectoryExists();

    std::ifstream test(filePath_);
    if (!test.is_open()) {
        data_ = nlohmann::json::object();
        data_["llm"]["provider"] = "OpenCode";
        data_["llm"]["opencode"]["url"] = "http://127.0.0.1:4096";
        data_["llm"]["opencode"]["username"] = "opencode";
        data_["llm"]["opencode"]["password"] = "";
        data_["llm"]["opencode"]["model"] = "";
        data_["llm"]["opencode"]["agent"] = "build";
        data_["llm"]["opencode"]["thinking_effort"] = "";
        data_["llm"]["opencode"]["working_directory"] = ".";
        data_["llm"]["openai"]["api_key"] = "";
        data_["llm"]["openai"]["model"] = "gpt-3.5-turbo";
        data_["llm"]["openai"]["max_tokens"] = 500;
        data_["llm"]["openai"]["temperature"] = 0.7;
        data_["evolution"]["num_workers"] = 4;
        data_["evolution"]["max_retries"] = 5;
        data_["evolution"]["max_generations"] = 1000;
        data_["evolution"]["sleep_ms"] = 100;
        data_["global_stop"]["listen_port"] = 9999;
        data_["global_stop"]["enabled"] = true;
        data_["scheduler"]["expression"] = "0 0 * * *";
        save();
    } else {
        test.close();
        reload();
    }
}

bool Config::reload() {
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        data_ = nlohmann::json::object();
        return true;
    }
    try {
        file >> data_;
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[Config] Failed to parse " << filePath_ << ": " << e.what() << std::endl;
        data_ = nlohmann::json::object();
        return false;
    }
}

bool Config::save() const {
    std::ofstream file(filePath_);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to write " << filePath_ << std::endl;
        return false;
    }
    file << data_.dump(4);
    return true;
}

bool Config::hasKey(const std::string& key) const {
    return getNode(key) != nullptr;
}

void Config::removeKey(const std::string& key) {
    if (key.empty()) return;
    std::vector<std::string> parts;
    std::stringstream ss(key);
    std::string part;
    while (std::getline(ss, part, '.'))
        parts.push_back(part);

    nlohmann::json* current = &data_;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->is_object() || !current->contains(parts[i]))
            return;
        current = &(*current)[parts[i]];
    }
    if (current->is_object() && current->contains(parts.back()))
        current->erase(parts.back());
    save();
}

nlohmann::json* Config::getNode(const std::string& key, bool createIfMissing) {
    if (key.empty()) return &data_;
    std::vector<std::string> parts;
    std::stringstream ss(key);
    std::string part;
    while (std::getline(ss, part, '.'))
        parts.push_back(part);

    nlohmann::json* current = &data_;
    for (const auto& p : parts) {
        if (!current->is_object()) {
            if (createIfMissing)
                *current = nlohmann::json::object();
            else
                return nullptr;
        }
        if (!current->contains(p)) {
            if (createIfMissing)
                (*current)[p] = nlohmann::json::object();
            else
                return nullptr;
        }
        current = &(*current)[p];
    }
    return current;
}

const nlohmann::json* Config::getNode(const std::string& key) const {
    if (key.empty()) return &data_;
    std::vector<std::string> parts;
    std::stringstream ss(key);
    std::string part;
    while (std::getline(ss, part, '.'))
        parts.push_back(part);

    const nlohmann::json* current = &data_;
    for (const auto& p : parts) {
        if (!current->is_object() || !current->contains(p))
            return nullptr;
        current = &(*current)[p];
    }
    return current;
}
