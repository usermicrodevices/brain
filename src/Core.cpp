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
