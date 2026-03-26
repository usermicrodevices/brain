#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

class Config {
public:
    // Construct with config file path (default: "config/core.json")
    explicit Config(const std::string& filePath = "config/core.json");

    // Reload config from file
    bool reload();

    // Save current config to file
    bool save() const;

    // Check if a key exists (supports nested keys using dot notation, e.g., "deepseek.session_id")
    bool hasKey(const std::string& key) const;

    // Get value as a specific type; throws if key not found or type mismatch
    template<typename T>
    T getKey(const std::string& key) const;

    // Set value for a key (creates nested structure if needed)
    template<typename T>
    void setKey(const std::string& key, const T& value);

    // Remove a key
    void removeKey(const std::string& key);

private:
    std::string filePath_;
    nlohmann::json data_;

    // Helper: navigate to the nested JSON node using dot notation
    nlohmann::json* getNode(const std::string& key, bool createIfMissing = false);
    const nlohmann::json* getNode(const std::string& key) const;
};

// Template implementations (usually placed in header)
template<typename T>
T Config::getKey(const std::string& key) const {
    const auto* node = getNode(key);
    if (!node)
        throw std::runtime_error("Config key not found: " + key);
    try {
        return node->get<T>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Config key type mismatch: " + key + " - " + e.what());
    }
}

template<typename T>
void Config::setKey(const std::string& key, const T& value) {
    auto* node = getNode(key, true);
    *node = value;
    save();  // persist after each set
}
