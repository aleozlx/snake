#pragma once

#include <glad/gl.h>
#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <vector>

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
