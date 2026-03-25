#include "Serializer.hpp"

Serializer* Serializer::instance_ = nullptr;

Serializer::Serializer(const Metrics& initialBest, int generation, const std::string& tmpSourcePath)
    : best_(initialBest), bestGen_(generation), tmpRoot_(tmpSourcePath) {
    instance_ = this;
    installSignalHandler();
    // Initially write the best source to original files (even if not improved yet)
    //auto [header, source] = readBestSource(tmpRoot_);
    //writeFile("include/Core.hpp", header);
    //writeFile("src/Core.cpp", source);
}

Serializer::~Serializer() {
    instance_ = nullptr;
}

void Serializer::updateBest(const Metrics& metrics, int generation, const std::string& header, const std::string& source) {
    best_ = metrics;
    bestGen_ = generation;
    writeFile(tmpRoot_+"/include/Core.hpp", header);
    writeFile(tmpRoot_+"/src/Core.cpp", source);
}

void Serializer::saveBest() const {
    if (!best_.valid) {
        std::cout << "No valid program found.\n";
        return;
    }

    auto [header, source] = readBestSource(tmpRoot_);
    writeFile("include/Core.hpp", header);
    writeFile("src/Core.cpp", source);

    std::ofstream outRes("best_results.txt");
    if (outRes) {
        outRes << "Generation: " << bestGen_ << "\n";
        outRes << "Time (µs): " << best_.time_us << "\n";
        outRes << "Memory (KB): " << best_.memory_kb << "\n";
        outRes.close();
        std::cout << "Best results saved to best_results.txt\n";
    } else {
        std::cerr << "Error writing best_results.txt\n";
    }
}

void Serializer::signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (instance_) {
            std::cout << "\n\nExiting... Saving best brain...\n";
            instance_->saveBest();
        }
        exit(0);
    }
}

void Serializer::installSignalHandler() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}
