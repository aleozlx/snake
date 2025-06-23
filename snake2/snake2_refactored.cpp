//
// Refactored Snake Game - Event-Driven Architecture
// This demonstrates the separation of app infrastructure from game logic
//

#include "snake_dep.h"
#include "pathfinding.h"
#include "snake_draw.h"
#include "snake_app.h"  // Use the clean header interface
#include "snake_ui.h"   // UI system
#include "snake_theme.h" // Centralized color theme
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <memory>

// External font data (from fonts/font0.cpp)
extern const bool font_5x7[36][7][5];
extern int getCharIndex(char c);

// Use centralized color theme
using namespace SnakeTheme;

// ===== GAME LOGIC (Event-driven) =====

class SnakeGameLogic {
public:
    SnakeGameLogic(SnakeApp* app) : m_app(app), m_ui(app) {
        // Subscribe to events
        auto* eventSystem = m_app->getEventSystem();
        eventSystem->subscribe(EventType::GAME_TICK, 
            [this](const Event& e) { onGameTick(e); });
        eventSystem->subscribe(EventType::GAME_RENDER, 
            [this](const Event& e) { onRender(e); });
        eventSystem->subscribe(EventType::INPUT_KEYBOARD, 
            [this](const Event& e) { onKeyboardInput(e); });
        eventSystem->subscribe(EventType::INPUT_GAMEPAD_BUTTON, 
            [this](const Event& e) { onGamepadButton(e); });
        eventSystem->subscribe(EventType::INPUT_GAMEPAD_AXIS, 
            [this](const Event& e) { onGamepadAxis(e); });
        eventSystem->subscribe(EventType::GAME_EXIT, 
            [this](const Event& e) { onExit(e); });
 
        initializeGame();
    }
    
private:
    void initializeGame() {
        m_gridWidth = m_app->getConfig().gridWidth;
        m_gridHeight = m_app->getConfig().gridHeight;
        
        m_tileGrid = std::make_unique<TileGrid>(m_gridWidth, m_gridHeight);
        
        int numControllers = m_app->getNumControllers();
        int totalSnakes = std::min(4, std::max(1, numControllers));
        
        m_snakes.clear();
        int startX = m_gridWidth / 2;
        int startY = m_gridHeight / 2;
        
        for (int i = 0; i < totalSnakes; i++) {
            int offsetX = i * 3; // Space them out
            int offsetY = (i % 2 == 0) ? 0 : ((i % 4 < 2) ? 2 : -2);
            
            Snake snake(startX + offsetX, startY + offsetY, Point(1, 0));
            snake.color = GameColors::SNAKE_PLAYERS[i];
            snake.score = 0;
            m_snakes.push_back(snake);
        }
        
        initializeLevelFeatures();
        placeFood();
        
        m_gameOver = false;
        m_gamePaused = false;
        m_lastMoveTime = 0.0f;
        m_moveInterval = 0.2f;
        
        std::cout << "🐍 Game logic initialized (event-driven)" << std::endl;
        std::cout << "Grid: " << m_gridWidth << "x" << m_gridHeight << std::endl;
        std::cout << "Level: " << m_level << ", Snakes: " << totalSnakes << std::endl;
        for (size_t i = 0; i < m_snakes.size(); i++) {
            std::cout << "Snake[" << i << "] at: " << m_snakes[i].body[0].x << "," << m_snakes[i].body[0].y << std::endl;
        }
    }
    
    void onGameTick(const Event& event) {
        // Pause game when dialogues are shown (same as original snake2.cpp)
        if (m_gameOver || m_gamePaused || m_ui.isAnyDialogShown()) return;
        
        float currentTime = event.tick.currentTime;
        
        // Update game at fixed intervals
        if (currentTime - m_lastMoveTime > m_moveInterval) {
            updateSnakes();
            m_lastMoveTime = currentTime;
        }
        
        // Update Pacman at its own interval (Level 1)
        if (m_pacmanActive && currentTime - m_lastPacmanMoveTime > m_pacmanMoveInterval) {
            updatePacman();
            m_lastPacmanMoveTime = currentTime;
        }
        
        // Update AI Snakes at their own interval (Level 2+)
        if (!m_aiSnakes.empty() && currentTime - m_lastAISnakeMoveTime > m_aiSnakeMoveInterval) {
            updateAISnakes();
            m_lastAISnakeMoveTime = currentTime;
        }
        
        // Write to IPC buffer if in IPC mode
        if (m_app->getConfig().ipcMode) {
            char gridData[32 * 20]; // Maximum grid size
            createIPCGridData(gridData);
            m_app->writeIPCSlot(gridData, 0); // No button tracking for IPC
        }
    }
    
