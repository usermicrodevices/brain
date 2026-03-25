#pragma once

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include "Compiler.hpp"

class Serializer {
public:
    Serializer(const std::string& initialSource, int generation, const Compiler::Metrics& initialBest);
    ~Serializer();

    void updateBest(const std::string& source, const Compiler::Metrics& best, int generation);
    void saveBest() const;

private:
    std::string bestSource;
    Compiler::Metrics best;
    int bestGen;
    static Serializer* instance;
    static void signal_handler(int);
    static void installSignalHandler();
};
