#pragma once

#include <glad/gl.h>
#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <vector>
#include <cstdint>

#include "snake_types.h"

// Point structure for 2D coordinates (inherits from ix2)
struct Point : ix2 {
    // Constructor with default values
    Point(int x = 0, int y = 0) : ix2(x, y) {}
    inline Point(const ix2& other) : ix2(other) {}
};

// RGB Color structure (inherits from fx3)
struct RGBColor : fx3 {
    // Constructor with RGB values - now constexpr
    constexpr RGBColor(float red = 0.f, float green = 0.f, float blue = 0.f) : fx3(red, green, blue) {}

    // Allow conversion from fx3 and enable fx3 operators
    inline RGBColor(const fx3& other) : fx3(other) {}
    
    // Common color blending operations
    static RGBColor blend(const RGBColor& a, const RGBColor& b, float t) {
        return RGBColor(a.r + t * (b.r - a.r), 
                        a.g + t * (b.g - a.g), 
                        a.b + t * (b.b - a.b));
    }
};

// Navigation algorithm types for AI snakes
enum NavigationType {
    NAV_NAIVE = 0,    // Simple direction-based pathfinding
    NAV_ASTAR,     // A* pathfinding algorithm

    NAV_TYPE_COUNT
};

// Multi-snake support structure
struct Snake {
    std::vector<Point> body;
    Point direction;
    bool movementPaused;
    int score;
    SDL_GameController* controller; // nullptr for keyboard-controlled snake
    int controllerIndex; // -1 for keyboard, 0+ for controller index
    RGBColor color; // Snake color
    bool isAlive;
    NavigationType navType; // Navigation algorithm for AI snakes
    
    Snake(int startX, int startY, Point startDir, SDL_GameController* ctrl = nullptr, int ctrlIdx = -1, float red = 0.0f, float green = 1.0f, float blue = 0.0f, NavigationType nav = NAV_NAIVE) 
        : direction(startDir), movementPaused(false), score(0), controller(ctrl), controllerIndex(ctrlIdx), color(red, green, blue), isAlive(true), navType(nav) {
        body.clear();
        body.push_back(Point(startX, startY));
        body.push_back(Point(startX - startDir.x, startY - startDir.y));
        body.push_back(Point(startX - 2*startDir.x, startY - 2*startDir.y));
    }
};

// Tile Content System - unified data structure for collision detection, pathfinding, and IPC
enum class TileContent : uint8_t {
    EMPTY = 0,
    BORDER = 1,
    SNAKE_HEAD = 2,
    SNAKE_BODY = 3,
    AI_SNAKE_HEAD = 4,
    AI_SNAKE_BODY = 5,
    PACMAN = 6,
    FOOD = 7
};

// Tile Grid Management
class TileGrid {
public:
    TileGrid(int width, int height);
    ~TileGrid();
    
    // Basic operations
    void clear();
    void setBorder();
    TileContent getTile(int x, int y) const;
    void setTile(int x, int y, TileContent content);
    bool isOccupied(int x, int y) const;
    bool isValidPosition(int x, int y) const;
    
    // Bulk operations for efficiency
    void updateFromGameState(const std::vector<Snake>& playerSnakes, 
                           const std::vector<Snake>& aiSnakes,
                           const Point& food, 
                           bool pacmanActive, const Point& pacman);
    
    // IPC grid generation (char-based for compatibility)
    void createIPCGrid(char* gridData) const;
    
    // Pathfinding support
    bool isPathBlocked(const Point& pos) const;
    
    // Debugging
    void debugPrint() const;
    
    // Getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
private:
    int m_width, m_height;
    TileContent** m_grid;
    
    void allocateGrid();
    void deallocateGrid();
    char tileToIPCChar(TileContent tile, int x, int y) const;
};
