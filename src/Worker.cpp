#include "Worker.hpp"
#include "GlobalStop.hpp"

Worker::Worker(int id, LLM& llm, const std::string& tmpRoot,
               GlobalStop& globalStop, const std::map<std::string, std::string>& bestSources)
    : id_(id), llm_(llm), tmpRoot_(tmpRoot),
      globalStop_(globalStop), bestSources_(bestSources),
      bestMetrics_{0, 0, false}
{
}

Worker::~Worker() {
    cleanupTmpfs();
}

void Worker::setupTmpfs() {
    mkdir(tmpRoot_.c_str(), 0755);
    mkdir((tmpRoot_ + "include").c_str(), 0755);
    mkdir((tmpRoot_ + "src").c_str(), 0755);
    mkdir((tmpRoot_ + "thirdparty").c_str(), 0755);
    writeAllSources(tmpRoot_, bestSources_);
    // Copy thirdparty
    system(("cp -r thirdparty/* " + tmpRoot_ + "thirdparty/ 2>/dev/null").c_str());
}

void Worker::cleanupTmpfs() {
    system(("rm -rf " + tmpRoot_).c_str());
}

std::string Worker::compile() {
    return compileAndCaptureErrors(tmpRoot_, "brain_worker_" + std::to_string(id_));
}

Metrics Worker::benchmark() {
    std::string binName = "brain_worker_" + std::to_string(id_);
    std::string errFile = tmpRoot_ + binName + ".err";
    std::string outputFile = tmpRoot_ + binName + ".out";
    std::string binFile = tmpRoot_ + binName;

    // Create a simple benchmark program that measures random expression generation
    std::string benchSrc = tmpRoot_ + "src/BenchmarkTask.cpp";
    {
        std::ofstream out(benchSrc);
        out << R"(
#include <chrono>
#include <random>
#include <string>
#include <sys/resource.h>

int main() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> distNumStmts(9, 11);
    std::uniform_int_distribution<int> distConst(1, 1000);
    std::uniform_int_distribution<int> distOp(0, 3);
    const char ops[] = {'+', '-', '*', '/'};

    volatile size_t dummy = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        int a = distConst(rng);
        int b = distConst(rng);
        char op = ops[distOp(rng)];
        while (op == '/' && b == 0) b = distConst(rng);
        std::string expr = std::to_string(a) + " " + op + " " + std::to_string(b);
        dummy += expr.size();
    }
    auto end = std::chrono::steady_clock::now();
    double time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    long memory_kb = usage.ru_maxrss;

    std::cout << "time:" << time_us << "\n";
    std::cout << "mem:" << memory_kb << "\n";
    return 0;
}
)";
    }

    // Compile benchmark task
    std::string cmd = "g++ -pipe -std=c++20 -O2 -o " + binFile + " " + benchSrc + " 2> " + errFile;
    pid_t pid = fork();
    if (pid == -1) return {0, 0, false};
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);
    int status;
    waitpid(pid, &status, 0);
    std::remove(errFile.c_str());

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return {0, 0, false};
    }

    // Run benchmark
    std::string runCmd = binFile + " > " + outputFile;
    pid = fork();
    if (pid == -1) return {0, 0, false};
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", runCmd.c_str(), nullptr);
        _exit(1);
    }
    setpgid(pid, pid);
    waitpid(pid, &status, 0);

    Metrics m = {0, 0, false};
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
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
    std::remove(benchSrc.c_str());
    return m;
}

std::string Worker::buildAnalysisPrompt() const {
    std::string prompt = "analyze for search optimizations for my sources\n\n";
    for (const auto& [path, content] : bestSources_) {
        prompt += "--- " + path + " ---\n```cpp\n" + content + "\n```\n\n";
    }
    return prompt;
}

std::string Worker::buildErrorPrompt(const std::string& errors) const {
    std::string prompt = "The following code has compilation errors. Please fix them and return the corrected code.\n\n";
    prompt += "Errors:\n```\n" + errors + "\n```\n\n";
    prompt += "Current sources:\n\n";
    for (const auto& [path, content] : bestSources_) {
        prompt += "--- " + path + " ---\n```cpp\n" + content + "\n```\n\n";
    }
    return prompt;
}

