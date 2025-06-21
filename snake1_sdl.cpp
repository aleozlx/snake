#include <glad/gl.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

// Try to include SDL2_image if available
#ifdef SDL_IMAGE_AVAILABLE
#include <SDL2/SDL_image.h>
#endif

// Linux force feedback includes for rumble support
#ifdef __linux__
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#endif

// Vertex shader source for rendering squares, circles, and textures
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
uniform vec2 u_offset;
uniform vec2 u_scale;
out vec2 texCoord;
out vec2 fragTexCoord;
void main() {
    texCoord = aPos;
    fragTexCoord = aTexCoord;
    vec2 pos = (aPos * u_scale) + u_offset;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

// Fragment shader source with proper circle rendering and texture support
const char* fragmentShaderSource = R"(
#version 330 core
in vec2 texCoord;
in vec2 fragTexCoord;
out vec4 FragColor;
uniform vec3 u_color;
uniform int u_shape_type; // 0 = rectangle, 1 = circle, 2 = ring, 3 = texture
uniform float u_inner_radius; // For ring shapes
uniform sampler2D u_texture; // For texture rendering
uniform bool u_use_texture; // Whether to use texture
void main() {
    if (u_shape_type == 3 || u_use_texture) {
        // Texture rendering
        vec4 texColor = texture(u_texture, fragTexCoord);
        if (texColor.a < 0.1) discard; // Alpha testing for transparency
        FragColor = texColor;
    } else if (u_shape_type == 0) {
        // Rectangle (default behavior)
        FragColor = vec4(u_color, 1.0);
    } else if (u_shape_type == 1) {
        // Circle with anti-aliasing
        // Convert texture coordinates from [0,1] to [-1,1] centered
        vec2 uv = (texCoord - 0.5) * 2.0;
        
        // Calculate distance from center
        float dist = length(uv);
        
        // Create smooth circle with anti-aliasing
        float radius = 1.0;
        float smoothness = 0.1;
        float alpha = 1.0 - smoothstep(radius - smoothness, radius + smoothness, dist);
        
        // Discard fragments outside the circle for performance
        if (alpha < 0.01) discard;
        
        FragColor = vec4(u_color, alpha);
    } else if (u_shape_type == 2) {
        // Ring (hollow circle)
        vec2 uv = (texCoord - 0.5) * 2.0;
        float dist = length(uv);
        
        float outerRadius = 1.0;
        float innerRadius = u_inner_radius * 2.0;
        float smoothness = 0.1;
        
        float outerAlpha = 1.0 - smoothstep(outerRadius - smoothness, outerRadius + smoothness, dist);
        float innerAlpha = smoothstep(innerRadius - smoothness, innerRadius + smoothness, dist);
        
        float alpha = outerAlpha * innerAlpha;
        
        // Discard fragments outside the ring
        if (alpha < 0.01) discard;
        
        FragColor = vec4(u_color, alpha);
    }
}
)";

// Game constants
int GRID_WIDTH = 32;
int GRID_HEIGHT = 20;

// Game state
struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

std::vector<Point> snake;
Point food;
Point direction(1, 0);
bool gameOver = false;
bool movementPaused = false;
bool gamePaused = false;
bool exitConfirmation = false;
bool resetConfirmation = false;
int score = 0;
int level = 0; // Level system like snake1
float lastMoveTime = 0.0f;
float MOVE_INTERVAL = 0.2f;
float flashTimer = 0.0f;
const float FLASH_INTERVAL = 0.1f;

// Pacman state (level 1+ feature)
Point pacman;
Point pacmanDirection(0, 0); // Pacman's current direction
float lastPacmanMoveTime = 0.0f;
float PACMAN_MOVE_INTERVAL = 0.3f; // Pacman moves slightly slower than snake
bool pacmanActive = false; // Only active in level 1+

// Input tracking for gamepad display
bool usingGamepadInput = false;
int lastButtonPressed = -1; // For visual debug
float lastButtonTime = 0.0f;

// Rumble/vibration system
SDL_Haptic* haptic = nullptr;
bool rumbleSupported = false;
float rumbleEndTime = 0.0f; // When current rumble should end
const float RUMBLE_DURATION = 0.3f; // Duration of rumble effect in seconds

// SDL2 variables
SDL_Window* window = nullptr;
SDL_GLContext glContext = nullptr;
SDL_GameController* gameController = nullptr;
bool running = true;

// OpenGL objects
GLuint shaderProgram;
GLuint VAO, VBO;
GLint u_offset, u_color, u_scale, u_shape_type, u_inner_radius, u_texture, u_use_texture;
GLuint appleTexture = 0; // Apple texture for food

// Square vertices with texture coordinates (position + texcoord)
float squareVertices[] = {
    // Position   // TexCoord
    0.0f, 0.0f,   0.0f, 1.0f,  // Bottom-left
    1.0f, 0.0f,   1.0f, 1.0f,  // Bottom-right
    1.0f, 1.0f,   1.0f, 0.0f,  // Top-right
    0.0f, 1.0f,   0.0f, 0.0f   // Top-left
};

GLuint indices[] = {
    0, 1, 2,
    2, 3, 0
};

// Helper function to check if a move is valid
bool isValidMove(const Point& newHead) {
    // Check boundary collision - pause before entering the border area
    if (newHead.x == 0 || newHead.x == GRID_WIDTH-1 || newHead.y == 0 || newHead.y == GRID_HEIGHT-1) {
        return false;
    }
    
    // Check self collision
    for (const auto& segment : snake) {
        if (newHead == segment) {
            return false;
        }
    }
    
    // Check pacman collision - if pacman is in the way, treat as collision
    if (pacmanActive && newHead == pacman) {
        return false;
    }
    
    return true;
}

