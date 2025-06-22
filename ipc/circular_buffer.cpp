#include "circular_buffer.h"
#include <iostream>

#define MAGIC_NUMBER 0xBEEFCAFE

MemoryMappedCircularBuffer::MemoryMappedCircularBuffer() 
    : fd(-1), mapped_memory(MAP_FAILED), header(nullptr), 
      buffer_data(nullptr), file_size(0), is_initialized(false) {
}

MemoryMappedCircularBuffer::~MemoryMappedCircularBuffer() {
    cleanup();
}

bool MemoryMappedCircularBuffer::create_buffer_file(const char* filename) {
    int fd = open(filename, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        std::cerr << "Failed to create buffer file: " << filename << std::endl;
        return false;
    }
    
    // Set file size
    if (ftruncate(fd, TOTAL_BUFFER_SIZE) == -1) {
        std::cerr << "Failed to set file size" << std::endl;
        close(fd);
        return false;
    }
    
    // Initialize header
    CircularBufferHeader header = {0};
    header.write_index = 0;
    header.read_index = 0;
    header.total_writes = 0;
    header.total_reads = 0;
    header.magic_number = MAGIC_NUMBER;
    
    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
        std::cerr << "Failed to write initial header" << std::endl;
        close(fd);
        return false;
    }
    
    close(fd);
    std::cout << "Created buffer file: " << filename << " (Size: " << TOTAL_BUFFER_SIZE << " bytes)" << std::endl;
    return true;
}

bool MemoryMappedCircularBuffer::initialize(const char* filename) {
    if (is_initialized) {
        std::cerr << "Buffer already initialized" << std::endl;
        return false;
    }
    
    // Try to open existing file, create if it doesn't exist
    fd = open(filename, O_RDWR);
    if (fd == -1) {
        std::cout << "File doesn't exist, creating new buffer file..." << std::endl;
        if (!create_buffer_file(filename)) {
            return false;
        }
        fd = open(filename, O_RDWR);
        if (fd == -1) {
            std::cerr << "Failed to open newly created file" << std::endl;
            return false;
        }
    }
    
    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "Failed to get file size" << std::endl;
        close(fd);
        fd = -1;
        return false;
    }
    
    file_size = sb.st_size;
    
    // Verify file size
    if (file_size < TOTAL_BUFFER_SIZE) {
        std::cout << "File too small, extending to required size..." << std::endl;
        if (ftruncate(fd, TOTAL_BUFFER_SIZE) == -1) {
            std::cerr << "Failed to extend file size" << std::endl;
            close(fd);
            fd = -1;
            return false;
        }
        file_size = TOTAL_BUFFER_SIZE;
    }
    
    // Memory map the file
    mapped_memory = mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_memory == MAP_FAILED) {
        std::cerr << "Failed to memory map file" << std::endl;
        close(fd);
        fd = -1;
        return false;
    }
    
    // Set up pointers
    header = static_cast<CircularBufferHeader*>(mapped_memory);
    buffer_data = static_cast<char*>(mapped_memory) + sizeof(CircularBufferHeader);
    
    // Verify magic number
    if (header->magic_number != MAGIC_NUMBER) {
        std::cout << "Invalid or uninitialized file, resetting..." << std::endl;
        reset();
    }
    
    is_initialized = true;
    
    std::cout << "Memory-mapped circular buffer initialized:" << std::endl;
    std::cout << "  File: " << filename << std::endl;
    std::cout << "  Size: " << file_size << " bytes" << std::endl;
    std::cout << "  Stages: " << BUFFER_STAGES << std::endl;
    std::cout << "  Slot size: " << SLOT_SIZE << " bytes" << std::endl;
    std::cout << "  Write index: " << header->write_index << std::endl;
    std::cout << "  Read index: " << header->read_index << std::endl;
    std::cout << "  Total writes: " << header->total_writes << std::endl;
    std::cout << "  Total reads: " << header->total_reads << std::endl;
    
    return true;
}

bool MemoryMappedCircularBuffer::write_slot(const void* data, size_t data_size) {
    if (!is_initialized || !data) {
        return false;
    }
    
    if (data_size > SLOT_SIZE) {
        std::cerr << "Data size (" << data_size << ") exceeds slot size (" << SLOT_SIZE << ")" << std::endl;
        return false;
    }
    
    // Calculate write position
    char* write_ptr = buffer_data + (header->write_index * SLOT_SIZE);
    
    // Clear the slot first
    memset(write_ptr, 0, SLOT_SIZE);
    
    // Copy data
    memcpy(write_ptr, data, data_size);
    
    // Update header
    header->write_index = (header->write_index + 1) % BUFFER_STAGES;
    header->total_writes++;
    
    // Force write to disk
    msync(mapped_memory, sizeof(CircularBufferHeader), MS_SYNC);
    
    return true;
}

