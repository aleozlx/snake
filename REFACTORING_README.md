# Snake Game Refactoring - Event-Driven Architecture

This document explains the refactoring of the snake game from a monolithic structure to an event-driven architecture.

## Overview

The original `snake2.cpp` was a large monolithic file (3000+ lines) that mixed application infrastructure concerns with game logic. The refactored version demonstrates proper separation of concerns using an event-driven architecture.

## Files Created

### 1. `include/snake_app.h`
- **Purpose**: Defines the event system interface
- **Contents**: 
  - Event types enum
  - Event structure
  - EventSystem abstract interface
- **Benefits**: Clean API for event-driven programming

### 2. `snake2/snake2_refactored.cpp`
- **Purpose**: Complete refactored implementation demonstrating the new architecture
- **Structure**:
  - **Event System**: Generic event publishing/subscription mechanism
  - **Application Infrastructure**: Handles SDL2/OpenGL initialization, main loop, input handling
  - **Game Logic**: Pure game state and rules, subscribes to events
  - **Main Function**: Simplified orchestration

## Architecture Benefits

### Separation of Concerns
- **Application Infrastructure**: SDL2/OpenGL setup, window management, input handling
- **Game Logic**: Snake movement, collision detection, scoring
- **Event System**: Decoupled communication between components

### Event-Driven Design
```cpp
// Game logic subscribes to events
eventSystem->subscribe(EventType::GAME_TICK, 
    [this](const Event& e) { onGameTick(e); });
eventSystem->subscribe(EventType::INPUT_KEYBOARD, 
    [this](const Event& e) { onKeyboardInput(e); });

// Infrastructure publishes events
Event tickEvent;
tickEvent.type = EventType::GAME_TICK;
eventSystem->publish(tickEvent);
```

### Clean Main Function
```cpp
int main(int argc, char* argv[]) {
    // Create app infrastructure
    auto appInfra = std::make_unique<SnakeAppInfrastructure>();
    appInfra->initialize(config);
    
    // Create game logic (auto-subscribes to events)
    auto gameLogic = std::make_unique<SnakeGameLogic>(appInfra.get());
    
    // Run event-driven loop
    appInfra->run();
    
    return 0;
}
```

## Event Types

The system defines several event types:

- `GAME_TICK`: Regular game update events
- `GAME_RENDER`: Rendering events
- `GAME_EXIT`: Application shutdown
- `SNAKE_FOOD_EATEN`: Snake ate food
- `INPUT_KEYBOARD`: Keyboard input
- `INPUT_GAMEPAD_BUTTON`: Gamepad button input

## Key Classes

### SnakeAppInfrastructure
- Manages SDL2/OpenGL resources
- Handles input and publishes input events
- Runs the main game loop
- Publishes timing events (GAME_TICK, GAME_RENDER)

### SnakeGameLogic
- Subscribes to relevant events
- Maintains game state (snakes, food, score)
- Implements game rules and logic
- Renders game elements during GAME_RENDER events

### EventSystem
- Generic publish/subscribe mechanism
- Type-safe event routing
- Decouples producers from consumers

## Building

The refactored version is built alongside the original:

```bash
cd build_x64
make snake2_refactored  # Build refactored version
make snake2             # Build original version
```

## Running

```bash
# Run refactored version
./snake2_refactored

# Run with IPC mode
./snake2_refactored -e
```

## Controls

Same as original:
- **Arrow Keys/WASD**: Move snake
- **Space**: Pause/unpause
- **R**: Reset game
- **Gamepad**: D-pad for movement, A for speed up, X for pause, Y for reset

## Benefits of This Architecture

1. **Maintainability**: Clear separation makes code easier to understand and modify
2. **Testability**: Game logic can be tested independently of rendering/input
3. **Extensibility**: New features can be added as event subscribers
4. **Modularity**: Components can be developed and tested independently
5. **Reusability**: Event system can be reused for other games

## Potential Extensions

This architecture enables easy addition of:
- **Network multiplayer**: Network events
- **AI opponents**: AI events
- **Physics system**: Physics events
- **Audio system**: Audio events
- **Save/load system**: Persistence events
- **Mod support**: Plugin events

## Comparison with Original

| Aspect | Original | Refactored |
|--------|----------|------------|
| Lines of code | 3000+ in single file | Split into focused classes |
| Main function | Complex setup/loop | Simple orchestration |
| Testing | Difficult (monolithic) | Easy (isolated components) |
| Adding features | Modify existing code | Subscribe to events |
| Code organization | Mixed concerns | Clear separation |

This refactoring demonstrates how to transform a working but monolithic codebase into a clean, maintainable, event-driven architecture without losing functionality. 