#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "Core.hpp"
#include "Compiler.hpp"

class Benchmark {
public:
    Benchmark(const Compiler& compiler);
    Compiler::Metrics run(const std::string& source) const;

private:
    const Compiler& compiler_;
};
