#!/bin/bash
set -e

g++ *.cpp -Iheader -o test

echo "Build successful. Starting..."
sudo ./test
