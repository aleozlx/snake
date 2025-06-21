#!/bin/bash
set -e
mkdir -pv build_x64
set -x
cmake -S . -B build_x64
cmake --build build_x64
