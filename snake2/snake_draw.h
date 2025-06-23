#pragma once

#include "snake_dep.h"

namespace SnakeDraw {

// Drawing context structure that holds OpenGL uniform locations and grid dimensions
struct DrawContext {
    // Grid dimensions
    int gridWidth;
    int gridHeight;
    
    // OpenGL uniform locations
    GLint u_offset;
    GLint u_color;
    GLint u_scale;
    GLint u_shape_type;
    GLint u_inner_radius;
    GLint u_texture;
    GLint u_use_texture;
    GLint u_aspect_ratio;
    
    DrawContext(int gw, int gh, GLint offset, GLint color, GLint scale, GLint shape_type,
                GLint inner_radius, GLint texture, GLint use_texture, GLint aspect_ratio)
        : gridWidth(gw), gridHeight(gh), u_offset(offset), u_color(color), u_scale(scale),
          u_shape_type(shape_type), u_inner_radius(inner_radius), u_texture(texture),
          u_use_texture(use_texture), u_aspect_ratio(aspect_ratio) {}
};

// Basic drawing functions
void drawSquare(int x, int y, const RGBColor& color, const DrawContext& ctx);
void drawSmallSquare(float x, float y, float size, const RGBColor& color, const DrawContext& ctx);
void drawCircle(float x, float y, float diameter, const RGBColor& color, const DrawContext& ctx);
void drawPerfectCircle(float centerX, float centerY, float radius, const RGBColor& color, const DrawContext& ctx);
void drawTexturedSquare(int x, int y, GLuint texture, const DrawContext& ctx);

// Text rendering functions
void drawChar(char c, float startX, float startY, float charSize, const RGBColor& color, const DrawContext& ctx);
void drawText(const char* text, float startX, float startY, float charSize, const RGBColor& color, const DrawContext& ctx);

// Game entity drawing functions
void drawSnakeEyes(int headX, int headY, int foodX, int foodY, const RGBColor& snakeColor, Point snakeDirection, const DrawContext& ctx);
void drawPacman(const Point& pacman, const Point& pacmanDirection, const DrawContext& ctx);

// UI drawing functions
void drawConfirmationDialogue(const char* message, const RGBColor& bgColor, const DrawContext& ctx);

} // namespace SnakeDraw 