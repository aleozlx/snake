#!/bin/bash
set -e
mkdir -pv build_x64
#git clone https://github.com/glfw/glfw
#git clone https://github.com/Dav1dde/glad
mkdir -pv glfw/build
SYSROOT="/opt/homebrew/opt/x86_64-unknown-linux-gnu/toolchain/x86_64-unknown-linux-gnu/sysroot"
TOOLCHAIN_FLAG="-DCMAKE_TOOLCHAIN_FILE=${PWD}/x86_64-linux-gnu.toolchain.cmake"
set -x
cmake -S glfw -B glfw/build $TOOLCHAIN_FLAG \
    -DCMAKE_INSTALL_PREFIX=$SYSROOT/usr \
    -DGLFW_BUILD_WAYLAND=OFF \
    -DCMAKE_C_FLAGS="-D_POSIX_C_SOURCE=199309L -D_GNU_SOURCE"
cmake --build glfw/build
cmake --install glfw/build
cmake -S . -B build_x64 $TOOLCHAIN_FLAG
cmake --build build_x64
