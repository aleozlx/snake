#include "snake_ui.h"
#include "snake_theme.h" // Centralized color theme
#include <iostream>

// Use centralized color theme
using namespace SnakeTheme;

SnakeUI::SnakeUI(SnakeApp* app) : m_app(app) {
    // Cache grid dimensions from app config
    m_gridWidth = m_app->getConfig().gridWidth;
    m_gridHeight = m_app->getConfig().gridHeight;
    
    std::cout << "🎨 UI system initialized" << std::endl;
}

// Dialog management
void SnakeUI::showExitConfirmation() {
    m_exitConfirmation = true;
    std::cout << "🚪 Exit confirmation dialog shown" << std::endl;
}

void SnakeUI::showResetConfirmation() {
    m_resetConfirmation = true;
    std::cout << "🔄 Reset confirmation dialog shown" << std::endl;
}

void SnakeUI::hideExitConfirmation() {
    m_exitConfirmation = false;
    std::cout << "❌ Exit confirmation dialog hidden" << std::endl;
}

void SnakeUI::hideResetConfirmation() {
    m_resetConfirmation = false;
    std::cout << "❌ Reset confirmation dialog hidden" << std::endl;
}

// UI rendering
void SnakeUI::renderUI(const std::vector<Snake>& snakes, const std::vector<Snake>& aiSnakes, 
                       int level, bool gamePaused, bool anySnakePaused) {
    // Check if any snake is paused for border coloring
    bool anyPlayerSnakePaused = false;
    for (const auto& snake : snakes) {
        if (snake.movementPaused) {
            anyPlayerSnakePaused = true;
            break;
        }
    }
    
    // Check if any AI snake is paused
    bool anyAISnakePaused = false;
    for (const auto& aiSnake : aiSnakes) {
        if (aiSnake.movementPaused) {
            anyAISnakePaused = true;
            break;
        }
    }
    
    bool totalAnySnakePaused = anyPlayerSnakePaused || anyAISnakePaused || anySnakePaused;
    
    // Draw UI elements in order
    drawCornerMarkers();
    drawLevelInfo(level);
    drawScores(snakes);
    drawGamepadDebug();
    drawBorder(gamePaused, totalAnySnakePaused, m_exitConfirmation, m_resetConfirmation);
}

void SnakeUI::renderDialogs() {
    // Draw confirmation dialogues LAST (on top of everything)
    if (m_exitConfirmation) {
        auto ctx = getDrawContext();
        SnakeDraw::drawConfirmationDialogue("CONFIRM EXIT", UIColors::DIALOG_EXIT_BG, ctx);
    }
    if (m_resetConfirmation) {
        auto ctx = getDrawContext();
        SnakeDraw::drawConfirmationDialogue("CONFIRM RESET", UIColors::DIALOG_RESET_BG, ctx);
    }
}

// Gamepad input tracking
void SnakeUI::updateGamepadInput(int buttonPressed, float currentTime) {
    m_usingGamepadInput = true;
    m_lastButtonPressed = buttonPressed;
    m_lastButtonTime = currentTime;
}

// Helper method to create drawing context
SnakeDraw::DrawContext SnakeUI::getDrawContext() const {
    return SnakeDraw::DrawContext(m_gridWidth, m_gridHeight, 
                                 m_app->getOffsetUniform(), m_app->getColorUniform(), 
                                 m_app->getScaleUniform(), m_app->getShapeTypeUniform(),
                                 m_app->getInnerRadiusUniform(), m_app->getTextureUniform(),
                                 m_app->getUseTextureUniform(), m_app->getAspectRatioUniform());
}

// Individual UI element rendering
void SnakeUI::drawLevelInfo(int level) {
    auto ctx = getDrawContext();
    
    // Display level info
    float cellWidth = 2.0f / m_gridWidth;
    float cellHeight = 2.0f / m_gridHeight;
    float levelTextX = (2 * cellWidth) - 1.0f; // Grid position 2 to NDC
    float levelTextY = ((m_gridHeight - 2) * cellHeight) - 1.0f; // Near top
    float textSize = cellHeight * 0.8f; // Text size is 80% of a cell height
    
    // Display level number
    char levelText[16];
    snprintf(levelText, sizeof(levelText), "LVL %d", level);
        SnakeDraw::drawText(levelText, levelTextX, levelTextY, textSize, Colors::LIGHT_GRAY, ctx);
    
    // Display level description below  
    if (level == 0) {
        SnakeDraw::drawText("JUST SNAKE", levelTextX, levelTextY - textSize * 1.2f, textSize * 0.7f, 
                           UIColors::LEVEL_DESC, ctx);
    } else if (level == 1) {
        SnakeDraw::drawText("PACMAN", levelTextX, levelTextY - textSize * 1.2f, textSize * 0.7f, 
                           UIColors::LEVEL_DESC, ctx);
    } else if (level >= 2) {
        SnakeDraw::drawText("NPC SNAKE", levelTextX, levelTextY - textSize * 1.2f, textSize * 0.7f, 
                           UIColors::LEVEL_DESC, ctx);
    }
}

