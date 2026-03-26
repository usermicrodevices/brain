#!/bin/bash
rm -rf brain
g++ -std=c++20 -pthread -Iinclude src/*.cpp -o brain
./brain
