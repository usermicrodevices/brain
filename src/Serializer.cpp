#include "Serializer.hpp"

Serializer::Serializer(const Metrics& initialBest, int generation, const std::string& tmpRoot)
    : best_(initialBest), bestGen_(generation), tmpRoot_(tmpRoot) {
}

Serializer::~Serializer() {
}

void Serializer::updateBest(const Metrics& metrics, int generation, const std::map<std::string, std::string>& sources) {
    best_ = metrics;
    bestGen_ = generation;
    writeAllSources(tmpRoot_, sources);
}

void Serializer::saveBest() const {
    if (!best_.valid) {
        std::cout << "No valid program found.\n";
        return;
    }

    auto sources = readAllSources(tmpRoot_);
    for (const auto& [path, content] : sources) {
        writeFile(path, content);
    }

    std::ofstream outRes("best_results.txt");
    if (outRes) {
        outRes << "Generation: " << bestGen_ << "\n";
        outRes << "Time (us): " << best_.time_us << "\n";
        outRes << "Memory (KB): " << best_.memory_kb << "\n";
        outRes.close();
        std::cout << "Best results saved to best_results.txt\n";
    } else {
        std::cerr << "Error writing best_results.txt\n";
    }
}