// Helper function to check if a pacman move is valid
bool isValidPacmanMove(const Point& newPos) {
    // Check boundary collision
    if (newPos.x <= 0 || newPos.x >= GRID_WIDTH-1 || newPos.y <= 0 || newPos.y >= GRID_HEIGHT-1) {
        return false;
    }
    
    // Check snake collision - pacman can't move into snake
    for (const auto& segment : snake) {
        if (newPos == segment) {
            return false;
        }
    }
    
    return true;
}

// Simple AI for pacman to move toward food
Point calculatePacmanDirection() {
    if (!pacmanActive) return Point(0, 0);
    
    // Calculate direction toward food
    int dx = food.x - pacman.x;
    int dy = food.y - pacman.y;
    
    // Try to move toward food, prioritizing the axis with greater distance
    std::vector<Point> possibleMoves;
    
    if (abs(dx) >= abs(dy)) {
        // Prioritize horizontal movement
        if (dx > 0) possibleMoves.push_back(Point(1, 0));  // Right
        if (dx < 0) possibleMoves.push_back(Point(-1, 0)); // Left
        if (dy > 0) possibleMoves.push_back(Point(0, 1));  // Up
        if (dy < 0) possibleMoves.push_back(Point(0, -1)); // Down
    } else {
        // Prioritize vertical movement
        if (dy > 0) possibleMoves.push_back(Point(0, 1));  // Up
        if (dy < 0) possibleMoves.push_back(Point(0, -1)); // Down
        if (dx > 0) possibleMoves.push_back(Point(1, 0));  // Right
        if (dx < 0) possibleMoves.push_back(Point(-1, 0)); // Left
    }
    
    // Try each possible move in order of preference
    for (const auto& move : possibleMoves) {
        Point newPos = Point(pacman.x + move.x, pacman.y + move.y);
        if (isValidPacmanMove(newPos)) {
            return move;
        }
    }
    
    // If no preferred move is valid, try any valid move
    std::vector<Point> allMoves = {Point(1,0), Point(-1,0), Point(0,1), Point(0,-1)};
    for (const auto& move : allMoves) {
        Point newPos = Point(pacman.x + move.x, pacman.y + move.y);
        if (isValidPacmanMove(newPos)) {
            return move;
        }
    }
    
    // No valid moves - stay still
    return Point(0, 0);
}

void initializeGame() {
    snake.clear();
    snake.push_back(Point(GRID_WIDTH/2, GRID_HEIGHT/2));
    snake.push_back(Point(GRID_WIDTH/2-1, GRID_HEIGHT/2));
    snake.push_back(Point(GRID_WIDTH/2-2, GRID_HEIGHT/2));
    
    direction = Point(1, 0);
    gameOver = false;
    movementPaused = false;
    gamePaused = false;
    exitConfirmation = false;
    resetConfirmation = false;
    score = 0;
    
    // Initialize pacman for level 1+
    pacmanActive = (level >= 1);
    if (pacmanActive) {
        // Spawn pacman at random unoccupied location
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, GRID_WIDTH-2);
        std::uniform_int_distribution<> disY(1, GRID_HEIGHT-2);
        
        do {
            pacman = Point(disX(gen), disY(gen));
        } while (std::find(snake.begin(), snake.end(), pacman) != snake.end());
        
        pacmanDirection = Point(0, 0); // Start stationary
        lastPacmanMoveTime = 0.0f;
        std::cout << "Pacman spawned at (" << pacman.x << "," << pacman.y << ") for Level " << level << std::endl;
    }
    
    // Generate food (only in playable area, excluding borders)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(1, GRID_WIDTH-2);
    std::uniform_int_distribution<> disY(1, GRID_HEIGHT-2);
    
    do {
        food = Point(disX(gen), disY(gen));
    } while (std::find(snake.begin(), snake.end(), food) != snake.end() || 
             (pacmanActive && food == pacman));
}