void SnakeUI::drawScores(const std::vector<Snake>& snakes) {
    auto ctx = getDrawContext();
    
    // Display scores in top right corner
    float cellWidth = 2.0f / m_gridWidth;
    float cellHeight = 2.0f / m_gridHeight;
    float scoreTextX = ((m_gridWidth - 8) * cellWidth) - 1.0f; // Top right corner position
    float scoreTextY = ((m_gridHeight - 2) * cellHeight) - 1.0f; // Near top
    float textSize = cellHeight * 0.8f;
    
    for (size_t i = 0; i < snakes.size(); i++) {
        char scoreText[32];
        snprintf(scoreText, sizeof(scoreText), "P%zu: %d", i + 1, snakes[i].score);
        SnakeDraw::drawText(scoreText, scoreTextX, scoreTextY - (i * textSize * 1.5f), textSize * 0.6f, 
                           snakes[i].color, ctx);
    }
}

void SnakeUI::drawGamepadDebug() {
    auto ctx = getDrawContext();
    
    // Visual debug: Display last pressed button name if gamepad input detected
    if (m_lastButtonPressed >= 0 && m_usingGamepadInput) {
        float cellWidth = 2.0f / m_gridWidth;
        float cellHeight = 2.0f / m_gridHeight;
        float levelTextX = (2 * cellWidth) - 1.0f; // Grid position 2 to NDC
        float levelTextY = ((m_gridHeight - 2) * cellHeight) - 1.0f; // Near top
        float textSize = cellHeight * 0.8f; // Text size is 80% of a cell height
        
        float buttonTextX = levelTextX;
        float buttonTextY = levelTextY - textSize * 3.5f; // Closer to level description
        
        // Get button name - simplified for SDL2
        const char* buttonName = "UNKNOWN";
        switch (m_lastButtonPressed) {
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
        
        SnakeDraw::drawText(buttonName, buttonTextX, buttonTextY, textSize * 0.6f, Colors::YELLOW, ctx);
        
        // Display "GAMEPAD:" label above the button name
        SnakeDraw::drawText("GAMEPAD", buttonTextX, buttonTextY + textSize * 0.8f, textSize * 0.6f, Colors::CYAN, ctx);
    }
}

void SnakeUI::drawCornerMarkers() {
    auto ctx = getDrawContext();
    
    // Corner markers with meaningful colors
    SnakeDraw::drawSquare(0, 0, GameColors::CORNER_BOTTOM_LEFT, ctx);                           // Bottom-left: Yellow
    SnakeDraw::drawSquare(m_gridWidth - 1, 0, GameColors::CORNER_BOTTOM_RIGHT, ctx);               // Bottom-right: Cyan
    SnakeDraw::drawSquare(0, m_gridHeight - 1, GameColors::CORNER_TOP_LEFT, ctx);           // Top-left: Magenta
    SnakeDraw::drawSquare(m_gridWidth - 1, m_gridHeight - 1, GameColors::CORNER_TOP_RIGHT, ctx); // Top-right: White
}

void SnakeUI::drawBorder(bool gamePaused, bool anySnakePaused, bool exitConfirmation, bool resetConfirmation) {
    float currentTime = m_app->getCurrentTime();
    const float FLASH_INTERVAL = 0.1f;
    RGBColor borderColor;
    
    if (exitConfirmation) {
        borderColor = StateColors::BORDER_EXIT_CONFIRM;
    } else if (resetConfirmation) {
        borderColor = StateColors::BORDER_RESET_CONFIRM;
    } else if (gamePaused) {
        borderColor = StateColors::BORDER_PAUSED;
    } else if (anySnakePaused) {
        bool showRed = ((int)(currentTime / FLASH_INTERVAL) % 2) == 0;
        borderColor = showRed ? StateColors::BORDER_COLLISION : StateColors::BORDER_NORMAL;
    } else {
        borderColor = StateColors::BORDER_NORMAL;
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