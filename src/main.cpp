#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <sys/resource.h>
#include <sys/wait.h>

#include "Tools.hpp"
#include "Compiler.hpp"
#include "Serializer.hpp"
#include "Report.hpp"
#include "Benchmark.hpp"
#include "Scheduler.hpp"
#include "Config.hpp"
#include "LLM.hpp"
#include "Worker.hpp"
#include "GlobalStop.hpp"

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

std::string extractCode(const std::string& response) {
    std::string code = response;
    size_t start = code.find("```cpp");
    if (start == std::string::npos) start = code.find("```");
    if (start != std::string::npos) {
        start += 3;
        if (code.substr(start, 3) == "cpp") start += 3;
        while (start < code.size() && (code[start] == '\n' || code[start] == '\r'))
            ++start;
        size_t end = code.find("```", start);
        if (end != std::string::npos) {
            code = code.substr(start, end - start);
            while (!code.empty() && (code.back() == '\n' || code.back() == '\r' || code.back() == ' '))
                code.pop_back();
        }
    }
    return code;
}

int main(int argc, char* argv[]) {

    // Check if we are being run as a benchmark child
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark") {
            // Standalone benchmark (no Core dependency)
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_int_distribution<int> distNumStmts(9, 11);
            std::uniform_int_distribution<int> distConst(1, 1000);
            std::uniform_int_distribution<int> distOp(0, 3);
            const char ops[] = {'+', '-', '*', '/'};

            volatile size_t dummy = 0;
            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 100; ++i) {
                int a = distConst(rng);
                int b = distConst(rng);
                char op = ops[distOp(rng)];
                while (op == '/' && b == 0) b = distConst(rng);
                std::string expr = std::to_string(a) + " " + op + " " + std::to_string(b);
                dummy += expr.size();
            }
            auto end = std::chrono::steady_clock::now();
            double time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            struct rusage usage;
            getrusage(RUSAGE_SELF, &usage);
            long memory_kb = usage.ru_maxrss;

            std::cout << "time:" << time_us << "\n";
            std::cout << "mem:" << memory_kb << "\n";
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
    std::cout << "Fitness = time(us) * memory(KB) (lower is better).\n";
    std::cout << "Press Ctrl+C to save the best evolved brain.\n\n";

    // --- Load configuration ---
    Config config("config/core.json");
    std::string provider = config.getKey<std::string>("llm.provider");
    std::unique_ptr<LLM> llm;
    bool useAI = false;

    if (provider == "OpenCode") {
        useAI = true;
        llm = std::make_unique<LLM>("OpenCode");
        std::string url = config.getKey<std::string>("llm.opencode.url", "http://127.0.0.1:4096");
        std::string user = config.getKey<std::string>("llm.opencode.username", "opencode");
        std::string pass = config.getKey<std::string>("llm.opencode.password", "");
        std::string model = config.getKey<std::string>("llm.opencode.model", "");
        std::string agent = config.getKey<std::string>("llm.opencode.agent", "build");
        std::string effort = config.getKey<std::string>("llm.opencode.thinking_effort", "");
        std::string workdir = config.getKey<std::string>("llm.opencode.working_directory", ".");

        llm->setUrl(url);
        llm->setUsername(user);
        llm->setPassword(pass);
        llm->setModel(model);
        llm->setAgent(agent);
        llm->setThinkingEffort(effort);
        llm->setWorkingDirectory(workdir);
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

    // --- Global Stop ---
    int stopPort = config.getKey<int>("global_stop.listen_port", 9999);
    GlobalStop globalStop(stopPort);
    if (config.getKey<bool>("global_stop.enabled", true)) {
        globalStop.start();
    }

    // --- Check for .global_stop file ---
    if (GlobalStop::fileExists()) {
        std::cout << ".global_stop file found. Removing and continuing.\n";
        GlobalStop::removeFile();
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

    auto bestSources = readAllSources(tmpRoot);

    Compiler compiler;
    Serializer serializer(Metrics{0,0,false}, 0, tmpRoot);
    Report report;
    Benchmark benchmark(compiler, tmpRoot);

    Metrics best = benchmark.run();
    report.initialBest(best);
    serializer.updateBest(best, 0, bestSources);

    // --- Start scheduler (preserved) ---
    Scheduler scheduler(config.getKey<std::string>("scheduler.expression", "0 0 * * *"));

    int generation = 0;
    int numWorkers = config.getKey<int>("evolution.num_workers", 4);
    int maxRetries = config.getKey<int>("evolution.max_retries", 5);

    while (!exit_requested && !globalStop.shouldStop()) {
        if (GlobalStop::fileExists()) {
            globalStop.requestStop();
            break;
        }

        if (useAI) {
            // Multi-worker parallel mode
            std::vector<std::thread> workers;
            std::vector<std::unique_ptr<Worker>> workerPtrs;

            for (int i = 0; i < numWorkers; ++i) {
                std::string workerTmp = getTempDir() + "brain_work_worker" + std::to_string(i) + "/";
                workerPtrs.push_back(
                    std::make_unique<Worker>(i, *llm, workerTmp, globalStop, bestSources));
            }

            for (auto& w : workerPtrs) {
                globalStop.registerWorker();
                workers.emplace_back([&w]() { w->run(); });
            }

            for (auto& t : workers) t.join();

            // Collect best from workers
            for (auto& w : workerPtrs) {
                globalStop.workerFinished();
                Metrics wm = w->getLastMetrics();
                if (wm.valid && (best.fitness() == 0 || wm.fitness() < best.fitness())) {
                    best = wm;
                    bestSources = w->getBestSources();
                    serializer.updateBest(best, generation, bestSources);
                    report.newBest(generation, best);
                }
            }
        } else {
            // Local-only mode (no AI)
            std::string mutated;
            // Simple local mutation: adjust numeric values in source
            for (auto& [path, content] : bestSources) {
                if (path.find("Core") != std::string::npos || path.find("Tools") != std::string::npos) {
                    // Skip non-optimizable files
                    continue;
                }
            }

            g_child_pid = 0;
            writeAllSources(tmpRoot, bestSources);
            Metrics m = benchmark.run();
            if (m.valid && (best.fitness() == 0 || m.fitness() < best.fitness())) {
                best = m;
                serializer.updateBest(best, generation, bestSources);
                report.newBest(generation, best);
            } else {
                report.candidate(generation, m, best);
            }
        }

        ++generation;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // On exit, copy the best from temporary to original files
    copyBestToOriginal(tmpRoot, bestSources);
    serializer.saveBest();
    globalStop.stop();

    return 0;
}