void drawSquare(int x, int y, float r, float g, float b) {
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    float ndcX = (x * cellWidth) - 1.0f;
    float ndcY = (y * cellHeight) - 1.0f;
    
    glUniform2f(u_offset, ndcX, ndcY);
    glUniform2f(u_scale, cellWidth, cellHeight);
    glUniform3f(u_color, r, g, b);
    glUniform1i(u_shape_type, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Draw a small square for text rendering (much smaller than game tiles)
void drawSmallSquare(float x, float y, float size, float r, float g, float b) {
    // x, y are in NDC coordinates, size is the width/height in NDC space
    glUniform2f(u_offset, x, y);
    glUniform2f(u_scale, size, size);
    glUniform3f(u_color, r, g, b);
    glUniform1i(u_shape_type, 0); // Rectangle shape
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Draw a perfect circle with anti-aliasing
void drawCircle(float x, float y, float diameter, float r, float g, float b) {
    // x, y are in NDC coordinates (center of circle), diameter is the size in NDC space
    glUniform2f(u_offset, x - diameter*0.5f, y - diameter*0.5f);
    glUniform2f(u_scale, diameter, diameter);
    glUniform3f(u_color, r, g, b);
    glUniform1i(u_shape_type, 1); // Circle shape
    
    // Enable alpha blending for anti-aliasing
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
}

// Load texture from file (with optional SDL2_image support)
GLuint loadTexture(const char* filename) {
#ifdef SDL_IMAGE_AVAILABLE
    SDL_Surface* surface = IMG_Load(filename);
    if (!surface) {
        std::cout << "Failed to load texture " << filename << ": " << IMG_GetError() << std::endl;
        return 0;
    }
#else
    // Try to load BMP files using SDL2's built-in support
    SDL_Surface* surface = SDL_LoadBMP(filename);
    if (!surface) {
        std::cout << "Failed to load BMP texture " << filename << ": " << SDL_GetError() << std::endl;
        std::cout << "Note: Only BMP files supported without SDL2_image" << std::endl;
        return 0;
    }
#endif
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Determine format
    GLenum format = GL_RGB;
    if (surface->format->BytesPerPixel == 4) {
        format = GL_RGBA;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    SDL_FreeSurface(surface);
    
    std::cout << "Loaded texture: " << filename << " (ID: " << texture << ")" << std::endl;
    return texture;
}

// Create a simple apple bitmap if no file is found
GLuint createAppleBitmap() {
    // Create a simple 16x16 red apple bitmap
    const int size = 16;
    unsigned char appleData[size * size * 4]; // RGBA
    
    // Simple apple pattern
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            
            // Create a simple apple shape
            float centerX = size / 2.0f;
            float centerY = size / 2.0f + 1;
            float dist = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
            
            if (dist < size / 3.0f) {
                // Red apple body
                appleData[idx + 0] = 220; // R
                appleData[idx + 1] = 20;  // G
                appleData[idx + 2] = 20;  // B
                appleData[idx + 3] = 255; // A
            } else if (y < 4 && x >= 6 && x <= 9) {
                // Green stem
                appleData[idx + 0] = 20;  // R
                appleData[idx + 1] = 150; // G
                appleData[idx + 2] = 20;  // B
                appleData[idx + 3] = 255; // A
            } else {
                // Transparent
                appleData[idx + 0] = 0;
                appleData[idx + 1] = 0;
                appleData[idx + 2] = 0;
                appleData[idx + 3] = 0;
            }
        }
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, appleData);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    std::cout << "Created procedural apple bitmap (ID: " << texture << ")" << std::endl;
    return texture;
}

// Draw textured square (for apple)
void drawTexturedSquare(int x, int y, GLuint texture) {
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    float ndcX = (x * cellWidth) - 1.0f;
    float ndcY = (y * cellHeight) - 1.0f;
    
    glUniform2f(u_offset, ndcX, ndcY);
    glUniform2f(u_scale, cellWidth, cellHeight);
    glUniform1i(u_use_texture, GL_TRUE);
    glUniform1i(u_shape_type, 3); // Texture mode
    
    // Bind and use the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(u_texture, 0);
    
    // Enable alpha blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
    glUniform1i(u_use_texture, GL_FALSE); // Reset texture mode
}

// Forward declarations for external font data
extern const bool font_5x7[36][7][5];
extern int getCharIndex(char c);

// Simple character rendering using small squares (5x7 character matrix)
void drawChar(char c, float startX, float startY, float charSize, float r, float g, float b) {
    int charIndex = getCharIndex(c);
    
    if (charIndex >= 0) {
        float pixelSize = charSize / 7.0f; // Each character pixel is 1/7th of character height
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (font_5x7[charIndex][row][col]) {
                    float pixelX = startX + (col * pixelSize);
                    float pixelY = startY + ((6 - row) * pixelSize);
                    drawSmallSquare(pixelX, pixelY, pixelSize, r, g, b);
                }
            }
        }
    }
}

void drawText(const char* text, float startX, float startY, float charSize, float r, float g, float b) {
    float x = startX;
    float charWidth = charSize * (5.0f / 7.0f); // Character width is 5/7 of height
    while (*text) {
        drawChar(*text, x, startY, charSize, r, g, b);
        x += charWidth + (charSize * 0.2f); // Character width + small space
        text++;
    }
}

// Draw round eyes on the snake head that look towards the food
void drawSnakeEyes(int headX, int headY, int foodX, int foodY, float snakeR, float snakeG, float snakeB) {
    // Calculate cell dimensions in NDC space
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    
    // Convert head position to NDC coordinates (center of the cell)
    float headNdcX = (headX * cellWidth) - 1.0f + (cellWidth * 0.5f);
    float headNdcY = (headY * cellHeight) - 1.0f + (cellHeight * 0.5f);
    
    // Use the snake's movement direction for eye positioning (front of head)
    float moveDirX = direction.x;
    float moveDirY = direction.y;
    
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
    float eyeDiameter = cellWidth * 0.35f; // Eyes are 35% of cell width
    float pupilDiameter = eyeDiameter * 0.5f;  // Pupils are 50% of eye size
    
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
    
    // Draw the round eyes (white circles) - now they'll be proper circles!
    drawCircle(leftEyeX, leftEyeY, eyeDiameter, 1.0f, 1.0f, 1.0f);
    drawCircle(rightEyeX, rightEyeY, eyeDiameter, 1.0f, 1.0f, 1.0f);
    
    // Calculate pupil offset based on food looking direction (pupils look towards food)
    float pupilOffsetAmount = eyeDiameter * 0.2f; // How much pupils can move within the eye
    float pupilLeftX = leftEyeX + (foodDirX * pupilOffsetAmount);
    float pupilLeftY = leftEyeY + (foodDirY * pupilOffsetAmount);
    float pupilRightX = rightEyeX + (foodDirX * pupilOffsetAmount);
    float pupilRightY = rightEyeY + (foodDirY * pupilOffsetAmount);
    
    // Draw the round pupils (black circles) - perfect circular pupils!
    drawCircle(pupilLeftX, pupilLeftY, pupilDiameter, 0.0f, 0.0f, 0.0f);
    drawCircle(pupilRightX, pupilRightY, pupilDiameter, 0.0f, 0.0f, 0.0f);
    
    // Add round highlights to make eyes more lively - tiny circular highlights!
    float highlightDiameter = pupilDiameter * 0.4f; // Smaller highlight circles
    float highlightOffsetX = pupilDiameter * 0.15f;
    float highlightOffsetY = pupilDiameter * 0.15f;
    
    drawCircle(pupilLeftX + highlightOffsetX, pupilLeftY + highlightOffsetY, highlightDiameter, 1.0f, 1.0f, 1.0f);
    drawCircle(pupilRightX + highlightOffsetX, pupilRightY + highlightOffsetY, highlightDiameter, 1.0f, 1.0f, 1.0f);
}

