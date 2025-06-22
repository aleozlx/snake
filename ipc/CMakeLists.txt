# IPC utilities

# Add circular buffer library
add_library(circular_buffer STATIC
    circular_buffer.cpp
)

# Add buffer dump utility
add_executable(buffer_dump
    buffer_dump.cpp
)

target_link_libraries(buffer_dump
    PRIVATE circular_buffer
)