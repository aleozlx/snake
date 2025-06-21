#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

// Linux force feedback includes for rumble support
#ifdef __linux__
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#endif

// Vertex shader source for rendering squares and circles
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 u_offset;
uniform vec2 u_scale;
out vec2 texCoord;
void main() {
    texCoord = aPos; // Pass original vertex position [0,1] as texture coordinate
    vec2 pos = (aPos * u_scale) + u_offset;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

// Fragment shader source with proper circle rendering
const char* fragmentShaderSource = R"(
#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform vec3 u_color;
uniform int u_shape_type; // 0 = rectangle, 1 = circle, 2 = ring
uniform float u_inner_radius; // For ring shapes
void main() {
    if (u_shape_type == 0) {
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

// Game constants (will be calculated based on screen aspect ratio)
int GRID_WIDTH = 20;   // Will be calculated
int GRID_HEIGHT = 20;  // Base height

// Screen dimensions and calculated values (will be set in main)
float screenWidth = 1.0f;
float screenHeight = 1.0f;
float aspectRatio = 1.0f;
float cellSize = 0.1f;  // Will be calculated based on screen

// Game state
struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

std::vector<Point> snake;
Point food;
Point direction(1, 0); // Start moving right
bool gameOver = false;
bool movementPaused = false; // Flag to track if movement is paused due to invalid direction
bool gamePaused = false; // Flag to track if game is manually paused by user
bool exitConfirmation = false; // Flag to track if showing exit confirmation dialogue
bool resetConfirmation = false; // Flag to track if showing reset confirmation dialogue
int score = 0;
int level = 0; // Level counter - starts at 0 (Classic Snake)
float lastMoveTime = 0.0f;
float MOVE_INTERVAL = 0.2f; // Move every 200ms (adjustable)
float flashTimer = 0.0f; // Timer for flashing boundary effect
const float FLASH_INTERVAL = 0.1f; // Flash every 300ms

// Pacman state (level 1+ feature)
Point pacman;
Point pacmanDirection(0, 0); // Pacman's current direction
float lastPacmanMoveTime = 0.0f;
float PACMAN_MOVE_INTERVAL = 0.3f; // Pacman moves slightly slower than snake
bool pacmanActive = false; // Only active in level 1+

// Button state tracking for proper press detection
bool aButtonPressed = false;
bool bButtonPressed = false;
bool xButtonPressed = false;
bool yButtonPressed = false;
bool startButtonPressed = false;
bool selectButtonPressed = false;
bool dpadUpPressed = false;
bool dpadDownPressed = false;
bool dpadLeftPressed = false;
bool dpadRightPressed = false;
bool leftBumperPressed = false;
bool rightBumperPressed = false;

// Input source tracking
bool usingGamepadInput = false;
bool usingKeyboardInput = false;
int lastButtonPressed = -1; // Visual debug: show last button number
int lastKeyPressed = -1; // Visual debug: show last key number
float keyPressTime = 0.0f; // Time when key was pressed

// Rumble/vibration system
#ifdef __linux__
int rumbleFd = -1; // File descriptor for force feedback device
int rumbleEffectId = -1; // ID of the rumble effect
bool rumbleInitialized = false;
float rumbleEndTime = 0.0f; // When current rumble should end
const float RUMBLE_DURATION = 0.3f; // Duration of rumble effect in seconds
#endif

// OpenGL objects
GLuint shaderProgram;
GLuint VAO, VBO;
GLint u_offset, u_color, u_scale, u_shape_type, u_inner_radius;

// Square vertices (unit square)
float squareVertices[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f
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
    movementPaused = false; // Reset pause state
    gamePaused = false; // Reset manual pause state
    exitConfirmation = false; // Reset exit confirmation state
    resetConfirmation = false; // Reset reset confirmation state
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
    // Transform grid coordinates to NDC coordinates
    // Map grid cells to fill entire screen: each cell takes up 2.0f/GRID_WIDTH of NDC space
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    float ndcX = (x * cellWidth) - 1.0f;
    float ndcY = (y * cellHeight) - 1.0f;
    
    glUniform2f(u_offset, ndcX, ndcY);
    glUniform2f(u_scale, cellWidth, cellHeight);
    glUniform3f(u_color, r, g, b);
    glUniform1i(u_shape_type, 0); // Rectangle shape
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

// Draw a ring (hollow circle) with anti-aliasing
void drawRing(float x, float y, float diameter, float innerRadiusRatio, float r, float g, float b) {
    // x, y are in NDC coordinates (center of ring), diameter is outer diameter
    // innerRadiusRatio is ratio of inner radius to outer radius (0.0 to 1.0)
    glUniform2f(u_offset, x - diameter*0.5f, y - diameter*0.5f);
    glUniform2f(u_scale, diameter, diameter);
    glUniform3f(u_color, r, g, b);
    glUniform1i(u_shape_type, 2); // Ring shape
    glUniform1f(u_inner_radius, innerRadiusRatio * 0.5f); // Convert to shader space
    
    // Enable alpha blending for anti-aliasing
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
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

// Helper function to get button name string
const char* getButtonName(int buttonIndex) {
    switch (buttonIndex) {
        case GLFW_GAMEPAD_BUTTON_A: return "A";
        case GLFW_GAMEPAD_BUTTON_B: return "B";
        case GLFW_GAMEPAD_BUTTON_X: return "X";
        case GLFW_GAMEPAD_BUTTON_Y: return "Y";
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return "L_BUMP";
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return "R_BUMP";
        case GLFW_GAMEPAD_BUTTON_BACK: return "MENU";
        case GLFW_GAMEPAD_BUTTON_START: return "VIEW";
        case GLFW_GAMEPAD_BUTTON_GUIDE: return "GUIDE";
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return "L_THUMB";
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return "R_THUMB";
        case GLFW_GAMEPAD_BUTTON_DPAD_UP: return "DPAD_UP";
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return "DPAD_RIGHT";
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return "DPAD_DOWN";
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return "DPAD_LEFT";
        // Steam Deck back buttons (L4, L5, R4, R5) - these are typically mapped to higher button indices
        case 15: return "L4"; // Left back button 1
        case 16: return "L5"; // Left back button 2  
        case 17: return "R4"; // Right back button 1
        case 18: return "R5"; // Right back button 2
        default: return "UNKNOWN";
    }
}

// Rumble/vibration system functions
#ifdef __linux__
bool initializeRumble() {
    // Try to find and open a force feedback device
    const char* devicePaths[] = {
        "/dev/input/event0", "/dev/input/event1", "/dev/input/event2", "/dev/input/event3",
        "/dev/input/event4", "/dev/input/event5", "/dev/input/event6", "/dev/input/event7",
        "/dev/input/event8", "/dev/input/event9", "/dev/input/event10", "/dev/input/event11"
    };
    
    for (int i = 0; i < 12; i++) {
        int fd = open(devicePaths[i], O_RDWR);
        if (fd < 0) continue;
        
        // Check if this device supports force feedback
        unsigned long features[4];
        if (ioctl(fd, EVIOCGBIT(0, EV_MAX), features) < 0) {
            close(fd);
            continue;
        }
        
        if (!(features[0] & (1 << EV_FF))) {
            close(fd);
            continue;
        }
        
        // Check if it supports rumble
        unsigned long ffFeatures[8]; // Increase buffer size for safety
        if (ioctl(fd, EVIOCGBIT(EV_FF, FF_MAX), ffFeatures) < 0) {
            close(fd);
            continue;
        }
        
        // FF_RUMBLE is an effect type (0x50), check if it's supported
        // We need to check if the bit at position FF_RUMBLE is set
        int rumbleBit = FF_RUMBLE;
        int byteIndex = rumbleBit / (sizeof(unsigned long) * 8);
        int bitIndex = rumbleBit % (sizeof(unsigned long) * 8);
        
        if (byteIndex >= 8 || !(ffFeatures[byteIndex] & (1UL << bitIndex))) {
            close(fd);
            continue;
        }
        
        std::cout << "Found force feedback device: " << devicePaths[i] << std::endl;
        rumbleFd = fd;
        
        // Create a rumble effect
        struct ff_effect effect;
        memset(&effect, 0, sizeof(effect));
        effect.type = FF_RUMBLE;
        effect.id = -1;
        effect.u.rumble.strong_magnitude = 0xFFFF; // 100% strength for strong motor
        effect.u.rumble.weak_magnitude = 0xC000;   // 75% strength for weak motor
        effect.replay.length = (int)(RUMBLE_DURATION * 1000); // Duration in milliseconds
        effect.replay.delay = 0;
        
        if (ioctl(rumbleFd, EVIOCSFF, &effect) < 0) {
            std::cout << "Failed to create rumble effect" << std::endl;
            close(rumbleFd);
            rumbleFd = -1;
            continue;
        }
        
        rumbleEffectId = effect.id;
        rumbleInitialized = true;
        std::cout << "Rumble system initialized successfully!" << std::endl;
        return true;
    }
    
    std::cout << "No force feedback device found or failed to initialize rumble" << std::endl;
    return false;
}

void cleanupRumble() {
    if (rumbleFd >= 0) {
        if (rumbleEffectId >= 0) {
            ioctl(rumbleFd, EVIOCRMFF, rumbleEffectId);
        }
        close(rumbleFd);
        rumbleFd = -1;
        rumbleEffectId = -1;
        rumbleInitialized = false;
    }
}

void triggerRumble() {
    if (!rumbleInitialized || rumbleFd < 0 || rumbleEffectId < 0) return;
    
    // Stop any existing rumble
    struct input_event stop;
    stop.type = EV_FF;
    stop.code = rumbleEffectId;
    stop.value = 0;
    write(rumbleFd, &stop, sizeof(stop));
    
    // Start new rumble
    struct input_event play;
    play.type = EV_FF;
    play.code = rumbleEffectId;
    play.value = 1;
    write(rumbleFd, &play, sizeof(play));
    
    rumbleEndTime = glfwGetTime() + RUMBLE_DURATION;
    std::cout << "🎮 RUMBLE! Collision detected!" << std::endl;
}

void updateRumble() {
    if (!rumbleInitialized) return;
    
    // Check if rumble should stop
    float currentTime = glfwGetTime();
    if (rumbleEndTime > 0.0f && currentTime >= rumbleEndTime) {
        if (rumbleFd >= 0 && rumbleEffectId >= 0) {
            struct input_event stop;
            stop.type = EV_FF;
            stop.code = rumbleEffectId;
            stop.value = 0;
            write(rumbleFd, &stop, sizeof(stop));
        }
        rumbleEndTime = 0.0f;
    }
}
#else
// Dummy functions for non-Linux platforms
bool initializeRumble() { 
    std::cout << "Rumble not supported on this platform" << std::endl;
    return false; 
}
void cleanupRumble() {}
void triggerRumble() {}
void updateRumble() {}
#endif

void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    
    // Set uniforms - scale to fill one grid cell and fill entire screen
    float scaleX = 2.0f / GRID_WIDTH;
    float scaleY = 2.0f / GRID_HEIGHT;
    
    // Don't apply aspect ratio correction - let tiles stretch to fill screen completely
    // This ensures no black gaps between tiles
    
    glUniform2f(u_scale, scaleX, scaleY);
    
    // Draw food first (background layer)
    drawSquare(food.x, food.y, 1.0f, 0.0f, 0.0f); // Red
    
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
    
    // Draw directional indicators to debug orientation
    // Draw corner markers to show screen orientation
    drawSquare(0, 0, 1.0f, 1.0f, 0.0f);                    // Bottom-left: Yellow
    drawSquare(GRID_WIDTH-1, 0, 0.0f, 1.0f, 1.0f);         // Bottom-right: Cyan  
    drawSquare(0, GRID_HEIGHT-1, 1.0f, 0.0f, 1.0f);        // Top-left: Magenta
    drawSquare(GRID_WIDTH-1, GRID_HEIGHT-1, 1.0f, 1.0f, 1.0f); // Top-right: White
    
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
    
    // Visual debug: Display last pressed button name below level info
    if (lastButtonPressed >= 0) {
        float buttonTextX = levelTextX;
        float buttonTextY = levelTextY - textSize * 3.0f; // Below level info
        
        // Get the button name and display it
        const char* buttonName = getButtonName(lastButtonPressed);
        drawText(buttonName, buttonTextX, buttonTextY, textSize * 0.6f, 1.0f, 1.0f, 0.0f); // Yellow text
        
        // Display "GAMEPAD:" label above the button name
        drawText("GAMEPAD", buttonTextX, buttonTextY + textSize * 0.8f, textSize * 0.6f, 0.0f, 1.0f, 1.0f); // Cyan text
    }
    
    // Visual debug: Display last pressed key in top-right corner
    if (lastKeyPressed >= 0) {
        float currentTime = glfwGetTime();
        if (currentTime - keyPressTime < 5.0f) { // Show for 5 seconds
            // Convert grid coordinates to NDC coordinates for text rendering
            float cellWidth = 2.0f / GRID_WIDTH;
            float cellHeight = 2.0f / GRID_HEIGHT;
            float textX = ((GRID_WIDTH - 10) * cellWidth) - 1.0f; // Right side
            float textY = ((GRID_HEIGHT - 3) * cellHeight) - 1.0f; // Near top
            
            float textSize = cellHeight * 0.8f; // Text size is 80% of a cell height
            
            // Display keyboard warning
            drawText("KEYBOARD", textX, textY + textSize * 1.2f, textSize, 1.0f, 0.0f, 0.0f); // Red text
            
            // Display the key code
            if (lastKeyPressed == 256) {
                drawText("ESC", textX, textY, textSize, 1.0f, 0.5f, 0.0f); // Orange ESC
            } else {
                // For other keys, just show "KEY" since we don't have full character set
                drawText("KEY", textX, textY, textSize, 1.0f, 0.2f, 0.2f); // Light red
            }
        }
    }
    
    // Draw snake above text elements - change color based on game state
    for (size_t i = 0; i < snake.size(); i++) {
        float intensity = i == 0 ? 1.0f : 0.6f; // Head brighter than body
        
        float r, g, b; // Store the colors for potential eye rendering
        
        if (exitConfirmation) {
            // Red snake when showing exit confirmation
            r = intensity; g = 0.0f; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b); // Red
        } else if (resetConfirmation) {
            // Orange snake when showing reset confirmation
            r = intensity; g = intensity * 0.5f; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b); // Orange
        } else if (gamePaused) {
            // Yellow snake when paused
            r = intensity; g = intensity; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b); // Yellow
        } else if (movementPaused) {
            // Purple snake when movement is paused
            r = intensity; g = 0.0f; b = intensity;
            drawSquare(snake[i].x, snake[i].y, r, g, b); // Purple
        } else {
            // Normal green snake
            r = 0.0f; g = intensity; b = 0.0f;
            drawSquare(snake[i].x, snake[i].y, r, g, b); // Green
        }
        
        // Draw eyes on the snake's head (first segment)
        if (i == 0 && !gameOver) {
            drawSnakeEyes(snake[i].x, snake[i].y, food.x, food.y, r, g, b);
        }
    }
    
    // Draw border with different effects for different pause states
    float borderR, borderG, borderB;
    if (exitConfirmation) {
        // Steady orange when showing exit confirmation
        borderR = 1.0f; borderG = 0.5f; borderB = 0.0f; // Orange
    } else if (resetConfirmation) {
        // Steady red-orange when showing reset confirmation
        borderR = 1.0f; borderG = 0.3f; borderB = 0.0f; // Red-orange
    } else if (gamePaused) {
        // Steady orange when manually paused
        borderR = 1.0f; borderG = 0.5f; borderB = 0.0f; // Orange
    } else if (movementPaused) {
        // Flash between red and gray based on timer when movement paused
        bool showRed = ((int)(flashTimer / FLASH_INTERVAL) % 2) == 0;
        if (showRed) {
            borderR = 1.0f; borderG = 0.0f; borderB = 0.0f; // Red
        } else {
            borderR = 0.5f; borderG = 0.5f; borderB = 0.5f; // Gray
        }
    } else {
        borderR = 0.5f; borderG = 0.5f; borderB = 0.5f; // Gray
    }
    
    for (int i = 1; i < GRID_WIDTH-1; i++) { // Skip corners by starting at 1 and ending at GRID_WIDTH-2
        drawSquare(i, 0, borderR, borderG, borderB);              // Bottom
        drawSquare(i, GRID_HEIGHT-1, borderR, borderG, borderB);  // Top
    }
    for (int i = 1; i < GRID_HEIGHT-1; i++) { // Skip corners by starting at 1 and ending at GRID_HEIGHT-2
        drawSquare(0, i, borderR, borderG, borderB);              // Left
        drawSquare(GRID_WIDTH-1, i, borderR, borderG, borderB);   // Right
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

// Keyboard callback with visual debug
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        // Store key info for visual debug
        lastKeyPressed = key;
        keyPressTime = glfwGetTime();
        
        // Show visual warning instead of immediate exit
        std::cout << ">>> KEYBOARD INPUT DETECTED <<<" << std::endl;
        std::cout << "Key " << key << " (scancode: " << scancode << ") pressed!" << std::endl;
        
        // Special handling for ESC key - show exit confirmation
        if (key == 256) { // ESC
            std::cout << "ESC key detected - showing exit confirmation!" << std::endl;
            exitConfirmation = true; // Show confirmation dialogue
        }
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Get primary monitor for fullscreen
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    // Calculate aspect ratio and game area dimensions for square cells
    screenWidth = mode->width;
    screenHeight = mode->height;
    aspectRatio = screenWidth / screenHeight;
    
    // Steam Deck might report orientation differently than expected
    // If tiles appear tall instead of square, the aspect ratio might be inverted
    bool invertAspectRatio = true; // Enable for Steam Deck orientation fix
    if (invertAspectRatio) {
        aspectRatio = screenHeight / screenWidth;
        std::cout << "Using inverted aspect ratio for Steam Deck orientation" << std::endl;
    }
    
    // Use a fixed grid size and apply aspect ratio correction in scale
    GRID_WIDTH = 32;   // Fixed width
    GRID_HEIGHT = 20;  // Fixed height
    
    std::cout << "Screen: " << screenWidth << "x" << screenHeight << ", aspect ratio: " << aspectRatio << std::endl;
    std::cout << "Grid dimensions: " << GRID_WIDTH << "x" << GRID_HEIGHT << std::endl;
    
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Snake Game", monitor, NULL);
    if (!window) { 
        std::cerr << "Failed to create GLFW window\n"; 
        glfwTerminate(); 
        return -1; 
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    
    // Hide the mouse cursor for gaming
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    if (!gladLoadGL(glfwGetProcAddress)) {
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

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Initialize game
    initializeGame();
    
    // Initialize rumble system
    initializeRumble();
    
    std::cout << "Snake Game Controls (GAMEPAD ONLY):\n";
    std::cout << "Steam Deck Controller:\n";
    std::cout << "  D-pad: Move snake (Up/Down/Left/Right)\n";
    std::cout << "  Left Analog Stick: Alternative movement control\n";
    std::cout << "  A button: Speed up movement / Confirm action\n";
    std::cout << "  B button: Slow down movement / Cancel action\n";
    std::cout << "  X button: Pause/Unpause game\n";
    std::cout << "  Y button: Show RESET confirmation\n";
    std::cout << "  Left Bumper (L1): Decrease level (Level 1 → 0)\n";
    std::cout << "  Right Bumper (R1): Increase level (Level 0 → 1)\n";
    std::cout << "  Start button: Alternative quit\n";
    std::cout << "  Menu button (≡, left top): Pause/Unpause\n";
    std::cout << "  View button (⧉, right top): Show EXIT confirmation\n";
    std::cout << "\nLevel Features:\n";
    std::cout << "  Level 0: Classic Snake (previous state)\n";
    std::cout << "  Level 1: PACMAN COMPETITION! Yellow Pacman competes for food\n";
    std::cout << "    - Pacman tries to reach food first\n";
    std::cout << "    - If Pacman gets food, snake must wait for next food\n";
    std::cout << "    - If snake hits Pacman, snake turns purple (like hitting wall)\n";
    std::cout << "    - Pacman cannot move through snake\n";
    std::cout << "\nConfirmation Dialogues:\n";
    std::cout << "  Exit: Red snake, orange border, A=Exit, B=Cancel\n";
    std::cout << "  Reset: Orange snake, red-orange border, A=Reset, B=Cancel\n";
    std::cout << "\nRumble Effects:\n";
    std::cout << "  🎮 Controller vibrates when snake hits boundaries, itself, or Pacman\n";
    std::cout << "  Duration: 0.3 seconds per collision\n";
    std::cout << "Keyboard input is DISABLED for pure controller experience.\n";

    // Main game loop
    while (!glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        
        // Update flash timer for boundary flashing effect
        flashTimer = currentTime;
        
        // Update rumble system
        updateRumble();
        
        // No automatic timeout - confirmation dialogue stays until user responds
        
        // Update game logic at fixed intervals (only if not paused or in any confirmation)
        if (!gamePaused && !exitConfirmation && !resetConfirmation && currentTime - lastMoveTime > MOVE_INTERVAL) {
            updateGame();
            lastMoveTime = currentTime;
        }
        
        // Update pacman at its own interval
        if (!gamePaused && !exitConfirmation && !resetConfirmation && pacmanActive && currentTime - lastPacmanMoveTime > PACMAN_MOVE_INTERVAL) {
            updatePacman();
            lastPacmanMoveTime = currentTime;
        }
        
        // Handle gamepad input (Steam Deck support)
        // Check all possible joystick slots
        int jid = -1;
        for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
            if (glfwJoystickPresent(i) && glfwJoystickIsGamepad(i)) {
                jid = i;
                break;
            }
        }
        
        if (jid != -1) {
            // Debug: Show controller information
            static bool controllerInfoPrinted = false;
            if (!controllerInfoPrinted) {
                const char* name = glfwGetJoystickName(jid);
                const char* guid = glfwGetJoystickGUID(jid);
                std::cout << "=== CONTROLLER DETECTED ===" << std::endl;
                std::cout << "Controller Name: " << (name ? name : "Unknown") << std::endl;
                std::cout << "Controller GUID: " << (guid ? guid : "Unknown") << std::endl;
                std::cout << "Using RAW GAMEPAD INPUT (not keyboard emulation)" << std::endl;
                std::cout << "=========================" << std::endl;
                controllerInfoPrinted = true;
            }
            
            GLFWgamepadstate state;
            if (glfwGetGamepadState(jid, &state)) {
                usingGamepadInput = true;
                
                // Debug: Show which buttons are pressed with detailed button mapping
                static bool anyButtonPressed = false;
                bool buttonCurrentlyPressed = false;
                
                for (int i = 0; i < GLFW_GAMEPAD_BUTTON_LAST; i++) {
                    if (state.buttons[i] == GLFW_PRESS) {
                        buttonCurrentlyPressed = true;
                        lastButtonPressed = i; // Store for visual debug
                        // Print detailed button information
                        std::cout << ">>> RAW GAMEPAD BUTTON " << i << " PRESSED <<<" << std::endl;
                        
                        // Map button numbers to names for debugging
                        const char* buttonName = "UNKNOWN";
                        switch (i) {
                            case GLFW_GAMEPAD_BUTTON_A: buttonName = "A"; break;
                            case GLFW_GAMEPAD_BUTTON_B: buttonName = "B"; break;
                            case GLFW_GAMEPAD_BUTTON_X: buttonName = "X"; break;
                            case GLFW_GAMEPAD_BUTTON_Y: buttonName = "Y"; break;
                            case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: buttonName = "LEFT_BUMPER"; break;
                            case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: buttonName = "RIGHT_BUMPER"; break;
                            case GLFW_GAMEPAD_BUTTON_BACK: buttonName = "BACK/MENU"; break;
                            case GLFW_GAMEPAD_BUTTON_START: buttonName = "START"; break;
                            case GLFW_GAMEPAD_BUTTON_GUIDE: buttonName = "GUIDE/VIEW"; break;
                            case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: buttonName = "LEFT_THUMB"; break;
                            case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: buttonName = "RIGHT_THUMB"; break;
                            case GLFW_GAMEPAD_BUTTON_DPAD_UP: buttonName = "DPAD_UP"; break;
                            case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: buttonName = "DPAD_RIGHT"; break;
                            case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: buttonName = "DPAD_DOWN"; break;
                            case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: buttonName = "DPAD_LEFT"; break;
                        }
                        std::cout << "Button name: " << buttonName << std::endl;
                        break;
                    }
                }
                
                if (buttonCurrentlyPressed && !anyButtonPressed) {
                    anyButtonPressed = true;
                } else if (!buttonCurrentlyPressed) {
                    anyButtonPressed = false;
                }
                
                // D-pad controls with proper press detection
                if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS) {
                    std::cout << "D-pad UP pressed" << std::endl;
                    if (!dpadUpPressed && direction.y == 0) {
                        Point newDir = Point(0, 1);
                        Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                        if (isValidMove(testHead) || movementPaused) {
                            direction = newDir;
                        }
                        dpadUpPressed = true;
                    }
                } else {
                    dpadUpPressed = false;
                }
                
                if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS) {
                    std::cout << "D-pad DOWN pressed" << std::endl;
                    if (!dpadDownPressed && direction.y == 0) {
                        Point newDir = Point(0, -1);
                        Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                        if (isValidMove(testHead) || movementPaused) {
                            direction = newDir;
                        }
                        dpadDownPressed = true;
                    }
                } else {
                    dpadDownPressed = false;
                }
                
                if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS) {
                    std::cout << "D-pad LEFT pressed" << std::endl;
                    if (!dpadLeftPressed && direction.x == 0) {
                        Point newDir = Point(-1, 0);
                        Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                        if (isValidMove(testHead) || movementPaused) {
                            direction = newDir;
                        }
                        dpadLeftPressed = true;
                    }
                } else {
                    dpadLeftPressed = false;
                }
                
                if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS) {
                    std::cout << "D-pad RIGHT pressed" << std::endl;
                    if (!dpadRightPressed && direction.x == 0) {
                        Point newDir = Point(1, 0);
                        Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                        if (isValidMove(testHead) || movementPaused) {
                            direction = newDir;
                        }
                        dpadRightPressed = true;
                    }
                } else {
                    dpadRightPressed = false;
                }
                
                // Left analog stick controls (with deadzone)
                const float deadzone = 0.3f;
                float leftX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
                float leftY = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
                
                if (abs(leftX) > deadzone || abs(leftY) > deadzone) {
                    if (abs(leftX) > abs(leftY)) {
                        // Horizontal movement
                        if (leftX > deadzone && direction.x == 0) {
                            Point newDir = Point(1, 0);  // Right
                            Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                            if (isValidMove(testHead) || movementPaused) {
                                direction = newDir;
                            }
                        }
                        else if (leftX < -deadzone && direction.x == 0) {
                            Point newDir = Point(-1, 0); // Left
                            Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                            if (isValidMove(testHead) || movementPaused) {
                                direction = newDir;
                            }
                        }
                    } else {
                        // Vertical movement (note: Y-axis is typically inverted on gamepads)
                        if (leftY < -deadzone && direction.y == 0) {
                            Point newDir = Point(0, 1);  // Up
                            Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                            if (isValidMove(testHead) || movementPaused) {
                                direction = newDir;
                            }
                        }
                        else if (leftY > deadzone && direction.y == 0) {
                            Point newDir = Point(0, -1); // Down
                            Point testHead = Point(snake[0].x + newDir.x, snake[0].y + newDir.y);
                            if (isValidMove(testHead) || movementPaused) {
                                direction = newDir;
                            }
                        }
                    }
                }
                
                // Steam Deck controls - handle all buttons properly
                
                // A button - Speed up movement OR Confirm action
                if (state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS) {
                    if (!aButtonPressed) {
                        aButtonPressed = true;
                        if (exitConfirmation) {
                            // Confirm exit in confirmation mode
                            std::cout << "A button - Exit confirmed!" << std::endl;
                            glfwSetWindowShouldClose(window, GLFW_TRUE);
                        } else if (resetConfirmation) {
                            // Confirm reset in confirmation mode
                            std::cout << "A button - Reset confirmed!" << std::endl;
                            initializeGame();
                        } else {
                            // Normal speed up function
                            MOVE_INTERVAL = std::max(0.05f, MOVE_INTERVAL - 0.05f);
                            std::cout << "A button - Speed increased! Interval: " << MOVE_INTERVAL << "s (" << (MOVE_INTERVAL * 1000) << "ms)" << std::endl;
                        }
                    }
                } else {
                    aButtonPressed = false;
                }
                
                // B button - Slow down movement OR Cancel confirmation
                if (state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS) {
                    if (!bButtonPressed) {
                        bButtonPressed = true;
                        if (exitConfirmation) {
                            // Cancel exit in confirmation mode
                            exitConfirmation = false;
                            std::cout << "B button - Exit cancelled!" << std::endl;
                        } else if (resetConfirmation) {
                            // Cancel reset in confirmation mode
                            resetConfirmation = false;
                            std::cout << "B button - Reset cancelled!" << std::endl;
                        } else {
                            // Normal speed down function
                            MOVE_INTERVAL = std::min(1.0f, MOVE_INTERVAL + 0.05f);
                            std::cout << "B button - Speed decreased! Interval: " << MOVE_INTERVAL << "s (" << (MOVE_INTERVAL * 1000) << "ms)" << std::endl;
                        }
                    }
                } else {
                    bButtonPressed = false;
                }
                
                // X button - Pause/Unpause
                if (state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS) {
                    if (!xButtonPressed) {
                        xButtonPressed = true;
                        gamePaused = !gamePaused;
                        std::cout << "X button - Game " << (gamePaused ? "paused" : "unpaused") << std::endl;
                    }
                } else {
                    xButtonPressed = false;
                }
                
                // Y button - Show reset confirmation
                if (state.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS) {
                    if (!yButtonPressed) {
                        yButtonPressed = true;
                        if (!resetConfirmation && !exitConfirmation) {
                            resetConfirmation = true;
                            std::cout << "Y button - Showing reset confirmation" << std::endl;
                        } else {
                            std::cout << "Y button pressed but already in confirmation mode" << std::endl;
                        }
                    }
                } else {
                    yButtonPressed = false;
                }
                
                // START button now handled above as View button - no separate handler needed
                
                // Menu button (left top button) - Pause/Unpause
                if (state.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS) {
                    if (!selectButtonPressed) {
                        selectButtonPressed = true;
                        gamePaused = !gamePaused;
                        std::cout << "Menu button (left top) - Game " << (gamePaused ? "paused" : "unpaused") << std::endl;
                    }
                } else {
                    selectButtonPressed = false;
                }
                
                // View button (right top button) - Show exit confirmation (mapped to START button #7)
                if (state.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS) {
                    if (!startButtonPressed) { // Avoid double-processing with the existing START handler
                        startButtonPressed = true;
                        std::cout << ">>> VIEW BUTTON (START #7) DETECTED <<<" << std::endl;
                        if (!exitConfirmation) {
                            exitConfirmation = true;
                            std::cout << "View button (right top) - Showing exit confirmation" << std::endl;
                            std::cout << "Exit confirmation state set to TRUE" << std::endl;
                        } else {
                            std::cout << "View button pressed but already in exit confirmation mode" << std::endl;
                        }
                    }
                } else {
                    startButtonPressed = false;
                }
                
                // Left Bumper - Decrease level (only during normal play)
                if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS) {
                    if (!leftBumperPressed) {
                        leftBumperPressed = true;
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
                    }
                } else {
                    leftBumperPressed = false;
                }
                
                // Right Bumper - Increase level (only during normal play)
                if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS) {
                    if (!rightBumperPressed) {
                        rightBumperPressed = true;
                        if (!gamePaused && !exitConfirmation && !resetConfirmation) {
                            int newLevel = level + 1;
                            if (newLevel <= 1) {
                                changeLevel(newLevel);
                                std::cout << "Right Bumper - Level increased to " << level << std::endl;
                            } else {
                                std::cout << "Right Bumper - Already at maximum level (1)" << std::endl;
                            }
                        } else {
                            std::cout << "Right Bumper - Level change blocked (game paused/in dialogue)" << std::endl;
                        }
                    }
                } else {
                    rightBumperPressed = false;
                }
            }
        } else {
            static bool printed = false;
            if (!printed) {
                std::cout << "No gamepad detected" << std::endl;
                printed = true;
            }
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup rumble system
    cleanupRumble();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

