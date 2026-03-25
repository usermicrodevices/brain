#include "Reporter.hpp"

void Reporter::initialBest(const Metrics& best) const {
    std::cout << "Initial best: time=" << best.time_us
              << " µs, mem=" << best.memory_kb
              << " KB, fitness=" << best.fitness() << "\n";
}

void Reporter::candidate(int gen, const Metrics& candidate, const Metrics& best) const {
    std::cout << "\rGen " << std::setw(4) << gen
              << " | candidate: time=" << std::setw(8) << candidate.time_us
              << " µs, mem=" << std::setw(5) << candidate.memory_kb
              << " KB, fitness=" << std::setw(10) << candidate.fitness()
              << " | best: fitness=" << std::setw(10) << best.fitness()
              << "   " << std::flush;
}

void Reporter::newBest(int gen, const Metrics& best) const {
    std::cout << "\nNew best! Gen " << gen << ": "
              << "time=" << best.time_us << " µs, mem=" << best.memory_kb
              << " KB, fitness=" << best.fitness() << "   "
              << "\n";
}