// Modular confirmation dialogue rendering
void drawConfirmationDialogue(const char* message, float bgR, float bgG, float bgB) {
    int centerX = GRID_WIDTH / 2;
    int centerY = GRID_HEIGHT / 2;
    
    // Draw dialogue box background with custom color
    for (int x = centerX - 8; x <= centerX + 8; x++) {
        for (int y = centerY - 3; y <= centerY + 3; y++) {
            if (x >= 1 && x < GRID_WIDTH-1 && y >= 1 && y < GRID_HEIGHT-1) {
                drawSquare(x, y, bgR, bgG, bgB); // Custom background color
            }
        }
    }
    
    // Draw dialogue border (bright white)
    for (int x = centerX - 8; x <= centerX + 8; x++) {
        if (x >= 1 && x < GRID_WIDTH-1) {
            drawSquare(x, centerY - 3, 1.0f, 1.0f, 1.0f); // Top border
            drawSquare(x, centerY + 3, 1.0f, 1.0f, 1.0f); // Bottom border
        }
    }
    for (int y = centerY - 3; y <= centerY + 3; y++) {
        if (y >= 1 && y < GRID_HEIGHT-1) {
            drawSquare(centerX - 8, y, 1.0f, 1.0f, 1.0f); // Left border
            drawSquare(centerX + 8, y, 1.0f, 1.0f, 1.0f); // Right border
        }
    }
    
    // Draw text using the bitmap font system
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    
    // Main message text - centered in the dialogue
    float titleTextSize = cellHeight * 0.6f; // Text size for dialogue
    float titleX = ((centerX - 6) * cellWidth) - 1.0f; // One tile margin from left border
    float titleY = ((centerY + 1) * cellHeight) - 1.0f; // Upper area
    drawText(message, titleX, titleY, titleTextSize, 1.0f, 1.0f, 1.0f); // White text
    
    // Button prompts with labels
    float buttonTextSize = cellHeight * 0.4f; // Smaller for button labels
    
    // A button (left side) - YES/CONFIRM
    float aButtonX = ((centerX - 4) * cellWidth) - 1.0f;
    float aButtonY = ((centerY - 2) * cellHeight) - 1.0f;
    drawSquare(centerX - 4, centerY - 2, 0.0f, 1.0f, 0.0f); // Green A button
    drawSquare(centerX - 3, centerY - 2, 0.0f, 1.0f, 0.0f);
    drawText("A", aButtonX + cellWidth * 0.3f, aButtonY + cellHeight * 0.2f, buttonTextSize, 0.0f, 0.0f, 0.0f); // Black "A" on green
    
    // B button (right side) - NO/CANCEL  
    float bButtonX = ((centerX + 2) * cellWidth) - 1.0f;
    float bButtonY = ((centerY - 2) * cellHeight) - 1.0f;
    drawSquare(centerX + 2, centerY - 2, 1.0f, 0.0f, 0.0f); // Red B button
    drawSquare(centerX + 3, centerY - 2, 1.0f, 0.0f, 0.0f);
    drawText("B", bButtonX + cellWidth * 0.3f, bButtonY + cellHeight * 0.2f, buttonTextSize, 1.0f, 1.0f, 1.0f); // White "B" on red
}

// Rumble/vibration system functions
bool initializeRumble() {
    if (!gameController) {
        std::cout << "No game controller available for rumble" << std::endl;
        return false;
    }
    
    // Check if the controller supports haptic feedback
    if (SDL_GameControllerHasRumble(gameController)) {
        rumbleSupported = true;
        std::cout << "🎮 Rumble support detected and enabled!" << std::endl;
        return true;
    } else {
        std::cout << "Controller does not support rumble" << std::endl;
        return false;
    }
}

void triggerRumble() {
    if (!rumbleSupported || !gameController) return;
    
    // Trigger rumble with SDL2's simple interface
    // Parameters: controller, low_freq_rumble, high_freq_rumble, duration_ms
    if (SDL_GameControllerRumble(gameController, 0xFFFF, 0xC000, (Uint32)(RUMBLE_DURATION * 1000)) == 0) {
        rumbleEndTime = (SDL_GetTicks() / 1000.0f) + RUMBLE_DURATION;
        std::cout << "🎮 RUMBLE! Collision detected!" << std::endl;
    } else {
        std::cout << "Failed to trigger rumble: " << SDL_GetError() << std::endl;
    }
}

void updateRumble() {
    // SDL2 handles rumble timing automatically, but we track it for logging
    if (rumbleSupported && rumbleEndTime > 0.0f) {
        float currentTime = SDL_GetTicks() / 1000.0f;
        if (currentTime >= rumbleEndTime) {
            rumbleEndTime = 0.0f;
            // Rumble automatically stops after the duration
        }
    }
}