bool MemoryMappedCircularBuffer::read_slot(void* data, size_t max_size, size_t* bytes_read) {
    if (!is_initialized || !data) {
        return false;
    }
    
    if (!has_data()) {
        if (bytes_read) *bytes_read = 0;
        return false;
    }
    
    // Calculate read position
    const char* read_ptr = buffer_data + (header->read_index * SLOT_SIZE);
    
    // Copy data
    size_t copy_size = std::min(max_size, static_cast<size_t>(SLOT_SIZE));
    memcpy(data, read_ptr, copy_size);
    
    if (bytes_read) {
        *bytes_read = copy_size;
    }
    
    // Update header
    header->read_index = (header->read_index + 1) % BUFFER_STAGES;
    header->total_reads++;
    
    // Force write to disk
    msync(mapped_memory, sizeof(CircularBufferHeader), MS_SYNC);
    
    return true;
}

bool MemoryMappedCircularBuffer::peek_slot(void* data, size_t max_size, size_t* bytes_read) {
    if (!is_initialized || !data) {
        return false;
    }
    
    if (!has_data()) {
        if (bytes_read) *bytes_read = 0;
        return false;
    }
    
    // Calculate read position (without advancing)
    const char* read_ptr = buffer_data + (header->read_index * SLOT_SIZE);
    
    // Copy data
    size_t copy_size = std::min(max_size, static_cast<size_t>(SLOT_SIZE));
    memcpy(data, read_ptr, copy_size);
    
    if (bytes_read) {
        *bytes_read = copy_size;
    }
    
    return true;
}

void* MemoryMappedCircularBuffer::get_write_slot_ptr() {
    if (!is_initialized) {
        return nullptr;
    }
    
    return buffer_data + (header->write_index * SLOT_SIZE);
}

const void* MemoryMappedCircularBuffer::get_read_slot_ptr() {
    if (!is_initialized || !has_data()) {
        return nullptr;
    }
    
    return buffer_data + (header->read_index * SLOT_SIZE);
}

void MemoryMappedCircularBuffer::advance_write_pointer() {
    if (!is_initialized) {
        return;
    }
    
    header->write_index = (header->write_index + 1) % BUFFER_STAGES;
    header->total_writes++;
    
    // Force write to disk
    msync(mapped_memory, sizeof(CircularBufferHeader), MS_SYNC);
}

void MemoryMappedCircularBuffer::advance_read_pointer() {
    if (!is_initialized || !has_data()) {
        return;
    }
    
    header->read_index = (header->read_index + 1) % BUFFER_STAGES;
    header->total_reads++;
    
    // Force write to disk
    msync(mapped_memory, sizeof(CircularBufferHeader), MS_SYNC);
}

void MemoryMappedCircularBuffer::get_stats(uint32_t* write_idx, uint32_t* read_idx, 
                                          uint32_t* total_writes, uint32_t* total_reads) {
    if (!is_initialized) {
        return;
    }
    
    if (write_idx) *write_idx = header->write_index;
    if (read_idx) *read_idx = header->read_index;
    if (total_writes) *total_writes = header->total_writes;
    if (total_reads) *total_reads = header->total_reads;
}

bool MemoryMappedCircularBuffer::has_data() {
    if (!is_initialized) {
        return false;
    }
    
    return header->read_index != header->write_index;
}

bool MemoryMappedCircularBuffer::is_full() {
    if (!is_initialized) {
        return false;
    }
    
    return ((header->write_index + 1) % BUFFER_STAGES) == header->read_index;
}

void MemoryMappedCircularBuffer::reset() {
    if (!is_initialized) {
        return;
    }
    
    // Clear header
    header->write_index = 0;
    header->read_index = 0;
    header->total_writes = 0;
    header->total_reads = 0;
    header->magic_number = MAGIC_NUMBER;
    
    // Clear buffer data
    memset(buffer_data, 0, BUFFER_STAGES * SLOT_SIZE);
    
    // Force write to disk
    msync(mapped_memory, file_size, MS_SYNC);
    
    std::cout << "Buffer reset" << std::endl;
}

void MemoryMappedCircularBuffer::cleanup() {
    if (mapped_memory != MAP_FAILED) {
        // Sync any pending writes
        msync(mapped_memory, file_size, MS_SYNC);
        
        // Unmap memory
        munmap(mapped_memory, file_size);
        mapped_memory = MAP_FAILED;
    }
    
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    
    header = nullptr;
    buffer_data = nullptr;
    file_size = 0;
    is_initialized = false;
} 