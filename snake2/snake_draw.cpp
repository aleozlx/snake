#include "snake_draw.h"
#include "snake_dep.h"
#include <algorithm>
#include <cmath>

// External font data (from fonts/font0.cpp)
extern const bool font_5x7[36][7][5];
extern int getCharIndex(char c);

namespace SnakeDraw {

// Use external getCharIndex function from font0.cpp

void drawSquare(int x, int y, const RGBColor& color, const DrawContext& ctx) {
    float cellWidth = 2.0f / ctx.gridWidth;
    float cellHeight = 2.0f / ctx.gridHeight;
    float ndcX = (x * cellWidth) - 1.0f;
    float ndcY = (y * cellHeight) - 1.0f;
    
    glUniform2f(ctx.u_offset, ndcX, ndcY);
    glUniform2f(ctx.u_scale, cellWidth, cellHeight);
    glUniform3f(ctx.u_color, color.r, color.g, color.b);
    glUniform1i(ctx.u_shape_type, 0); // Rectangle
    glUniform1i(ctx.u_use_texture, GL_FALSE); // Disable texture
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void drawSmallSquare(float x, float y, float size, const RGBColor& color, const DrawContext& ctx) {
    // x, y are in NDC coordinates, size is the width/height in NDC space
    glUniform2f(ctx.u_offset, x, y);
    glUniform2f(ctx.u_scale, size, size);
    glUniform3f(ctx.u_color, color.r, color.g, color.b);
    glUniform1i(ctx.u_shape_type, 0); // Rectangle shape
    glUniform1i(ctx.u_use_texture, GL_FALSE); // Disable texture
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void drawCircle(float x, float y, float diameter, const RGBColor& color, const DrawContext& ctx) {
    // x, y are in NDC coordinates (center of circle), diameter is the size in NDC space
    glUniform2f(ctx.u_offset, x - diameter * 0.5f, y - diameter * 0.5f);
    glUniform2f(ctx.u_scale, diameter, diameter);
    glUniform3f(ctx.u_color, color.r, color.g, color.b);
    glUniform1i(ctx.u_shape_type, 1); // Circle shape
    glUniform1i(ctx.u_use_texture, GL_FALSE); // Disable texture
    
    // Simple aspect ratio - disable correction for manual debugging
    float aspectRatio = 1.0f; // No aspect ratio correction
    glUniform1f(ctx.u_aspect_ratio, aspectRatio);
    
    // Enable alpha blending for anti-aliasing
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
}

void drawPerfectCircle(float centerX, float centerY, float radius, const RGBColor& color, const DrawContext& ctx) {
    // Simply use the radius as provided, but force square dimensions
    float diameter = radius * 2.0f;
    
    glUniform2f(ctx.u_offset, centerX - diameter * 0.5f, centerY - diameter * 0.5f);
    glUniform2f(ctx.u_scale, diameter, diameter); // Force square: same dimension for both X and Y
    glUniform3f(ctx.u_color, color.r, color.g, color.b);
    glUniform1i(ctx.u_shape_type, 1); // Circle shape
    glUniform1i(ctx.u_use_texture, GL_FALSE); // Disable texture
    
    // Disable aspect ratio correction for this function
    glUniform1f(ctx.u_aspect_ratio, 1.0f); // No aspect ratio correction
    
    // Enable alpha blending for anti-aliasing
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
}

void drawTexturedSquare(int x, int y, GLuint texture, const DrawContext& ctx) {
    float cellWidth = 2.0f / ctx.gridWidth;
    float cellHeight = 2.0f / ctx.gridHeight;
    float ndcX = (x * cellWidth) - 1.0f;
    float ndcY = (y * cellHeight) - 1.0f;
    
    glUniform2f(ctx.u_offset, ndcX, ndcY);
    glUniform2f(ctx.u_scale, cellWidth, cellHeight);
    glUniform1i(ctx.u_use_texture, GL_TRUE);
    glUniform1i(ctx.u_shape_type, 3); // Texture mode
    
    // Bind and use the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(ctx.u_texture, 0);
    
    // Enable alpha blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
    glUniform1i(ctx.u_use_texture, GL_FALSE); // Reset texture mode
}

// Simple character rendering using small squares (5x7 character matrix)
void drawChar(char c, float startX, float startY, float charSize, const RGBColor& color, const DrawContext& ctx) {
    int charIndex = getCharIndex(c);
    
    if (charIndex >= 0 && charIndex < 36) {
        float pixelSize = charSize / 7.0f; // Each character pixel is 1/7th of character height
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (font_5x7[charIndex][row][col]) {
                    float pixelX = startX + (col * pixelSize);
                    float pixelY = startY + ((6 - row) * pixelSize);
                    drawSmallSquare(pixelX, pixelY, pixelSize, color, ctx);
                }
            }
        }
    }
}

void drawText(const char* text, float startX, float startY, float charSize, const RGBColor& color, const DrawContext& ctx) {
    float x = startX;
    float charWidth = charSize * (5.0f / 7.0f); // Character width is 5/7 of height
    while (*text) {
        drawChar(*text, x, startY, charSize, color, ctx);
        x += charWidth + (charSize * 0.2f); // Character width + small space
        text++;
    }
}

// Draw round eyes on the snake head that look towards the food
void drawSnakeEyes(int headX, int headY, int foodX, int foodY, const RGBColor& snakeColor, Point snakeDirection, const DrawContext& ctx) {
    // Calculate cell dimensions in NDC space
    float cellWidth = 2.0f / ctx.gridWidth;
    float cellHeight = 2.0f / ctx.gridHeight;
    
    // Convert head position to NDC coordinates (center of the cell)
    float headNdcX = (headX * cellWidth) - 1.0f + (cellWidth * 0.5f);
    float headNdcY = (headY * cellHeight) - 1.0f + (cellHeight * 0.5f);
    
    // Use the snake's movement direction for eye positioning (front of head)
    float moveDirX = snakeDirection.x;
    float moveDirY = snakeDirection.y;
    
    // Calculate direction vector from head to food (for pupil tracking)
    float foodDirX = foodX - headX;
    float foodDirY = foodY - headY;
    
    // Normalize the food direction vector
    float length = sqrt(foodDirX * foodDirX + foodDirY * foodDirY);
    if (length > 0) {
        foodDirX /= length;
        foodDirY /= length;
    }
    
    // Eye size for perfect circles
    float eyeDiameter = cellWidth * 0.35f;    // Eyes are 35% of cell width
    float pupilDiameter = eyeDiameter * 0.5f; // Pupils are 50% of eye size
    
    // Eye spacing and positioning based on movement direction
    float eyeSpacing = cellWidth * 0.2f; // Distance between eye centers
    float eyeOffsetFromCenter = cellWidth * 0.25f; // How far from center towards front of head
    
    // Calculate perpendicular vector for eye spacing (90 degree rotation of movement direction)
    float perpX = -moveDirY;
    float perpY = moveDirX;
    
    // Position the two eyes at the front of the head along the perpendicular axis
    float leftEyeX = headNdcX + (moveDirX * eyeOffsetFromCenter) + (perpX * eyeSpacing);
    float leftEyeY = headNdcY + (moveDirY * eyeOffsetFromCenter) + (perpY * eyeSpacing);
    
    float rightEyeX = headNdcX + (moveDirX * eyeOffsetFromCenter) - (perpX * eyeSpacing);
    float rightEyeY = headNdcY + (moveDirY * eyeOffsetFromCenter) - (perpY * eyeSpacing);
    
    // Create color constants
    constexpr RGBColor WHITE(1.0f, 1.0f, 1.0f);
    constexpr RGBColor BLACK(0.0f, 0.0f, 0.0f);
    
    // Draw the round eyes (white circles) - now they'll be proper circles!
    drawCircle(leftEyeX, leftEyeY, eyeDiameter, WHITE, ctx);
    drawCircle(rightEyeX, rightEyeY, eyeDiameter, WHITE, ctx);
    
    // Calculate pupil offset based on food looking direction (pupils look towards food)
    float pupilOffsetAmount = eyeDiameter * 0.2f; // How much pupils can move within the eye
    float pupilLeftX = leftEyeX + (foodDirX * pupilOffsetAmount);
    float pupilLeftY = leftEyeY + (foodDirY * pupilOffsetAmount);
    float pupilRightX = rightEyeX + (foodDirX * pupilOffsetAmount);
    float pupilRightY = rightEyeY + (foodDirY * pupilOffsetAmount);
    
    // Draw the round pupils (black circles) - perfect circular pupils!
    drawCircle(pupilLeftX, pupilLeftY, pupilDiameter, BLACK, ctx);
    drawCircle(pupilRightX, pupilRightY, pupilDiameter, BLACK, ctx);
    
    // Add round highlights to make eyes more lively - tiny circular highlights!
    float highlightDiameter = pupilDiameter * 0.4f; // Smaller highlight circles
    float highlightOffsetX = pupilDiameter * 0.15f;
    float highlightOffsetY = pupilDiameter * 0.15f;
    
    drawCircle(pupilLeftX + highlightOffsetX, pupilLeftY + highlightOffsetY, highlightDiameter, WHITE, ctx);
    drawCircle(pupilRightX + highlightOffsetX, pupilRightY + highlightOffsetY, highlightDiameter, WHITE, ctx);
}

// Draw Pacman (Level 1 feature)
void drawPacman(const Point& pacman, const Point& pacmanDirection, const DrawContext& ctx) {
    // Calculate cell dimensions and position
    float cellWidth = 2.0f / ctx.gridWidth;
    float cellHeight = 2.0f / ctx.gridHeight;
    float pacmanNdcX = (pacman.x * cellWidth) - 1.0f + (cellWidth * 0.5f);
    float pacmanNdcY = (pacman.y * cellHeight) - 1.0f + (cellHeight * 0.5f);
    
    // Use minimum cell dimension for diameter
    float diameter = std::min(cellWidth, cellHeight) * 0.9f;
    
    // Create color constants
    constexpr RGBColor YELLOW(1.0f, 1.0f, 0.0f);
    constexpr RGBColor DARK_GRAY(0.1f, 0.1f, 0.1f);
    
    // Draw yellow circle for pacman body
    drawCircle(pacmanNdcX, pacmanNdcY, diameter, YELLOW, ctx);
    
    // Draw black circle mouth (half diameter, positioned near edge based on direction)
    float mouthDiameter = diameter * 0.5f;
    float mouthOffset = diameter * 0.3f;
    
    float mouthX = pacmanNdcX;
    float mouthY = pacmanNdcY;
    
    // Position mouth based on direction
    if (pacmanDirection.x == 1 || (pacmanDirection.x == 0 && pacmanDirection.y == 0)) {
        // Right or stationary
        mouthX += mouthOffset;
    } else if (pacmanDirection.x == -1) {
        // Left
        mouthX -= mouthOffset;
    } else if (pacmanDirection.y == 1) {
        // Up
        mouthY += mouthOffset;
    } else if (pacmanDirection.y == -1) {
        // Down
        mouthY -= mouthOffset;
    }
    
    // Draw the black mouth circle
    drawCircle(mouthX, mouthY, mouthDiameter, DARK_GRAY, ctx);
}

// Modular confirmation dialogue rendering
void drawConfirmationDialogue(const char* message, const RGBColor& bgColor, const DrawContext& ctx) {
    int centerX = ctx.gridWidth / 2;
    int centerY = ctx.gridHeight / 2;
    
    // Create color constants
    constexpr RGBColor WHITE(1.0f, 1.0f, 1.0f);
    constexpr RGBColor BLACK(0.0f, 0.0f, 0.0f);
    constexpr RGBColor GREEN(0.0f, 1.0f, 0.0f);
    constexpr RGBColor RED(1.0f, 0.0f, 0.0f);
    
    // Draw dialogue box background with custom color
    for (int x = centerX - 8; x <= centerX + 8; x++) {
        for (int y = centerY - 3; y <= centerY + 3; y++) {
            if (x >= 1 && x < ctx.gridWidth - 1 && y >= 1 && y < ctx.gridHeight - 1) {
                drawSquare(x, y, bgColor, ctx); // Custom background color
            }
        }
    }
    
    // Draw dialogue border (bright white)
    for (int x = centerX - 8; x <= centerX + 8; x++) {
        if (x >= 1 && x < ctx.gridWidth - 1) {
            drawSquare(x, centerY - 3, WHITE, ctx); // Top border
            drawSquare(x, centerY + 3, WHITE, ctx); // Bottom border
        }
    }
    for (int y = centerY - 3; y <= centerY + 3; y++) {
        if (y >= 1 && y < ctx.gridHeight - 1) {
            drawSquare(centerX - 8, y, WHITE, ctx); // Left border
            drawSquare(centerX + 8, y, WHITE, ctx); // Right border
        }
    }
    
    // Draw text using the bitmap font system
    float cellWidth = 2.0f / ctx.gridWidth;
    float cellHeight = 2.0f / ctx.gridHeight;
    
    // Main message text - centered in the dialogue
    float titleTextSize = cellHeight * 0.6f; // Text size for dialogue
    float titleX = ((centerX - 6) * cellWidth) - 1.0f; // One tile margin from left border
    float titleY = ((centerY + 1) * cellHeight) - 1.0f; // Upper area
    drawText(message, titleX, titleY, titleTextSize, WHITE, ctx); // White text
    
    // Button prompts with labels
    float buttonTextSize = cellHeight * 0.4f; // Smaller for button labels
    
    // A button (left side) - YES/CONFIRM
    float aButtonX = ((centerX - 4) * cellWidth) - 1.0f;
    float aButtonY = ((centerY - 2) * cellHeight) - 1.0f;
    drawSquare(centerX - 4, centerY - 2, GREEN, ctx); // Green A button
    drawSquare(centerX - 3, centerY - 2, GREEN, ctx);
    drawText("A", aButtonX + cellWidth * 0.3f, aButtonY + cellHeight * 0.2f, buttonTextSize, BLACK, ctx); // Black "A" on green
    
    // B button (right side) - NO/CANCEL
    float bButtonX = ((centerX + 2) * cellWidth) - 1.0f;
    float bButtonY = ((centerY - 2) * cellHeight) - 1.0f;
    drawSquare(centerX + 2, centerY - 2, RED, ctx); // Red B button
    drawSquare(centerX + 3, centerY - 2, RED, ctx);
    drawText("B", bButtonX + cellWidth * 0.3f, bButtonY + cellHeight * 0.2f, buttonTextSize, WHITE, ctx); // White "B" on red
}



} // namespace SnakeDraw 