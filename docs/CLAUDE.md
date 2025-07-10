# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a C++ project using CMake. The main build commands are:

```bash
# Create build directory and configure
mkdir -p build_x64 && cd build_x64
cmake -G Ninja -S . -B build_x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=znver2 -mtune=znver2 -O3"

# Build all targets
cmake --build build_x64

# Build specific targets
cmake --build build_x64 --target snake_basic   # Basic snake game (snake0)
cmake --build build_x64 --target snake1        # Enhanced version with fonts
cmake --build build_x64 --target snake2        # Refactored version with event system
cmake --build build_x64 --target buffer_test   # Test for circular buffer
cmake --build build_x64 --target buffer_dump   # Utility to inspect buffer files
```

## Testing

Run tests with:
```bash
cd build_x64
./tests/buffer_test
```

## Project Architecture

This is a Snake game implementation with three evolutionary versions demonstrating different architectural approaches:

### Core Components

- **snake0/**: Basic implementation (`snake_basic` executable)
- **snake1/**: Enhanced version with font rendering (`snake1` executable) 
- **snake2/**: Fully refactored version with clean event-driven architecture (`snake2` executable)

### Key Libraries and Systems

1. **Event System**: snake2 uses a clean event-driven architecture with `EventSystem` class supporting subscription/publishing of game events (GAME_TICK, GAME_RENDER, INPUT_*, etc.)

2. **Memory-Mapped Circular Buffer**: High-performance IPC system using `MemoryMappedCircularBuffer` class:
   - Located in `ipc/` directory
   - Uses memory-mapped files for persistence
   - 10 stages with 1KB slots
   - Single writer, single reader design
   - Comprehensive documentation in `ipc/CIRCULAR_BUFFER_README.md`

3. **Pathfinding Algorithm**: A* pathfinding implementation in `algorithm/`:
   - Multiple pathfinding strategies (naive, A*, greedy axis)
   - Callback-based collision detection
   - Used for AI snake movement

4. **Graphics Pipeline**: OpenGL-based rendering with:
   - Custom shader system (`snake2/shaders/`)
   - Texture support for sprites
   - Font rendering system

### Dependencies

- **SDL2**: Window management and input handling
- **OpenGL**: Graphics rendering (using GLAD loader)
- **POSIX**: Memory mapping for circular buffer (Linux/Unix only)

### Key Design Patterns

- **PIMPL Pattern**: `SnakeApp` class hides implementation details
- **Event-Driven Architecture**: Clean separation between input, game logic, and rendering
- **Component-Based Design**: Modular systems (UI, drawing, tile grid)

### File Structure

- `include/`: Shared headers (`snake_app.h`, `snake_types.h`, `circular_buffer.h`)
- `algorithm/`: Pathfinding algorithms
- `fonts/`: Font rendering data
- `glad_out/`: OpenGL loader (generated)
- `ipc/`: Inter-process communication with circular buffer
- `tests/`: Unit tests for components

### Development Notes

- The project evolved from snake0 (basic) → snake1 (enhanced) → snake2 (refactored)
- snake2 represents the clean, maintainable architecture with proper separation of concerns
- Shader files are automatically copied to build directory for snake2
- Buffer files (`.dat`) are created for IPC functionality