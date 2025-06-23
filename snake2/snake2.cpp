#include "snake_dep.h"  // IWYU pragma: keep
#include "circular_buffer.h"
#include "pathfinding.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Try to include SDL2_image if available
#ifdef SDL_IMAGE_AVAILABLE
#ifdef __APPLE__
#include <SDL_image.h>
#else
#include <SDL2/SDL_image.h>
#endif
#endif

// Debug mode for smaller grid in IPC mode
#define IPC_DEBUG_SMALL_GRID 0  // Set to 1 to enable smaller grid for IPC debugging

// Shader loading functions
std::string loadShaderFromFile(const char* filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open shader file: " << filepath << std::endl;
    return "";
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  
  std::string content = buffer.str();
  std::cout << "Loaded shader: " << filepath << " (" << content.length() << " bytes)" << std::endl;
  return content;
}

GLuint compileShader(const std::string& source, GLenum shaderType, const char* shaderName) {
  GLuint shader = glCreateShader(shaderType);
  const char* sourcePtr = source.c_str();
  glShaderSource(shader, 1, &sourcePtr, NULL);
  glCompileShader(shader);
  
  // Check for compilation errors
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLchar infoLog[512];
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    std::cerr << "ERROR: " << shaderName << " shader compilation failed:\n" << infoLog << std::endl;
    glDeleteShader(shader);
    return 0;
  }
  
  std::cout << "✅ " << shaderName << " shader compiled successfully" << std::endl;
  return shader;
}

// Game constants
#if IPC_DEBUG_SMALL_GRID
int GRID_WIDTH = 16;
int GRID_HEIGHT = 10;  // Maintain 1.6:1 aspect ratio (16:10)
#else
int GRID_WIDTH = 32;
int GRID_HEIGHT = 20;
#endif

// 2D array to track what's in each tile (allocated dynamically based on grid size)
TileContent** tileGrid = nullptr;

// Gyroscope and food physics (Level 2+ feature)
bool gyroSupported = false;
SDL_Sensor *gyroSensor = nullptr;
float foodVelocityX = 0.0f;
float foodVelocityY = 0.0f;
float foodPosX = 0.0f; // Continuous position for smooth movement
float foodPosY = 0.0f;
const float FOOD_FRICTION = 0.95f;      // Friction to slow down food movement
const float GYRO_SENSITIVITY = 0.5f;    // How much gyro affects food movement
const float FOOD_BOUNCE_DAMPING = 0.7f; // Energy loss when bouncing off walls
float lastGyroUpdateTime = 0.0f;
const float GYRO_UPDATE_INTERVAL = 0.016f; // ~60 FPS for smooth physics

// Game state
std::vector<Snake> snakes; // Multiple snakes instead of single snake
std::vector<Snake> aiSnakes; // AI-controlled snakes (separate for extensibility)
int numControllers = 0;
std::vector<SDL_GameController*> gameControllers; // Array of controllers

Point food;
bool gameOver = false;
bool gamePaused = false;
bool exitConfirmation = false;
bool resetConfirmation = false;
int score = 0;
int level = 0; // Level system: 0=JUST SNAKE, 1=PACMAN, 2=MULTI SNAKE
float lastMoveTime = 0.0f;
float MOVE_INTERVAL = 0.2f;
float flashTimer = 0.0f;
const float FLASH_INTERVAL = 0.1f;

// Pacman state (level 1 feature)
Point pacman;
Point pacmanDirection(0, 0); // Pacman's current direction
float lastPacmanMoveTime = 0.0f;
float PACMAN_MOVE_INTERVAL = 0.3f; // Pacman moves slightly slower than snake
bool pacmanActive = false;         // Only active in level 1

// AI Snake state (level 2+ feature)
float lastAISnakeMoveTime = 0.0f;
float AI_SNAKE_MOVE_INTERVAL = 0.25f; // AI snake moves at moderate speed

// A* pathfinding structures and functions moved to algorithm/pathfinding.h

// Input tracking for gamepad display
bool usingGamepadInput = false;
int lastButtonPressed = -1; // For visual debug
float lastButtonTime = 0.0f;

// Sensor detection tracking for display
bool hasGyroscope = false;
bool hasAccelerometer = false;
bool hasLeftGyro = false;
bool hasRightGyro = false;
bool hasLeftAccel = false;
bool hasRightAccel = false;

// Rumble/vibration system
bool rumbleSupported = false;
float rumbleEndTime = 0.0f;         // When current rumble should end
const float RUMBLE_DURATION = 0.3f; // Duration of rumble effect in seconds

// SDL2 variables
SDL_Window *window = nullptr;
SDL_GLContext glContext = nullptr;
bool running = true;

// IPC Mode variables
bool ipcMode = false;
MemoryMappedCircularBuffer* circularBuffer = nullptr;

// OpenGL objects
GLuint shaderProgram;
GLuint VAO, VBO;
GLint u_offset, u_color, u_scale, u_shape_type, u_inner_radius, u_texture,
    u_use_texture, u_aspect_ratio;
GLuint appleTexture = 0; // Apple texture for food

// Square vertices with texture coordinates (position + texcoord)
float squareVertices[] = {
    // Position   // TexCoord
    0.0f, 0.0f, 0.0f, 1.0f, // Bottom-left
    1.0f, 0.0f, 1.0f, 1.0f, // Bottom-right
    1.0f, 1.0f, 1.0f, 0.0f, // Top-right
    0.0f, 1.0f, 0.0f, 0.0f  // Top-left
};

GLuint indices[] = {0, 1, 2, 2, 3, 0};

// Forward declarations
bool isValidMoveForSnake(const Point &newHead, int snakeIndex);
void clearTileGrid();

// Callback function for pathfinding algorithm
bool isPositionOccupiedCallback(const Point& pos, void* context) {
  // Check boundaries
  if (pos.x <= 0 || pos.x >= GRID_WIDTH - 1 || pos.y <= 0 || pos.y >= GRID_HEIGHT - 1) {
    return true;
  }
  
  // Use tile grid for O(1) lookup if available
  if (tileGrid != nullptr && pos.x >= 0 && pos.x < GRID_WIDTH && pos.y >= 0 && pos.y < GRID_HEIGHT) {
    TileContent content = tileGrid[pos.x][pos.y];
    return content != TileContent::EMPTY && content != TileContent::FOOD;
  }
  
  // Fallback to original method if tile grid is not available
  // Check player snakes
  for (const auto& snake : snakes) {
    for (const auto& segment : snake.body) {
      if (pos == segment) {
        return true;
      }
    }
  }
  
  // Check AI snakes
  for (const auto& aiSnake : aiSnakes) {
    for (const auto& segment : aiSnake.body) {
      if (pos == segment) {
        return true;
      }
    }
  }
  
  // Check pacman
  if (pacmanActive && pos == pacman) {
    return true;
  }
  
  return false;
}

// Tile grid management functions
void initializeTileGrid() {
  if (tileGrid != nullptr) {
    // Clean up existing grid
    for (int x = 0; x < GRID_WIDTH; x++) {
      delete[] tileGrid[x];
    }
    delete[] tileGrid;
  }
  
  // Allocate new grid
  tileGrid = new TileContent*[GRID_WIDTH];
  for (int x = 0; x < GRID_WIDTH; x++) {
    tileGrid[x] = new TileContent[GRID_HEIGHT];
  }
  
  clearTileGrid();
}

void clearTileGrid() {
  if (tileGrid == nullptr) return;
  
  // Clear all tiles and set borders
  for (int x = 0; x < GRID_WIDTH; x++) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
      if (x == 0 || x == GRID_WIDTH - 1 || y == 0 || y == GRID_HEIGHT - 1) {
        tileGrid[x][y] = TileContent::BORDER;
      } else {
        tileGrid[x][y] = TileContent::EMPTY;
      }
    }
  }
}

void cleanupTileGrid() {
  if (tileGrid == nullptr) return;
  
  for (int x = 0; x < GRID_WIDTH; x++) {
    delete[] tileGrid[x];
  }
  delete[] tileGrid;
  tileGrid = nullptr;
}

void setTileContent(int x, int y, TileContent content) {
  if (tileGrid != nullptr && x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
    tileGrid[x][y] = content;
  }
}

// A* pathfinding helper functions moved to algorithm/pathfinding.cpp

// A* pathfinding algorithm moved to algorithm/pathfinding.cpp

// Helper function to check if a move is valid
bool isValidMove(const Point &newHead) {
  // Legacy function - only checks snake[0] for backward compatibility
  return isValidMoveForSnake(newHead, 0);
}

bool isValidMoveForSnake(const Point &newHead, int snakeIndex) {
  // Check boundary collision - pause before entering the border area
  if (newHead.x == 0 || newHead.x == GRID_WIDTH - 1 || newHead.y == 0 ||
      newHead.y == GRID_HEIGHT - 1) {
    return false;
  }

  // Check self collision for the specific snake
  if (snakeIndex >= 0 && snakeIndex < snakes.size()) {
    for (const auto &segment : snakes[snakeIndex].body) {
      if (newHead == segment) {
        return false;
      }
    }
  }

  // Check collision with other snakes
  for (int i = 0; i < snakes.size(); i++) {
    if (i != snakeIndex) { // Don't check against self
      for (const auto &segment : snakes[i].body) {
        if (newHead == segment) {
          return false;
        }
      }
    }
  }

  // Check collision with AI snakes
  for (const auto &aiSnake : aiSnakes) {
    for (const auto &segment : aiSnake.body) {
      if (newHead == segment) {
        return false;
      }
    }
  }

  // Check pacman collision - if pacman is in the way, treat as collision
  if (pacmanActive && newHead == pacman) {
    return false;
  }

  return true;
}

// Helper function to check if an AI snake move is valid
bool isValidMoveForAISnake(const Point &newHead, int aiSnakeIndex) {
  // Check boundary collision
  if (newHead.x == 0 || newHead.x == GRID_WIDTH - 1 || newHead.y == 0 ||
      newHead.y == GRID_HEIGHT - 1) {
    return false;
  }

  // Check self collision for the specific AI snake
  if (aiSnakeIndex >= 0 && aiSnakeIndex < aiSnakes.size()) {
    for (const auto &segment : aiSnakes[aiSnakeIndex].body) {
      if (newHead == segment) {
        return false;
      }
    }
  }

  // Check collision with player snakes
  for (const auto &snake : snakes) {
    for (const auto &segment : snake.body) {
      if (newHead == segment) {
        return false;
      }
    }
  }

  // Check collision with other AI snakes
  for (int i = 0; i < aiSnakes.size(); i++) {
    if (i != aiSnakeIndex) { // Don't check against self
      for (const auto &segment : aiSnakes[i].body) {
        if (newHead == segment) {
          return false;
        }
      }
    }
  }

  // Check pacman collision
  if (pacmanActive && newHead == pacman) {
    return false;
  }

  return true;
}

// Helper function to check if a pacman move is valid
bool isValidPacmanMove(const Point &newPos) {
  // Check boundary collision
  if (newPos.x <= 0 || newPos.x >= GRID_WIDTH - 1 || newPos.y <= 0 ||
      newPos.y >= GRID_HEIGHT - 1) {
    return false;
  }

  // Check snake collision - pacman can't move into snake
  for (const auto &segment : snakes[0].body) {
    if (newPos == segment) {
      return false;
    }
  }

  return true;
}

