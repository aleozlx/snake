# Test executables

# Add circular buffer test executable
add_executable(buffer_test
    buffer_test.cpp
)
target_link_libraries(buffer_test
    circular_buffer
) 