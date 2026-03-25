#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <sys/wait.h>

#include "Tools.hpp"
#include "Compiler.hpp"
#include "Serializer.hpp"
#include "Reporter.hpp"
#include "Benchmark.hpp"
#include "Scheduler.hpp"

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

int main() {
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

    auto [ownHeader, ownSource] = readBestSource();

    Compiler compiler;
    Serializer serializer(Metrics{0,0,false});
    Reporter reporter;
    Benchmark benchmark(compiler);

    std::string bestSource = ownSource;
    Metrics best = benchmark.run(bestSource);
    reporter.initialBest(best);
    serializer.updateBest(best, 0, ownHeader, bestSource);

    // Start the scheduler
    //Scheduler scheduler; // runs default "0 0 * * *" every midnight
    // for testing you can use run once
    // runs once after N minutes or edit as examples: "+5m", "+2h30m", "+90s"
    //Scheduler scheduler("+1m");

    int generation = 0;
    while (!exit_requested) {
        if (exit_requested) break;
        std::string mutated = mutate(bestSource);
        if (exit_requested) break;

        g_child_pid = 0;
        Metrics m = benchmark.run(mutated);
        if (exit_requested) break;

        if (m.valid && m.fitness() < best.fitness()) {
            best = m;
            bestSource = mutated;
            serializer.updateBest(best, generation, ownHeader, bestSource);
            reporter.newBest(generation, best);
        } else {
            reporter.candidate(generation, m, best);
        }
        ++generation;
        // Small sleep to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    serializer.saveBest();

    return 0;
}
