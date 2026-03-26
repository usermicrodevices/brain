#include "Benchmark.hpp"

Benchmark::Benchmark(const Compiler& compiler, const std::string& tmpRoot)
    : compiler_(compiler), tmpRoot_(tmpRoot) {}

Metrics Benchmark::run() const {
    std::string binName = "brain_bench";
    if (compileProgram(binName, tmpRoot_)) {
        return compiler_.runAndMeasure(tmpRoot_, binName, "--benchmark");
    }
    return {0, 0, false};
}