// Simple AI for pacman to move toward food
Point calculatePacmanDirection() {
  if (!pacmanActive)
    return Point(0, 0);

  // Calculate direction toward food
  int dx = food.x - pacman.x;
  int dy = food.y - pacman.y;

  // Try to move toward food, prioritizing the axis with greater distance
  std::vector<Point> possibleMoves;

  if (abs(dx) >= abs(dy)) {
    // Prioritize horizontal movement
    if (dx > 0)
      possibleMoves.push_back(Point(1, 0)); // Right
    if (dx < 0)
      possibleMoves.push_back(Point(-1, 0)); // Left
    if (dy > 0)
      possibleMoves.push_back(Point(0, 1)); // Up
    if (dy < 0)
      possibleMoves.push_back(Point(0, -1)); // Down
  } else {
    // Prioritize vertical movement
    if (dy > 0)
      possibleMoves.push_back(Point(0, 1)); // Up
    if (dy < 0)
      possibleMoves.push_back(Point(0, -1)); // Down
    if (dx > 0)
      possibleMoves.push_back(Point(1, 0)); // Right
    if (dx < 0)
      possibleMoves.push_back(Point(-1, 0)); // Left
  }

  // Try each possible move in order of preference
  for (const auto &move : possibleMoves) {
    Point newPos = Point(pacman.x + move.x, pacman.y + move.y);
    if (isValidPacmanMove(newPos)) {
      return move;
    }
  }

  // If no preferred move is valid, try any valid move
  std::vector<Point> allMoves = {Point(1, 0), Point(-1, 0), Point(0, 1),
                                 Point(0, -1)};
  for (const auto &move : allMoves) {
    Point newPos = Point(pacman.x + move.x, pacman.y + move.y);
    if (isValidPacmanMove(newPos)) {
      return move;
    }
  }

  // No valid moves - stay still
  return Point(0, 0);
}

// Naive pathfinding (original algorithm) - now uses pathfinding library
Point calculateNaiveDirection(int aiSnakeIndex) {
  if (aiSnakeIndex >= aiSnakes.size())
    return Point(0, 0);

  const Snake& aiSnake = aiSnakes[aiSnakeIndex];
  Point head = aiSnake.body[0];

  // Use pathfinding library for naive direction calculation
  return calculateNaivePathDirection(head, food, GRID_WIDTH, GRID_HEIGHT, 
                                   isPositionOccupiedCallback, nullptr, 
                                   aiSnake.direction);
}

// A* pathfinding direction calculation - now uses pathfinding library
Point calculateAStarDirection(int aiSnakeIndex) {
  if (aiSnakeIndex >= aiSnakes.size())
    return Point(0, 0);

  const Snake& aiSnake = aiSnakes[aiSnakeIndex];
  Point head = aiSnake.body[0];

  // Use pathfinding library for A* direction calculation
  Point direction = calculateAStarPathDirection(head, food, GRID_WIDTH, GRID_HEIGHT, 
                                              isPositionOccupiedCallback, nullptr);
  
  // Validate the move using game-specific validation
  Point newHead = Point(head.x + direction.x, head.y + direction.y);
  if (isValidMoveForAISnake(newHead, aiSnakeIndex)) {
    return direction;
  }
  
  // Fallback to naive algorithm if A* fails
  return calculateNaiveDirection(aiSnakeIndex);
}

// Main AI direction calculation function
Point calculateAISnakeDirection(int aiSnakeIndex) {
  if (aiSnakeIndex >= aiSnakes.size())
    return Point(0, 0);

  const Snake& aiSnake = aiSnakes[aiSnakeIndex];
  
  switch (aiSnake.navType) {
    case NAV_ASTAR:
      return calculateAStarDirection(aiSnakeIndex);
    case NAV_NAIVE:
    default:
      return calculateNaiveDirection(aiSnakeIndex);
  }
}