void cleanupRumble() {
    if (rumbleSupported && gameController) {
        // Stop any ongoing rumble
        SDL_GameControllerRumble(gameController, 0, 0, 0);
        rumbleSupported = false;
    }
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    
    // Draw food (apple texture)
    if (appleTexture != 0) {
        drawTexturedSquare(food.x, food.y, appleTexture);
    } else {
        // Fallback to red square if texture failed to load
        drawSquare(food.x, food.y, 1.0f, 0.0f, 0.0f);
    }
    
    // Draw pacman (if active) - yellow circle with black circle mouth
    if (pacmanActive) {
        // Calculate cell dimensions and position
        float cellWidth = 2.0f / GRID_WIDTH;
        float cellHeight = 2.0f / GRID_HEIGHT;
        float pacmanNdcX = (pacman.x * cellWidth) - 1.0f + (cellWidth * 0.5f);
        float pacmanNdcY = (pacman.y * cellHeight) - 1.0f + (cellHeight * 0.5f);
        
        // Draw yellow circle for pacman body
        float diameter = cellWidth * 0.9f;
        drawCircle(pacmanNdcX, pacmanNdcY, diameter, 1.0f, 1.0f, 0.0f); // Yellow circle
        
        // Draw black circle mouth (half diameter, positioned near edge based on direction)
        float mouthDiameter = diameter * 0.5f; // Half the pacman's diameter
        float mouthOffset = diameter * 0.3f;   // Distance from center to mouth position
        
        float mouthX = pacmanNdcX;
        float mouthY = pacmanNdcY;
        
        // Position mouth based on direction
        if (pacmanDirection.x == 1 || (pacmanDirection.x == 0 && pacmanDirection.y == 0)) { // Right or stationary
            mouthX += mouthOffset; // Move mouth to right side
        } else if (pacmanDirection.x == -1) { // Left
            mouthX -= mouthOffset; // Move mouth to left side
        } else if (pacmanDirection.y == 1) { // Up
            mouthY += mouthOffset; // Move mouth to top side
        } else if (pacmanDirection.y == -1) { // Down
            mouthY -= mouthOffset; // Move mouth to bottom side
        }
        
        // Draw the black mouth circle
        drawCircle(mouthX, mouthY, mouthDiameter, 0.1f, 0.1f, 0.1f); // Dark mouth
    }
    
    // Draw corner markers
    drawSquare(0, 0, 1.0f, 1.0f, 0.0f);
    drawSquare(GRID_WIDTH-1, 0, 0.0f, 1.0f, 1.0f);
    drawSquare(0, GRID_HEIGHT-1, 1.0f, 0.0f, 1.0f);
    drawSquare(GRID_WIDTH-1, GRID_HEIGHT-1, 1.0f, 1.0f, 1.0f);
    
    // Display level counter in top-left corner
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    float levelTextX = (2 * cellWidth) - 1.0f; // Grid position 2 to NDC
    float levelTextY = ((GRID_HEIGHT - 2) * cellHeight) - 1.0f; // Near top
    float textSize = cellHeight * 0.8f; // Text size is 80% of a cell height
    
    // Display level number
    if (level == 0) {
        drawText("LVL 0", levelTextX, levelTextY, textSize, 0.8f, 0.8f, 0.8f); // Gray text
    } else if (level == 1) {
        drawText("LVL 1", levelTextX, levelTextY, textSize, 0.8f, 0.8f, 0.8f); // Gray text
    }
    
    // Display level description below
    if (level == 0) {
        drawText("JUST SNAKE", levelTextX, levelTextY - textSize * 1.2f, textSize * 0.7f, 1.0f, 0.8f, 0.0f); // Orange text
    } else if (level == 1) {
        drawText("PACMAN", levelTextX, levelTextY - textSize * 1.2f, textSize * 0.7f, 1.0f, 0.8f, 0.0f); // Orange text
    }
    
    // Visual debug: Display last pressed button name if gamepad input detected
    if (lastButtonPressed >= 0 && usingGamepadInput) {
        float buttonTextX = levelTextX;
        float buttonTextY = levelTextY - textSize * 3.0f; // Below level info
        
        // Get button name - simplified for SDL2
        const char* buttonName = "UNKNOWN";
        switch (lastButtonPressed) {
            case SDL_CONTROLLER_BUTTON_A: buttonName = "A"; break;
            case SDL_CONTROLLER_BUTTON_B: buttonName = "B"; break;
            case SDL_CONTROLLER_BUTTON_X: buttonName = "X"; break;
            case SDL_CONTROLLER_BUTTON_Y: buttonName = "Y"; break;
            case SDL_CONTROLLER_BUTTON_BACK: buttonName = "BACK"; break;
            case SDL_CONTROLLER_BUTTON_GUIDE: buttonName = "GUIDE"; break;
            case SDL_CONTROLLER_BUTTON_START: buttonName = "START"; break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: buttonName = "LSTICK"; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: buttonName = "RSTICK"; break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: buttonName = "LSHOULDER"; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: buttonName = "RSHOULDER"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: buttonName = "DPAD_UP"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: buttonName = "DPAD_DOWN"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: buttonName = "DPAD_LEFT"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: buttonName = "DPAD_RIGHT"; break;
            case SDL_CONTROLLER_BUTTON_MISC1: buttonName = "MISC1"; break;
            case SDL_CONTROLLER_BUTTON_PADDLE1: buttonName = "PADDLE1"; break;
            case SDL_CONTROLLER_BUTTON_PADDLE2: buttonName = "PADDLE2"; break;
            case SDL_CONTROLLER_BUTTON_PADDLE3: buttonName = "PADDLE3"; break;
            case SDL_CONTROLLER_BUTTON_PADDLE4: buttonName = "PADDLE4"; break;
            case SDL_CONTROLLER_BUTTON_TOUCHPAD: buttonName = "TOUCHPAD"; break;
        }
        
        drawText(buttonName, buttonTextX, buttonTextY, textSize * 0.6f, 1.0f, 1.0f, 0.0f); // Yellow text
        
        // Display "GAMEPAD:" label above the button name
        drawText("GAMEPAD", buttonTextX, buttonTextY + textSize * 0.8f, textSize * 0.6f, 0.0f, 1.0f, 1.0f); // Cyan text
    }
    
    // Draw snake with eyes on head
    for (size_t i = 0; i < snake.size(); i++) {
        float intensity = i == 0 ? 1.0f : 0.6f;
        float r, g, b; // Store colors for eye rendering
        
        if (exitConfirmation) {
            r = intensity; g = 0.0f; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b);
        } else if (resetConfirmation) {
            r = intensity; g = intensity * 0.5f; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b);
        } else if (gamePaused) {
            r = intensity; g = intensity; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b);
        } else if (movementPaused) {
            r = intensity; g = 0.0f; b = intensity;
            drawSquare(snake[i].x, snake[i].y, r, g, b);
        } else {
            r = 0.0f; g = intensity; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b);
        }
        
        // Draw eyes on the snake's head (first segment)
        if (i == 0 && !gameOver) {
            drawSnakeEyes(snake[i].x, snake[i].y, food.x, food.y, r, g, b);
        }
    }
    
    // Draw border
    float borderR, borderG, borderB;
    if (exitConfirmation) {
        borderR = 1.0f; borderG = 0.5f; borderB = 0.0f;
    } else if (resetConfirmation) {
        borderR = 1.0f; borderG = 0.3f; borderB = 0.0f;
    } else if (gamePaused) {
        borderR = 1.0f; borderG = 0.5f; borderB = 0.0f;
    } else if (movementPaused) {
        bool showRed = ((int)(flashTimer / FLASH_INTERVAL) % 2) == 0;
        if (showRed) {
            borderR = 1.0f; borderG = 0.0f; borderB = 0.0f;
        } else {
            borderR = 0.5f; borderG = 0.5f; borderB = 0.5f;
        }
    } else {
        borderR = 0.5f; borderG = 0.5f; borderB = 0.5f;
    }
    
    for (int i = 1; i < GRID_WIDTH-1; i++) {
        drawSquare(i, 0, borderR, borderG, borderB);
        drawSquare(i, GRID_HEIGHT-1, borderR, borderG, borderB);
    }
    for (int i = 1; i < GRID_HEIGHT-1; i++) {
        drawSquare(0, i, borderR, borderG, borderB);
        drawSquare(GRID_WIDTH-1, i, borderR, borderG, borderB);
    }
    
    // Draw confirmation dialogues using modular system
    if (exitConfirmation) {
        drawConfirmationDialogue("CONFIRM EXIT", 0.1f, 0.1f, 0.3f); // Dark blue background
    }
    
    if (resetConfirmation) {
        drawConfirmationDialogue("CONFIRM RESET", 0.3f, 0.1f, 0.1f); // Dark red background
    }
}

