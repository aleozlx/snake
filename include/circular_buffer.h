#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

// Configuration macros
#define BUFFER_STAGES 10
#define SLOT_SIZE 1024  // Each slot is 1KB - adjust as needed
#define TOTAL_BUFFER_SIZE (BUFFER_STAGES * SLOT_SIZE + sizeof(CircularBufferHeader))

// Header structure stored at the beginning of the mapped file
struct CircularBufferHeader {
    uint32_t write_index;    // Current write position (0 to BUFFER_STAGES-1)
    uint32_t read_index;     // Current read position (0 to BUFFER_STAGES-1)
    uint32_t total_writes;   // Total number of writes (for debugging/stats)
    uint32_t total_reads;    // Total number of reads (for debugging/stats)
    uint32_t magic_number;   // Magic number to verify file integrity (0xBEEFCAFE)
    char padding[44];        // Padding to make header 64 bytes
};

class MemoryMappedCircularBuffer {
private:
    int fd;                           // File descriptor
    void* mapped_memory;              // Pointer to mapped memory
    CircularBufferHeader* header;     // Pointer to header in mapped memory
    char* buffer_data;                // Pointer to buffer data area
    size_t file_size;                 // Total file size
    bool is_initialized;              // Initialization flag

public:
    MemoryMappedCircularBuffer();
    ~MemoryMappedCircularBuffer();
    
    // Initialize the buffer with a file
    bool initialize(const char* filename);
    
    // Write data to the next slot in the buffer
    bool write_slot(const void* data, size_t data_size);
    
    // Read data from the current read slot
    bool read_slot(void* data, size_t max_size, size_t* bytes_read = nullptr);
    
    // Peek at data without advancing read pointer
    bool peek_slot(void* data, size_t max_size, size_t* bytes_read = nullptr);
    
    // Get pointer to current write slot (for direct access)
    void* get_write_slot_ptr();
    
    // Get pointer to current read slot (for direct access)
    const void* get_read_slot_ptr();
    
    // Advance write pointer (use after direct write)
    void advance_write_pointer();
    
    // Advance read pointer (use after direct read)
    void advance_read_pointer();
    
    // Get buffer statistics
    void get_stats(uint32_t* write_idx, uint32_t* read_idx, 
                   uint32_t* total_writes, uint32_t* total_reads);
    
    // Check if buffer has data to read
    bool has_data();
    
    // Check if buffer is full
    bool is_full();
    
    // Reset buffer (clear all data and reset pointers)
    void reset();
    
    // Cleanup and close
    void cleanup();
    
    // Static helper to create a new buffer file
    static bool create_buffer_file(const char* filename);
};

#endif // CIRCULAR_BUFFER_H 