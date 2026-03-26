#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

static std::random_device rd;
static std::mt19937 rng(rd());

struct Metrics {
    double time_us;
    long memory_kb;
    bool valid;
    double fitness() const { return time_us * memory_kb; }
};

static std::vector<std::string> splitLines(const std::string& src) {
    std::vector<std::string> lines;
    std::string line;
    std::stringstream ss(src);
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static std::string joinLines(const std::vector<std::string>& lines) {
    std::stringstream ss;
    for (const auto& l : lines) ss << l << "\n";
    return ss.str();
}

static std::string mutate(const std::string& source) {
    auto lines = splitLines(source);
    for (auto& line : lines) {
        if (line.find("distNumStmts(") != std::string::npos) {
            size_t start = line.find('(');
            size_t end = line.find(')', start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string range = line.substr(start + 1, end - start - 1);
                size_t comma = range.find(',');
                if (comma != std::string::npos) {
                    int low = std::stoi(range.substr(0, comma));
                    int high = std::stoi(range.substr(comma + 1));
                    int delta = (std::rand() % 3) - 1;
                    low = std::max(5, low + delta);
                    high = std::max(low + 1, high + delta);
                    std::string newRange = std::to_string(low) + "," + std::to_string(high);
                    line = line.substr(0, start + 1) + newRange + line.substr(end);
                }
            }
        }
        else if (line.find("distConst(") != std::string::npos) {
            size_t start = line.find('(');
            size_t end = line.find(')', start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string range = line.substr(start + 1, end - start - 1);
                size_t comma = range.find(',');
                if (comma != std::string::npos) {
                    int low = std::stoi(range.substr(0, comma));
                    int high = std::stoi(range.substr(comma + 1));
                    int delta = (std::rand() % 11) - 5;
                    low = std::max(1, low + delta);
                    high = std::max(low + 1, high + delta);
                    std::string newRange = std::to_string(low) + "," + std::to_string(high);
                    line = line.substr(0, start + 1) + newRange + line.substr(end);
                }
            }
        }
    }
    return joinLines(lines);
}

// -----------------------------------------------------------------
// Helper: get temporary directory (RAM disk if available)
// -----------------------------------------------------------------
static std::string getTempDir() {
    struct stat st;
    if (stat("/dev/shm", &st) == 0 && S_ISDIR(st.st_mode)) return "/dev/shm/";
    return "./";
}

static std::string get_file_content(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Read sources from a given root path (default current dir)
static std::pair<std::string, std::string> readSources(const std::string& root = ".") {
    std::string header(get_file_content(root + "include/Core.hpp"));
    std::string source(get_file_content(root + "src/Core.cpp"));
    return {header, source};
}

// Write a file
static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (out) {
        out << content;
        out.close();
        std::cout << "Updated " << path << "\n";
    } else {
        std::cerr << "Error writing " << path << "\n";
    }
}

// Copy the best core from temporary root to original location
static void copyBestToOriginal(const std::string& tmpRoot) {
    auto [header, source] = readSources(tmpRoot);
    writeFile("include/Core.hpp", header);
    writeFile("src/Core.cpp", source);
}

// -----------------------------------------------------------------
// Compile a program from the temporary project root
// -----------------------------------------------------------------
static bool compileProgram(const std::string& app_name, const std::string& root) {
    std::string tmpDir = root;//getTempDir();
    std::string binFile = tmpDir + app_name;

    setenv("TMPDIR", tmpDir.c_str(), 1);
    std::string errFile = tmpDir + app_name + ".err";
    std::string cmd = "g++ -pipe -std=c++20 -O2 -I" + root + "include " + root + "src/*.cpp -o " + binFile + " 2> " + errFile;
    std::cout << "\n" << cmd << "\n";

    pid_t pid = fork();
    if (pid == -1) return false;
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);

    int status;
    pid_t ret = waitpid(pid, &status, 0);
    bool success = (ret == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (!success) {
        std::ifstream err(errFile);
        if (err) {
            std::string line;
            while (std::getline(err, line)) std::cerr << line << std::endl;
        }
    }
    std::remove(errFile.c_str());
    return success;
}

// -----------------------------------------------------------------
// Run an executable and parse its output (time and memory)
// -----------------------------------------------------------------
struct ProgramMetrics {
    double time_us;
    long memory_kb;
    bool valid;
};

static ProgramMetrics runProgram(const std::string& app_name, const std::string& root) {
    ProgramMetrics m = {0, 0, false};
    std::string tmpDir = getTempDir() + root;
    std::string binFile = tmpDir + app_name;
    std::string outputFile = tmpDir + app_name + ".out";

    std::string cmd = binFile + " > " + outputFile;
    pid_t pid = fork();
    if (pid == -1) return m;
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);

    int status;
    pid_t ret = waitpid(pid, &status, 0);
    if (ret != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::ifstream in(outputFile);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("time:") == 0)
                m.time_us = std::stod(line.substr(5));
            else if (line.find("mem:") == 0)
                m.memory_kb = std::stol(line.substr(4));
        }
        m.valid = true;
    }
    std::remove(outputFile.c_str());
    std::remove(binFile.c_str());
    return m;
}