    void onRender(const Event& event) {
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        
        glUseProgram(m_app->getShaderProgram());
        glBindVertexArray(m_app->getVAO());
        
        drawFood();
        drawPacman();
        drawSnakes();
        drawAISnakes();
        
        // Check if any snake is paused for UI rendering
        bool anySnakePaused = false;
        for (const auto& snake : m_snakes) {
            if (snake.movementPaused) {
                anySnakePaused = true;
                break;
            }
        }
        
        m_ui.renderUI(m_snakes, m_aiSnakes, m_level, m_gamePaused, anySnakePaused);
        
        // Draw confirmation dialogues LAST (on top of everything)
        m_ui.renderDialogs();
    }
    
    void onKeyboardInput(const Event& event) {
        if (!event.input.isPressed) return; // Only handle key press
        
        int key = event.input.keyCode;
        
        std::cout << "⌨️ Keyboard input: " << key << std::endl;
        
        switch (key) {
            case SDLK_UP:
                changeSnakeDirection(0, Point(0, 1));
                break;
            case SDLK_DOWN:
                changeSnakeDirection(0, Point(0, -1));
                break;
            case SDLK_LEFT:
                changeSnakeDirection(0, Point(-1, 0));
                break;
            case SDLK_RIGHT:
                changeSnakeDirection(0, Point(1, 0));
                break;
            
            case SDLK_w:
                changeSnakeDirection(0, Point(0, 1));
                break;
            case SDLK_s:
                changeSnakeDirection(0, Point(0, -1));
                break;
            case SDLK_a:
                changeSnakeDirection(0, Point(-1, 0));
                break;
            case SDLK_d:
                changeSnakeDirection(0, Point(1, 0));
                break;
            
            case SDLK_RETURN: // Enter = A button
                if (m_ui.isExitConfirmationShown()) {
                    std::cout << "Exit confirmed!" << std::endl;
                    m_app->shutdown();
                } else if (m_ui.isResetConfirmationShown()) {
                    std::cout << "Reset confirmed!" << std::endl;
                    resetGame();
                } else {
                    m_moveInterval = std::max(0.05f, m_moveInterval - 0.05f);
                    std::cout << "Speed increased! Interval: " << m_moveInterval << "s" << std::endl;
                }
                break;
            
            case SDLK_ESCAPE: // Esc = B button
                if (m_ui.isExitConfirmationShown()) {
                    m_ui.hideExitConfirmation();
                    std::cout << "Exit cancelled!" << std::endl;
                } else if (m_ui.isResetConfirmationShown()) {
                    m_ui.hideResetConfirmation();
                    std::cout << "Reset cancelled!" << std::endl;
                } else {
                    m_moveInterval = std::min(1.0f, m_moveInterval + 0.05f);
                    std::cout << "Speed decreased! Interval: " << m_moveInterval << "s" << std::endl;
                }
                break;
            
            case SDLK_SPACE: // Space = X button
                m_gamePaused = !m_gamePaused;
                std::cout << "Game " << (m_gamePaused ? "paused" : "unpaused") << std::endl;
                break;
            
            case SDLK_r: // R = Y button
                if (!m_ui.isAnyDialogShown()) {
                    m_ui.showResetConfirmation();
                }
                break;
            
            case SDLK_PAGEUP:
                changeLevelUp();
                break;
            
            case SDLK_PAGEDOWN:
                changeLevelDown();
                break;
        }
    }
    
    void onGamepadButton(const Event& event) {
        if (!event.input.isPressed) return;
        
        int button = event.input.buttonId;
        int controllerId = event.input.controllerId;
        
        // Track gamepad input for visual debug
        m_ui.updateGamepadInput(button, m_app->getCurrentTime());
        
        std::cout << "🎮 Gamepad " << controllerId << " button: " << button << std::endl;
        
        // Map controller to snake: controller 0 -> snake 0, controller 1 -> snake 1, etc.
        int snakeIndex = std::min(controllerId, (int)m_snakes.size() - 1);
        if (snakeIndex < 0) snakeIndex = 0; // Fallback to first snake
        
        switch (button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                changeSnakeDirection(snakeIndex, Point(0, 1));
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                changeSnakeDirection(snakeIndex, Point(0, -1));
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                changeSnakeDirection(snakeIndex, Point(-1, 0));
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                changeSnakeDirection(snakeIndex, Point(1, 0));
                break;
            case SDL_CONTROLLER_BUTTON_A:
                if (m_ui.isExitConfirmationShown()) {
                    // Exit confirmed
                    std::cout << "Exit confirmed!" << std::endl;
                    m_app->shutdown();
                } else if (m_ui.isResetConfirmationShown()) {
                    // Reset confirmed
                    std::cout << "Reset confirmed!" << std::endl;
                    resetGame();
                } else {
                    // Speed up
                    m_moveInterval = std::max(0.05f, m_moveInterval - 0.05f);
                    std::cout << "Speed up! Interval: " << m_moveInterval << std::endl;
                }
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (m_ui.isExitConfirmationShown()) {
                    m_ui.hideExitConfirmation();
                    std::cout << "Exit cancelled!" << std::endl;
                } else if (m_ui.isResetConfirmationShown()) {
                    m_ui.hideResetConfirmation();
                    std::cout << "Reset cancelled!" << std::endl;
                } else {
                    // Slow down
                    m_moveInterval = std::min(1.0f, m_moveInterval + 0.05f);
                    std::cout << "Slow down! Interval: " << m_moveInterval << std::endl;
                }
                break;
            case SDL_CONTROLLER_BUTTON_X:
                m_gamePaused = !m_gamePaused;
                std::cout << "Game " << (m_gamePaused ? "paused" : "unpaused") << std::endl;
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                if (!m_ui.isAnyDialogShown()) {
                    m_ui.showResetConfirmation();
                }
                break;
            case SDL_CONTROLLER_BUTTON_START:
                if (!m_ui.isExitConfirmationShown()) {
                    m_ui.showExitConfirmation();
                }
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                // Change level down
                changeLevelDown();
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                // Change level up
                changeLevelUp();
                break;
        }
    }
    