void initializeGame() {
  snakes.clear();
  aiSnakes.clear();
  
  // Initialize snakes: max(1, controller_count) but limit to 4 snakes maximum
  int totalSnakes = std::min(4, std::max(1, numControllers));
  
  // Colors for different snakes
  RGBColor colors[] = {
    RGBColor(0.0f, 1.0f, 0.0f), // Green - keyboard + controller 0 (snake[0])
    RGBColor(1.0f, 0.0f, 0.0f), // Red - controller 1 (snake[1])
    RGBColor(0.0f, 0.0f, 1.0f), // Blue - controller 2 (snake[2])
    RGBColor(1.0f, 1.0f, 0.0f), // Yellow - controller 3 (snake[3])
    RGBColor(1.0f, 0.0f, 1.0f), // Magenta - controller 4 (snake[4])
    RGBColor(0.0f, 1.0f, 1.0f), // Cyan - controller 5 (snake[5])
    RGBColor(1.0f, 0.5f, 0.0f), // Orange - controller 6 (snake[6])
    RGBColor(0.5f, 0.0f, 1.0f)  // Purple - controller 7 (snake[7])
  };
  
  // AI snake colors (different from player snake colors)
  RGBColor aiColors[] = {
    RGBColor(1.0f, 0.5f, 0.8f), // Pink - AI snake 0
    RGBColor(0.8f, 0.3f, 0.8f), // Purple - AI snake 1
    RGBColor(0.6f, 0.8f, 0.2f), // Lime - AI snake 2
    RGBColor(0.9f, 0.6f, 0.1f)  // Orange - AI snake 3
  };
  
  // Create snakes based on controller count (minimum 1)
  int startX = GRID_WIDTH / 2;
  int startY = GRID_HEIGHT / 2;
  
  for (int i = 0; i < totalSnakes && i < 4; i++) {
    int offsetX = i * 3; // Space them out
    int offsetY = (i % 2 == 0) ? 0 : ((i % 4 < 2) ? 2 : -2); // Alternate positions
    
    SDL_GameController* controller = (i < gameControllers.size()) ? gameControllers[i] : nullptr;
    
    snakes.push_back(Snake(startX + offsetX, startY + offsetY, Point(1, 0), 
                          controller, i, colors[i].r, colors[i].g, colors[i].b));
  }

  gameOver = false;
  gamePaused = false;
  exitConfirmation = false;
  resetConfirmation = false;

  // Initialize pacman for level 1 only (not level 2+)
  pacmanActive = (level == 1);
  if (pacmanActive) {
    // Spawn pacman at random unoccupied location
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(1, GRID_WIDTH - 2);
    std::uniform_int_distribution<> disY(1, GRID_HEIGHT - 2);

    do {
      pacman = Point(disX(gen), disY(gen));
    } while (std::find(snakes[0].body.begin(), snakes[0].body.end(), pacman) != snakes[0].body.end());

    pacmanDirection = Point(0, 0); // Start stationary
    lastPacmanMoveTime = 0.0f;
    std::cout << "Pacman spawned at (" << pacman.x << "," << pacman.y
              << ") for Level " << level << std::endl;
  }

  // Initialize AI snakes for level 2+
  if (level >= 2) {
    // Spawn one AI snake for now (can be extended for more AI snakes)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(2, GRID_WIDTH - 3);
    std::uniform_int_distribution<> disY(2, GRID_HEIGHT - 3);

    Point aiStartPos;
    bool validPosition = false;
    int attempts = 0;
    
    do {
      aiStartPos = Point(disX(gen), disY(gen));
      validPosition = true;
      attempts++;
      
      // Check if position is far enough from player snakes
      for (const auto& snake : snakes) {
        for (const auto& segment : snake.body) {
          int distance = abs(aiStartPos.x - segment.x) + abs(aiStartPos.y - segment.y);
          if (distance < 5) { // Minimum distance of 5 cells
            validPosition = false;
            break;
          }
        }
        if (!validPosition) break;
      }
    } while (!validPosition && attempts < 50);

    if (validPosition) {
      // Create AI snake with pink color, moving left initially
      aiSnakes.push_back(Snake(aiStartPos.x, aiStartPos.y, Point(-1, 0), 
                              nullptr, -1, aiColors[0].r, aiColors[0].g, aiColors[0].b, NAV_ASTAR));
      lastAISnakeMoveTime = 0.0f;
      std::cout << "NPC Snake spawned at (" << aiStartPos.x << "," << aiStartPos.y
                << ") for Level " << level << " (A* pathfinding)" << std::endl;
    }
  }

  // Generate food (only in playable area, excluding borders)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> disX(1, GRID_WIDTH - 2);
  std::uniform_int_distribution<> disY(1, GRID_HEIGHT - 2);

  do {
    food = Point(disX(gen), disY(gen));
    bool foodValid = true;
    
    // Check against player snakes
    for (const auto& snake : snakes) {
      if (std::find(snake.body.begin(), snake.body.end(), food) != snake.body.end()) {
        foodValid = false;
        break;
      }
    }
    
    // Check against AI snakes
    if (foodValid) {
      for (const auto& aiSnake : aiSnakes) {
        if (std::find(aiSnake.body.begin(), aiSnake.body.end(), food) != aiSnake.body.end()) {
          foodValid = false;
          break;
        }
      }
    }
    
    // Check against pacman
    if (foodValid && pacmanActive && food == pacman) {
      foodValid = false;
    }
    
    if (foodValid) break;
  } while (true);

  // Initialize food physics for Level 2+ (MULTI SNAKE mode)
  if (level >= 2) {
    foodPosX = (float)food.x;
    foodPosY = (float)food.y;
    foodVelocityX = 0.0f;
    foodVelocityY = 0.0f;
    lastGyroUpdateTime = 0.0f;
    std::cout << "Food physics initialized for Level " << level
              << " (MULTI SNAKE mode)" << std::endl;
  }
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

void drawSquare(int x, int y, const RGBColor& color) {
  drawSquare(x, y, color.r, color.g, color.b);
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
  // x, y are in NDC coordinates (center of circle), diameter is the size in NDC
  // space
  glUniform2f(u_offset, x - diameter * 0.5f, y - diameter * 0.5f);
  glUniform2f(u_scale, diameter, diameter);
  glUniform3f(u_color, r, g, b);
  glUniform1i(u_shape_type, 1); // Circle shape

  // Simple aspect ratio - disable correction for manual debugging
  float aspectRatio = 1.0f; // No aspect ratio correction - debug manually
  glUniform1f(u_aspect_ratio, aspectRatio);

  // Debug output for significant circles
  static int debugCount = 0;
  if (debugCount < 5) {
    std::cout << "Circle: diameter=" << diameter
              << " aspectRatio=" << aspectRatio
              << " (no correction for manual debug)" << std::endl;
    debugCount++;
  }

  // Enable alpha blending for anti-aliasing
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glDisable(GL_BLEND);
}

// Draw a perfect circle using square dimensions
void drawPerfectCircle(float centerX, float centerY, float radius, float r,
                       float g, float b) {
  // Simply use the radius as provided, but force square dimensions
  float diameter = radius * 2.0f;

  glUniform2f(u_offset, centerX - diameter * 0.5f, centerY - diameter * 0.5f);
  glUniform2f(u_scale, diameter,
              diameter); // Force square: same dimension for both X and Y
  glUniform3f(u_color, r, g, b);
  glUniform1i(u_shape_type, 1); // Circle shape

  // Disable aspect ratio correction for this function
  glUniform1f(u_aspect_ratio, 1.0f); // No aspect ratio correction

  // Enable alpha blending for anti-aliasing
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glDisable(GL_BLEND);
}

// Load texture from file (with optional SDL2_image support)
GLuint loadTexture(const char *filename) {
#ifdef SDL_IMAGE_AVAILABLE
  SDL_Surface *surface = IMG_Load(filename);
  if (!surface) {
    std::cout << "Failed to load texture " << filename << ": " << IMG_GetError()
              << std::endl;
    return 0;
  }
#else
  // Try to load BMP files using SDL2's built-in support
  SDL_Surface *surface = SDL_LoadBMP(filename);
  if (!surface) {
    std::cout << "Failed to load BMP texture " << filename << ": "
              << SDL_GetError() << std::endl;
    std::cout << "Note: Only BMP files supported without SDL2_image"
              << std::endl;
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

  glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format,
               GL_UNSIGNED_BYTE, surface->pixels);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  SDL_FreeSurface(surface);

  std::cout << "Loaded texture: " << filename << " (ID: " << texture << ")"
            << std::endl;
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
      float dist =
          sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));

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

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, appleData);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  std::cout << "Created procedural apple bitmap (ID: " << texture << ")"
            << std::endl;
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
void drawChar(char c, float startX, float startY, float charSize, float r,
              float g, float b) {
  int charIndex = getCharIndex(c);

  if (charIndex >= 0) {
    float pixelSize =
        charSize / 7.0f; // Each character pixel is 1/7th of character height
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

void drawText(const char *text, float startX, float startY, float charSize,
              float r, float g, float b) {
  float x = startX;
  float charWidth =
      charSize * (5.0f / 7.0f); // Character width is 5/7 of height
  while (*text) {
    drawChar(*text, x, startY, charSize, r, g, b);
    x += charWidth + (charSize * 0.2f); // Character width + small space
    text++;
  }
}

// Draw round eyes on the snake head that look towards the food
void drawSnakeEyes(int headX, int headY, int foodX, int foodY, float snakeR,
                   float snakeG, float snakeB, Point snakeDirection) {
  // Calculate cell dimensions in NDC space
  float cellWidth = 2.0f / GRID_WIDTH;
  float cellHeight = 2.0f / GRID_HEIGHT;

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
  float eyeOffsetFromCenter =
      cellWidth * 0.25f; // How far from center towards front of head

  // Calculate perpendicular vector for eye spacing (90 degree rotation of
  // movement direction)
  float perpX = -moveDirY;
  float perpY = moveDirX;

  // Position the two eyes at the front of the head along the perpendicular axis
  float leftEyeX =
      headNdcX + (moveDirX * eyeOffsetFromCenter) + (perpX * eyeSpacing);
  float leftEyeY =
      headNdcY + (moveDirY * eyeOffsetFromCenter) + (perpY * eyeSpacing);

  float rightEyeX =
      headNdcX + (moveDirX * eyeOffsetFromCenter) - (perpX * eyeSpacing);
  float rightEyeY =
      headNdcY + (moveDirY * eyeOffsetFromCenter) - (perpY * eyeSpacing);

  // Draw the round eyes (white circles) - now they'll be proper circles!
  drawCircle(leftEyeX, leftEyeY, eyeDiameter, 1.0f, 1.0f, 1.0f);
  drawCircle(rightEyeX, rightEyeY, eyeDiameter, 1.0f, 1.0f, 1.0f);

  // Calculate pupil offset based on food looking direction (pupils look towards
  // food)
  float pupilOffsetAmount =
      eyeDiameter * 0.2f; // How much pupils can move within the eye
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

  drawCircle(pupilLeftX + highlightOffsetX, pupilLeftY + highlightOffsetY,
             highlightDiameter, 1.0f, 1.0f, 1.0f);
  drawCircle(pupilRightX + highlightOffsetX, pupilRightY + highlightOffsetY,
             highlightDiameter, 1.0f, 1.0f, 1.0f);
}

// Modular confirmation dialogue rendering
void drawConfirmationDialogue(const char *message, float bgR, float bgG,
                              float bgB) {
  int centerX = GRID_WIDTH / 2;
  int centerY = GRID_HEIGHT / 2;

  // Draw dialogue box background with custom color
  for (int x = centerX - 8; x <= centerX + 8; x++) {
    for (int y = centerY - 3; y <= centerY + 3; y++) {
      if (x >= 1 && x < GRID_WIDTH - 1 && y >= 1 && y < GRID_HEIGHT - 1) {
        drawSquare(x, y, bgR, bgG, bgB); // Custom background color
      }
    }
  }

  // Draw dialogue border (bright white)
  for (int x = centerX - 8; x <= centerX + 8; x++) {
    if (x >= 1 && x < GRID_WIDTH - 1) {
      drawSquare(x, centerY - 3, 1.0f, 1.0f, 1.0f); // Top border
      drawSquare(x, centerY + 3, 1.0f, 1.0f, 1.0f); // Bottom border
    }
  }
  for (int y = centerY - 3; y <= centerY + 3; y++) {
    if (y >= 1 && y < GRID_HEIGHT - 1) {
      drawSquare(centerX - 8, y, 1.0f, 1.0f, 1.0f); // Left border
      drawSquare(centerX + 8, y, 1.0f, 1.0f, 1.0f); // Right border
    }
  }

  // Draw text using the bitmap font system
  float cellWidth = 2.0f / GRID_WIDTH;
  float cellHeight = 2.0f / GRID_HEIGHT;

  // Main message text - centered in the dialogue
  float titleTextSize = cellHeight * 0.6f; // Text size for dialogue
  float titleX =
      ((centerX - 6) * cellWidth) - 1.0f; // One tile margin from left border
  float titleY = ((centerY + 1) * cellHeight) - 1.0f; // Upper area
  drawText(message, titleX, titleY, titleTextSize, 1.0f, 1.0f,
           1.0f); // White text

  // Button prompts with labels
  float buttonTextSize = cellHeight * 0.4f; // Smaller for button labels

  // A button (left side) - YES/CONFIRM
  float aButtonX = ((centerX - 4) * cellWidth) - 1.0f;
  float aButtonY = ((centerY - 2) * cellHeight) - 1.0f;
  drawSquare(centerX - 4, centerY - 2, 0.0f, 1.0f, 0.0f); // Green A button
  drawSquare(centerX - 3, centerY - 2, 0.0f, 1.0f, 0.0f);
  drawText("A", aButtonX + cellWidth * 0.3f, aButtonY + cellHeight * 0.2f,
           buttonTextSize, 0.0f, 0.0f, 0.0f); // Black "A" on green

  // B button (right side) - NO/CANCEL
  float bButtonX = ((centerX + 2) * cellWidth) - 1.0f;
  float bButtonY = ((centerY - 2) * cellHeight) - 1.0f;
  drawSquare(centerX + 2, centerY - 2, 1.0f, 0.0f, 0.0f); // Red B button
  drawSquare(centerX + 3, centerY - 2, 1.0f, 0.0f, 0.0f);
  drawText("B", bButtonX + cellWidth * 0.3f, bButtonY + cellHeight * 0.2f,
           buttonTextSize, 1.0f, 1.0f, 1.0f); // White "B" on red
}

// Rumble/vibration system functions
bool initializeRumble() {
  if (gameControllers.empty()) {
    std::cout << "No game controllers available for rumble" << std::endl;
    return false;
  }

  // Check if any controller supports haptic feedback
  for (auto controller : gameControllers) {
    if (SDL_GameControllerHasRumble(controller)) {
      rumbleSupported = true;
      std::cout << "🎮 Rumble support detected and enabled!" << std::endl;
      return true;
    }
  }
  
  std::cout << "No controllers support rumble" << std::endl;
  return false;
}

// Gyroscope system functions
bool initializeGyroscope() {
  // Reset sensor detection flags
  hasGyroscope = false;
  hasAccelerometer = false;
  hasLeftGyro = false;
  hasRightGyro = false;
  hasLeftAccel = false;
  hasRightAccel = false;

  // Initialize SDL sensor subsystem
  if (SDL_InitSubSystem(SDL_INIT_SENSOR) < 0) {
    std::cout << "Failed to initialize SDL sensor subsystem: " << SDL_GetError()
              << std::endl;
    return false;
  }

  // Check for available sensors
  int numSensors = SDL_NumSensors();
  std::cout << "=== SDL2 SENSOR DETECTION ===" << std::endl;
  std::cout << "Found " << numSensors << " sensors" << std::endl;

  if (numSensors == 0) {
    std::cout << "No sensors detected by SDL2" << std::endl;
    std::cout << "Steam Deck troubleshooting:" << std::endl;
    std::cout << "1. Enable 'Generic Gamepad Configuration' in Steam"
              << std::endl;
    std::cout << "2. Set Gyro to 'As Joystick' in controller settings"
              << std::endl;
    std::cout << "3. Try disabling Steam Input for this game" << std::endl;
    std::cout << "4. Check if gyro works in Steam's controller test"
              << std::endl;
    return false;
  }

  // First pass: detect all sensor types for display
  for (int i = 0; i < numSensors; i++) {
    SDL_SensorType sensorType = SDL_SensorGetDeviceType(i);
    const char *sensorName = SDL_SensorGetDeviceName(i);
    SDL_SensorID sensorID = SDL_SensorGetDeviceInstanceID(i);

    // Track sensor types for display
    switch (sensorType) {
    case SDL_SENSOR_GYRO:
      hasGyroscope = true;
      break;
    case SDL_SENSOR_ACCEL:
      hasAccelerometer = true;
      break;
    case SDL_SENSOR_GYRO_L:
      hasLeftGyro = true;
      break;
    case SDL_SENSOR_GYRO_R:
      hasRightGyro = true;
      break;
    case SDL_SENSOR_ACCEL_L:
      hasLeftAccel = true;
      break;
    case SDL_SENSOR_ACCEL_R:
      hasRightAccel = true;
      break;
    case SDL_SENSOR_INVALID:
    case SDL_SENSOR_UNKNOWN:
      break;
    }

    std::cout << "Sensor " << i << ":" << std::endl;
    std::cout << "  Name: " << (sensorName ? sensorName : "Unknown")
              << std::endl;
    std::cout << "  Type: " << sensorType << " (";

    switch (sensorType) {
    case SDL_SENSOR_INVALID:
      std::cout << "INVALID";
      break;
    case SDL_SENSOR_UNKNOWN:
      std::cout << "UNKNOWN";
      break;
    case SDL_SENSOR_ACCEL:
      std::cout << "ACCELEROMETER";
      break;
    case SDL_SENSOR_GYRO:
      std::cout << "GYROSCOPE";
      break;
    case SDL_SENSOR_ACCEL_L:
      std::cout << "LEFT_ACCELEROMETER";
      break;
    case SDL_SENSOR_GYRO_L:
      std::cout << "LEFT_GYROSCOPE";
      break;
    case SDL_SENSOR_ACCEL_R:
      std::cout << "RIGHT_ACCELEROMETER";
      break;
    case SDL_SENSOR_GYRO_R:
      std::cout << "RIGHT_GYROSCOPE";
      break;
    default:
      std::cout << "TYPE_" << sensorType;
      break;
    }
    std::cout << ")" << std::endl;
    std::cout << "  Instance ID: " << sensorID << std::endl;

    // Try to open any gyroscope type (including left/right variants)
    if (sensorType == SDL_SENSOR_GYRO || sensorType == SDL_SENSOR_GYRO_L ||
        sensorType == SDL_SENSOR_GYRO_R) {

      std::cout << "  Attempting to open gyroscope..." << std::endl;
      gyroSensor = SDL_SensorOpen(i);
      if (gyroSensor) {
        gyroSupported = true;
        std::cout << "  ✅ SUCCESS: Gyroscope opened!" << std::endl;
        std::cout << "🌀 Gyroscope initialized: "
                  << (sensorName ? sensorName : "Unknown Gyro") << std::endl;
        std::cout << "Level 2 MULTI SNAKE mode available!" << std::endl;
        std::cout << "=============================" << std::endl;
        return true;
      } else {
        std::cout << "  ❌ FAILED to open: " << SDL_GetError() << std::endl;
      }
    }
  }

  // If no gyroscope found, try accelerometer as fallback
  std::cout << std::endl
            << "No gyroscope found, checking for accelerometer as fallback..."
            << std::endl;
  for (int i = 0; i < numSensors; i++) {
    SDL_SensorType sensorType = SDL_SensorGetDeviceType(i);

    if (sensorType == SDL_SENSOR_ACCEL || sensorType == SDL_SENSOR_ACCEL_L ||
        sensorType == SDL_SENSOR_ACCEL_R) {

      std::cout
          << "Found accelerometer, attempting to use for tilt detection..."
          << std::endl;
      gyroSensor = SDL_SensorOpen(i);
      if (gyroSensor) {
        gyroSupported = true;
        std::cout << "✅ Using accelerometer for Level 2 MULTI SNAKE mode"
                  << std::endl;
        std::cout
            << "Note: This uses tilt instead of rotation for food movement"
            << std::endl;
        std::cout << "=============================" << std::endl;
        return true;
      }
    }
  }

  std::cout << "❌ No gyroscope or accelerometer sensors found" << std::endl;
        std::cout << "Level 2 MULTI SNAKE mode disabled" << std::endl;
  std::cout << "=============================" << std::endl;
  return false;
}

void cleanupGyroscope() {
  if (gyroSensor) {
    SDL_SensorClose(gyroSensor);
    gyroSensor = nullptr;
    gyroSupported = false;
    std::cout << "Gyroscope cleaned up" << std::endl;
  }
}

// Update food physics based on gyroscope data (Level 2+ feature)
void updateFoodPhysics(float deltaTime) {
  if (!gyroSupported || level < 2)
    return;

  // Get sensor data (3 axes: X, Y, Z)
  float sensorData[3];
  if (SDL_SensorGetData(gyroSensor, sensorData, 3) == 0) {
    float forceX = 0.0f;
    float forceY = 0.0f;

    // Determine sensor type to handle data appropriately
    SDL_SensorType sensorType = SDL_SensorGetType(gyroSensor);

    if (sensorType == SDL_SENSOR_GYRO || sensorType == SDL_SENSOR_GYRO_L ||
        sensorType == SDL_SENSOR_GYRO_R) {
      // Gyroscope data (angular velocity in rad/s)
      // Use X and Y axis rotation to simulate gravity
      forceX = sensorData[0] * GYRO_SENSITIVITY;
      forceY = sensorData[1] * GYRO_SENSITIVITY;
    } else if (sensorType == SDL_SENSOR_ACCEL ||
               sensorType == SDL_SENSOR_ACCEL_L ||
               sensorType == SDL_SENSOR_ACCEL_R) {
      // Accelerometer data (acceleration in m/s²)
      // Use tilt detection - when device tilts, accelerometer shows gravity
      // direction Normalize and scale the accelerometer data
      float accelX = sensorData[0] / 9.81f; // Normalize by gravity (9.81 m/s²)
      float accelY = sensorData[1] / 9.81f;

      // Apply accelerometer forces (inverted for intuitive tilt behavior)
      forceX = -accelX * GYRO_SENSITIVITY * 2.0f; // Scale up for accelerometer
      forceY = -accelY * GYRO_SENSITIVITY * 2.0f;
    } else {
      // Unknown sensor type, try to use raw data
      forceX = sensorData[0] * GYRO_SENSITIVITY;
      forceY = sensorData[1] * GYRO_SENSITIVITY;
    }

    // Apply forces to food velocity
    foodVelocityX += forceX * deltaTime;
    foodVelocityY += forceY * deltaTime;

    // Apply friction to gradually slow down movement
    foodVelocityX *= FOOD_FRICTION;
    foodVelocityY *= FOOD_FRICTION;

    // Update continuous food position
    foodPosX += foodVelocityX * deltaTime;
    foodPosY += foodVelocityY * deltaTime;

    // Handle boundary collisions with bouncing
    bool bounced = false;
    if (foodPosX < 1.0f) {
      foodPosX = 1.0f;
      foodVelocityX = -foodVelocityX * FOOD_BOUNCE_DAMPING;
      bounced = true;
    } else if (foodPosX >= GRID_WIDTH - 1.0f) {
      foodPosX = GRID_WIDTH - 1.0f;
      foodVelocityX = -foodVelocityX * FOOD_BOUNCE_DAMPING;
      bounced = true;
    }

    if (foodPosY < 1.0f) {
      foodPosY = 1.0f;
      foodVelocityY = -foodVelocityY * FOOD_BOUNCE_DAMPING;
      bounced = true;
    } else if (foodPosY >= GRID_HEIGHT - 1.0f) {
      foodPosY = GRID_HEIGHT - 1.0f;
      foodVelocityY = -foodVelocityY * FOOD_BOUNCE_DAMPING;
      bounced = true;
    }

    // Update discrete food position for game logic
    int newFoodX = (int)round(foodPosX);
    int newFoodY = (int)round(foodPosY);

    // Ensure food doesn't land on snake or pacman
    bool validPosition = true;
    for (const auto &segment : snakes[0].body) {
      if (newFoodX == segment.x && newFoodY == segment.y) {
        validPosition = false;
        break;
      }
    }

    if (pacmanActive && newFoodX == pacman.x && newFoodY == pacman.y) {
      validPosition = false;
    }

    // Only update food position if it's valid
    if (validPosition) {
      food.x = newFoodX;
      food.y = newFoodY;
    } else {
      // If food would land on snake/pacman, bounce it away
      foodVelocityX = -foodVelocityX * 1.5f;
      foodVelocityY = -foodVelocityY * 1.5f;
    }

    // Debug output for significant movements
    static float lastDebugTime = 0.0f;
    float currentTime = SDL_GetTicks() / 1000.0f;
    if (bounced || (currentTime - lastDebugTime > 2.0f &&
                    (abs(foodVelocityX) > 0.1f || abs(foodVelocityY) > 0.1f))) {
      const char *sensorTypeName = "UNKNOWN";
      if (sensorType == SDL_SENSOR_GYRO)
        sensorTypeName = "GYRO";
      else if (sensorType == SDL_SENSOR_ACCEL)
        sensorTypeName = "ACCEL";

      std::cout << "🌀 " << sensorTypeName << ": X=" << forceX
                << " Y=" << forceY << " | Food vel: X=" << foodVelocityX
                << " Y=" << foodVelocityY << std::endl;
      lastDebugTime = currentTime;
    }
  }
}

void triggerRumble() {
  if (!rumbleSupported || gameControllers.empty())
    return;

  // Trigger rumble with SDL2's simple interface on all controllers
  // Parameters: controller, low_freq_rumble, high_freq_rumble, duration_ms
  for (auto controller : gameControllers) {
    if (SDL_GameControllerRumble(controller, 0xFFFF, 0xC000,
                                 (Uint32)(RUMBLE_DURATION * 1000)) == 0) {
      rumbleEndTime = (SDL_GetTicks() / 1000.0f) + RUMBLE_DURATION;
      std::cout << "🎮 RUMBLE! Collision detected!" << std::endl;
      break; // Only need to log once
    }
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
  if (rumbleSupported && !gameControllers.empty()) {
    // Stop any ongoing rumble on all controllers
    for (auto controller : gameControllers) {
      SDL_GameControllerRumble(controller, 0, 0, 0);
    }
    rumbleSupported = false;
  }
}

void render() {
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(shaderProgram);
  glBindVertexArray(VAO);

  // Clear tile grid and repopulate it during rendering
  clearTileGrid();

  // Draw food (apple texture)
  setTileContent(food.x, food.y, TileContent::FOOD);
  if (appleTexture != 0) {
    drawTexturedSquare(food.x, food.y, appleTexture);
  } else {
    // Fallback to red square if texture failed to load
    drawSquare(food.x, food.y, 1.0f, 0.0f, 0.0f);
  }

  // Draw pacman (if active) - yellow circle with black circle mouth
  if (pacmanActive) {
    setTileContent(pacman.x, pacman.y, TileContent::PACMAN);
    // Calculate cell dimensions and position
    float cellWidth = 2.0f / GRID_WIDTH;
    float cellHeight = 2.0f / GRID_HEIGHT;
    float pacmanNdcX = (pacman.x * cellWidth) - 1.0f + (cellWidth * 0.5f);
    float pacmanNdcY = (pacman.y * cellHeight) - 1.0f + (cellHeight * 0.5f);

    // Simple approach: use minimum cell dimension for diameter
    float diameter = std::min(cellWidth, cellHeight) * 0.9f;

    // Draw yellow circle for pacman body - simple drawCircle call
    drawCircle(pacmanNdcX, pacmanNdcY, diameter, 1.0f, 1.0f, 0.0f);

    // Draw black circle mouth (half diameter, positioned near edge based on
    // direction)
    float mouthDiameter = diameter * 0.5f;
    float mouthOffset = diameter * 0.3f;

    float mouthX = pacmanNdcX;
    float mouthY = pacmanNdcY;

    // Position mouth based on direction
    if (pacmanDirection.x == 1 ||
        (pacmanDirection.x == 0 &&
         pacmanDirection.y == 0)) {       // Right or stationary
      mouthX += mouthOffset;              // Move mouth to right side
    } else if (pacmanDirection.x == -1) { // Left
      mouthX -= mouthOffset;              // Move mouth to left side
    } else if (pacmanDirection.y == 1) {  // Up
      mouthY += mouthOffset;              // Move mouth to top side
    } else if (pacmanDirection.y == -1) { // Down
      mouthY -= mouthOffset;              // Move mouth to bottom side
    }

    // Draw the black mouth circle - simple drawCircle call
    drawCircle(mouthX, mouthY, mouthDiameter, 0.1f, 0.1f, 0.1f);
  }

  // Draw corner markers
  drawSquare(0, 0, 1.0f, 1.0f, 0.0f);
  drawSquare(GRID_WIDTH - 1, 0, 0.0f, 1.0f, 1.0f);
  drawSquare(0, GRID_HEIGHT - 1, 1.0f, 0.0f, 1.0f);
  drawSquare(GRID_WIDTH - 1, GRID_HEIGHT - 1, 1.0f, 1.0f, 1.0f);

  // Display level counter in top-left corner
  float cellWidth = 2.0f / GRID_WIDTH;
  float cellHeight = 2.0f / GRID_HEIGHT;
  float levelTextX = (2 * cellWidth) - 1.0f; // Grid position 2 to NDC
  float levelTextY = ((GRID_HEIGHT - 2) * cellHeight) - 1.0f; // Near top
  float textSize = cellHeight * 0.8f; // Text size is 80% of a cell height

  // Display level number
  if (level == 0) {
    drawText("LVL 0", levelTextX, levelTextY, textSize, 0.8f, 0.8f,
             0.8f); // Gray text
  } else if (level == 1) {
    drawText("LVL 1", levelTextX, levelTextY, textSize, 0.8f, 0.8f,
             0.8f); // Gray text
  } else if (level == 2) {
    drawText("LVL 2", levelTextX, levelTextY, textSize, 0.8f, 0.8f,
             0.8f); // Gray text
  }

  // Display level description below
  const float kLevelDescColor[3] = {1.0f, 0.8f, 0.0f}; // Orange color for level descriptions
  
  if (level == 0) {
    drawText("JUST SNAKE", levelTextX, levelTextY - textSize * 1.2f,
             textSize * 0.7f, kLevelDescColor[0], kLevelDescColor[1], kLevelDescColor[2]);
  } else if (level == 1) {
    drawText("PACMAN", levelTextX, levelTextY - textSize * 1.2f,
             textSize * 0.7f, kLevelDescColor[0], kLevelDescColor[1], kLevelDescColor[2]);
  } else if (level == 2) {
    drawText("NPC SNAKE", levelTextX, levelTextY - textSize * 1.2f,
             textSize * 0.7f, kLevelDescColor[0], kLevelDescColor[1], kLevelDescColor[2]);
  }

  // Show IPC mode indicator next to level info
  if (ipcMode) {
    float ipcTextX = levelTextX + textSize * 6.0f; // To the right of level description
    float ipcTextY = levelTextY; // Same height as level number
    float ipcTextSize = textSize * 0.5f; // Small font like other indicators
    
    // Show IPC indicator
    drawText("IPC", ipcTextX, ipcTextY, ipcTextSize, 0.0f, 1.0f, 1.0f); // Cyan text
    
    if (circularBuffer) {
      uint32_t write_idx, read_idx, total_writes, total_reads;
      circularBuffer->get_stats(&write_idx, &read_idx, &total_writes, &total_reads);
      
      char statText[32];
      snprintf(statText, sizeof(statText), "W:%u R:%u", write_idx, read_idx);
      drawText(statText, ipcTextX, ipcTextY - ipcTextSize * 1.2f, 
               ipcTextSize * 0.8f, 1.0f, 1.0f, 0.0f); // Yellow text, smaller
    }
  }

  // Display gyroscope status for Level 2
  if (level >= 2) {
    float gyroTextX = levelTextX;
    float gyroTextY = levelTextY - textSize * 2.5f; // Below level description

    if (gyroSupported) {
      // Show gyroscope activity indicator
      float velocityMagnitude =
          sqrt(foodVelocityX * foodVelocityX + foodVelocityY * foodVelocityY);
      if (velocityMagnitude > 0.1f) {
        drawText("GYRO ACTIVE", gyroTextX, gyroTextY, textSize * 0.5f, 0.0f,
                 1.0f, 0.0f); // Green when active
      } else {
        drawText("GYRO READY", gyroTextX, gyroTextY, textSize * 0.5f, 0.0f,
                 0.8f, 1.0f); // Light blue when ready
      }

      // Show food velocity as bars (visual feedback)
      if (velocityMagnitude > 0.05f) {
        float barTextY = gyroTextY - textSize * 0.8f;
        char velText[32];
        snprintf(velText, sizeof(velText), "VEL %.1f", velocityMagnitude);
        drawText(velText, gyroTextX, barTextY, textSize * 0.4f, 1.0f, 1.0f,
                 0.0f); // Yellow velocity text
      }
    } else {
      drawText("NO GYRO", gyroTextX, gyroTextY, textSize * 0.5f, 1.0f, 0.0f,
               0.0f); // Red when not available
    }
  }

  // Visual debug: Display last pressed button name if gamepad input detected
  if (lastButtonPressed >= 0 && usingGamepadInput) {
    float buttonTextX = levelTextX;
    float buttonTextY = levelTextY - textSize * 3.0f; // Below level info

    // Get button name - simplified for SDL2
    const char *buttonName = "UNKNOWN";
    switch (lastButtonPressed) {
    case SDL_CONTROLLER_BUTTON_A:
      buttonName = "A";
      break;
    case SDL_CONTROLLER_BUTTON_B:
      buttonName = "B";
      break;
    case SDL_CONTROLLER_BUTTON_X:
      buttonName = "X";
      break;
    case SDL_CONTROLLER_BUTTON_Y:
      buttonName = "Y";
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      buttonName = "BACK";
      break;
    case SDL_CONTROLLER_BUTTON_GUIDE:
      buttonName = "GUIDE";
      break;
    case SDL_CONTROLLER_BUTTON_START:
      buttonName = "START";
      break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
      buttonName = "LSTICK";
      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
      buttonName = "RSTICK";
      break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      buttonName = "LSHOULDER";
      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      buttonName = "RSHOULDER";
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      buttonName = "DPAD_UP";
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      buttonName = "DPAD_DOWN";
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      buttonName = "DPAD_LEFT";
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      buttonName = "DPAD_RIGHT";
      break;
    case SDL_CONTROLLER_BUTTON_MISC1:
      buttonName = "MISC1";
      break;
    case SDL_CONTROLLER_BUTTON_PADDLE1:
      buttonName = "PADDLE1";
      break;
    case SDL_CONTROLLER_BUTTON_PADDLE2:
      buttonName = "PADDLE2";
      break;
    case SDL_CONTROLLER_BUTTON_PADDLE3:
      buttonName = "PADDLE3";
      break;
    case SDL_CONTROLLER_BUTTON_PADDLE4:
      buttonName = "PADDLE4";
      break;
    case SDL_CONTROLLER_BUTTON_TOUCHPAD:
      buttonName = "TOUCHPAD";
      break;
    }

    drawText(buttonName, buttonTextX, buttonTextY, textSize * 0.6f, 1.0f, 1.0f,
             0.0f); // Yellow text

    // Display "GAMEPAD:" label above the button name
    drawText("GAMEPAD", buttonTextX, buttonTextY + textSize * 0.8f,
             textSize * 0.6f, 0.0f, 1.0f, 1.0f); // Cyan text

    // Display sensor status on the same line as GAMEPAD
    float sensorTextX =
        buttonTextX + textSize * 4.5f; // To the right of "GAMEPAD"
    float sensorTextY = buttonTextY + textSize * 0.8f;
    float sensorSpacing = textSize * 0.4f;

    // Display each sensor type with color coding (same font size as GAMEPAD)
    float currentX = sensorTextX;
    float sensorFontSize = textSize * 0.6f; // Same as GAMEPAD font size

    // Gyroscope sensors
    if (hasGyroscope || hasLeftGyro || hasRightGyro) {
      drawText("GYRO", currentX, sensorTextY, sensorFontSize, 0.0f, 1.0f,
               1.0f); // Cyan for detected
      currentX += textSize * 2.5f;
    } else {
      drawText("GYRO", currentX, sensorTextY, sensorFontSize, 1.0f, 0.0f,
               0.0f); // Red for not detected
      currentX += textSize * 2.5f;
    }

    // Accelerometer sensors
    if (hasAccelerometer || hasLeftAccel || hasRightAccel) {
      drawText("ACCEL", currentX, sensorTextY, sensorFontSize, 0.0f, 1.0f,
               1.0f); // Cyan for detected
      currentX += textSize * 3.0f;
    } else {
      drawText("ACCEL", currentX, sensorTextY, sensorFontSize, 1.0f, 0.0f,
               0.0f); // Red for not detected
      currentX += textSize * 3.0f;
    }

    // Left sensors (if detected separately)
    if (hasLeftGyro || hasLeftAccel) {
      drawText("L", currentX, sensorTextY, sensorFontSize, 0.0f, 1.0f,
               1.0f); // Cyan for detected
      currentX += textSize * 1.0f;
    }

    // Right sensors (if detected separately)
    if (hasRightGyro || hasRightAccel) {
      drawText("R", currentX, sensorTextY, sensorFontSize, 0.0f, 1.0f,
               1.0f); // Cyan for detected
    }
  }

  // Draw all player snakes
  for (size_t snakeIdx = 0; snakeIdx < snakes.size(); snakeIdx++) {
    const Snake& currentSnake = snakes[snakeIdx];
    if (!currentSnake.isAlive) continue;
    
    for (size_t i = 0; i < currentSnake.body.size(); i++) {
      float intensity = i == 0 ? 1.0f : 0.6f;
      float r, g, b; // Store colors for eye rendering

      if (exitConfirmation) {
        r = intensity;
        g = 0.0f;
        b = 0.0f;
      } else if (resetConfirmation) {
        r = intensity;
        g = intensity * 0.5f;
        b = 0.0f;
      } else if (gamePaused) {
        r = intensity;
        g = intensity;
        b = 0.0f;
      } else if (currentSnake.movementPaused) {
        r = intensity;
        g = 0.0f;
        b = intensity;
      } else {
        // Use snake's individual color
        RGBColor snakeColor = currentSnake.color * intensity;
        r = snakeColor.r;
        g = snakeColor.g;
        b = snakeColor.b;
      }
      
      // Update tile grid
      TileContent tileType = (i == 0) ? TileContent::SNAKE_HEAD : TileContent::SNAKE_BODY;
      setTileContent(currentSnake.body[i].x, currentSnake.body[i].y, tileType);
      
      drawSquare(currentSnake.body[i].x, currentSnake.body[i].y, r, g, b);

      // Draw eyes on the snake's head (first segment)
      if (i == 0 && !gameOver) {
        drawSnakeEyes(currentSnake.body[i].x, currentSnake.body[i].y, food.x, food.y, r, g, b, currentSnake.direction);
      }
    }
  }

  // Draw all AI snakes
  for (size_t aiSnakeIdx = 0; aiSnakeIdx < aiSnakes.size(); aiSnakeIdx++) {
    const Snake& aiSnake = aiSnakes[aiSnakeIdx];
    if (!aiSnake.isAlive) continue;
    
    for (size_t i = 0; i < aiSnake.body.size(); i++) {
      float intensity = i == 0 ? 1.0f : 0.6f;
      float r, g, b; // Store colors for eye rendering

      if (exitConfirmation) {
        r = intensity;
        g = 0.0f;
        b = 0.0f;
      } else if (resetConfirmation) {
        r = intensity;
        g = intensity * 0.5f;
        b = 0.0f;
      } else if (gamePaused) {
        r = intensity;
        g = intensity;
        b = 0.0f;
      } else if (aiSnake.movementPaused) {
        r = intensity;
        g = 0.0f;
        b = intensity;
      } else {
        // Use AI snake's individual color
        RGBColor aiSnakeColor = aiSnake.color * intensity;
        r = aiSnakeColor.r;
        g = aiSnakeColor.g;
        b = aiSnakeColor.b;
      }
      
      // Update tile grid
      TileContent tileType = (i == 0) ? TileContent::AI_SNAKE_HEAD : TileContent::AI_SNAKE_BODY;
      setTileContent(aiSnake.body[i].x, aiSnake.body[i].y, tileType);
      
      drawSquare(aiSnake.body[i].x, aiSnake.body[i].y, r, g, b);

      // Draw eyes on the AI snake's head (first segment)
      if (i == 0 && !gameOver) {
        drawSnakeEyes(aiSnake.body[i].x, aiSnake.body[i].y, food.x, food.y, r, g, b, aiSnake.direction);
      }
    }
  }

  // Draw border
  float borderR, borderG, borderB;
  if (exitConfirmation) {
    borderR = 1.0f;
    borderG = 0.5f;
    borderB = 0.0f;
  } else if (resetConfirmation) {
    borderR = 1.0f;
    borderG = 0.3f;
    borderB = 0.0f;
  } else if (gamePaused) {
    borderR = 1.0f;
    borderG = 0.5f;
    borderB = 0.0f;
  } else if (!snakes.empty() && snakes[0].movementPaused) {
    bool showRed = ((int)(flashTimer / FLASH_INTERVAL) % 2) == 0;
    if (showRed) {
      borderR = 1.0f;
      borderG = 0.0f;
      borderB = 0.0f;
    } else {
      borderR = 0.5f;
      borderG = 0.5f;
      borderB = 0.5f;
    }
  } else {
    borderR = 0.5f;
    borderG = 0.5f;
    borderB = 0.5f;
  }

  for (int i = 1; i < GRID_WIDTH - 1; i++) {
    drawSquare(i, 0, borderR, borderG, borderB);
    drawSquare(i, GRID_HEIGHT - 1, borderR, borderG, borderB);
  }
  for (int i = 1; i < GRID_HEIGHT - 1; i++) {
    drawSquare(0, i, borderR, borderG, borderB);
    drawSquare(GRID_WIDTH - 1, i, borderR, borderG, borderB);
  }

  // Draw confirmation dialogues using modular system
  if (exitConfirmation) {
    drawConfirmationDialogue("CONFIRM EXIT", 0.1f, 0.1f,
                             0.3f); // Dark blue background
  }

  if (resetConfirmation) {
    drawConfirmationDialogue("CONFIRM RESET", 0.3f, 0.1f,
                             0.1f); // Dark red background
  }
}

void updateGame() {
  if (gameOver)
    return;

  // Update all snakes
  bool anySnakeGotFood = false;
  for (size_t snakeIdx = 0; snakeIdx < snakes.size(); snakeIdx++) {
    Snake& currentSnake = snakes[snakeIdx];
    if (!currentSnake.isAlive) continue;

    Point newHead = Point(currentSnake.body[0].x + currentSnake.direction.x, 
                         currentSnake.body[0].y + currentSnake.direction.y);
    bool snakeCanMove = isValidMove(newHead);
    bool snakeGotFood = false;

    // Check if the snake move is valid
    if (!snakeCanMove) {
      // Invalid move - pause movement until direction changes
      if (!currentSnake.movementPaused) {
        // Only trigger rumble on the first collision, not repeatedly while paused
        triggerRumble();
        std::cout << "COLLISION! Snake " << snakeIdx << " hit boundary, itself, or Pacman!" << std::endl;
      }
      currentSnake.movementPaused = true;

      // Even though snake is blocked, still allow pacman to compete for food
      snakeGotFood = false; // Snake can't get food if blocked
    } else {
      // Valid move - resume movement if it was paused
      if (currentSnake.movementPaused) {
        currentSnake.movementPaused = false;
        std::cout << "Movement resumed for Snake " << snakeIdx << "!" << std::endl;
      }

      // Move the snake
      currentSnake.body.insert(currentSnake.body.begin(), newHead);

      // Check if snake got the food
      snakeGotFood = (newHead == food);
      if (snakeGotFood) {
        anySnakeGotFood = true;
        currentSnake.score++;
        std::cout << "Snake " << snakeIdx << " scored! Score: " << currentSnake.score << std::endl;
      }
    }

    // If snake didn't get food, remove tail (unless it was blocked)
    if (!snakeGotFood && snakeCanMove) {
      currentSnake.body.pop_back();
    }
  }

  // Check if pacman got the food
  bool pacmanGotFood = false;
  if (pacmanActive && pacman == food) {
    pacmanGotFood = true;
  }

  // Check if any AI snake got the food
  bool anyAISnakeGotFood = false;
  for (size_t aiIdx = 0; aiIdx < aiSnakes.size(); aiIdx++) {
    Snake& aiSnake = aiSnakes[aiIdx];
    if (!aiSnake.isAlive) continue;
    
    if (!aiSnake.body.empty() && aiSnake.body[0] == food) {
      anyAISnakeGotFood = true;
      aiSnake.score++;
      std::cout << "NPC Snake " << aiIdx << " scored! Score: " << aiSnake.score << std::endl;
      break; // Only one AI snake can get food at a time
    }
  }

  // Generate new food if any snake got it, pacman got it, or AI snake got it
  if (anySnakeGotFood || pacmanGotFood || anyAISnakeGotFood) {
    // Generate new food (avoiding all snakes and pacman)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(1, GRID_WIDTH - 2);
    std::uniform_int_distribution<> disY(1, GRID_HEIGHT - 2);

    bool foodPlacementValid = false;
    do {
      food = Point(disX(gen), disY(gen));
      foodPlacementValid = true;
      
      // Check against all player snake bodies
      for (const Snake& snake : snakes) {
        if (std::find(snake.body.begin(), snake.body.end(), food) != snake.body.end()) {
          foodPlacementValid = false;
          break;
        }
      }
      
      // Check against all AI snake bodies
      if (foodPlacementValid) {
        for (const Snake& aiSnake : aiSnakes) {
          if (std::find(aiSnake.body.begin(), aiSnake.body.end(), food) != aiSnake.body.end()) {
            foodPlacementValid = false;
            break;
          }
        }
      }
      
      // Check against pacman
      if (foodPlacementValid && pacmanActive && food == pacman) {
        foodPlacementValid = false;
      }
    } while (!foodPlacementValid);
  }
}

void updatePacman() {
  if (!pacmanActive)
    return;

  // Calculate new direction toward food
  pacmanDirection = calculatePacmanDirection();

  // Move pacman
  Point newPacmanPos =
      Point(pacman.x + pacmanDirection.x, pacman.y + pacmanDirection.y);

  if (isValidPacmanMove(newPacmanPos)) {
    pacman = newPacmanPos;
    // std::cout << "Pacman moved to (" << pacman.x << "," << pacman.y << ")" <<
    // std::endl;
  }
}

void updateAISnakes() {
  if (aiSnakes.empty())
    return;

  // Update all AI snakes
  for (size_t aiIdx = 0; aiIdx < aiSnakes.size(); aiIdx++) {
    Snake& aiSnake = aiSnakes[aiIdx];
    if (!aiSnake.isAlive) continue;

    // Calculate new direction for AI snake
    Point newDirection = calculateAISnakeDirection(aiIdx);
    aiSnake.direction = newDirection;

    // Calculate new head position
    Point newHead = Point(aiSnake.body[0].x + aiSnake.direction.x, 
                         aiSnake.body[0].y + aiSnake.direction.y);
    bool aiSnakeCanMove = isValidMoveForAISnake(newHead, aiIdx);
    bool aiSnakeGotFood = false;

    // Check if the AI snake move is valid
    if (!aiSnakeCanMove) {
      // Invalid move - pause movement until direction changes
      if (!aiSnake.movementPaused) {
        std::cout << "COLLISION! NPC Snake " << aiIdx << " hit boundary, itself, or other entities!" << std::endl;
      }
      aiSnake.movementPaused = true;
    } else {
      // Valid move - resume movement if it was paused
      if (aiSnake.movementPaused) {
        aiSnake.movementPaused = false;
        std::cout << "Movement resumed for NPC Snake " << aiIdx << "!" << std::endl;
      }

      // Move the AI snake
      aiSnake.body.insert(aiSnake.body.begin(), newHead);

      // Check if AI snake got the food
      aiSnakeGotFood = (newHead == food);
    }

    // If AI snake didn't get food, remove tail (unless it was blocked)
    if (!aiSnakeGotFood && aiSnakeCanMove) {
      aiSnake.body.pop_back();
    }
  }
}

void changeLevel(int newLevel) {
  if (newLevel < 0 || newLevel > 2)
    return; // Levels 0, 1, and 2 supported
  if (newLevel == level)
    return; // No change needed

  int oldLevel = level;
  level = newLevel;

  std::cout << "Level changed from " << oldLevel << " to " << level
            << std::endl;

  // Handle pacman spawning/despawning
  if (level == 0) {
    // Level 0: Despawn pacman and AI snakes
    pacmanActive = false;
    aiSnakes.clear();
    std::cout << "Pacman and NPC snakes despawned for Level 0 (Classic Snake)" << std::endl;
  } else if (level == 1) {
    // Level 1: Spawn pacman, despawn NPC snakes
    pacmanActive = true;
    aiSnakes.clear();

    // Spawn pacman at random unoccupied location
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(1, GRID_WIDTH - 2);
    std::uniform_int_distribution<> disY(1, GRID_HEIGHT - 2);

    do {
      pacman = Point(disX(gen), disY(gen));
    } while (std::find(snakes[0].body.begin(), snakes[0].body.end(), pacman) != snakes[0].body.end() ||
             pacman == food);

    pacmanDirection = Point(0, 0); // Start stationary
    lastPacmanMoveTime = 0.0f;
    std::cout << "Pacman spawned at (" << pacman.x << "," << pacman.y
              << ") for Level " << level << std::endl;
  } else if (level >= 2) {
    // Level 2+: Despawn pacman, spawn NPC snakes
    pacmanActive = false;
    aiSnakes.clear();
    
    // Spawn AI snake
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(2, GRID_WIDTH - 3);
    std::uniform_int_distribution<> disY(2, GRID_HEIGHT - 3);

    Point aiStartPos;
    bool validPosition = false;
    int attempts = 0;
    
    do {
      aiStartPos = Point(disX(gen), disY(gen));
      validPosition = true;
      attempts++;
      
      // Check if position is far enough from player snakes
      for (const auto& snake : snakes) {
        for (const auto& segment : snake.body) {
          int distance = abs(aiStartPos.x - segment.x) + abs(aiStartPos.y - segment.y);
          if (distance < 5) { // Minimum distance of 5 cells
            validPosition = false;
            break;
          }
        }
        if (!validPosition) break;
      }
      
      // Check if not on food
      if (validPosition && aiStartPos == food) {
        validPosition = false;
      }
    } while (!validPosition && attempts < 50);

    if (validPosition) {
      // Create NPC snake with pink color, moving left initially, using A* pathfinding
      // Use colors from aiColors array for consistency
      RGBColor aiColors[] = {
        RGBColor(1.0f, 0.5f, 0.8f), // Pink - AI snake 0
        RGBColor(0.8f, 0.3f, 0.8f), // Purple - AI snake 1
        RGBColor(0.6f, 0.8f, 0.2f), // Lime - AI snake 2
        RGBColor(0.9f, 0.6f, 0.1f)  // Orange - AI snake 3
      };
      aiSnakes.push_back(Snake(aiStartPos.x, aiStartPos.y, Point(-1, 0), 
                              nullptr, -1, aiColors[0].r, aiColors[0].g, aiColors[0].b, NAV_ASTAR));
      lastAISnakeMoveTime = 0.0f;
      std::cout << "NPC Snake spawned at (" << aiStartPos.x << "," << aiStartPos.y
                << ") for Level " << level << " (A* pathfinding)" << std::endl;
    }
  }

      // Handle food physics for Level 2+ (MULTI SNAKE mode)
  if (level >= 2 && gyroSupported) {
    // Initialize food physics
    foodPosX = (float)food.x;
    foodPosY = (float)food.y;
    foodVelocityX = 0.0f;
    foodVelocityY = 0.0f;
    lastGyroUpdateTime = 0.0f;
            std::cout << "🌀 NPC SNAKE mode activated! Tilt device to move the food!"
                  << std::endl;
  } else if (level >= 2 && !gyroSupported) {
          std::cout << "🐍 Level 2 NPC SNAKE mode - gyroscope disabled for testing"
                << std::endl;
    std::cout << "Level 2 features NPC Snake that competes for food" << std::endl;
  } else if (level < 2) {
    // Reset food physics when leaving MULTI SNAKE mode
    foodVelocityX = 0.0f;
    foodVelocityY = 0.0f;
    std::cout << "Food physics disabled for Level " << level << std::endl;
  }
}

// Handle SDL2 keyboard events
void handleKeyboardEvent(SDL_KeyboardEvent *keyEvent) {
  if (keyEvent->type == SDL_KEYDOWN) {
    std::cout << ">>> KEYBOARD INPUT DETECTED <<<" << std::endl;

    switch (keyEvent->keysym.sym) {
    // Movement keys - Arrow keys
    case SDLK_UP:
      if (!snakes.empty() && snakes[0].direction.y == 0) {
        Point newDir = Point(0, 1);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "Arrow Up - Moving up" << std::endl;
        }
      }
      break;
    case SDLK_DOWN:
      if (!snakes.empty() && snakes[0].direction.y == 0) {
        Point newDir = Point(0, -1);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "Arrow Down - Moving down" << std::endl;
        }
      }
      break;
    case SDLK_LEFT:
      if (!snakes.empty() && snakes[0].direction.x == 0) {
        Point newDir = Point(-1, 0);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "Arrow Left - Moving left" << std::endl;
        }
      }
      break;
    case SDLK_RIGHT:
      if (!snakes.empty() && snakes[0].direction.x == 0) {
        Point newDir = Point(1, 0);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "Arrow Right - Moving right" << std::endl;
        }
      }
      break;
    
    // Movement keys - WASD
    case SDLK_w:
      if (!snakes.empty() && snakes[0].direction.y == 0) {
        Point newDir = Point(0, 1);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "W key - Moving up" << std::endl;
        }
      }
      break;
    case SDLK_s:
      if (!snakes.empty() && snakes[0].direction.y == 0) {
        Point newDir = Point(0, -1);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "S key - Moving down" << std::endl;
        }
      }
      break;
    case SDLK_a:
      if (!snakes.empty() && snakes[0].direction.x == 0) {
        Point newDir = Point(-1, 0);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "A key - Moving left" << std::endl;
        }
      }
      break;
    case SDLK_d:
      if (!snakes.empty() && snakes[0].direction.x == 0) {
        Point newDir = Point(1, 0);
        Point testHead = Point(snakes[0].body[0].x + newDir.x, snakes[0].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, 0) || snakes[0].movementPaused) {
          snakes[0].direction = newDir;
          std::cout << "D key - Moving right" << std::endl;
        }
      }
      break;
    
    // Action keys (gamepad equivalents)
    case SDLK_RETURN: // Enter = A button
      if (exitConfirmation) {
        std::cout << "Enter key - Exit confirmed!" << std::endl;
        running = false;
      } else if (resetConfirmation) {
        std::cout << "Enter key - Reset confirmed!" << std::endl;
        initializeGame();
        resetConfirmation = false;
      } else {
        MOVE_INTERVAL = std::max(0.05f, MOVE_INTERVAL - 0.05f);
        std::cout << "Enter key - Speed increased! Interval: " << MOVE_INTERVAL
                  << "s" << std::endl;
      }
      break;
    
    case SDLK_ESCAPE: // Esc = B button
      if (exitConfirmation) {
        exitConfirmation = false;
        std::cout << "ESC key - Exit cancelled!" << std::endl;
      } else if (resetConfirmation) {
        resetConfirmation = false;
        std::cout << "ESC key - Reset cancelled!" << std::endl;
      } else {
        MOVE_INTERVAL = std::min(1.0f, MOVE_INTERVAL + 0.05f);
        std::cout << "ESC key - Speed decreased! Interval: " << MOVE_INTERVAL
                  << "s" << std::endl;
      }
      break;
    
    case SDLK_SPACE: // Space = X button
      gamePaused = !gamePaused;
      std::cout << "Space key - Game " << (gamePaused ? "paused" : "unpaused")
                << std::endl;
      break;
    
    case SDLK_r: // R = Y button
      if (!resetConfirmation && !exitConfirmation) {
        resetConfirmation = true;
        std::cout << "R key - Showing reset confirmation" << std::endl;
      }
      break;
    
    case SDLK_PAGEUP: // Page Up = Right Shoulder (increase level)
      if (!gamePaused && !exitConfirmation && !resetConfirmation) {
        int newLevel = level + 1;
        if (newLevel <= 2) {
          changeLevel(newLevel);
          std::cout << "Page Up - Level increased to " << level << std::endl;
        } else {
          std::cout << "Page Up - Already at maximum level (2)" << std::endl;
        }
      } else {
        std::cout
            << "Page Up - Level change blocked (game paused/in dialogue)"
            << std::endl;
      }
      break;
    
    case SDLK_PAGEDOWN: // Page Down = Left Shoulder (decrease level)
      if (!gamePaused && !exitConfirmation && !resetConfirmation) {
        int newLevel = level - 1;
        if (newLevel >= 0) {
          changeLevel(newLevel);
          std::cout << "Page Down - Level decreased to " << level << std::endl;
        } else {
          std::cout << "Page Down - Already at minimum level (0)" << std::endl;
        }
      } else {
        std::cout
            << "Page Down - Level change blocked (game paused/in dialogue)"
            << std::endl;
      }
      break;
    
    default:
      std::cout << "Unhandled key: " << keyEvent->keysym.sym << std::endl;
      break;
    }
  }
}

