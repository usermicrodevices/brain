#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>
#include <unistd.h>

namespace Core {
    extern std::uniform_int_distribution<int> distNumStmts;
    extern std::uniform_int_distribution<int> distConst;

    std::string readOwnSource();
    std::string generateRandomProgram();
    std::string mutate(const std::string& source);
    void runBenchmark();
}