    void onExit(const Event& event) {
        std::cout << "🐍 Game logic received exit event" << std::endl;
        // Cleanup game state if needed
    }
    
    void updateSnakes() {
        // Centralized food collection system (like original)
        bool anySnakeGotFood = false;
        bool pacmanGotFood = false;
        bool anyAISnakeGotFood = false;
        
        // Update all player snakes
        for (size_t snakeIdx = 0; snakeIdx < m_snakes.size(); snakeIdx++) {
            Snake& snake = m_snakes[snakeIdx];
            
            Point newHead = Point(snake.body[0].x + snake.direction.x, 
                                 snake.body[0].y + snake.direction.y);
            bool snakeCanMove = !isCollision(newHead, snakeIdx);
            bool snakeGotFood = false;
            
            // Check if the snake move is valid
            if (!snakeCanMove) {
                // Invalid move - pause movement until direction changes
                if (!snake.movementPaused) {
                    std::cout << "🔴 Snake " << snakeIdx << " collision!" << std::endl;
                }
                snake.movementPaused = true;
                snakeGotFood = false; // Snake can't get food if blocked
            } else {
                // Valid move - resume movement if it was paused
                if (snake.movementPaused) {
                    snake.movementPaused = false;
                    std::cout << "🟢 Snake " << snakeIdx << " movement resumed!" << std::endl;
                }
                
                // Move the snake
                snake.body.insert(snake.body.begin(), newHead);
                
                // Check if snake got the food
                snakeGotFood = (newHead == m_food);
                if (snakeGotFood) {
                    anySnakeGotFood = true;
                    snake.score++;
                    std::cout << "🍎 Snake " << snakeIdx << " scored! Score: " << snake.score << std::endl;
                }
            }
            
            // If snake didn't get food, remove tail (unless it was blocked)
            if (!snakeGotFood && snakeCanMove) {
                snake.body.pop_back();
            }
        }
        
        // Check if pacman got the food
        if (m_pacmanActive && m_pacman == m_food) {
            pacmanGotFood = true;
            std::cout << "🟡 Pacman got the food!" << std::endl;
        }
        
        // Check if any AI snake got the food  
        for (size_t aiIdx = 0; aiIdx < m_aiSnakes.size(); aiIdx++) {
            Snake& aiSnake = m_aiSnakes[aiIdx];
            if (!aiSnake.body.empty() && aiSnake.body[0] == m_food) {
                anyAISnakeGotFood = true;
                aiSnake.score++;
                std::cout << "🤖 NPC Snake " << aiIdx << " scored! Score: " << aiSnake.score << std::endl;
                break; // Only one AI snake can get food at a time
            }
        }
        
        // Generate new food if any entity got it
        if (anySnakeGotFood || pacmanGotFood || anyAISnakeGotFood) {
            placeFood();
        }
    }
    
    void updateSnake(int snakeIndex) {
        // This method is now only used for individual snake updates if needed
        // The main snake update logic is in updateSnakes() for centralized food collection
        if (snakeIndex >= m_snakes.size()) return;
        
        Snake& snake = m_snakes[snakeIndex];
        Point newHead = Point(snake.body[0].x + snake.direction.x, 
                             snake.body[0].y + snake.direction.y);
        
        if (isCollision(newHead, snakeIndex)) {
            snake.movementPaused = true;
            std::cout << "🔴 Snake " << snakeIndex << " collision!" << std::endl;
            return;
        }
        
        if (snake.movementPaused) {
            snake.movementPaused = false;
            std::cout << "🟢 Snake " << snakeIndex << " movement resumed!" << std::endl;
        }
        
        // Move snake (but don't handle food here - that's done in updateSnakes())
        snake.body.insert(snake.body.begin(), newHead);
        snake.body.pop_back(); // Always remove tail in individual updates
    }
    
