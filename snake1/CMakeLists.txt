add_executable(snake1
    ${CMAKE_SOURCE_DIR}/fonts/font0.cpp
    snake1.cpp
)
target_link_libraries(snake1 PRIVATE glad SDL2::SDL2)
