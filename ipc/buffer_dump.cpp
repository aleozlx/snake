#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <cctype>
#include "../include/circular_buffer.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <buffer_file>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -n <bytes>    Number of bytes to dump per slot (default: 64)" << std::endl;
    std::cout << "  -c <columns>  Number of bytes per row in hex dump (default: 16)" << std::endl;
    std::cout << "  -h            Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " -n 128 -c 32 snake2.dat" << std::endl;
}

void printHexDump(const char* data, size_t size, size_t slot_number, size_t offset_in_slot, size_t bytes_per_line = 16) {
    for (size_t i = 0; i < size; i += bytes_per_line) {
        // Print slot number and offset
        std::cout << "slot[" << std::setw(2) << slot_number << "] " 
                  << std::setfill('0') << std::setw(8) << std::hex 
                  << (offset_in_slot + i) << ": ";
        
        // Print hex bytes
        for (size_t j = 0; j < bytes_per_line; j++) {
            if (i + j < size) {
                std::cout << std::setw(2) << std::hex 
                          << (static_cast<unsigned char>(data[i + j]) & 0xFF) << " ";
            } else {
                std::cout << "   ";  // Padding for incomplete lines
            }
        }
        
        std::cout << " |";
        
        // Print ASCII representation
        for (size_t j = 0; j < bytes_per_line && i + j < size; j++) {
            char c = data[i + j];
            std::cout << (std::isprint(c) ? c : '.');
        }
        
        std::cout << "|" << std::dec << std::endl;
    }
}

void printHeader(const CircularBufferHeader& header) {
    std::cout << "=== CIRCULAR BUFFER HEADER ===" << std::endl;
    std::cout << "Write index:  " << header.write_index << std::endl;
    std::cout << "Read index:   " << header.read_index << std::endl;
    std::cout << "Total writes: " << header.total_writes << std::endl;
    std::cout << "Total reads:  " << header.total_reads << std::endl;
    std::cout << "Magic number: 0x" << std::hex << header.magic_number << std::dec << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    const char* filename = nullptr;
    size_t bytes_to_dump = 64;  // Default: dump 64 bytes per slot
    size_t bytes_per_line = 16; // Default: 16 bytes per row
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: -n requires a number argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            bytes_to_dump = std::stoul(argv[++i]);
            if (bytes_to_dump > SLOT_SIZE) {
                std::cerr << "Warning: Requested " << bytes_to_dump 
                          << " bytes, but slot size is only " << SLOT_SIZE 
                          << " bytes. Limiting to " << SLOT_SIZE << "." << std::endl;
                bytes_to_dump = SLOT_SIZE;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: -c requires a number argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            bytes_per_line = std::stoul(argv[++i]);
            if (bytes_per_line == 0) {
                std::cerr << "Error: Column size must be greater than 0" << std::endl;
                return 1;
            }
            if (bytes_per_line > 256) {
                std::cerr << "Warning: Large column size (" << bytes_per_line 
                          << ") may produce very wide output" << std::endl;
            }
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        } else {
            std::cerr << "Error: Unknown option " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (!filename) {
        std::cerr << "Error: Buffer file not specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Open the buffer file
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return 1;
    }
    
    // Read and display the header
    CircularBufferHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file) {
        std::cerr << "Error: Cannot read header from " << filename << std::endl;
        return 1;
    }
    
    printHeader(header);
    
    // Verify magic number
    if (header.magic_number != 0xBEEFCAFE) {
        std::cerr << "Warning: Magic number mismatch. Expected 0xBEEFCAFE, got 0x" 
                  << std::hex << header.magic_number << std::dec << std::endl;
    }
    
    // Read and dump each slot
    char* buffer = new char[bytes_to_dump];
    
    std::cout << "Dumping " << bytes_to_dump << " bytes from each of " 
              << BUFFER_STAGES << " slots:" << std::endl;
    std::cout << std::endl;
    
    for (int slot = 0; slot < BUFFER_STAGES; slot++) {
        // Calculate offset for this slot (header + slot_number * SLOT_SIZE)
        size_t slot_offset = sizeof(CircularBufferHeader) + (slot * SLOT_SIZE);
        
        // Seek to the beginning of this slot
        file.seekg(slot_offset);
        if (!file) {
            std::cerr << "Error: Cannot seek to slot " << slot << std::endl;
            break;
        }
        
        // Read the requested number of bytes from this slot
        file.read(buffer, bytes_to_dump);
        size_t bytes_read = file.gcount();
        
        if (bytes_read == 0) {
            std::cout << "slot[" << std::setw(2) << slot << "] <empty or EOF>" << std::endl;
            continue;
        }
        
        // Print the hex dump for this slot
        printHexDump(buffer, bytes_read, slot, 0, bytes_per_line);
        std::cout << std::endl;
    }
    
    delete[] buffer;
    file.close();
    
    return 0;
} 