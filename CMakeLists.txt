cmake_minimum_required(VERSION 3.18)
project(Hello LANGUAGES C CXX)

# Enable testing
enable_testing()

# Generate compile_commands.json for better LLM code analysis
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static analysis integration
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

# Development build options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Add sanitizers for debug builds
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
endif()

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
include_directories(${CMAKE_SOURCE_DIR}/algorithm)

if(FALSE)
    add_subdirectory(.hello)
endif()

add_subdirectory(ipc)
add_subdirectory(tests)
add_subdirectory(snake0)
add_subdirectory(snake1)
add_subdirectory(snake2)

# Add test targets
add_test(NAME buffer_test COMMAND tests/buffer_test)
# add_test(NAME snake2_basic_test COMMAND snake2/snake2 --version)

