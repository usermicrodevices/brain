#pragma once

#include <chrono>
#include <string>
#include <sys/resource.h>

#include "Tools.hpp"

class Core
{
public:
    Core();
    char getRandomOp();
    std::string randomExpression();
    Metrics benchmark(size_t iterations = 100);

private:
    std::uniform_int_distribution<int> distNumStmts;
    std::uniform_int_distribution<int> distConst;

};
