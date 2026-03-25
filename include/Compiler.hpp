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


extern pid_t g_child_pid;   // for signal handler to kill the child

class Compiler {
public:
    bool compile(const std::string& source, const std::string& exeName) const;
    struct Metrics {
        double time_us;
        long memory_kb;
        bool valid;
        double fitness() const { return time_us * memory_kb; }
    };
    Metrics runAndMeasure(const std::string& exeName, const std::string& arg = "") const;
};
