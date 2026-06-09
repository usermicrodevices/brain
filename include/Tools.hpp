#pragma once

#include <chrono>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
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

// List files matching extension in a directory
static std::vector<std::string> listDir(const std::string& dir, const std::string& ext = "") {
    std::vector<std::string> files;
    DIR* d = opendir(dir.c_str());
    if (!d) return files;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (!ext.empty()) {
            if (name.size() < ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
                continue;
        }
        files.push_back(name);
    }
    closedir(d);
    return files;
}

// Read ALL sources from a given root path (include/*.hpp + src/*.cpp)
static std::map<std::string, std::string> readAllSources(const std::string& root = ".") {
    std::map<std::string, std::string> sources;
    std::string includeDir = root + "include/";
    std::string srcDir = root + "src/";

    for (const auto& f : listDir(includeDir, ".hpp")) {
        sources["include/" + f] = get_file_content(includeDir + f);
    }
    for (const auto& f : listDir(srcDir, ".cpp")) {
        sources["src/" + f] = get_file_content(srcDir + f);
    }
    return sources;
}

// Write all sources back to disk
static void writeAllSources(const std::string& root, const std::map<std::string, std::string>& sources) {
    for (const auto& [path, content] : sources) {
        std::string fullPath = root + path;
        // Ensure directory exists
        size_t lastSlash = fullPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string dir = fullPath.substr(0, lastSlash);
            mkdir(dir.c_str(), 0755);
        }
        std::ofstream out(fullPath);
        if (out) {
            out << content;
            out.close();
        } else {
            std::cerr << "Error writing " << fullPath << "\n";
        }
    }
}

// Write a single file
static void writeFile(const std::string& path, const std::string& content) {
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dir = path.substr(0, lastSlash);
        mkdir(dir.c_str(), 0755);
    }
    std::ofstream out(path);
    if (out) {
        out << content;
        out.close();
        std::cout << "Updated " << path << "\n";
    } else {
        std::cerr << "Error writing " << path << "\n";
    }
}

// Copy best sources from temporary to original location
static void copyBestToOriginal(const std::string& tmpRoot, const std::map<std::string, std::string>& sources) {
    for (const auto& [path, content] : sources) {
        writeFile(path, content);
    }
}

// -----------------------------------------------------------------
// Compile a program from the temporary project root
// -----------------------------------------------------------------
static bool compileProgram(const std::string& app_name, const std::string& root) {
    std::string tmpDir = root;
    std::string binFile = tmpDir + app_name;

    setenv("TMPDIR", tmpDir.c_str(), 1);
    std::string errFile = tmpDir + app_name + ".err";
    std::string cmd = "g++ -pipe -std=c++20 -O2 -I" + root + "include -I" + root + "thirdparty " + root + "src/*.cpp -o " + binFile + " 2> " + errFile;
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
// Compile and return error string (instead of printing)
// -----------------------------------------------------------------
static std::string compileAndCaptureErrors(const std::string& root, const std::string& app_name) {
    std::string binFile = root + app_name;
    std::string errFile = root + app_name + ".err";
    std::string cmd = "g++ -pipe -std=c++20 -O2 -I" + root + "include -I" + root + "thirdparty " + root + "src/*.cpp -o " + binFile + " 2> " + errFile;

    pid_t pid = fork();
    if (pid == -1) return "fork failed";
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);

    int status;
    pid_t ret = waitpid(pid, &status, 0);
    bool success = (ret == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);

    std::string errors;
    if (!success) {
        std::ifstream err(errFile);
        if (err) {
            std::string line;
            while (std::getline(err, line)) errors += line + "\n";
            err.close();
        }
    }
    std::remove(errFile.c_str());
    return errors;
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
