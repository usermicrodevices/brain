#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "Tools.hpp"
#include "Compiler.hpp"

class Benchmark {
public:
    Benchmark(const Compiler& compiler, const std::string& tmpRoot);
    Metrics run() const;

private:
    const Compiler& compiler_;
    std::string tmpRoot_;
};
