#include "Core.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <random>
#include <vector>

namespace Core {

    static std::random_device rd;
    static std::mt19937 rng(rd());

    std::uniform_int_distribution<int> distNumStmts(9, 11);
    std::uniform_int_distribution<int> distConst(1, 1000);

    static char getRandomOp() {
        const char ops[] = {'+', '-', '*', '/'};
        static std::uniform_int_distribution<int> distOp(0, 3);
        return ops[distOp(rng)];
    }

    static std::string randomExpression() {
        int a = distConst(rng);
        int b = distConst(rng);
        char op = getRandomOp();
        while (op == '/' && b == 0) b = distConst(rng);
        return std::to_string(a) + " " + op + " " + std::to_string(b);
    }

    // -----------------------------------------------------------------
    // Helper: get temporary directory (RAM disk if available)
    // -----------------------------------------------------------------
    static std::string getTempDir() {
        struct stat st;
        if (stat("/dev/shm", &st) == 0 && S_ISDIR(st.st_mode)) return "/dev/shm/";
        return "./";
    }

    // -----------------------------------------------------------------
    // Compile a source string to an executable in the temp dir
    // -----------------------------------------------------------------
    static bool compileProgram(const std::string& source, const std::string& exeName) {
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
        std::remove(srcFile.c_str());
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

    static ProgramMetrics runProgram(const std::string& exeName) {
        ProgramMetrics m = {0, 0, false};
        std::string tmpDir = getTempDir();
        std::string exeFile = tmpDir + exeName;
        std::string outputFile = tmpDir + exeName + ".out";

        std::string cmd = exeFile + " > " + outputFile;
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
        std::remove(exeFile.c_str());
        return m;
    }

    // -----------------------------------------------------------------
    // generateRandomProgram – unchanged
    // -----------------------------------------------------------------
    std::string generateRandomProgram() {
        std::stringstream ss;
        ss << "#include <iostream>\n";
        ss << "#include <chrono>\n";
        ss << "#include <sys/resource.h>\n";
        ss << "#include <unistd.h>\n\n";
        ss << "long getMemoryUsage() {\n";
        ss << "    struct rusage usage;\n";
        ss << "    getrusage(RUSAGE_SELF, &usage);\n";
        ss << "    return usage.ru_maxrss;\n";
        ss << "}\n\n";
        ss << "int main() {\n";
        ss << "    auto start = std::chrono::high_resolution_clock::now();\n\n";
        ss << "    // --- Random computation ---\n";
        int stmts = distNumStmts(rng);
        for (int i = 0; i < stmts; ++i) {
            ss << "    volatile int x" << i << " = " << randomExpression() << ";\n";
        }
        ss << "    auto end = std::chrono::high_resolution_clock::now();\n";
        ss << "    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();\n\n";
        ss << "    std::cout << \"time:\" << elapsed << \"\\n\";\n";
        ss << "    std::cout << \"mem:\" << getMemoryUsage() << \"\\n\";\n";
        ss << "    return 0;\n";
        ss << "}\n";
        return ss.str();
    }

    // -----------------------------------------------------------------
    // runBenchmark – compiles 3 random programs, runs them, and outputs metrics
    // -----------------------------------------------------------------
    void runBenchmark() {
        auto start = std::chrono::high_resolution_clock::now();
        long max_mem = 0;
        int num_tests = 3;

        for (int i = 0; i < num_tests; ++i) {
            std::string prog = generateRandomProgram();
            std::string exe = "prog_" + std::to_string(i);
            if (compileProgram(prog, exe)) {
                ProgramMetrics m = runProgram(exe);
                if (m.valid && m.memory_kb > max_mem) max_mem = m.memory_kb;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "time:" << elapsed << "\n";
        std::cout << "mem:" << max_mem << "\n";
    }

    // -----------------------------------------------------------------
    // readOwnSource – combine Core.hpp and Core.cpp and append main()
    // -----------------------------------------------------------------
    std::string readOwnSource() {
        std::ifstream hdr("include/Core.hpp");
        std::ifstream cpp("src/Core.cpp");
        if (!hdr || !cpp) {
            std::cerr << "Error: cannot read core source files.\n";
            exit(1);
        }

        std::stringstream combined;

        // Read header, skipping '#pragma once'
        std::string line;
        while (std::getline(hdr, line)) {
            if (line.find("#pragma once") == std::string::npos) {
                combined << line << '\n';
            }
        }
        combined << '\n';

        // Read cpp, skipping '#include "Core.hpp"'
        while (std::getline(cpp, line)) {
            if (line.find("#include \"Core.hpp\"") == std::string::npos) {
                combined << line << '\n';
            }
        }
        combined << '\n';

        // Append a main function that calls runBenchmark when --benchmark is given
        combined << "int main(int argc, char* argv[]) {\n";
        combined << "    for (int i = 1; i < argc; ++i) {\n";
        combined << "        if (std::string(argv[i]) == \"--benchmark\") {\n";
        combined << "            Core::runBenchmark();\n";
        combined << "            return 0;\n";
        combined << "        }\n";
        combined << "    }\n";
        combined << "    return 0;\n";
        combined << "}\n";

        return combined.str();
    }

    // -----------------------------------------------------------------
    // splitLines, joinLines, mutate – unchanged
    // -----------------------------------------------------------------
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

    std::string mutate(const std::string& source) {
        auto lines = splitLines(source);
        for (auto& line : lines) {
            if (line.find("std::uniform_int_distribution<int> distNumStmts(") != std::string::npos) {
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
            else if (line.find("std::uniform_int_distribution<int> distConst(") != std::string::npos) {
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

} // namespace Core