// Handle SDL2 gamepad button down events
void handleGamepadButtonDown(SDL_ControllerButtonEvent *buttonEvent) {
  std::cout << ">>> SDL2 GAMEPAD BUTTON " << buttonEvent->button
            << " PRESSED (Controller " << buttonEvent->which << ") <<<" << std::endl;

  // Track gamepad input for display
  usingGamepadInput = true;
  lastButtonPressed = buttonEvent->button;
  lastButtonTime = SDL_GetTicks() / 1000.0f;

  // Find which snake corresponds to this controller (controller i -> snake[i])
  int snakeIndex = -1;
  for (int i = 0; i < gameControllers.size(); i++) {
    if (gameControllers[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gameControllers[i])) == buttonEvent->which) {
      snakeIndex = i; // Controller 0 -> snake[0], Controller 1 -> snake[1], etc.
      break;
    }
  }

  if (snakeIndex == -1 || snakeIndex >= snakes.size()) {
    std::cout << "Warning: Controller " << buttonEvent->which << " not mapped to any snake!" << std::endl;
    return;
  }

  std::cout << "Input mapped to Snake " << snakeIndex << " (Controller " << snakeIndex << ")" << std::endl;

  switch (buttonEvent->button) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP:
    if (snakes[snakeIndex].direction.y == 0) {
      Point newDir = Point(0, 1);
      Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
      if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
        snakes[snakeIndex].direction = newDir;
      }
    }
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
    if (snakes[snakeIndex].direction.y == 0) {
      Point newDir = Point(0, -1);
      Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
      if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
        snakes[snakeIndex].direction = newDir;
      }
    }
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    if (snakes[snakeIndex].direction.x == 0) {
      Point newDir = Point(-1, 0);
      Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
      if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
        snakes[snakeIndex].direction = newDir;
      }
    }
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    if (snakes[snakeIndex].direction.x == 0) {
      Point newDir = Point(1, 0);
      Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
      if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
        snakes[snakeIndex].direction = newDir;
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
      std::cout << "A button - Speed increased! Interval: " << MOVE_INTERVAL
                << "s" << std::endl;
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
      std::cout << "B button - Speed decreased! Interval: " << MOVE_INTERVAL
                << "s" << std::endl;
    }
    break;
  case SDL_CONTROLLER_BUTTON_X:
    gamePaused = !gamePaused;
    std::cout << "X button - Game " << (gamePaused ? "paused" : "unpaused")
              << std::endl;
    break;
  case SDL_CONTROLLER_BUTTON_Y:
    if (!resetConfirmation && !exitConfirmation) {
      resetConfirmation = true;
      std::cout << "Y button - Showing reset confirmation" << std::endl;
    }
    break;
  case SDL_CONTROLLER_BUTTON_BACK:
    gamePaused = !gamePaused;
    std::cout << "BACK button - Game " << (gamePaused ? "paused" : "unpaused")
              << std::endl;
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
      std::cout
          << "Left Bumper - Level change blocked (game paused/in dialogue)"
          << std::endl;
    }
    break;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
    if (!gamePaused && !exitConfirmation && !resetConfirmation) {
      int newLevel = level + 1;
      if (newLevel <= 2) {
        changeLevel(newLevel);
        std::cout << "Right Bumper - Level increased to " << level << std::endl;
      } else {
        std::cout << "Right Bumper - Already at maximum level (2)" << std::endl;
      }
    } else {
      std::cout
          << "Right Bumber - Level change blocked (game paused/in dialogue)"
          << std::endl;
    }
    break;
  }
}

