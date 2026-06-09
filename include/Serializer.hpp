#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>

#include "Tools.hpp"
#include "Compiler.hpp"

class Serializer {
public:
    Serializer(const Metrics& initialBest, int generation = 0, const std::string& tmpRoot = "");
    ~Serializer();

    void updateBest(const Metrics& metrics, int generation, const std::map<std::string, std::string>& sources);
    void saveBest() const;

private:
    Metrics best_;
    int bestGen_;
    std::string tmpRoot_;
};