void updateGame() {
    if (gameOver) return;
    
    Point newHead = Point(snake[0].x + direction.x, snake[0].y + direction.y);
    bool snakeCanMove = isValidMove(newHead);
    bool snakeGotFood = false;
    
    // Check if the snake move is valid
    if (!snakeCanMove) {
        // Invalid move - pause movement until direction changes
        if (!movementPaused) {
            // Only trigger rumble on the first collision, not repeatedly while paused
            triggerRumble();
            std::cout << "COLLISION! Snake hit boundary, itself, or Pacman!" << std::endl;
        }
        movementPaused = true;
        
        // Even though snake is blocked, still allow pacman to compete for food
        snakeGotFood = false; // Snake can't get food if blocked
    } else {
        // Valid move - resume movement if it was paused
        if (movementPaused) {
            movementPaused = false;
            std::cout << "Movement resumed!" << std::endl;
        }
        
        // Move the snake
        snake.insert(snake.begin(), newHead);
        
        // Check if snake got the food
        snakeGotFood = (newHead == food);
    }
    
    // Check if pacman got the food (regardless of snake's state)
    bool pacmanGotFood = false;
    if (pacmanActive && pacman == food) {
        pacmanGotFood = true;
    }
    
    // Handle food competition outcomes
    if (snakeGotFood && !pacmanGotFood) {
        // Snake wins the food
        score++;
        std::cout << "Snake scored! Score: " << score << std::endl;
        
        // Generate new food (avoiding snake and pacman)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, GRID_WIDTH-2);
        std::uniform_int_distribution<> disY(1, GRID_HEIGHT-2);
        
        do {
            food = Point(disX(gen), disY(gen));
        } while (std::find(snake.begin(), snake.end(), food) != snake.end() || 
                 (pacmanActive && food == pacman));
    } else if (pacmanGotFood && !snakeGotFood) {
        // Pacman wins the food - snake doesn't grow
        std::cout << "Pacman got the food! Generating new food..." << std::endl;
        
        // If snake moved but didn't get food, remove tail. If snake was blocked, don't change snake.
        if (snakeCanMove) {
            snake.pop_back(); // Remove tail since no food was eaten by snake
        }
        
        // Generate new food (avoiding snake and pacman)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, GRID_WIDTH-2);
        std::uniform_int_distribution<> disY(1, GRID_HEIGHT-2);
        
        do {
            food = Point(disX(gen), disY(gen));
        } while (std::find(snake.begin(), snake.end(), food) != snake.end() || 
                 (pacmanActive && food == pacman));
    } else if (snakeGotFood && pacmanGotFood) {
        // Both reached food at same time - snake wins
        score++;
        std::cout << "Snake and Pacman reached food simultaneously - Snake wins! Score: " << score << std::endl;
        
        // Generate new food
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, GRID_WIDTH-2);
        std::uniform_int_distribution<> disY(1, GRID_HEIGHT-2);
        
        do {
            food = Point(disX(gen), disY(gen));
        } while (std::find(snake.begin(), snake.end(), food) != snake.end() || 
                 (pacmanActive && food == pacman));
    } else {
        // No food was eaten
        if (snakeCanMove) {
            snake.pop_back(); // Remove tail only if snake actually moved
        }
    }
}

void updatePacman() {
    if (!pacmanActive) return;
    
    // Calculate new direction toward food
    pacmanDirection = calculatePacmanDirection();
    
    // Move pacman
    Point newPacmanPos = Point(pacman.x + pacmanDirection.x, pacman.y + pacmanDirection.y);
    
    if (isValidPacmanMove(newPacmanPos)) {
        pacman = newPacmanPos;
        // std::cout << "Pacman moved to (" << pacman.x << "," << pacman.y << ")" << std::endl;
    }
}

