#pragma once

#include <atomic>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Git.hpp"

class Scheduler {
public:
    // schedule: either a cron expression (e.g., "0 0 * * *") or a relative time (e.g., "+5m", "+2h30m", "+90s")
    Scheduler(const std::string& schedule = "0 0 * * *");
    ~Scheduler();

    void start();
    void stop();

private:
    std::atomic<bool> running;
    std::thread worker;
    std::string schedule;
    void run();
};
