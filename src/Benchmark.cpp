#include "Benchmark.hpp"

Benchmark::Benchmark(const Compiler& compiler) : compiler_(compiler) {}

Metrics Benchmark::run(const std::string& source) const {
    std::string exeName = "bench_candidate";
    if (compiler_.compile(source, exeName)) {
        return compiler_.runAndMeasure(exeName, "--benchmark");
    }
    return {0, 0, false};
}
