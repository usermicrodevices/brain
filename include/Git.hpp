#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

class Git {
public:
    // Commit and push the entire best/ directory to the specified branch.
    static bool commitAndPush(const std::string& branch, const std::string& message);

    // Create a pull request from head branch to base branch.
    static bool createPullRequest(const std::string& base, const std::string& head);

    // Check if the best/ directory exists and contains any files.
    static bool bestFolderExists();
};