    bool isCollision(const Point& pos, int snakeIndex) {
        // Update tile grid with current game state
        updateTileGrid();
        
        if (!m_tileGrid->isValidPosition(pos.x, pos.y)) {
            return true; // Out of bounds
        }
        
        TileContent tile = m_tileGrid->getTile(pos.x, pos.y);
        
        if (tile == TileContent::EMPTY || tile == TileContent::FOOD) {
            return false;
        }
        
        return true; // Everything else is a collision
    }
    
    // Helper method to update the tile grid with current game state
    void updateTileGrid() {
        if (!m_tileGrid) return;
        
        m_tileGrid->updateFromGameState(m_snakes, m_aiSnakes, m_food, m_pacmanActive, m_pacman);
    }
    
    void changeSnakeDirection(int snakeIndex, const Point& newDirection) {
        if (snakeIndex >= m_snakes.size()) return;
        
        Snake& snake = m_snakes[snakeIndex];
        
        // Prevent reversing into self
        if ((newDirection.x != 0 && snake.direction.x != 0) ||
            (newDirection.y != 0 && snake.direction.y != 0)) {
            return;
        }
        
        Point testHead = Point(snake.body[0].x + newDirection.x, 
                              snake.body[0].y + newDirection.y);
        
        if (!isCollision(testHead, snakeIndex) || snake.movementPaused) {
            snake.direction = newDirection;
            
            std::cout << "🔄 Snake " << snakeIndex << " direction changed to (" 
                      << newDirection.x << "," << newDirection.y << ")" << std::endl;
        }
    }
    
