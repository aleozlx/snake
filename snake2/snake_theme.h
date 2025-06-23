#pragma once

#include "snake_dep.h"

// ===== SNAKE GAME COLOR THEME =====
// Centralized color definitions for consistency across the snake game

namespace SnakeTheme {

// Basic color palette
namespace Colors {
    constexpr RGBColor RED(1.0f, 0.0f, 0.0f);
    constexpr RGBColor GREEN(0.0f, 1.0f, 0.0f);
    constexpr RGBColor BLUE(0.0f, 0.0f, 1.0f);
    constexpr RGBColor YELLOW(1.0f, 1.0f, 0.0f);
    constexpr RGBColor CYAN(0.0f, 1.0f, 1.0f);
    constexpr RGBColor MAGENTA(1.0f, 0.0f, 1.0f);
    constexpr RGBColor WHITE(1.0f, 1.0f, 1.0f);
    constexpr RGBColor BLACK(0.0f, 0.0f, 0.0f);
    constexpr RGBColor GRAY(0.5f, 0.5f, 0.5f);
    constexpr RGBColor LIGHT_GRAY(0.8f, 0.8f, 0.8f);
    constexpr RGBColor DARK_GRAY(0.1f, 0.1f, 0.1f);
    constexpr RGBColor ORANGE(1.0f, 0.5f, 0.0f);
    constexpr RGBColor PURPLE(1.0f, 0.0f, 1.0f);
    constexpr RGBColor PINK(1.0f, 0.5f, 0.8f);
    constexpr RGBColor DARK_ORANGE(1.0f, 0.3f, 0.0f);
}

// Game entity colors
namespace GameColors {
    // Snake player colors array for multiplayer (indexed by player number)
    constexpr RGBColor SNAKE_PLAYERS[4] = {
        Colors::GREEN,   // snake[0] (keyboard + controller 0)
        Colors::RED,     // snake[1] (controller 1)
        Colors::BLUE,    // snake[2] (controller 2)
        Colors::YELLOW,  // snake[3] (controller 3)
    };
    
    // AI Snake colors
    constexpr RGBColor SNAKE_AI = Colors::PINK;
    
    // Special entity colors
    constexpr RGBColor FOOD = Colors::RED;
    constexpr RGBColor PACMAN = Colors::YELLOW;
    constexpr RGBColor BORDER = Colors::GRAY;
    
    // Corner marker colors
    constexpr RGBColor CORNER_BOTTOM_LEFT = Colors::YELLOW;   // Bottom-left
    constexpr RGBColor CORNER_BOTTOM_RIGHT = Colors::CYAN;    // Bottom-right
    constexpr RGBColor CORNER_TOP_LEFT = Colors::MAGENTA;     // Top-left
    constexpr RGBColor CORNER_TOP_RIGHT = Colors::WHITE;      // Top-right
}

// UI and state colors
namespace UIColors {
    // Text colors
    constexpr RGBColor TEXT_PRIMARY = Colors::WHITE;
    constexpr RGBColor TEXT_SECONDARY = Colors::LIGHT_GRAY;
    constexpr RGBColor TEXT_SUCCESS = Colors::GREEN;
    constexpr RGBColor TEXT_ERROR = Colors::RED;
    constexpr RGBColor TEXT_WARNING = Colors::ORANGE;
    
    // Level description color
    constexpr RGBColor LEVEL_DESC = RGBColor(1.0f, 0.8f, 0.0f); // Custom orange-yellow
    
    // Button colors
    constexpr RGBColor BUTTON_CONFIRM = Colors::GREEN;  // A button
    constexpr RGBColor BUTTON_CANCEL = Colors::RED;     // B button
    
    // Dialog background colors
    constexpr RGBColor DIALOG_EXIT_BG(0.1f, 0.1f, 0.3f);   // Dark blue
    constexpr RGBColor DIALOG_RESET_BG(0.3f, 0.1f, 0.1f);  // Dark red
}

// State-based colors (game states)
namespace StateColors {
    // Game pause states
    constexpr RGBColor PAUSED = Colors::YELLOW;
    constexpr RGBColor MOVEMENT_BLOCKED = Colors::PURPLE;
    
    // Border states
    constexpr RGBColor BORDER_NORMAL = Colors::GRAY;
    constexpr RGBColor BORDER_PAUSED = Colors::ORANGE;
    constexpr RGBColor BORDER_COLLISION = Colors::RED;  // Flashing red for collisions
    constexpr RGBColor BORDER_EXIT_CONFIRM = Colors::ORANGE;
    constexpr RGBColor BORDER_RESET_CONFIRM = Colors::DARK_ORANGE;
    
    // Snake intensity values (for multiplying with colors)
    constexpr float SNAKE_HEAD_INTENSITY = 1.0f;     // Full brightness for head
    constexpr float SNAKE_BODY_INTENSITY = 0.6f;     // Dimmed for body segments
}

// Drawing helper colors (for eyes, highlights, etc.)
namespace DrawingColors {
    constexpr RGBColor EYE_WHITE = Colors::WHITE;
    constexpr RGBColor EYE_PUPIL = Colors::BLACK;
    constexpr RGBColor EYE_HIGHLIGHT = Colors::WHITE;
    
    constexpr RGBColor PACMAN_BODY = Colors::YELLOW;
    constexpr RGBColor PACMAN_MOUTH = Colors::DARK_GRAY;
}

} // namespace SnakeTheme 