void changeLevel(int newLevel) {
    if (newLevel < 0 || newLevel > 1) return; // Only levels 0 and 1 supported
    if (newLevel == level) return; // No change needed
    
    int oldLevel = level;
    level = newLevel;
    
    std::cout << "Level changed from " << oldLevel << " to " << level << std::endl;
    
    // Handle pacman spawning/despawning
    if (level == 0) {
        // Level 0: Despawn pacman
        pacmanActive = false;
        std::cout << "Pacman despawned for Level 0 (Classic Snake)" << std::endl;
    } else if (level == 1) {
        // Level 1: Spawn pacman
        pacmanActive = true;
        
        // Spawn pacman at random unoccupied location
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, GRID_WIDTH-2);
        std::uniform_int_distribution<> disY(1, GRID_HEIGHT-2);
        
        do {
            pacman = Point(disX(gen), disY(gen));
        } while (std::find(snake.begin(), snake.end(), pacman) != snake.end() || pacman == food);
        
        pacmanDirection = Point(0, 0); // Start stationary
        lastPacmanMoveTime = 0.0f;
        std::cout << "Pacman spawned at (" << pacman.x << "," << pacman.y << ") for Level " << level << std::endl;
    }
}

// Handle SDL2 keyboard events
void handleKeyboardEvent(SDL_KeyboardEvent* keyEvent) {
    if (keyEvent->type == SDL_KEYDOWN) {
        std::cout << ">>> KEYBOARD INPUT DETECTED <<<" << std::endl;
        
        if (keyEvent->keysym.sym == SDLK_ESCAPE) {
            std::cout << "ESC key - showing exit confirmation!" << std::endl;
            exitConfirmation = true;
        }
    }
}

// Handle SDL2 gamepad button down events
void handleGamepadButtonDown(SDL_ControllerButtonEvent* buttonEvent) {
    std::cout << ">>> SDL2 GAMEPAD BUTTON " << buttonEvent->button << " PRESSED <<<" << std::endl;
    
    // Track gamepad input for display
    usingGamepadInput = true;
    lastButtonPressed = buttonEvent->button;
    lastButtonTime = SDL_GetTicks() / 1000.0f;
    
    switch (buttonEvent->button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            if (direction.y == 0) {
                Point newDir = Point(0, 1);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            }
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            if (direction.y == 0) {
                Point newDir = Point(0, -1);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            }
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            if (direction.x == 0) {
                Point newDir = Point(-1, 0);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            }
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            if (direction.x == 0) {
                Point newDir = Point(1, 0);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            }
            break;
        case SDL_CONTROLLER_BUTTON_A:
            if (exitConfirmation) {
                std::cout << "A button - Exit confirmed!" << std::endl;
                running = false;
            } else if (resetConfirmation) {
                std::cout << "A button - Reset confirmed!" << std::endl;
                initializeGame();
                resetConfirmation = false;
            } else {
                MOVE_INTERVAL = std::max(0.05f, MOVE_INTERVAL - 0.05f);
                std::cout << "A button - Speed increased! Interval: " << MOVE_INTERVAL << "s" << std::endl;
            }
            break;
        case SDL_CONTROLLER_BUTTON_B:
            if (exitConfirmation) {
                exitConfirmation = false;
                std::cout << "B button - Exit cancelled!" << std::endl;
            } else if (resetConfirmation) {
                resetConfirmation = false;
                std::cout << "B button - Reset cancelled!" << std::endl;
            } else {
                MOVE_INTERVAL = std::min(1.0f, MOVE_INTERVAL + 0.05f);
                std::cout << "B button - Speed decreased! Interval: " << MOVE_INTERVAL << "s" << std::endl;
            }
            break;
        case SDL_CONTROLLER_BUTTON_X:
            gamePaused = !gamePaused;
            std::cout << "X button - Game " << (gamePaused ? "paused" : "unpaused") << std::endl;
            break;
        case SDL_CONTROLLER_BUTTON_Y:
            if (!resetConfirmation && !exitConfirmation) {
                resetConfirmation = true;
                std::cout << "Y button - Showing reset confirmation" << std::endl;
            }
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            gamePaused = !gamePaused;
            std::cout << "BACK button - Game " << (gamePaused ? "paused" : "unpaused") << std::endl;
            break;
        case SDL_CONTROLLER_BUTTON_START:
            if (!exitConfirmation) {
                exitConfirmation = true;
                std::cout << "Start button - Showing exit confirmation" << std::endl;
            }
            break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            if (!gamePaused && !exitConfirmation && !resetConfirmation) {
                int newLevel = level - 1;
                if (newLevel >= 0) {
                    changeLevel(newLevel);
                    std::cout << "Left Bumper - Level decreased to " << level << std::endl;
                } else {
                    std::cout << "Left Bumper - Already at minimum level (0)" << std::endl;
                }
            } else {
                std::cout << "Left Bumper - Level change blocked (game paused/in dialogue)" << std::endl;
            }
            break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            if (!gamePaused && !exitConfirmation && !resetConfirmation) {
                int newLevel = level + 1;
                if (newLevel <= 1) {
                    changeLevel(newLevel);
                    std::cout << "Right Bumper - Level increased to " << level << std::endl;
                } else {
                    std::cout << "Right Bumper - Already at maximum level (1)" << std::endl;
                }
            } else {
                std::cout << "Right Bumber - Level change blocked (game paused/in dialogue)" << std::endl;
            }
            break;
    }
}