    void placeFood() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, m_gridWidth - 2);
        std::uniform_int_distribution<> disY(1, m_gridHeight - 2);
        
        updateTileGrid();
        
        bool validPlacement = false;
        do {
            m_food = Point(disX(gen), disY(gen));
            
            if (m_tileGrid && m_tileGrid->isValidPosition(m_food.x, m_food.y)) {
                TileContent tile = m_tileGrid->getTile(m_food.x, m_food.y);
                validPlacement = (tile == TileContent::EMPTY);
            } else {
                validPlacement = false;
            }
        } while (!validPlacement);
        
        std::cout << "🍎 Food placed at (" << m_food.x << "," << m_food.y << ")" << std::endl;
    }
    
    void resetGame() {
        std::cout << "🔄 Resetting game..." << std::endl;
        initializeGame();
    }
    
    void changeLevelUp() {
        if (!m_gamePaused && !m_ui.isAnyDialogShown()) {
            int newLevel = m_level + 1;
            if (newLevel <= 2) {
                changeLevel(newLevel);
                std::cout << "🔼 Level increased to " << m_level << std::endl;
            } else {
                std::cout << "⚠️ Already at maximum level (2)" << std::endl;
            }
        } else {
            std::cout << "⚠️ Level change blocked (game paused/in dialogue)" << std::endl;
        }
    }
    
    void changeLevelDown() {
        if (!m_gamePaused && !m_ui.isAnyDialogShown()) {
            int newLevel = m_level - 1;
            if (newLevel >= 0) {
                changeLevel(newLevel);
                std::cout << "🔽 Level decreased to " << m_level << std::endl;
            } else {
                std::cout << "⚠️ Already at minimum level (0)" << std::endl;
            }
        } else {
            std::cout << "⚠️ Level change blocked (game paused/in dialogue)" << std::endl;
        }
    }
    
    void changeLevel(int newLevel) {
        if (newLevel < 0 || newLevel > 2) return; // Levels 0, 1, and 2 supported
        if (newLevel == m_level) return; // No change needed
        
        int oldLevel = m_level;
        m_level = newLevel;
        
        std::cout << "🎯 Level changed from " << oldLevel << " to " << m_level << std::endl;
        
        // Only reinitialize level-specific features, keep existing snakes
        initializeLevelFeatures();
        
        // Place new food (avoiding existing snakes)
        placeFood();
    }
    
    void initializeLevelFeatures() {
        m_aiSnakes.clear();
        m_pacmanActive = false;
        
        if (m_level == 1) {
            initializePacman();
        } else if (m_level >= 2) {
            initializeAISnakes();
        }
        
        std::cout << "🎯 Level " << m_level << " features initialized" << std::endl;
    }
    
    // Helper method to create drawing context
    SnakeDraw::DrawContext getDrawContext() const {
        return SnakeDraw::DrawContext(m_gridWidth, m_gridHeight, 
                                     m_app->getOffsetUniform(), m_app->getColorUniform(), 
                                     m_app->getScaleUniform(), m_app->getShapeTypeUniform(),
                                     m_app->getInnerRadiusUniform(), m_app->getTextureUniform(),
                                     m_app->getUseTextureUniform(), m_app->getAspectRatioUniform());
    }

    void drawFood() {
        auto ctx = getDrawContext();
        GLuint appleTexture = m_app->getAppleTexture();
        if (appleTexture != 0) {
            SnakeDraw::drawTexturedSquare(m_food.x, m_food.y, appleTexture, ctx);
        } else {
            // Fallback to red square if texture failed to load
            SnakeDraw::drawSquare(m_food.x, m_food.y, GameColors::FOOD, ctx);
        }
    }
    
    void drawSnakes() {
        for (size_t snakeIdx = 0; snakeIdx < m_snakes.size(); snakeIdx++) {
            const Snake& snake = m_snakes[snakeIdx];
            
            for (size_t i = 0; i < snake.body.size(); i++) {
                float intensity = i == 0 ? 1.0f : 0.6f; // Head brighter
                RGBColor segmentColor;
                
                if (m_gamePaused) {
                    // Yellow when paused
                    segmentColor = StateColors::PAUSED * intensity;
                } else if (snake.movementPaused) {
                    // Purple when movement is paused (collision)
                    segmentColor = StateColors::MOVEMENT_BLOCKED * intensity;
                } else {
                    segmentColor = snake.color * intensity;
                }
                
                auto ctx = getDrawContext();
                SnakeDraw::drawSquare(snake.body[i].x, snake.body[i].y, segmentColor, ctx);
                
                // Draw eyes on the snake's head (first segment)
                if (i == 0 && !m_gameOver) {
                    SnakeDraw::drawSnakeEyes(snake.body[i].x, snake.body[i].y, m_food.x, m_food.y, segmentColor, snake.direction, ctx);
                }
            }
        }
    }
    
    // Snake eye rendering moved to SnakeDraw namespace
    
    void drawPacman() {
        if (!m_pacmanActive) return;
        
        auto ctx = getDrawContext();
        SnakeDraw::drawPacman(m_pacman, m_pacmanDirection, ctx);
    }
    
    void drawAISnakes() {
        for (size_t aiIdx = 0; aiIdx < m_aiSnakes.size(); aiIdx++) {
            const Snake& aiSnake = m_aiSnakes[aiIdx];
            
            for (size_t i = 0; i < aiSnake.body.size(); i++) {
                float intensity = i == 0 ? 1.0f : 0.6f; // Head brighter
                RGBColor segmentColor;
                
                if (m_ui.isExitConfirmationShown()) {
                    // Red when showing exit confirmation
                    segmentColor = UIColors::TEXT_ERROR * intensity;
                } else if (m_ui.isResetConfirmationShown()) {
                    // Orange when showing reset confirmation
                    segmentColor = UIColors::TEXT_WARNING * intensity;
                } else if (m_gamePaused) {
                    // Yellow when paused
                    segmentColor = StateColors::PAUSED * intensity;
                } else if (aiSnake.movementPaused) {
                    // Purple when movement is paused (collision)
                    segmentColor = StateColors::MOVEMENT_BLOCKED * intensity;
                } else {
                    segmentColor = aiSnake.color * intensity;
                }
                
                auto ctx = getDrawContext();
                SnakeDraw::drawSquare(aiSnake.body[i].x, aiSnake.body[i].y, segmentColor, ctx);
                
                // Draw eyes on the AI snake's head (first segment)
                if (i == 0 && !m_gameOver) {
                    SnakeDraw::drawSnakeEyes(aiSnake.body[i].x, aiSnake.body[i].y, m_food.x, m_food.y, segmentColor, aiSnake.direction, ctx);
                }
            }
        }
    }
    
    void drawBorder() {
        float currentTime = m_app->getCurrentTime();
        const float FLASH_INTERVAL = 0.1f;
        RGBColor borderColor;
        
        if (m_ui.isExitConfirmationShown()) {
            borderColor = StateColors::BORDER_EXIT_CONFIRM;
        } else if (m_ui.isResetConfirmationShown()) {
            borderColor = StateColors::BORDER_RESET_CONFIRM;
        } else if (m_gamePaused) {
            borderColor = StateColors::BORDER_PAUSED;
        } else {
            // Check if any snake is paused
            bool anySnakePaused = false;
            for (const auto& snake : m_snakes) {
                if (snake.movementPaused) {
                    anySnakePaused = true;
                    break;
                }
            }
            
            if (anySnakePaused) {
                bool showRed = ((int)(currentTime / FLASH_INTERVAL) % 2) == 0;
                borderColor = showRed ? StateColors::BORDER_COLLISION : StateColors::BORDER_NORMAL;
            } else {
                borderColor = StateColors::BORDER_NORMAL;
            }
        }
        
        auto ctx = getDrawContext();
        for (int x = 0; x < m_gridWidth; x++) {
            SnakeDraw::drawSquare(x, 0, borderColor, ctx); // Bottom
            SnakeDraw::drawSquare(x, m_gridHeight - 1, borderColor, ctx); // Top
        }
        for (int y = 0; y < m_gridHeight; y++) {
            SnakeDraw::drawSquare(0, y, borderColor, ctx); // Left
            SnakeDraw::drawSquare(m_gridWidth - 1, y, borderColor, ctx); // Right
        }
    }
    
    void drawCornerMarkers() {
        auto ctx = getDrawContext();
        
        // Corner markers with meaningful colors
        SnakeDraw::drawSquare(0, 0, GameColors::CORNER_BOTTOM_LEFT, ctx);                           // Bottom-left: Yellow
        SnakeDraw::drawSquare(m_gridWidth - 1, 0, GameColors::CORNER_BOTTOM_RIGHT, ctx);               // Bottom-right: Cyan
        SnakeDraw::drawSquare(0, m_gridHeight - 1, GameColors::CORNER_TOP_LEFT, ctx);           // Top-left: Magenta
        SnakeDraw::drawSquare(m_gridWidth - 1, m_gridHeight - 1, GameColors::CORNER_TOP_RIGHT, ctx); // Top-right: White
    }
    
    // Missing event handler
    void onGamepadAxis(const Event& event) {
        // Handle analog stick input
        float deadzone = 0.3f;
        float value = event.input.axisValue / 32767.0f;
        int controllerId = event.input.controllerId;
        
        // Track gamepad input for visual debug
        if (abs(value) > deadzone) {
            m_ui.setUsingGamepadInput(true);
        }
        
        // Map controller to snake index
        int snakeIndex = std::min(controllerId, (int)m_snakes.size() - 1);
        if (snakeIndex < 0) return;
        
        // Handle left stick X axis
        if (abs(value) > deadzone && m_snakes[snakeIndex].direction.x == 0) {
            if (value > deadzone) {
                changeSnakeDirection(snakeIndex, Point(1, 0)); // Right
            } else if (value < -deadzone) {
                changeSnakeDirection(snakeIndex, Point(-1, 0)); // Left
            }
        }
    }
    
    // Level system methods
    void initializePacman() {
        m_pacmanActive = true;
        // Place pacman randomly
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(1, m_gridWidth - 2);
        std::uniform_int_distribution<> disY(1, m_gridHeight - 2);
        
        do {
            m_pacman = Point(disX(gen), disY(gen));
        } while (isOccupiedBySnake(m_pacman));
        
        m_pacmanDirection = Point(0, 0);
        std::cout << "🟡 Pacman spawned at (" << m_pacman.x << "," << m_pacman.y << ")" << std::endl;
    }
    
    void initializeAISnakes() {
        // Add one AI snake for level 2+
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(2, m_gridWidth - 3);
        std::uniform_int_distribution<> disY(2, m_gridHeight - 3);
        
        Point aiStartPos;
        bool validPosition = false;
        int attempts = 0;
        
        do {
            aiStartPos = Point(disX(gen), disY(gen));
            validPosition = !isOccupiedBySnake(aiStartPos) && !(aiStartPos == m_food);
            attempts++;
        } while (!validPosition && attempts < 50);
        
        if (validPosition) {
            // Create AI snake with A* pathfinding (like original)
            Snake aiSnake(aiStartPos.x, aiStartPos.y, Point(-1, 0), nullptr, -1, 
                         GameColors::SNAKE_AI.r, GameColors::SNAKE_AI.g, GameColors::SNAKE_AI.b, NAV_ASTAR);
            m_aiSnakes.push_back(aiSnake);
            std::cout << "🤖 AI Snake spawned at (" << aiStartPos.x << "," << aiStartPos.y 
                      << ") with A* pathfinding" << std::endl;
        }
    }
    
    bool isOccupiedBySnake(const Point& pos) {
        // Update tile grid with current game state
        updateTileGrid();
        
        if (!m_tileGrid || !m_tileGrid->isValidPosition(pos.x, pos.y)) {
            return false;
        }
        
        TileContent tile = m_tileGrid->getTile(pos.x, pos.y);
        return (tile == TileContent::SNAKE_HEAD || tile == TileContent::SNAKE_BODY);
    }
    
    // Pacman update logic (Level 1)
    void updatePacman() {
        if (!m_pacmanActive) return;
        
        // Calculate new direction toward food
        m_pacmanDirection = calculatePacmanDirection();
        
        // Move pacman
        Point newPacmanPos = Point(m_pacman.x + m_pacmanDirection.x, m_pacman.y + m_pacmanDirection.y);
        
        if (!isPositionOccupiedForPacman(newPacmanPos)) {
            m_pacman = newPacmanPos;
            
            // Check if pacman got the food
            if (m_pacman == m_food) {
                std::cout << "🟡 Pacman ate the food!" << std::endl;
                placeFood(); // Generate new food
            }
        }
    }
    
    // Simple AI for pacman to move toward food
    Point calculatePacmanDirection() {
        if (!m_pacmanActive) return Point(0, 0);
        
        // Use pathfinding library with greedy axis prioritization algorithm
        return calculateGreedyAxisPathDirection(m_pacman, m_food, m_gridWidth, m_gridHeight, 
                                               isPositionOccupiedForPacmanCallback, this);
    }
    
    // Callback function for Pacman pathfinding algorithm
    static bool isPositionOccupiedForPacmanCallback(const Point& pos, void* context) {
        SnakeGameLogic* game = static_cast<SnakeGameLogic*>(context);
        return game->isPositionOccupiedForPacman(pos);
    }
    
    // Helper method for Pacman pathfinding position checking - uses unified tile grid
    bool isPositionOccupiedForPacman(const Point& pos) {
        // Ensure tile grid is updated
        updateTileGrid();
        
        if (!m_tileGrid || !m_tileGrid->isValidPosition(pos.x, pos.y)) {
            return true; // Out of bounds
        }
        
        TileContent tile = m_tileGrid->getTile(pos.x, pos.y);
        
        // Pacman can move to empty tiles and food tiles, but not into snakes or AI snakes
        return !(tile == TileContent::EMPTY || tile == TileContent::FOOD);
    }
    

    
    // AI Snake update logic (Level 2+)
    void updateAISnakes() {
        if (m_aiSnakes.empty()) return;
        
        // Update all AI snakes
        for (size_t aiIdx = 0; aiIdx < m_aiSnakes.size(); aiIdx++) {
            updateAISnake(aiIdx);
        }
    }
    
    void updateAISnake(int aiSnakeIndex) {
        if (aiSnakeIndex >= m_aiSnakes.size()) return;
        
        Snake& aiSnake = m_aiSnakes[aiSnakeIndex];
        
        // Calculate new direction for AI snake using A* pathfinding
        Point newDirection = calculateAISnakeDirection(aiSnakeIndex);
        aiSnake.direction = newDirection;
        
        // Calculate new head position
        Point newHead = Point(aiSnake.body[0].x + aiSnake.direction.x, 
                             aiSnake.body[0].y + aiSnake.direction.y);
        
        bool aiSnakeCanMove = isValidMoveForAISnake(newHead, aiSnakeIndex);
        bool aiSnakeGotFood = false;
        
        // Check if the AI snake move is valid
        if (!aiSnakeCanMove) {
            // Invalid move - pause movement until direction changes
            if (!aiSnake.movementPaused) {
                std::cout << "🤖 NPC Snake " << aiSnakeIndex << " collision!" << std::endl;
            }
            aiSnake.movementPaused = true;
        } else {
            // Valid move - resume movement if it was paused
            if (aiSnake.movementPaused) {
                aiSnake.movementPaused = false;
                std::cout << "🤖 NPC Snake " << aiSnakeIndex << " movement resumed!" << std::endl;
            }
            
            // Move the AI snake
            aiSnake.body.insert(aiSnake.body.begin(), newHead);
            
            // Check if AI snake got the food
            aiSnakeGotFood = (newHead == m_food);
            if (aiSnakeGotFood) {
                aiSnake.score++;
                std::cout << "🤖 NPC Snake " << aiSnakeIndex << " scored! Score: " << aiSnake.score << std::endl;
                
                // Check if pacman also got food (competition)
                bool pacmanGotFood = (m_pacmanActive && m_pacman == m_food);
                if (pacmanGotFood) {
                    std::cout << "🟡 Pacman also got the food!" << std::endl;
                }
                
                placeFood(); // Generate new food immediately
            }
        }
        
        // If AI snake didn't get food, remove tail (unless it was blocked)
        if (!aiSnakeGotFood && aiSnakeCanMove) {
            aiSnake.body.pop_back();
        }
    }
    
    // Callback function for pathfinding algorithm
    static bool isPositionOccupiedCallback(const Point& pos, void* context) {
        SnakeGameLogic* game = static_cast<SnakeGameLogic*>(context);
        return game->isPositionOccupiedForPathfinding(pos);
    }
    
    // Helper method for pathfinding position checking - uses unified tile grid
    bool isPositionOccupiedForPathfinding(const Point& pos) {
        // Ensure tile grid is updated
        updateTileGrid();
        
        // Use tile grid for O(1) pathfinding queries
        if (!m_tileGrid) return true;
        return m_tileGrid->isPathBlocked(pos);
    }
    
    // Naive pathfinding (original algorithm) - uses pathfinding library
    Point calculateNaiveDirection(int aiSnakeIndex) {
        if (aiSnakeIndex >= m_aiSnakes.size()) return Point(0, 0);
        
        const Snake& aiSnake = m_aiSnakes[aiSnakeIndex];
        Point head = aiSnake.body[0];
        
        // Use pathfinding library for naive direction calculation
        return calculateNaivePathDirection(head, m_food, m_gridWidth, m_gridHeight, 
                                         isPositionOccupiedCallback, this, 
                                         aiSnake.direction);
    }
    
    // A* pathfinding direction calculation - uses pathfinding library
    Point calculateAStarDirection(int aiSnakeIndex) {
        if (aiSnakeIndex >= m_aiSnakes.size()) return Point(0, 0);
        
        const Snake& aiSnake = m_aiSnakes[aiSnakeIndex];
        Point head = aiSnake.body[0];
        
        // Use pathfinding library for A* direction calculation
        Point direction = calculateAStarPathDirection(head, m_food, m_gridWidth, m_gridHeight, 
                                                    isPositionOccupiedCallback, this);
        
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
        if (aiSnakeIndex >= m_aiSnakes.size()) return Point(0, 0);
        
        const Snake& aiSnake = m_aiSnakes[aiSnakeIndex];
        
        switch (aiSnake.navType) {
            case NAV_ASTAR:
                return calculateAStarDirection(aiSnakeIndex);
            case NAV_NAIVE:
            default:
                return calculateNaiveDirection(aiSnakeIndex);
        }
    }
    
    // Helper function to check if an AI snake move is valid - uses unified tile grid
    bool isValidMoveForAISnake(const Point& newHead, int aiSnakeIndex) {
        // Update tile grid with current game state
        updateTileGrid();
        
        // Use unified tile grid for O(1) collision detection
        if (!m_tileGrid || !m_tileGrid->isValidPosition(newHead.x, newHead.y)) {
            return false; // Out of bounds or tile grid not available
        }
        
        TileContent tile = m_tileGrid->getTile(newHead.x, newHead.y);
        
        // AI snakes can move to empty tiles and food tiles
        if (tile == TileContent::EMPTY || tile == TileContent::FOOD) {
            return true;
        }
        
        // Everything else is a collision
        return false;
    }
    
    // IPC functionality - now uses unified tile grid
    void createIPCGridData(char* gridData) {
        // Update tile grid with current game state
        updateTileGrid();
        
        // Use unified tile grid to generate IPC data
        if (m_tileGrid) {
            m_tileGrid->createIPCGrid(gridData);
        } else {
            // Fallback: initialize with spaces if tile grid is not available
            for (int i = 0; i < m_gridWidth * m_gridHeight; i++) {
                gridData[i] = ' ';
            }
        }
    }
    