// Handle SDL2 gamepad axis motion (analog sticks)
void handleGamepadAxis(SDL_ControllerAxisEvent *axisEvent) {
  const float deadzone = 0.3f;

  // Find which snake corresponds to this controller (controller i -> snake[i])
  int snakeIndex = -1;
  for (int i = 0; i < gameControllers.size(); i++) {
    if (gameControllers[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gameControllers[i])) == axisEvent->which) {
      snakeIndex = i; // Controller 0 -> snake[0], Controller 1 -> snake[1], etc.
      break;
    }
  }

  if (snakeIndex == -1 || snakeIndex >= snakes.size()) {
    return; // Controller not mapped to any snake
  }

  if (axisEvent->axis == SDL_CONTROLLER_AXIS_LEFTX) {
    float value = axisEvent->value / 32767.0f;
    if (abs(value) > deadzone && snakes[snakeIndex].direction.x == 0) {
      // Track gamepad input for display
      usingGamepadInput = true;
      lastButtonTime = SDL_GetTicks() / 1000.0f;

      if (value > deadzone) {
        Point newDir = Point(1, 0);
        Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
          snakes[snakeIndex].direction = newDir;
        }
      } else if (value < -deadzone) {
        Point newDir = Point(-1, 0);
        Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
          snakes[snakeIndex].direction = newDir;
        }
      }
    }
  } else if (axisEvent->axis == SDL_CONTROLLER_AXIS_LEFTY) {
    float value = axisEvent->value / 32767.0f;
    if (abs(value) > deadzone && snakes[snakeIndex].direction.y == 0) {
      // Track gamepad input for display
      usingGamepadInput = true;
      lastButtonTime = SDL_GetTicks() / 1000.0f;

      if (value < -deadzone) {
        Point newDir = Point(0, 1);
        Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
          snakes[snakeIndex].direction = newDir;
        }
      } else if (value > deadzone) {
        Point newDir = Point(0, -1);
        Point testHead = Point(snakes[snakeIndex].body[0].x + newDir.x, snakes[snakeIndex].body[0].y + newDir.y);
        if (isValidMoveForSnake(testHead, snakeIndex) || snakes[snakeIndex].movementPaused) {
          snakes[snakeIndex].direction = newDir;
        }
      }
    }
  }
}

