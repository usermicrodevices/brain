#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <sys/wait.h>

#include "Tools.hpp"
#include "Compiler.hpp"
#include "Serializer.hpp"
#include "Report.hpp"
#include "Benchmark.hpp"
#include "Scheduler.hpp"
#include "Core.hpp"
#include "Config.hpp"
#include "LLM.hpp"

volatile std::sig_atomic_t exit_requested = 0;
extern pid_t g_child_pid;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        exit_requested = 1;
        if (g_child_pid > 0) {
            kill(-g_child_pid, SIGTERM);
        }
    }
}

// Helper: extract C++ code from an LLM response (remove markdown fences)
std::string extractCode(const std::string& response) {
    // Look for ```cpp ... ``` or just ``` ... ```
    std::string code = response;
    size_t start = code.find("```cpp");
    if (start == std::string::npos) start = code.find("```");
    if (start != std::string::npos) {
        start += 3; // skip ```
        // skip optional "cpp"
        if (code.substr(start, 3) == "cpp") start += 3;
        // skip whitespace
        while (start < code.size() && (code[start] == '\n' || code[start] == '\r'))
            ++start;
        size_t end = code.find("```", start);
        if (end != std::string::npos) {
            code = code.substr(start, end - start);
            // trim trailing whitespace
            while (!code.empty() && (code.back() == '\n' || code.back() == '\r' || code.back() == ' '))
                code.pop_back();
        }
    }
    // If no fences, assume whole response is the code
    return code;
}

int main(int argc, char* argv[]) {

    // Check if we are being run as a benchmark child
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark") {
            Core core;
            Metrics m = core.benchmark();
            std::cout << "time:" << m.time_us << "\n";
            std::cout << "mem:" << m.memory_kb << "\n";
            return 0;
        }
    }

    // --- Setup signal handling ---
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    std::cout << "=== Brain Evolution ===\n";
    std::cout << "All temporary files in RAM disk (/dev/shm).\n";
    std::cout << "Fitness = time(µs) * memory(KB) (lower is better).\n";
    std::cout << "Press Ctrl+C to save the best evolved brain.\n\n";

    // --- Load configuration ---
    Config config("config/core.json");
    std::string provider = config.getKey<std::string>("llm.provider");
    std::unique_ptr<LLM> llm;
    bool useAI = false;

    if (provider == "DeepSeek") {
        useAI = true;
        llm = std::make_unique<LLM>("DeepSeek");
        bool thinking = config.getKey<bool>("llm.deepseek.thinking_enabled");
        bool search = config.getKey<bool>("llm.deepseek.search_enabled");
        llm->setThinkingEnabled(thinking);
        llm->setSearchEnabled(search);
        llm->setVerbose(false); // you can make this configurable if you like
        // Use existing session ID if present
        std::string sessionId = config.getKey<std::string>("llm.deepseek.session_id");
        if (!sessionId.empty()) {
            llm->setSessionId(sessionId);
        } else { // Create a new session and save its ID
            if (llm->newSession()) {
                std::string newId = llm->getSessionId(); // we need to add this getter
                if (!newId.empty()) {
                    config.setKey("llm.deepseek.session_id", newId);
                    config.save(); // persist immediately
                }
            }
        }
    } else if (provider == "OpenAI") {
        useAI = true;
        llm = std::make_unique<LLM>("OpenAI");
        std::string apiKey = config.getKey<std::string>("llm.openai.api_key");
        if (!apiKey.empty()) llm->setApiKey(apiKey);
        std::string model = config.getKey<std::string>("llm.openai.model");
        llm->setModel(model);
        int maxTokens = config.getKey<int>("llm.openai.max_tokens");
        llm->setMaxTokens(maxTokens);
        double temp = config.getKey<double>("llm.openai.temperature");
        llm->setTemperature(temp);
    } else {
        std::cout << "No valid LLM provider configured. Using local mutation only.\n";
    }

    // --- Setup temporary work directory ---
    std::string tmpRoot = getTempDir() + "brain_work/";
    mkdir(tmpRoot.c_str(), 0755);
    mkdir((tmpRoot + "include").c_str(), 0755);
    mkdir((tmpRoot + "src").c_str(), 0755);
    mkdir((tmpRoot + "thirdparty").c_str(), 0755);
    if(system(("cp -r include/* " + tmpRoot + "include/").c_str()) != 0) {
        std::cout << "Error copy 'include' to temp dir\n";
        return 1;
    }
    if(system(("cp -r src/* " + tmpRoot + "src/").c_str()) != 0) {
        std::cout << "Error copy 'src' to temp dir\n";
        return 1;
    }
    if(system(("cp -r thirdparty/* " + tmpRoot + "thirdparty/").c_str()) != 0) {
        std::cout << "Error copy 'thirdparty' to temp dir\n";
        return 1;
    }

    auto [ownHeader, ownSource] = readSources(tmpRoot);

    Compiler compiler;
    Serializer serializer(Metrics{0,0,false}, 0, tmpRoot);
    Report report;
    Benchmark benchmark(compiler, tmpRoot);

    std::string bestSource = ownSource;
    Metrics best = benchmark.run();
    report.initialBest(best);
    serializer.updateBest(best, 0, ownHeader, bestSource);

    // ATTENTION: not delete this comment lines with examples and info
    // Start the scheduler
    // Scheduler scheduler; // runs default "0 0 * * *" every midnight
    // for testing you can use run once
    // runs once after N minutes or edit as examples: "+5m", "+2h30m", "+90s"
    // Scheduler scheduler(config.getKey<std::string>("scheduler.expression", "+1m");

    int generation = 0;
    while (!exit_requested) {
        if (exit_requested) break;

        std::string mutated;
        if (useAI) {
            // Build a prompt that includes the current source and fitness
            std::string prompt = "You are a C++ optimizer. The current Core.hpp is:\n```cpp\n" +
                                 bestSource + "\n```\n" +
                                 "Fitness (time*memory) is " + std::to_string(best.fitness()) +
                                 ". Suggest an improvement by modifying the parameters of distNumStmts and distConst. "
                                 "Return only the full new Core.hpp code inside a code block.";
            std::string response = llm->ask(prompt);
            if (!response.empty()) {
                mutated = extractCode(response);
                // If we got nothing after extraction, fall back to local
                if (mutated.empty()) {
                    std::cerr << "AI returned empty code, falling back to local mutation.\n";
                    mutated = mutate(bestSource);
                }
            } else {
                std::cerr << "AI call failed, falling back to local mutation.\n";
                mutated = mutate(bestSource);
            }
        } else {
            mutated = mutate(bestSource);
        }
        if (exit_requested) break;

        g_child_pid = 0;
        Metrics m = benchmark.run();
        if (exit_requested) break;

        if (m.valid && m.fitness() < best.fitness()) {
            best = m;
            bestSource = mutated;
            serializer.updateBest(best, generation, ownHeader, bestSource);
            report.newBest(generation, best);
        } else {
            report.candidate(generation, m, best);
        }
        ++generation;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // On exit, copy the best core from temporary to original files
    copyBestToOriginal(tmpRoot);
    serializer.saveBest();

    return 0;
}
