# Memory-Mapped Circular Buffer

A high-performance circular buffer implementation that uses memory-mapped files for persistence and efficiency.

## Features

- **Fixed Size Buffer**: 10 stages with configurable slot size (default 1KB per slot)
- **Memory Mapped**: Uses `mmap()` for direct memory access to file storage
- **Persistent**: Data survives program restarts
- **Thread-Safe Friendly**: Designed for single writer, single reader scenarios
- **Cross-Platform**: Works on Unix/Linux systems with `mmap()` support

## Configuration

The buffer can be configured using these macros in `include/circular_buffer.h`:

```cpp
#define BUFFER_STAGES 10        // Number of slots in the circular buffer
#define SLOT_SIZE 1024         // Size of each slot in bytes
```

## File Structure

The memory-mapped file contains:
1. **Header** (64 bytes): Contains metadata and pointers
2. **Buffer Data**: 10 slots × 1024 bytes each = 10KB

Total file size: 64 bytes + 10KB = 10,304 bytes

## Usage Examples

### Basic Usage

```cpp
#include "include/circular_buffer.h"

MemoryMappedCircularBuffer buffer;

// Initialize with a file
if (!buffer.initialize("my_buffer.dat")) {
    // Handle error
    return -1;
}

// Write data
char data[] = "Hello, World!";
buffer.write_slot(data, strlen(data) + 1);

// Read data
char read_buffer[SLOT_SIZE];
size_t bytes_read;
if (buffer.read_slot(read_buffer, sizeof(read_buffer), &bytes_read)) {
    printf("Read %zu bytes: %s\n", bytes_read, read_buffer);
}
```

### Direct Memory Access

```cpp
// Get direct pointer to write slot
void* write_ptr = buffer.get_write_slot_ptr();
if (write_ptr) {
    // Write directly to memory
    strcpy((char*)write_ptr, "Direct write!");
    buffer.advance_write_pointer();
}

// Get direct pointer to read slot
const void* read_ptr = buffer.get_read_slot_ptr();
if (read_ptr) {
    printf("Direct read: %s\n", (const char*)read_ptr);
    buffer.advance_read_pointer();
}
```

### Structured Data

```cpp
struct GameState {
    uint32_t frame;
    float x, y;
    uint32_t score;
};

GameState state = {100, 10.5f, 20.3f, 1500};

// Write structured data
buffer.write_slot(&state, sizeof(state));

// Read structured data
GameState read_state;
buffer.read_slot(&read_state, sizeof(read_state));
```

## API Reference

### Constructor/Destructor
- `MemoryMappedCircularBuffer()` - Constructor
- `~MemoryMappedCircularBuffer()` - Destructor (calls cleanup automatically)

### Initialization
- `bool initialize(const char* filename)` - Initialize buffer with file
- `static bool create_buffer_file(const char* filename)` - Create new buffer file

### Writing Data
- `bool write_slot(const void* data, size_t data_size)` - Write data to next slot
- `void* get_write_slot_ptr()` - Get direct write pointer
- `void advance_write_pointer()` - Advance write pointer (use with direct access)

### Reading Data
- `bool read_slot(void* data, size_t max_size, size_t* bytes_read)` - Read from current slot
- `bool peek_slot(void* data, size_t max_size, size_t* bytes_read)` - Peek without advancing
- `const void* get_read_slot_ptr()` - Get direct read pointer
- `void advance_read_pointer()` - Advance read pointer (use with direct access)

### Status/Control
- `bool has_data()` - Check if buffer has unread data
- `bool is_full()` - Check if buffer is full
- `void get_stats(...)` - Get buffer statistics
- `void reset()` - Clear buffer and reset pointers
- `void cleanup()` - Cleanup and close buffer

## Building

```bash
# Build the test program
mkdir build && cd build
cmake ..
make buffer_test

# Run the test
./buffer_test

# Build the buffer dump utility
mkdir build && cd build
cmake ..
make buffer_dump

# Run the buffer dump utility
./buffer_dump -n 64 snake2.dat
```

## Buffer Dump Utility

The `buffer_dump` utility is a hexdump-like program that allows you to inspect the contents of circular buffer files.

### Usage

```bash
./buffer_dump [options] <buffer_file>

Options:
  -n <bytes>    Number of bytes to dump per slot (default: 64)
  -h            Show help message

Examples:
  ./buffer_dump snake2.dat                    # Dump first 64 bytes of each slot
  ./buffer_dump -n 128 snake2.dat            # Dump first 128 bytes of each slot
  ./buffer_dump -n 1024 game_buffer.dat      # Dump entire slot contents
```

### Sample Output

```
=== CIRCULAR BUFFER HEADER ===
Write index:  3
Read index:   1
Total writes: 15
Total reads:  12
Magic number: 0xbeefcafe
=============================

Dumping 64 bytes from each of 10 slots:

slot[ 0] 00000000: 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23  |################|
slot[ 0] 00000010: 23 20 46 20 73 20 73 20 50 20 20 20 20 20 20 20  |# F s s P       |
...

slot[ 1] 00000000: 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23  |################|
slot[ 1] 00000010: 23 20 46 20 73 20 73 20 50 20 20 20 20 20 20 20  |# F s s P       |
...
```

The output shows:
- **Header Information**: Current read/write indices, total operations, and magic number
- **Slot Data**: Hexadecimal dump of each slot with ASCII representation
- **Format**: `slot[N] offset: hex_bytes |ascii_representation|`

## Performance Characteristics

- **Memory Access**: Direct memory access via `mmap()` - no system call overhead
- **Persistence**: Automatic synchronization to disk with `msync()`
- **Cache Friendly**: Sequential access patterns for optimal CPU cache usage
- **Lock-Free**: Single writer, single reader design requires no locking

## Use Cases

1. **Game State Recording**: Record game frames for replay systems
2. **Inter-Process Communication**: Share data between processes
3. **Data Logging**: High-performance logging with persistence
4. **Sensor Data Buffering**: Buffer sensor readings for processing
5. **Audio/Video Streaming**: Buffer media data with low latency

## Thread Safety Notes

- The buffer is designed for **single writer, single reader** scenarios
- For multi-threaded access, add your own synchronization (mutexes, semaphores)
- The `msync()` calls ensure data is written to disk but don't provide thread safety

## Error Handling

- All functions return `bool` or `nullptr` to indicate success/failure
- Check return values and handle errors appropriately
- Use `perror()` or `strerror(errno)` for system call error details

## Platform Support

- **Linux**: Full support with `mmap()`, `msync()`, etc.
- **macOS**: Full support (BSD-style mmap)
- **Windows**: Would require adaptation to use `CreateFileMapping()` and `MapViewOfFile()`

## Memory Layout

```
File Layout:
[Header - 64 bytes]
  ├── write_index (4 bytes)
  ├── read_index (4 bytes)  
  ├── total_writes (4 bytes)
  ├── total_reads (4 bytes)
  ├── magic_number (4 bytes)
  └── padding (44 bytes)
[Slot 0 - 1024 bytes]
[Slot 1 - 1024 bytes]
...
[Slot 9 - 1024 bytes]
```

## Example Output

```
=== Memory-Mapped Circular Buffer Test ===
Created buffer file: game_buffer.dat (Size: 10304 bytes)
Memory-mapped circular buffer initialized:
  File: game_buffer.dat
  Size: 10304 bytes
  Stages: 10
  Slot size: 1024 bytes
  Write index: 0
  Read index: 0
  Total writes: 0
  Total reads: 0
``` 