#include "Core.hpp"

std::uniform_int_distribution<int> distNumStmts(9, 11);
std::uniform_int_distribution<int> distConst(1, 1000);

static char getRandomOp() {
    const char ops[] = {'+', '-', '*', '/'};
    static std::uniform_int_distribution<int> distOp(0, 3);
    return ops[distOp(rng)];
}

static std::string randomExpression() {
    int a = distConst(rng);
    int b = distConst(rng);
    char op = getRandomOp();
    while (op == '/' && b == 0) b = distConst(rng);
    return std::to_string(a) + " " + op + " " + std::to_string(b);
}
