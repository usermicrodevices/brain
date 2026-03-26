#pragma once

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Tools.hpp"

extern pid_t g_child_pid;

class Compiler {
public:
    bool compile(const std::string& root, const std::string& binName) const;
    Metrics runAndMeasure(const std::string& root, const std::string& binName, const std::string& arg = "") const;
};
