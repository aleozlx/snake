add_executable(hello0 hello0.c)
add_executable(hello1 hello1.cpp)
target_link_libraries(hello1 PRIVATE glad SDL2::SDL2)
