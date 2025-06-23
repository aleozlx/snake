# Add refactored version demonstrating event-driven architecture
add_executable(snake2
    ${CMAKE_SOURCE_DIR}/fonts/font0.cpp
    ${CMAKE_SOURCE_DIR}/algorithm/pathfinding.cpp
    snake_draw.cpp
    snake_app.cpp
    snake_ui.cpp
    tile_grid.cpp
    snake2.cpp
)
target_link_libraries(snake2 
    PRIVATE glad 
    PRIVATE SDL2::SDL2 
    PRIVATE circular_buffer
)

# Copy shader files to build directory for both versions
add_custom_command(TARGET snake2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:snake2>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/snake2/shaders
    $<TARGET_FILE_DIR:snake2>/shaders
    COMMENT "Copying shader files to refactored build directory"
)
