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
    // We commit the original source files even if best folder is empty
    ensureGitignore();

    // Switch to branch (create if needed)
    std::string cmd = "git checkout " + branch + " 2>/dev/null || git checkout -b " + branch;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Git] Failed to switch to branch " << branch << "\n";
        return false;
    }

    // Force-add the entire include/ and src/ directories (they might be ignored)
    cmd = "git add -f include src";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Git] Failed to add include/ and src/\n";
        return false;
    }

    cmd = "git commit -m \"" + message + "\"";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Git] Commit failed (maybe no changes)\n";
        // It's okay if there are no changes, we still continue
    }

    cmd = "git push origin " + branch;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Git] Push failed\n";
        return false;
    }

    return true;
}

bool Git::createPullRequest(const std::string& base, const std::string& head) {
    // We don't require best folder for PR creation
    ensureGitignore();
    std::string cmd = "gh pr create --base " + base + " --head " + head
    + " --title \"Daily evolution update\""
    + " --body \"Automated commit from brain evolution\"";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[Git] Failed to create pull request (maybe already exists)\n";
    }
    return (ret == 0);
}
