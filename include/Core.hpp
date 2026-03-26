#pragma once

#include <string>

#include "Tools.hpp"

class Core
{
public:
    Core();
    char getRandomOp();
    std::string randomExpression();

private:
    std::uniform_int_distribution<int> distNumStmts;
    std::uniform_int_distribution<int> distConst;

};
