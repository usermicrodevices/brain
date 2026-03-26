#include "Core.hpp"

Core::Core() :
distNumStmts(9, 11),
distConst(1, 1000)
{
}

char Core::getRandomOp() {
    const char ops[] = {'+', '-', '*', '/'};
    static std::uniform_int_distribution<int> distOp(0, 3);
    return ops[distOp(rng)];
}

std::string Core::randomExpression() {
    int a = distConst(rng);
    int b = distConst(rng);
    char op = getRandomOp();
    while (op == '/' && b == 0) b = distConst(rng);
    return std::to_string(a) + " " + op + " " + std::to_string(b);
}

Metrics Core::benchmark(size_t iterations) {
    // Run randomExpression many times
    volatile size_t dummy = 0;  // prevent loop optimization
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        std::string expr = randomExpression();
        dummy += expr.size();
    }
    auto end = std::chrono::steady_clock::now();
    double time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    long memory_kb = usage.ru_maxrss;   // KB on Linux

    return Metrics{time_us, memory_kb, true};
}
