#!/bin/bash
set -e

g++ *.cpp -std=c++17 -Iheader -o test

echo "Build successful. Starting..."
sudo ./test
