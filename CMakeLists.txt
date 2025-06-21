cmake_minimum_required(VERSION 3.18)
project(Hello LANGUAGES C CXX)

add_executable(hello hello0.c)
# target_link_options(hello PRIVATE -static)

find_package(OpenGL REQUIRED)
#find_package(glfw3 REQUIRED)

# python3 -m glad --api "gl:core=3.3" --out-path=../glad_out --reproducible c

add_executable(triangle
    glad_out/src/gl.c
    hello1.cpp
)
target_include_directories(triangle
    PRIVATE glad_out/include
    PRIVATE glfw/include
    PRIVATE "${CMAKE_SOURCE_DIR}/external/linux-headers/include"
)
target_link_libraries(triangle
    #PRIVATE glfw
    PRIVATE "${CMAKE_SOURCE_DIR}/glfw/build/src/libglfw3.a"
    PRIVATE OpenGL::GL)


add_executable(snake_basic
    glad_out/src/gl.c
    snake0.cpp
)
target_include_directories(snake_basic
    PRIVATE glad_out/include
    PRIVATE glfw/include
    PRIVATE "${CMAKE_SOURCE_DIR}/external/linux-headers/include"
)
target_link_libraries(snake_basic
    #PRIVATE glfw
    PRIVATE "${CMAKE_SOURCE_DIR}/glfw/build/src/libglfw3.a"
    PRIVATE OpenGL::GL)


add_executable(snake_lvl1
    glad_out/src/gl.c
    font0.cpp
    snake1.cpp
)
target_include_directories(snake_lvl1
    PRIVATE glad_out/include
    PRIVATE glfw/include
    PRIVATE "${CMAKE_SOURCE_DIR}/external/linux-headers/include"
)
target_link_libraries(snake_lvl1
    #PRIVATE glfw
    PRIVATE "${CMAKE_SOURCE_DIR}/glfw/build/src/libglfw3.a"
    PRIVATE OpenGL::GL)

find_package(SDL2 REQUIRED)

add_executable(snake_lvl1_sdl
    glad_out/src/gl.c
    font0.cpp
    snake1_sdl.cpp
)
target_include_directories(snake_lvl1_sdl
    PRIVATE glad_out/include
    PRIVATE "${CMAKE_SOURCE_DIR}/external/linux-headers/include"
)
target_link_libraries(snake_lvl1_sdl
    PRIVATE OpenGL::GL
    PRIVATE SDL2::SDL2)

add_executable(snake
    glad_out/src/gl.c
    font0.cpp
    snake2.cpp
)
target_include_directories(snake
    PRIVATE glad_out/include
    PRIVATE "${CMAKE_SOURCE_DIR}/external/linux-headers/include"
)
target_link_libraries(snake
    PRIVATE OpenGL::GL
    PRIVATE SDL2::SDL2)