private:
    SnakeApp* m_app;
    SnakeUI m_ui;
    
    // Unified tile grid system for collision detection, pathfinding, and IPC
    std::unique_ptr<TileGrid> m_tileGrid;  
    
    // Game state
    std::vector<Snake> m_snakes;
    std::vector<Snake> m_aiSnakes;
    Point m_food;
    bool m_gameOver = false;
    bool m_gamePaused = false;
    int m_gridWidth = 32;
    int m_gridHeight = 20;
    int m_level = 0; // Level: 0=JUST SNAKE, 1=PACMAN, 2=MULTI SNAKE
    
    // Pacman state
    bool m_pacmanActive = false;
    Point m_pacman;
    Point m_pacmanDirection;
    float m_lastPacmanMoveTime = 0.0f;
    float m_pacmanMoveInterval = 0.3f; // Pacman moves slightly slower than snake
    
    // AI Snake state
    float m_lastAISnakeMoveTime = 0.0f;
    float m_aiSnakeMoveInterval = 0.25f; // AI snake moves at moderate speed
    
    // Timing
    float m_lastMoveTime = 0.0f;
    float m_moveInterval = 0.2f;
    

};

int main(int argc, char* argv[]) {
    std::cout << "🐍 Snake Game" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Parse command line arguments
    AppConfig config;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0) {
            config.ipcMode = true;
            config.fullscreen = false;
            std::cout << "🔗 IPC Mode enabled" << std::endl;
        }
    }
    
    // Create and initialize app infrastructure
    auto app = std::make_unique<SnakeApp>();
    if (!app->initialize(config)) {
        std::cerr << "❌ Failed to initialize app infrastructure" << std::endl;
        return -1;
    }
    
    // Initialize IPC mode if enabled
    if (config.ipcMode) {
        if (!app->initializeIPC()) {
            std::cerr << "❌ Failed to initialize IPC mode" << std::endl;
            return -1;
        }
    }
    
    // Create game logic (subscribes to events automatically)
    auto gameLogic = std::make_unique<SnakeGameLogic>(app.get());
    
    std::cout << "\n🎮 Controls: Arrow Keys/WASD, Space=Pause, R=Reset" << std::endl;
    std::cout << "==========================================\n" << std::endl;
    
    // Run the application (event-driven loop)
    app->run();
    
    // Cleanup
    gameLogic.reset();
    app->shutdown();
    
    std::cout << "👋 Thanks for playing!" << std::endl;
    return 0;
} 