bool Worker::applyCodeFromResponse(const std::string& response) {
    // Parse response for code blocks
    std::string remaining = response;
    std::map<std::string, std::string> extracted;

    while (true) {
        size_t fenceStart = remaining.find("```");
        if ( fenceStart == std::string::npos) break;
        size_t contentStart = fenceStart + 3;
        // Skip language identifier
        if (remaining.substr(contentStart, 3) == "cpp") contentStart += 3;
        while (contentStart < remaining.size() && (remaining[contentStart] == '\n' || remaining[contentStart] == '\r'))
            ++contentStart;
        size_t fenceEnd = remaining.find("```", contentStart);
        if (fenceEnd == std::string::npos) break;

        std::string code = remaining.substr(contentStart, fenceEnd - contentStart);
        // Trim trailing whitespace
        while (!code.empty() && (code.back() == '\n' || code.back() == '\r' || code.back() == ' '))
            code.pop_back();

        // Try to find file path hint in surrounding context
        // Look for patterns like "--- path/to/file ---" before the fence
        size_t searchStart = (fenceStart > 200) ? fenceStart - 200 : 0;
        std::string context = remaining.substr(searchStart, fenceStart - searchStart);
        std::string targetFile;
        
        // Check for file markers
        size_t markerPos = context.find("--- ");
        if (markerPos != std::string::npos) {
            size_t markerEnd = context.find(" ---", markerPos + 4);
            if (markerEnd != std::string::npos) {
                targetFile = context.substr(markerPos + 4, markerEnd - markerEnd - 4);
                // Clean up
                while (!targetFile.empty() && targetFile.back() == ' ')
                    targetFile.pop_back();
            }
        }

        if (!targetFile.empty()) {
            extracted[targetFile] = code;
        } else {
            // Try to match by content (first function/class name or filename hint)
            extracted["pending_" + std::to_string(extracted.size())] = code;
        }

        remaining = remaining.substr(fenceEnd + 3);
    }

    // Apply extracted code
    bool anyApplied = false;
    for (auto& [key, code] : extracted) {
        if (key.substr(0, 8) == "pending_") {
            // Try to match by looking for matching content in bestSources_
            bool matched = false;
            for (const auto& [path, origContent] : bestSources_) {
                // Simple heuristic: if code contains same includes/structure
                if (code.find("#include") != std::string::npos && 
                    origContent.find("#include") != std::string::npos) {
                    bestSources_[path] = code;
                    matched = true;
                    anyApplied = true;
                    break;
                }
            }
            if (!matched && !bestSources_.empty()) {
                // Fallback: replace first source file
                auto it = bestSources_.begin();
                it->second = code;
                anyApplied = true;
            }
        } else if (bestSources_.count(key)) {
            bestSources_[key] = code;
            anyApplied = true;
        }
    }

    return anyApplied;
}

void Worker::run() {
    std::string workerTmp = getTempDir() + "brain_work_worker" + std::to_string(id_) + "/";
    tmpRoot_ = workerTmp;
    setupTmpfs();

    llm_.setWorkingDirectory(tmpRoot_);
    llm_.newSession();

    int maxRetries = 5;
    while (!globalStop_.shouldStop()) {
        // 1. Send analysis prompt
        std::string prompt = buildAnalysisPrompt();
        std::string response = llm_.ask(prompt);
        if (response.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 2. Apply code from response
        applyCodeFromResponse(response);

        // 3. Try to compile
        std::string errors = compile();
        int retry = 0;
        while (!errors.empty() && retry < maxRetries && !globalStop_.shouldStop()) {
            std::cerr << "[Worker " << id_ << "] Compile error (retry " << retry + 1 << "):\n" << errors;
            std::string errorPrompt = buildErrorPrompt(errors);
            response = llm_.ask(errorPrompt);
            if (response.empty()) break;
            applyCodeFromResponse(response);
            errors = compile();
            ++retry;
        }

        if (errors.empty()) {
            // 4. Run benchmark
            Metrics m = benchmark();
            if (m.valid && (bestMetrics_.valid == false || m.fitness() < bestMetrics_.fitness())) {
                bestMetrics_ = m;
                bestSources_ = readAllSources(tmpRoot_);
                std::cout << "[Worker " << id_ << "] New best: fitness=" << m.fitness() << "\n";
            }
        } else {
            std::cerr << "[Worker " << id_ << "] Failed to compile after " << maxRetries << " retries\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

Metrics Worker::getLastMetrics() const { return bestMetrics_; }
std::map<std::string, std::string> Worker::getBestSources() const { return bestSources_; }
