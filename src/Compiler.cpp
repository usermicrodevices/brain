#include "Compiler.hpp"

pid_t g_child_pid = -1;

bool Compiler::compile(const std::string& source, const std::string& exeName) const {
    if (source.empty()) return false;
    std::string tmpDir = getTempDir();
    std::string srcFile = tmpDir + exeName + ".cpp";
    std::string exeFile = tmpDir + exeName;

    std::ofstream out(srcFile);
    if (!out) return false;
    out << source;
    out.close();

    setenv("TMPDIR", tmpDir.c_str(), 1);
    std::string errFile = tmpDir + exeName + ".err";
    std::string cmd = "g++ -pipe -std=c++20 -O2 -o " + exeFile + " " + srcFile + " 2> " + errFile;

    pid_t pid = fork();
    if (pid == -1) {
        std::remove(srcFile.c_str());
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);
    g_child_pid = pid;

    int status;
    pid_t ret = waitpid(pid, &status, 0);
    g_child_pid = -1;
    if (ret == -1) {
        // waitpid interrupted by signal (Ctrl+C)
        std::remove(srcFile.c_str());
        std::remove(errFile.c_str());
        return false;
    }

    bool success = (ret == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (!success) {
        std::ifstream err(errFile);
        if (err) {
            std::cerr << "Compilation error:\n";
            std::string line;
            while (std::getline(err, line)) std::cerr << line << std::endl;
            err.close();
        } else {
            std::cerr << "Compilation failed (no error output)." << std::endl;
        }
        std::remove(errFile.c_str());
    }
    std::remove(srcFile.c_str());
    return success;
}

Metrics Compiler::runAndMeasure(const std::string& exeName, const std::string& arg) const {
    Metrics m = {0, 0, false};
    std::string tmpDir = getTempDir();
    std::string exeFile = tmpDir + exeName;
    std::string outputFile = tmpDir + exeName + ".out";

    std::string cmd = exeFile + " " + arg + " > " + outputFile;

    pid_t pid = fork();
    if (pid == -1) return m;
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);
    g_child_pid = pid;   // store child PID for signal handler

    int status;
    pid_t ret = waitpid(pid, &status, 0);
    g_child_pid = -1;
    if (ret == -1) {
        // interrupted by signal; child will be killed by signal handler
        std::remove(outputFile.c_str());
        return m;
    }

    if (ret == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::ifstream in(outputFile);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("time:") == 0)
                m.time_us = std::stod(line.substr(5));
            else if (line.find("mem:") == 0)
                m.memory_kb = std::stol(line.substr(4));
        }
        m.valid = true;
    } else {
        std::cerr << "Execution failed for " << exeFile << std::endl;
    }
    std::remove(outputFile.c_str());
    std::remove(exeFile.c_str());
    return m;
}
