#include "circular_buffer.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Example data structure to store in the buffer
struct GameData {
    uint32_t frame_number;
    float position_x;
    float position_y;
    uint32_t score;
    char player_name[32];
    
    GameData(uint32_t frame = 0, float x = 0.0f, float y = 0.0f, uint32_t s = 0, const char* name = "Player") 
        : frame_number(frame), position_x(x), position_y(y), score(s) {
        strncpy(player_name, name, sizeof(player_name) - 1);
        player_name[sizeof(player_name) - 1] = '\0';
    }
};

void print_game_data(const GameData& data) {
    std::cout << "Frame: " << data.frame_number 
              << ", Pos: (" << data.position_x << ", " << data.position_y << ")"
              << ", Score: " << data.score 
              << ", Player: " << data.player_name << std::endl;
}

int main() {
    std::cout << "=== Memory-Mapped Circular Buffer Test ===" << std::endl;
    
    MemoryMappedCircularBuffer buffer;
    
    // Initialize the buffer with a file
    const char* buffer_file = "game_buffer.dat";
    if (!buffer.initialize(buffer_file)) {
        std::cerr << "Failed to initialize buffer!" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Writing Test Data ===" << std::endl;
    
    // Write some test data
    for (uint32_t i = 0; i < 15; i++) {  // Write more than buffer size to test wrap-around
        GameData game_data(i, i * 10.5f, i * 20.3f, i * 100, "TestPlayer");
        
        if (buffer.write_slot(&game_data, sizeof(game_data))) {
            std::cout << "Written: ";
            print_game_data(game_data);
        } else {
            std::cerr << "Failed to write slot " << i << std::endl;
        }
        
        // Small delay to simulate real-time writing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n=== Buffer Statistics ===" << std::endl;
    uint32_t write_idx, read_idx, total_writes, total_reads;
    buffer.get_stats(&write_idx, &read_idx, &total_writes, &total_reads);
    std::cout << "Write index: " << write_idx << std::endl;
    std::cout << "Read index: " << read_idx << std::endl;
    std::cout << "Total writes: " << total_writes << std::endl;
    std::cout << "Total reads: " << total_reads << std::endl;
    std::cout << "Has data: " << (buffer.has_data() ? "Yes" : "No") << std::endl;
    std::cout << "Is full: " << (buffer.is_full() ? "Yes" : "No") << std::endl;
    
    std::cout << "\n=== Reading Test Data ===" << std::endl;
    
    // Read all available data
    GameData read_data;
    size_t bytes_read;
    int read_count = 0;
    
    while (buffer.has_data() && read_count < 20) {  // Safety limit
        if (buffer.read_slot(&read_data, sizeof(read_data), &bytes_read)) {
            std::cout << "Read (" << bytes_read << " bytes): ";
            print_game_data(read_data);
            read_count++;
        } else {
            std::cerr << "Failed to read slot" << std::endl;
            break;
        }
    }
    
    std::cout << "\n=== Final Buffer Statistics ===" << std::endl;
    buffer.get_stats(&write_idx, &read_idx, &total_writes, &total_reads);
    std::cout << "Write index: " << write_idx << std::endl;
    std::cout << "Read index: " << read_idx << std::endl;
    std::cout << "Total writes: " << total_writes << std::endl;
    std::cout << "Total reads: " << total_reads << std::endl;
    std::cout << "Has data: " << (buffer.has_data() ? "Yes" : "No") << std::endl;
    
    std::cout << "\n=== Direct Access Test ===" << std::endl;
    
    // Test direct access (pointer-based)
    GameData direct_data(999, 123.45f, 678.90f, 9999, "DirectPlayer");
    
    // Get direct write pointer and write data
    void* write_ptr = buffer.get_write_slot_ptr();
    if (write_ptr) {
        memcpy(write_ptr, &direct_data, sizeof(direct_data));
        buffer.advance_write_pointer();
        std::cout << "Direct write successful" << std::endl;
    }
    
    // Get direct read pointer and read data
    const void* read_ptr = buffer.get_read_slot_ptr();
    if (read_ptr) {
        GameData direct_read_data;
        memcpy(&direct_read_data, read_ptr, sizeof(direct_read_data));
        buffer.advance_read_pointer();
        std::cout << "Direct read: ";
        print_game_data(direct_read_data);
    }
    
    std::cout << "\n=== Peek Test ===" << std::endl;
    
    // Write one more item to test peek
    GameData peek_data(777, 11.11f, 22.22f, 777, "PeekPlayer");
    buffer.write_slot(&peek_data, sizeof(peek_data));
    
    // Peek at the data without consuming it
    GameData peeked_data;
    if (buffer.peek_slot(&peeked_data, sizeof(peeked_data))) {
        std::cout << "Peeked: ";
        print_game_data(peeked_data);
    }
    
    // Verify the data is still there by reading it
    GameData final_read_data;
    if (buffer.read_slot(&final_read_data, sizeof(final_read_data))) {
        std::cout << "Read after peek: ";
        print_game_data(final_read_data);
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Buffer file '" << buffer_file << "' created and persisted." << std::endl;
    std::cout << "You can run this program again to see the persistent state." << std::endl;
    
    return 0;
} 