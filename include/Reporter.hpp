#pragma once

#include <iomanip>
#include <iostream>

#include "Compiler.hpp"

class Reporter {
public:
    void initialBest(const Compiler::Metrics& best) const;
    void candidate(int gen, const Compiler::Metrics& candidate, const Compiler::Metrics& best) const;
    void newBest(int gen, const Compiler::Metrics& best) const;
};
