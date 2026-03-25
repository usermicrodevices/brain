#include "Serializer.hpp"

Serializer* Serializer::instance = nullptr;

// Helper: split combined source into header and cpp parts.
static std::pair<std::string, std::string> splitCombinedSource(const std::string& combined) {
    size_t sep = combined.find("\n\n");
    if (sep == std::string::npos) return {combined, ""};
    std::string header = combined.substr(0, sep);
    std::string cpp = combined.substr(sep + 2);
    return {header, cpp};
}

// Helper: write a file with a comment header
static void writeFile(const std::string& path, const std::string& content, int generation, const Compiler::Metrics& best) {
    std::ofstream out(path);
    if (out) {
        out << "// Best program found at generation " << generation << "\n";
        out << "// Time: " << best.time_us << " µs, Memory: " << best.memory_kb << " KB\n";
        out << content;
        out.close();
        std::cout << "Updated " << path << "\n";
    } else {
        std::cerr << "Error writing " << path << "\n";
    }
}

Serializer::Serializer(const std::string& initialSource, int generation, const Compiler::Metrics& initialBest)
    : bestSource(initialSource), best(initialBest), bestGen(generation) {
    instance = this;
    installSignalHandler();
    // Initially write the best source to original files (even if not improved yet)
    auto [header, cpp] = splitCombinedSource(initialSource);
    writeFile("include/Core.hpp", header, generation, initialBest);
    writeFile("src/Core.cpp", cpp, generation, initialBest);
}

Serializer::~Serializer() {
    instance = nullptr;
}

void Serializer::updateBest(const std::string& source, const Compiler::Metrics& metrics, int generation) {
    bestSource = source;
    best = metrics;
    bestGen = generation;

    // Write the best core to the original source files immediately
    auto [header, cpp] = splitCombinedSource(source);
    writeFile("include/Core.hpp", header, generation, metrics);
    writeFile("src/Core.cpp", cpp, generation, metrics);
}

void Serializer::saveBest() const {
    if (!best.valid) {
        std::cout << "No valid program found.\n";
        return;
    }

    // Optional: create a backup in best/ (not committed)
    mkdir("best", 0755);
    mkdir("best/include", 0755);
    mkdir("best/src", 0755);

    auto [header, cpp] = splitCombinedSource(bestSource);
    writeFile("best/include/Core.hpp", header, bestGen, best);
    writeFile("best/src/Core.cpp", cpp, bestGen, best);

    std::ofstream outRes("best_results.txt");
    if (outRes) {
        outRes << "Generation: " << bestGen << "\n";
        outRes << "Time (µs): " << best.time_us << "\n";
        outRes << "Memory (KB): " << best.memory_kb << "\n";
        outRes.close();
        std::cout << "Best results saved to best_results.txt\n";
    } else {
        std::cerr << "Error writing best_results.txt\n";
    }
}

void Serializer::signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (instance) {
            std::cout << "\n\nExiting... Saving best brain...\n";
            instance->saveBest();
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
