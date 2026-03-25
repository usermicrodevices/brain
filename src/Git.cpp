#include "Git.hpp"

bool Git::bestFolderExists() {
    struct stat st;
    if (stat("best", &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    std::string cmd = "find best -type f | head -1 | grep -q .";
    return (system(cmd.c_str()) == 0);
}

static void ensureGitignore() {
    std::ifstream check(".gitignore");
    if (check.good()) return;
    check.close();
    std::ofstream out(".gitignore");
    if (out) {
        out << "# Ignore best folder and temporary files\n";
        out << "best/\n";
        out << "*.out\n";
        out << "*.err\n";
        out << "prog_*\n";
        out << "brain_candidate*\n";
        out << "bench_candidate*\n";
        out << "best_results.txt\n";
        out.close();
        std::cout << "[Git] Created .gitignore\n";
    }
}

bool Git::commitAndPush(const std::string& branch, const std::string& message) {
    if (!bestFolderExists()) return false; // no best folder, but we still commit original files
    ensureGitignore();

    // Switch to branch (create if needed)
    std::string cmd = "git checkout " + branch + " 2>/dev/null || git checkout -b " + branch;
    if (system(cmd.c_str()) != 0) return false;

    // Add all tracked files (including include/ and src/)
    cmd = "git add include src";
    if (system(cmd.c_str()) != 0) return false;
    // Also add any other important files (like run.sh, brain.cpp) – optional
    // But we avoid adding best/ because it's ignored.

    cmd = "git commit -m \"" + message + "\"";
    if (system(cmd.c_str()) != 0) return false;

    cmd = "git push origin " + branch;
    if (system(cmd.c_str()) != 0) return false;

    return true;
}

bool Git::createPullRequest(const std::string& base, const std::string& head) {
    if (!bestFolderExists()) return false;
    ensureGitignore();
    std::string cmd = "gh pr create --base " + base + " --head " + head
                      + " --title \"Daily evolution update\""
                      + " --body \"Automated commit from brain evolution\"";
    return (system(cmd.c_str()) == 0);
}
