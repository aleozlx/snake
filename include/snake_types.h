#pragma once

// Fundamental types for the snake game
// No other header dependency here

struct ix2 {
    union {
        struct { int x, y; };
        int d[2];
    };
    
    // Default constructor
    ix2(int x = 0, int y = 0) : x(x), y(y) {}

    // Default copy constructor andassignment operator
    ix2(const ix2&) = default;
    ix2& operator=(const ix2&) = default;
    
    // Array constructor
    ix2(const int coords[2]) : x(coords[0]), y(coords[1]) {}
    
    // Equality operator
    inline bool operator==(const ix2 &other) const {
        return x == other.x && y == other.y;
    }
};

struct fx3 {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
        float d[3];
    };
    
    // Default constructor - now constexpr
    constexpr fx3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}

    // Default copy constructor andassignment operator
    fx3(const fx3&) = default;
    fx3& operator=(const fx3&) = default;

    // Array constructor
    fx3(const float coords[3]) : x(coords[0]), y(coords[1]), z(coords[2]) {}
    
    // Equality operator
    inline bool operator==(const fx3 &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    // Multiply by a scalar - also make constexpr
    constexpr fx3 operator*(float s) const {
        return fx3(x * s, y * s, z * s);
    }
};
