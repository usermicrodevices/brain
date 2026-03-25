#include "Git.hpp"

bool Git::bestFolderExists() {
    struct stat st;
    if (stat("best", &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    // Quick check: see if there's at least one file (non‑empty)
    std::string cmd = "find best -type f | head -1 | grep -q .";
    return (system(cmd.c_str()) == 0);
}

bool Git::commitAndPush(const std::string& branch, const std::string& message) {
    if (!bestFolderExists()) return false;

    std::string cmd;
    // Ensure we are on the correct branch
    cmd = "git checkout " + branch + " 2>/dev/null || git checkout -b " + branch;
    if (system(cmd.c_str()) != 0) return false;

    // Add the best folder and commit
    cmd = "git add best";
    if (system(cmd.c_str()) != 0) return false;
    cmd = "git commit -m \"" + message + "\"";
    if (system(cmd.c_str()) != 0) return false;

    // Push to remote
    cmd = "git push origin " + branch;
    if (system(cmd.c_str()) != 0) return false;

    return true;
}

bool Git::createPullRequest(const std::string& base, const std::string& head) {
    if (!bestFolderExists()) return false;
    // Use GitHub CLI to create a PR
    std::string cmd = "gh pr create --base " + base + " --head " + head + " --title \"Periodicaly brain update\" --body \"Automated commit from brain evolution\"";
    return (system(cmd.c_str()) == 0);
}
