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
    Serializer(const Metrics& initialBest, int generation=0, const std::string& tmpSourcePath="/dev/shm");
    ~Serializer();

    void updateBest(const Metrics& metrics, int generation=0, const std::string& header="", const std::string& source="");
    void saveBest() const;

private:
    std::string tmpRoot_;
    Metrics best_;
    int bestGen_;
    static Serializer* instance_;

    static void signal_handler(int);
    static void installSignalHandler();
};