// IPC slot data structure (using conditional grid size)
struct IPCSlotData {
#if IPC_DEBUG_SMALL_GRID
    char grid[16 * 10];  // Small grid for debugging (160 bytes)
    char last_button;    // Last button pressed (1 byte)
    char padding[863];   // Pad to slot size (1024 - 160 - 1 = 863 bytes)
#else
    char grid[32 * 20];  // Normal grid representation (640 bytes)
    char last_button;    // Last button pressed (1 byte)
    char padding[383];   // Pad to slot size (1024 - 640 - 1 = 383 bytes)
#endif
};

// Initialize IPC mode
bool initializeIPC() {
    std::cout << "=== INITIALIZING IPC MODE ===" << std::endl;
    
#if IPC_DEBUG_SMALL_GRID
    std::cout << "🐛 DEBUG MODE: Using small grid (" << GRID_WIDTH << "x" << GRID_HEIGHT << ") for IPC debugging" << std::endl;
    std::cout << "Grid data size: " << (GRID_WIDTH * GRID_HEIGHT) << " bytes" << std::endl;
#else
    std::cout << "Normal grid size: " << GRID_WIDTH << "x" << GRID_HEIGHT << std::endl;
    std::cout << "Grid data size: " << (GRID_WIDTH * GRID_HEIGHT) << " bytes" << std::endl;
#endif
    
    // Create circular buffer instance
    circularBuffer = new MemoryMappedCircularBuffer();
    
    // Try to initialize with snake2.dat in current directory
    if (!circularBuffer->initialize("snake2.dat")) {
        std::cout << "Failed to open existing snake2.dat, creating new one..." << std::endl;
        
        // Create new buffer file
        if (!MemoryMappedCircularBuffer::create_buffer_file("snake2.dat")) {
            std::cout << "❌ Failed to create snake2.dat!" << std::endl;
            delete circularBuffer;
            circularBuffer = nullptr;
            return false;
        }
        
        // Try to initialize again
        if (!circularBuffer->initialize("snake2.dat")) {
            std::cout << "❌ Failed to initialize circular buffer!" << std::endl;
            delete circularBuffer;
            circularBuffer = nullptr;
            return false;
        }
    }
    
    std::cout << "✅ Circular buffer initialized: snake2.dat" << std::endl;
    std::cout << "Buffer stages: " << BUFFER_STAGES << ", Slot size: " << SLOT_SIZE << " bytes" << std::endl;
    std::cout << "============================" << std::endl;
    return true;
}

