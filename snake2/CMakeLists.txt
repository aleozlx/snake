add_executable(snake2
    ${CMAKE_SOURCE_DIR}/fonts/font0.cpp
    snake2.cpp
)
target_link_libraries(snake2 
    PRIVATE glad 
    PRIVATE SDL2::SDL2 
    PRIVATE circular_buffer
)
