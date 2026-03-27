#pragma once

#include <iomanip>
#include <iostream>

#include "Compiler.hpp"

class Report {
public:
    void initialBest(const Metrics& best) const;
    void candidate(int gen, const Metrics& candidate, const Metrics& best) const;
    void newBest(int gen, const Metrics& best) const;
};
