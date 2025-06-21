#!/bin/bash
set -e
mkdir -pv build_x64
#git clone https://github.com/glfw/glfw
#git clone https://github.com/Dav1dde/glad
mkdir -pv glfw/build
set -x
# cmake -S glfw -B glfw/build $TOOLCHAIN_FLAG \
#     -DGLFW_BUILD_WAYLAND=OFF \
#     -DCMAKE_C_FLAGS="-D_POSIX_C_SOURCE=199309L -D_GNU_SOURCE -I/home/aleyang/code/hello/external/linux-headers/include" \
#     -DX11_X11_INCLUDE_PATH=/usr/include/X11
# cmake --build glfw/build
#cmake --install glfw/build
cmake -S . -B build_x64 $TOOLCHAIN_FLAG \
    -DCMAKE_C_FLAGS="-I/home/aleyang/code/hello/external/linux-headers/include"
cmake --build build_x64
