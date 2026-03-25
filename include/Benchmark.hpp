#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "Compiler.hpp"

class Benchmark {
public:
    Benchmark(const Compiler& compiler);
    Metrics run(const std::string& source) const;

private:
    const Compiler& compiler_;
};
