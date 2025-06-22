cmake_minimum_required(VERSION 3.18)
project(Hello LANGUAGES C CXX)

# find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

# git clone https://github.com/Dav1dde/glad
# python3 -m glad --api "gl:core=3.3" --out-path=../glad_out --reproducible c

add_library(glad STATIC
    glad_out/src/gl.c
)
target_include_directories(glad
    PUBLIC ${CMAKE_SOURCE_DIR}/glad_out/include
    PUBLIC "${CMAKE_SOURCE_DIR}/external/linux-headers/include"
)

# global include directories
include_directories(${CMAKE_SOURCE_DIR}/include)





if(FALSE)
    add_subdirectory(.hello)
endif()

add_subdirectory(ipc)
add_subdirectory(tests)
add_subdirectory(snake0)
add_subdirectory(snake1)
add_subdirectory(snake2)

