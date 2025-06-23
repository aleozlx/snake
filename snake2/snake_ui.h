#ifndef SNAKE_UI_H
#define SNAKE_UI_H

#include "snake_dep.h"
#include "snake_draw.h"
#include "snake_app.h"
#include <vector>

// Forward declarations
struct Snake;

class SnakeUI {
public:
    SnakeUI(SnakeApp* app);
    
    // Dialog management
    void showExitConfirmation();
    void showResetConfirmation();
    void hideExitConfirmation();
    void hideResetConfirmation();
    bool isExitConfirmationShown() const { return m_exitConfirmation; }
    bool isResetConfirmationShown() const { return m_resetConfirmation; }
    bool isAnyDialogShown() const { return m_exitConfirmation || m_resetConfirmation; }
    
    // UI rendering
    void renderUI(const std::vector<Snake>& snakes, const std::vector<Snake>& aiSnakes, 
                  int level, bool gamePaused, bool anySnakePaused);
    void renderDialogs();
    
    // Gamepad input tracking for visual feedback
    void updateGamepadInput(int buttonPressed, float currentTime);
    void setUsingGamepadInput(bool using_gamepad) { m_usingGamepadInput = using_gamepad; }
    
private:
    // Helper method to get drawing context
    SnakeDraw::DrawContext getDrawContext() const;
    
    // Individual UI element rendering
    void drawLevelInfo(int level);
    void drawScores(const std::vector<Snake>& snakes);
    void drawGamepadDebug();
    void drawCornerMarkers();
    void drawBorder(bool gamePaused, bool anySnakePaused, bool exitConfirmation, bool resetConfirmation);
    
    SnakeApp* m_app;
    
    // Dialog state
    bool m_exitConfirmation = false;
    bool m_resetConfirmation = false;
    
    // Gamepad input tracking for visual debug
    bool m_usingGamepadInput = false;
    int m_lastButtonPressed = -1;
    float m_lastButtonTime = 0.0f;
    
    // Grid dimensions (cached from app config)
    int m_gridWidth;
    int m_gridHeight;
};

#endif // SNAKE_UI_H 