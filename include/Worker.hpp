#pragma once

#include <atomic>
#include <map>
#include <string>
#include <thread>

#include "LLM.hpp"
#include "Compiler.hpp"
#include "Tools.hpp"

class GlobalStop;

class Worker {
public:
    Worker(int id, LLM& llm, const std::string& tmpRoot,
           GlobalStop& globalStop, const std::map<std::string, std::string>& bestSources);
    ~Worker();

    void run();
    Metrics getLastMetrics() const;
    std::map<std::string, std::string> getBestSources() const;

private:
    int id_;
    LLM& llm_;
    std::string tmpRoot_;
    GlobalStop& globalStop_;
    std::map<std::string, std::string> bestSources_;
    Metrics bestMetrics_;

    void setupTmpfs();
    void cleanupTmpfs();
    std::string compile();
    Metrics benchmark();

    std::string buildAnalysisPrompt() const;
    std::string buildErrorPrompt(const std::string& errors) const;
    bool applyCodeFromResponse(const std::string& response);
};
