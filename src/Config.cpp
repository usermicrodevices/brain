#include "Config.hpp"

// Helper to create a directory if it doesn't exist
static bool createDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return true; // already exists
            else
                return false; // exists but is not a directory
    }
    // Create with 0755 permissions
    return mkdir(path.c_str(), 0755) == 0;
}

void Config::ensureDirectoryExists() const {
    // Extract directory part from filePath_
    size_t lastSlash = filePath_.find_last_of('/');
    if (lastSlash == std::string::npos)
        return; // no directory, assume current
        std::string dir = filePath_.substr(0, lastSlash);
    if (dir.empty())
        return;
    if (!createDirectory(dir)) {
        std::cerr << "[Config] Failed to create directory: " << dir << std::endl;
    }
}

Config::Config(const std::string& filePath) : filePath_(filePath) {
    ensureDirectoryExists();

    // Check if file exists; if not, create it with default content
    std::ifstream test(filePath_);
    if (!test.is_open()) {
        // File doesn't exist, create it with default JSON
        data_ = nlohmann::json::object();
        // Populate with default values (you can customize)
        data_["llm"]["provider"] = "DeepSeek";
        data_["llm"]["deepseek"]["session_id"] = "";
        data_["llm"]["deepseek"]["thinking_enabled"] = true;
        data_["llm"]["deepseek"]["search_enabled"] = true;
        data_["llm"]["openai"]["api_key"] = "";
        data_["llm"]["openai"]["model"] = "gpt-3.5-turbo";
        data_["llm"]["openai"]["max_tokens"] = 500;
        data_["llm"]["openai"]["temperature"] = 0.7;
        data_["evolution"]["max_generations"] = 1000;
        data_["evolution"]["sleep_ms"] = 100;
        save(); // Write default config
    } else {
        test.close();
        reload();
    }
}

bool Config::reload() {
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        // If file doesn't exist, create an empty JSON object
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
    file << data_.dump(4);  // pretty print with 4 spaces
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

    // Navigate to the parent node
    nlohmann::json* current = &data_;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->is_object() || !current->contains(parts[i]))
            return;  // path doesn't exist
        current = &(*current)[parts[i]];
    }
    // Remove the last part
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