// Handle SDL2 gamepad axis motion (analog sticks)
void handleGamepadAxis(SDL_ControllerAxisEvent* axisEvent) {
    const float deadzone = 0.3f;
    
    if (axisEvent->axis == SDL_CONTROLLER_AXIS_LEFTX) {
        float value = axisEvent->value / 32767.0f;
        if (abs(value) > deadzone && direction.x == 0) {
            // Track gamepad input for display
            usingGamepadInput = true;
            lastButtonTime = SDL_GetTicks() / 1000.0f;
            
            if (value > deadzone) {
                Point newDir = Point(1, 0);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            } else if (value < -deadzone) {
                Point newDir = Point(-1, 0);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            }
        }
    } else if (axisEvent->axis == SDL_CONTROLLER_AXIS_LEFTY) {
        float value = axisEvent->value / 32767.0f;
        if (abs(value) > deadzone && direction.y == 0) {
            // Track gamepad input for display
            usingGamepadInput = true;
            lastButtonTime = SDL_GetTicks() / 1000.0f;
            
            if (value < -deadzone) {
                Point newDir = Point(0, 1);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            } else if (value > deadzone) {
                Point newDir = Point(0, -1);
                Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                if (isValidMove(testHead) || movementPaused) {
                    direction = newDir;
                }
            }
        }
    }
}

int main() {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    // Initialize SDL2_image if available
#ifdef SDL_IMAGE_AVAILABLE
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cout << "SDL2_image could not initialize! Using fallback bitmap. IMG_Error: " << IMG_GetError() << std::endl;
    } else {
        std::cout << "SDL2_image initialized - PNG/JPG support available" << std::endl;
    }
#else
    std::cout << "SDL2_image not available - using BMP support and fallback bitmap" << std::endl;
#endif

    // Set OpenGL version
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Get display mode for fullscreen
    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
        std::cerr << "Failed to get display mode: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    
    std::cout << "Screen: " << displayMode.w << "x" << displayMode.h << std::endl;
    std::cout << "Grid dimensions: " << GRID_WIDTH << "x" << GRID_HEIGHT << std::endl;
    
    // Create fullscreen window
    window = SDL_CreateWindow("Snake Game - SDL2",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              displayMode.w, displayMode.h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    
    if (!window) {
        std::cerr << "Failed to create SDL2 window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    
    // Create OpenGL context
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // Enable VSync
    SDL_GL_SetSwapInterval(1);
    
    // Hide the mouse cursor for gaming
    SDL_ShowCursor(SDL_DISABLE);

    // Initialize GLAD
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Build and compile shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Get uniform locations
    u_offset = glGetUniformLocation(shaderProgram, "u_offset");
    u_color = glGetUniformLocation(shaderProgram, "u_color");
    u_scale = glGetUniformLocation(shaderProgram, "u_scale");
    u_shape_type = glGetUniformLocation(shaderProgram, "u_shape_type");
    u_inner_radius = glGetUniformLocation(shaderProgram, "u_inner_radius");
    u_texture = glGetUniformLocation(shaderProgram, "u_texture");
    u_use_texture = glGetUniformLocation(shaderProgram, "u_use_texture");

    // Setup vertex data
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    GLuint EBO;
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Initialize game
    initializeGame();
    
    // Load apple texture (try different formats)
    appleTexture = loadTexture("apple.bmp");  // Try BMP first (always supported)
    if (appleTexture == 0) {
        appleTexture = loadTexture("apple.png");  // Try PNG (requires SDL2_image)
    }
    if (appleTexture == 0) {
        appleTexture = loadTexture("apple.jpg");  // Try JPG (requires SDL2_image)
    }
    if (appleTexture == 0) {
        std::cout << "No apple image found, creating procedural apple bitmap..." << std::endl;
        appleTexture = createAppleBitmap();
    }
    
    // Initialize game controller if available
    if (SDL_NumJoysticks() > 0) {
        gameController = SDL_GameControllerOpen(0);
        if (gameController) {
            std::cout << "=== CONTROLLER DETECTED ===" << std::endl;
            std::cout << "Controller Name: " << SDL_GameControllerName(gameController) << std::endl;
            std::cout << "Using SDL2 GAMEPAD INPUT" << std::endl;
            std::cout << "=========================" << std::endl;
            
            // Initialize rumble system
            initializeRumble();
        }
    }
    
    std::cout << "Snake Game Controls (SDL2 Version):\n";
    std::cout << "  D-pad/Left Stick: Move snake\n";
    std::cout << "  A button: Speed up / Confirm\n";
    std::cout << "  B button: Slow down / Cancel\n";
    std::cout << "  X button: Pause/Unpause\n";
    std::cout << "  Y button: Reset confirmation\n";
    std::cout << "  Start button: Exit confirmation\n";

    // Main game loop
    while (running) {
        float currentTime = SDL_GetTicks() / 1000.0f;
        
        // Update flash timer
        flashTimer = currentTime;
        
        // Update rumble system
        updateRumble();
        
        // Handle SDL2 events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    handleKeyboardEvent(&event.key);
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    handleGamepadButtonDown(&event.cbutton);
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    handleGamepadAxis(&event.caxis);
                    break;
                default:
                    break;
            }
        }
        
        // Update game logic at fixed intervals
        if (!gamePaused && !exitConfirmation && !resetConfirmation && currentTime - lastMoveTime > MOVE_INTERVAL) {
            updateGame();
            lastMoveTime = currentTime;
        }
        
        // Update pacman at its own interval
        if (!gamePaused && !exitConfirmation && !resetConfirmation && pacmanActive && currentTime - lastPacmanMoveTime > PACMAN_MOVE_INTERVAL) {
            updatePacman();
            lastPacmanMoveTime = currentTime;
        }
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        render();

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    cleanupRumble();
    if (gameController) {
        SDL_GameControllerClose(gameController);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    
    // Cleanup textures
    if (appleTexture != 0) {
        glDeleteTextures(1, &appleTexture);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
#ifdef SDL_IMAGE_AVAILABLE
    IMG_Quit();
#endif
    SDL_Quit();
    return 0;
} 