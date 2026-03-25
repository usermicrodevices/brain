#pragma once

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include "Tools.hpp"
#include "Compiler.hpp"

class Serializer {
public:
    Serializer(const Metrics& initialBest, int generation = 0, const std::string& tmpRoot = "");
    ~Serializer();

    void updateBest(const Metrics& metrics, int generation, const std::string& header, const std::string& source);
    void saveBest() const;

private:
    Metrics best_;
    int bestGen_;
    std::string tmpRoot_;
    static Serializer* instance_;

    static void signal_handler(int);
    static void installSignalHandler();
};
