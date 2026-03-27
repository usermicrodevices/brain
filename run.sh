#!/bin/bash

# Check for nlohmann/json header in thirdparty
if [ ! -f "thirdparty/nlohmann/json.hpp" ]; then
    echo "nlohmann/json.hpp not found. Downloading..."
    mkdir -p thirdparty/nlohmann
    # Use curl to download the latest single-header version
    curl -L -o thirdparty/nlohmann/json.hpp https://github.com/nlohmann/json/releases/latest/download/json.hpp
    if [ $? -ne 0 ]; then
        echo "Failed to download json.hpp. Please install manually."
        exit 1
    fi
    echo "Download complete."
fi

# Remove old binary
rm -rf brain

# Compile with thirdparty include path
g++ -std=c++20 -pthread -Iinclude -Ithirdparty src/*.cpp -o brain

# Run the program
./brain