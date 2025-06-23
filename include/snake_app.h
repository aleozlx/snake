#pragma once

#include <functional>
#include <memory>

// Forward declarations
struct SDL_Window;
typedef void *SDL_GLContext;
typedef struct _SDL_GameController SDL_GameController;
typedef unsigned int GLuint;
typedef int GLint;

// ===== SNAKE APP HEADER =====
// Clean interface for the Snake Application
// Implementation details are in snake_app.cpp

// Event types that the game can subscribe to
enum class EventType {
    GAME_TICK = 0,
    GAME_RENDER,
    GAME_EXIT,
    SNAKE_FOOD_EATEN,
    INPUT_KEYBOARD,
    INPUT_GAMEPAD_BUTTON,
    INPUT_GAMEPAD_AXIS,
    EVENT_TYPE_COUNT
};

// Simple event structure
struct Event {
    EventType type;
    float timestamp;
    
    // Event-specific data (simplified for clean interface)
    struct {
        int keyCode = 0;
        int controllerId = 0;
        int buttonId = 0;
        float axisValue = 0.0f;
        bool isPressed = false;
    } input;
    
    struct {
        float deltaTime = 0.0f;
        float currentTime = 0.0f;
    } tick;
    
    Event() : type(EventType::GAME_TICK), timestamp(0.0f) {}
};

// Event callback function signature
using EventCallback = std::function<void(const Event&)>;

// Event system interface
class EventSystem {
public:
    virtual ~EventSystem() = default;
    virtual void subscribe(EventType eventType, EventCallback callback) = 0;
    virtual void unsubscribe(EventType eventType) = 0;
    virtual void publish(const Event& event) = 0;
};

// Configuration structure
struct AppConfig {
    bool fullscreen = true;
    int windowWidth = 800;
    int windowHeight = 600;
    bool ipcMode = false;
    bool enableGyroscope = true;
    bool enableRumble = true;
    int gridWidth = 32;
    int gridHeight = 20;
    const char* windowTitle = "Snake Game - Refactored";
};

// Main application class (PIMPL pattern for clean interface)
class SnakeApp {
public:
    SnakeApp();
    ~SnakeApp();
    
    // Non-copyable
    SnakeApp(const SnakeApp&) = delete;
    SnakeApp& operator=(const SnakeApp&) = delete;
    
    // Core lifecycle
    bool initialize(const AppConfig& config);
    void run();
    void shutdown();
    
    // Event system access
    EventSystem* getEventSystem();
    
    // Resource access for rendering
    SDL_Window* getWindow() const;
    GLuint getShaderProgram() const;
    GLuint getVAO() const;
    GLuint getAppleTexture() const;
    
    // Timing
    float getCurrentTime() const;
    float getDeltaTime() const;
    
    // Configuration access
    const AppConfig& getConfig() const;
    
    // Controller access
    int getNumControllers() const;
    

    
    // IPC functionality
    bool initializeIPC();
    void writeIPCSlot(const char* gridData, char lastButton);
    void cleanupIPC();
    
    // Uniform locations for rendering
    GLint getOffsetUniform() const;
    GLint getColorUniform() const;
    GLint getScaleUniform() const;
    GLint getShapeTypeUniform() const;
    GLint getInnerRadiusUniform() const;
    GLint getTextureUniform() const;
    GLint getUseTextureUniform() const;
    GLint getAspectRatioUniform() const;

private:
    // PIMPL pattern - all implementation details are hidden
    class Impl;
    std::unique_ptr<Impl> m_impl;
}; 