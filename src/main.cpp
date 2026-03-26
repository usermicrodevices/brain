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

int main(int argc, char* argv[]) {

    // Check if we are being run as a benchmark child
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark") {
            // Run the benchmark and exit – no recursion
            Core core;
            Metrics m = core.benchmark();
            std::cout << "time:" << m.time_us << "\n";
            std::cout << "mem:" << m.memory_kb << "\n";
            return 0;
        }
    }

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

    std::string tmpRoot = getTempDir() + "brain_work/";
    // Create a temporary working directory in /dev/shm
    mkdir(tmpRoot.c_str(), 0755);
    // Create include and src subdirectories
    mkdir((tmpRoot + "include").c_str(), 0755);
    mkdir((tmpRoot + "src").c_str(), 0755);
    // Copy only the include/ and src/ directories (headers and source files)
    if(system(("cp -r include/* " + tmpRoot + "include/").c_str()) != 0)
    {
        std::cout << "Error copy 'include' to temp dir\n";
        return 1;
    }
    if(system(("cp -r src/* " + tmpRoot + "src/").c_str()) != 0)
    {
        std::cout << "Error copy 'src' to temp dir\n";
        return 1;
    }

    // Now all operations will use tmpRoot as the base
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
    // Scheduler scheduler("+1m");

    int generation = 0;
    while (!exit_requested) {
        if (exit_requested) break;
        std::string mutated = mutate(bestSource);
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
