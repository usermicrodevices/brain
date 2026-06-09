#!/bin/bash

if [ ! -f "thirdparty/nlohmann/json.hpp" ]; then
    echo "nlohmann/json.hpp not found. Downloading..."
    mkdir -p thirdparty/nlohmann
    curl -L -o thirdparty/nlohmann/json.hpp https://github.com/nlohmann/json/releases/latest/download/json.hpp
    if [ $? -ne 0 ]; then
        echo "Failed to download json.hpp. Please install manually."
        exit 1
    fi
    echo "Download complete."
fi

rm -f brain

echo "=== Compiling brain ==="
g++ -std=c++20 -pthread -Iinclude -Ithirdparty src/*.cpp -o brain 2>&1
BUILD_RESULT=$?

if [ $BUILD_RESULT -ne 0 ]; then
    echo "=== Build FAILED (exit code $BUILD_RESULT) ==="
    exit $BUILD_RESULT
fi

echo "=== Build successful ==="

echo "=== Starting brain ==="
exec ./brain
