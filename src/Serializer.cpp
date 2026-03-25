#include "Serializer.hpp"

Serializer* Serializer::instance = nullptr;

Serializer::Serializer(const std::string& initialSource, int generation, const Compiler::Metrics& initialBest)
    : bestSource(initialSource), best(initialBest), bestGen(generation) {
    instance = this;
    installSignalHandler();
}

Serializer::~Serializer() {
    instance = nullptr;
}

void Serializer::updateBest(const std::string& source, const Compiler::Metrics& metrics, int generation) {
    bestSource = source;
    best = metrics;
    bestGen = generation;
}

// Helper: split combined source into header and cpp parts.
// The combined source is header + "\n\n" + cpp (as built in Core::readOwnSource).
static std::pair<std::string, std::string> splitCombinedSource(const std::string& combined) {
    // Find the first blank line that separates header and cpp (two consecutive newlines)
    size_t sep = combined.find("\n\n");
    if (sep == std::string::npos) {
        // fallback: assume it's all cpp? not likely
        return {combined, ""};
    }
    std::string header = combined.substr(0, sep);
    std::string cpp = combined.substr(sep + 2); // skip the two newlines
    return {header, cpp};
}

void Serializer::saveBest() const {
    if (!best.valid) {
        std::cout << "No valid program found.\n";
        return;
    }

    // Create the top-level "best" directory if it doesn't exist
    mkdir("best", 0755);

    // Copy the entire include/ and src/ directories (excluding best/ itself)
    // Use system commands for simplicity; they are standard on Unix.
    // We ignore errors because directories may already exist.
    system("cp -r include best/ 2>/dev/null");
    system("cp -r src best/ 2>/dev/null");

    // Now split the best source into header and cpp
    auto [headerBest, cppBest] = splitCombinedSource(bestSource);

    // Write the best header
    std::string headerFile = "best/include/Core.hpp";
    std::ofstream outHdr(headerFile);
    if (outHdr) {
        outHdr << "// Best program found at generation " << bestGen << "\n";
        outHdr << "// Time: " << best.time_us << " µs, Memory: " << best.memory_kb << " KB\n";
        outHdr << headerBest;
        outHdr.close();
        std::cout << "Best header saved to " << headerFile << "\n";
    } else {
        std::cerr << "Error writing " << headerFile << "\n";
    }

    // Write the best cpp
    std::string cppFile = "best/src/Core.cpp";
    std::ofstream outCpp(cppFile);
    if (outCpp) {
        outCpp << "// Best program found at generation " << bestGen << "\n";
        outCpp << "// Time: " << best.time_us << " µs, Memory: " << best.memory_kb << " KB\n";
        outCpp << cppBest;
        outCpp.close();
        std::cout << "Best source saved to " << cppFile << "\n";
    } else {
        std::cerr << "Error writing " << cppFile << "\n";
    }

    // Save results file
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