// Create downsampled grid representation
void createIPCGridData(char* gridData) {
    // Initialize grid with spaces (empty)
    for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
        gridData[i] = ' ';
    }
    
    // Draw border
    for (int x = 0; x < GRID_WIDTH; x++) {
        gridData[0 * GRID_WIDTH + x] = '#';  // Top border
        gridData[(GRID_HEIGHT - 1) * GRID_WIDTH + x] = '#';  // Bottom border
    }
    for (int y = 0; y < GRID_HEIGHT; y++) {
        gridData[y * GRID_WIDTH + 0] = '#';  // Left border
        gridData[y * GRID_WIDTH + (GRID_WIDTH - 1)] = '#';  // Right border
    }
    
    // Draw corner markers
    gridData[0 * GRID_WIDTH + 0] = 'Y';  // Yellow corner (top-left)
    gridData[0 * GRID_WIDTH + (GRID_WIDTH - 1)] = 'C';  // Cyan corner (top-right)
    gridData[(GRID_HEIGHT - 1) * GRID_WIDTH + 0] = 'M';  // Magenta corner (bottom-left)
    gridData[(GRID_HEIGHT - 1) * GRID_WIDTH + (GRID_WIDTH - 1)] = 'W';  // White corner (bottom-right)
    
    // Draw food
    if (food.x >= 0 && food.x < GRID_WIDTH && food.y >= 0 && food.y < GRID_HEIGHT) {
        gridData[food.y * GRID_WIDTH + food.x] = 'F';  // Food
    }
    
    // Draw pacman if active
    if (pacmanActive && pacman.x >= 0 && pacman.x < GRID_WIDTH && 
        pacman.y >= 0 && pacman.y < GRID_HEIGHT) {
        gridData[pacman.y * GRID_WIDTH + pacman.x] = 'P';  // Pacman
    }
    
    // Draw player snakes (body first, then head on top)
    for (const auto& snake : snakes) {
        for (size_t i = snake.body.size(); i > 0; i--) {
            const Point& segment = snake.body[i - 1];
            if (segment.x >= 0 && segment.x < GRID_WIDTH && 
                segment.y >= 0 && segment.y < GRID_HEIGHT) {
                if (i == 1) {
                    // Snake head
                    if (snake.movementPaused) {
                        gridData[segment.y * GRID_WIDTH + segment.x] = 'H';  // Paused head
                    } else {
                        gridData[segment.y * GRID_WIDTH + segment.x] = 'S';  // Snake head
                    }
                } else {
                    gridData[segment.y * GRID_WIDTH + segment.x] = 's';  // Snake body
                }
            }
        }
    }
    
    // Draw AI snakes (body first, then head on top)
    for (const auto& aiSnake : aiSnakes) {
        for (size_t i = aiSnake.body.size(); i > 0; i--) {
            const Point& segment = aiSnake.body[i - 1];
            if (segment.x >= 0 && segment.x < GRID_WIDTH && 
                segment.y >= 0 && segment.y < GRID_HEIGHT) {
                if (i == 1) {
                    // AI Snake head
                    if (aiSnake.movementPaused) {
                        gridData[segment.y * GRID_WIDTH + segment.x] = 'A';  // Paused AI head
                    } else {
                        gridData[segment.y * GRID_WIDTH + segment.x] = 'I';  // AI head
                    }
                } else {
                    gridData[segment.y * GRID_WIDTH + segment.x] = 'i';  // AI body
                }
            }
        }
    }
}

