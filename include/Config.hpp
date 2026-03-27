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
    explicit Config(const std::string& filePath = "config/core.json");

    bool reload();
    bool save() const;

    bool hasKey(const std::string& key) const;

    // Get value; returns default‑constructed T if missing or type mismatch
    template<typename T>
    T getKey(const std::string& key) const;

    // Get value with a user‑provided default
    template<typename T>
    T getKey(const std::string& key, const T& defaultValue) const;

    template<typename T>
    void setKey(const std::string& key, const T& value);

    void removeKey(const std::string& key);

private:
    std::string filePath_;
    nlohmann::json data_;

    void ensureDirectoryExists() const;
    nlohmann::json* getNode(const std::string& key, bool createIfMissing = false);
    const nlohmann::json* getNode(const std::string& key) const;
};

// Template implementations
template<typename T>
T Config::getKey(const std::string& key) const {
    const auto* node = getNode(key);
    if (!node) return T{};
    try {
        return node->get<T>();
    } catch (const nlohmann::json::exception&) {
        return T{};
    }
}

template<typename T>
T Config::getKey(const std::string& key, const T& defaultValue) const {
    const auto* node = getNode(key);
    if (!node) return defaultValue;
    try {
        return node->get<T>();
    } catch (const nlohmann::json::exception&) {
        return defaultValue;
    }
}

template<typename T>
void Config::setKey(const std::string& key, const T& value) {
    auto* node = getNode(key, true);
    *node = value;
    save();
}