// Write current game state to circular buffer
void writeIPCSlot() {
    if (!circularBuffer) return;
    
    IPCSlotData slotData = {};
    
    // Create downsampled grid
    createIPCGridData(slotData.grid);
    
    // Set last button pressed
    slotData.last_button = (char)lastButtonPressed;
    
    // Write to circular buffer
    if (!circularBuffer->write_slot(&slotData, sizeof(IPCSlotData))) {
        std::cout << "⚠️  Failed to write to circular buffer!" << std::endl;
    }
}

// Cleanup IPC mode
void cleanupIPC() {
    if (circularBuffer) {
        circularBuffer->cleanup();
        delete circularBuffer;
        circularBuffer = nullptr;
        std::cout << "IPC mode cleaned up" << std::endl;
    }
}

int main(int argc, char* argv[]) {
  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-e") == 0) {
      ipcMode = true;
      std::cout << "🔗 IPC Mode enabled via -e argument" << std::endl;
    }
  }
  // Initialize SDL2
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR) <
      0) {
    std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
    return -1;
  }

  // Initialize SDL2_image if available
#ifdef SDL_IMAGE_AVAILABLE
  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    std::cout
        << "SDL2_image could not initialize! Using fallback bitmap. IMG_Error: "
        << IMG_GetError() << std::endl;
  } else {
    std::cout << "SDL2_image initialized - PNG/JPG support available"
              << std::endl;
  }
#else
  std::cout
      << "SDL2_image not available - using BMP support and fallback bitmap"
      << std::endl;
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
  std::cout << "Grid dimensions: " << GRID_WIDTH << "x" << GRID_HEIGHT
            << std::endl;

#if IPC_DEBUG_SMALL_GRID
  std::cout << "🐛 IPC DEBUG MODE: Small grid enabled (16x10)" << std::endl;
  std::cout << "   To disable: Set IPC_DEBUG_SMALL_GRID to 0 and recompile" << std::endl;
#else
  std::cout << "Normal grid mode (32x20)" << std::endl;
  std::cout << "   To enable IPC debug: Set IPC_DEBUG_SMALL_GRID to 1 and recompile" << std::endl;
#endif

  // Create window - fullscreen or windowed based on IPC mode
  if (ipcMode) {
    // Windowed mode for IPC
    int windowWidth = 800;
    int windowHeight = 600;
    window = SDL_CreateWindow(
        "Snake Game - IPC Mode", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight, SDL_WINDOW_OPENGL);
    std::cout << "Created windowed IPC mode: " << windowWidth << "x" << windowHeight << std::endl;
  } else {
    // Fullscreen mode for normal gameplay
    window = SDL_CreateWindow(
        "Snake Game - SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        displayMode.w, displayMode.h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    std::cout << "Created fullscreen mode: " << displayMode.w << "x" << displayMode.h << std::endl;
  }

  if (!window) {
    std::cerr << "Failed to create SDL2 window: " << SDL_GetError()
              << std::endl;
    SDL_Quit();
    return -1;
  }

  // Create OpenGL context
  glContext = SDL_GL_CreateContext(window);
  if (!glContext) {
    std::cerr << "Failed to create OpenGL context: " << SDL_GetError()
              << std::endl;
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

  // Load and compile shaders from files
  std::string vertexShaderSource = loadShaderFromFile("shaders/vertex.vs");
  std::string fragmentShaderSource = loadShaderFromFile("shaders/fragment.fs");
  
  if (vertexShaderSource.empty() || fragmentShaderSource.empty()) {
    std::cerr << "Failed to load shader files!" << std::endl;
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return -1;
  }
  
  GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER, "Vertex");
  GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER, "Fragment");
  
  if (vertexShader == 0 || fragmentShader == 0) {
    std::cerr << "Failed to compile shaders!" << std::endl;
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return -1;
  }

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
  u_aspect_ratio = glGetUniformLocation(shaderProgram, "u_aspect_ratio");

  // Setup vertex data
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  GLuint EBO;
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices,
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  // Position attribute (location = 0)
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Texture coordinate attribute (location = 1)
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Initialize IPC mode if enabled
  if (ipcMode) {
    if (!initializeIPC()) {
      std::cerr << "Failed to initialize IPC mode, exiting..." << std::endl;
      cleanupIPC();
      // Cleanup and exit - controllers already cleaned up by main cleanup
      SDL_GL_DeleteContext(glContext);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return -1;
    }
  }

  // Load apple texture (try different formats)
  appleTexture = loadTexture("apple.bmp"); // Try BMP first (always supported)
  if (appleTexture == 0) {
    appleTexture = loadTexture("apple.png"); // Try PNG (requires SDL2_image)
  }
  if (appleTexture == 0) {
    appleTexture = loadTexture("apple.jpg"); // Try JPG (requires SDL2_image)
  }
  if (appleTexture == 0) {
    std::cout << "No apple image found, creating procedural apple bitmap..."
              << std::endl;
    appleTexture = createAppleBitmap();
  }

  // Initialize game controllers
  numControllers = SDL_NumJoysticks();
  gameControllers.clear();
  
  std::cout << "=== CONTROLLER DETECTION ===" << std::endl;
  std::cout << "Found " << numControllers << " controllers" << std::endl;
  
  for (int i = 0; i < numControllers && i < 4; i++) { // Max 4 controllers (4 snakes max)
    SDL_GameController* controller = SDL_GameControllerOpen(i);
    if (controller) {
      gameControllers.push_back(controller);
      
      // Set player index for LED indicators (player 1, 2, 3, etc.)
      int playerIndex = i + 1; // Player numbers start from 1
      
      std::cout << "Controller " << i << " (Player " << playerIndex << "): " 
                << SDL_GameControllerName(controller) << std::endl;
      
      // Try setting player index multiple times for PS4 controllers
      bool ledSetSuccessfully = false;
      for (int attempt = 0; attempt < 3; attempt++) {
        SDL_GameControllerSetPlayerIndex(controller, playerIndex);
        
        // Small delay to let the controller process the command
        SDL_Delay(50);
        
        int currentPlayerIndex = SDL_GameControllerGetPlayerIndex(controller);
        std::cout << "  Attempt " << (attempt + 1) << ": Set=" << playerIndex 
                  << ", Read=" << currentPlayerIndex << std::endl;
        
        if (currentPlayerIndex == playerIndex) {
          ledSetSuccessfully = true;
          break;
        }
      }
      
      if (ledSetSuccessfully) {
        std::cout << "  ✅ LED indicator set to Player " << playerIndex << std::endl;
      } else {
        std::cout << "  ⚠️  LED indicator failed to set after 3 attempts" << std::endl;
        std::cout << "     (PS4 controllers sometimes don't support this feature)" << std::endl;
      }
    } else {
      std::cout << "Failed to open controller " << i << ": " << SDL_GetError() << std::endl;
    }
  }
  
  numControllers = gameControllers.size(); // Update to actual opened controllers
  std::cout << "Successfully opened " << numControllers << " controllers" << std::endl;
  std::cout << "Total snakes: " << std::min(4, std::max(1, numControllers)) << std::endl;
  
  // Summary of snake assignments
  std::cout << "🐍 Snake Control Mapping:" << std::endl;
  const char* colors[] = {"Green", "Red", "Blue", "Yellow", "Magenta", "Cyan", "Orange", "Purple"};
  
  for (int i = 0; i < std::min(4, std::max(1, numControllers)) && i < 4; i++) {
    std::cout << "   Snake[" << i << "]: ";
    if (i == 0) {
      std::cout << "Keyboard";
      if (numControllers > 0) {
        std::cout << " + Controller 0";
      }
    } else {
      std::cout << "Controller " << i;
    }
    std::cout << " (" << colors[i] << ", Player " << (i+1) << ")" << std::endl;
  }
  std::cout << "===========================" << std::endl;

  // Initialize gyroscope system
  // initializeGyroscope(); // Disabled for testing
  
  // Initialize rumble system for all controllers
  initializeRumble();

  // Initialize tile grid for efficient collision detection
  initializeTileGrid();

  // Initialize game AFTER controllers are detected
  initializeGame();

  std::cout << "Snake Game Controls (SDL2 Version):\n";
  std::cout << "=== GAMEPAD CONTROLS ===\n";
  std::cout << "  D-pad/Left Stick: Move snake\n";
  std::cout << "  A button: Speed up / Confirm\n";
  std::cout << "  B button: Slow down / Cancel\n";
  std::cout << "  X button: Pause/Unpause\n";
  std::cout << "  Y button: Reset confirmation\n";
  std::cout << "  Start button: Exit confirmation\n";
  std::cout << "  L/R Shoulder: Change level (0=SNAKE, 1=PACMAN, 2=MULTI SNAKE)\n";
  std::cout << "=== KEYBOARD CONTROLS ===\n";
  std::cout << "  Arrow Keys / WASD: Move snake\n";
  std::cout << "  Enter: Speed up / Confirm\n";
  std::cout << "  Esc: Slow down / Cancel\n";
  std::cout << "  Space: Pause/Unpause\n";
  std::cout << "  R: Reset confirmation\n";
  std::cout << "  Page Down/Up: Change level (0=SNAKE, 1=PACMAN, 2=MULTI SNAKE)\n";

  // Main game loop
  while (running) {
    float currentTime = SDL_GetTicks() / 1000.0f;

    // Update flash timer
    flashTimer = currentTime;

    // Update rumble system
    updateRumble();

    // Update food physics based on gyroscope (Level 2+)
    if (currentTime - lastGyroUpdateTime > GYRO_UPDATE_INTERVAL) {
      // updateFoodPhysics(currentTime - lastGyroUpdateTime); // Disabled for testing
      lastGyroUpdateTime = currentTime;
    }

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
    if (!gamePaused && !exitConfirmation && !resetConfirmation &&
        currentTime - lastMoveTime > MOVE_INTERVAL) {
      updateGame();
      
      // Write to IPC buffer if in IPC mode
      if (ipcMode) {
        writeIPCSlot();
      }
      
      lastMoveTime = currentTime;
    }

    // Update pacman at its own interval
    if (!gamePaused && !exitConfirmation && !resetConfirmation &&
        pacmanActive &&
        currentTime - lastPacmanMoveTime > PACMAN_MOVE_INTERVAL) {
      updatePacman();
      lastPacmanMoveTime = currentTime;
    }

    // Update AI snakes at their own interval
    if (!gamePaused && !exitConfirmation && !resetConfirmation &&
        !aiSnakes.empty() &&
        currentTime - lastAISnakeMoveTime > AI_SNAKE_MOVE_INTERVAL) {
      updateAISnakes();
      lastAISnakeMoveTime = currentTime;
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    render();

    SDL_GL_SwapWindow(window);
  }

  // Cleanup
  cleanupRumble();
  // cleanupGyroscope(); // Disabled for testing
  cleanupIPC();
  cleanupTileGrid();
  
  // Close all game controllers
  for (auto controller : gameControllers) {
    if (controller) {
      SDL_GameControllerClose(controller);
    }
  }
  gameControllers.clear();

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