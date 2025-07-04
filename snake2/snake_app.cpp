#include "snake_dep.h"  // IWYU pragma: keep
#include "snake_app.h"
#include "circular_buffer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

// #define ENABLE_IPC

// ===== EVENT SYSTEM IMPLEMENTATION =====

namespace {

class EventSystemImpl : public EventSystem {
public:
    void subscribe(EventType eventType, EventCallback callback) override {
        m_subscribers[eventType].push_back(callback);
    }
    
    void unsubscribe(EventType eventType) override {
        m_subscribers[eventType].clear();
    }
    
    void publish(const Event& event) override {
        auto it = m_subscribers.find(event.type);
        if (it != m_subscribers.end()) {
            for (const auto& callback : it->second) {
                callback(event);
            }
        }
    }
    
private:
    std::unordered_map<EventType, std::vector<EventCallback>> m_subscribers;
};

} // anonymous namespace

// ===== SNAKE APP PIMPL IMPLEMENTATION =====

class SnakeApp::Impl {
public:
    AppConfig config;
    std::unique_ptr<EventSystem> eventSystem;
    
    // SDL/OpenGL resources
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    std::vector<SDL_GameController*> gameControllers;
    
    // OpenGL resources
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint appleTexture = 0;
    
    // Uniform locations
    GLint u_offset = -1;
    GLint u_color = -1;
    GLint u_scale = -1;
    GLint u_shape_type = -1;
    GLint u_inner_radius = -1;
    GLint u_texture = -1;
    GLint u_use_texture = -1;
    GLint u_aspect_ratio = -1;
    
    // Application state
    bool running = false;
    float currentTime = 0.0f;
    float deltaTime = 0.0f;
    float lastFrameTime = 0.0f;
    

#ifdef ENABLE_IPC
    // IPC state
    MemoryMappedCircularBuffer* circularBuffer = nullptr;
#endif
    
    Impl() : eventSystem(std::make_unique<EventSystemImpl>()) {}
    
    // ===== LOW-LEVEL IMPLEMENTATION METHODS =====
    
    bool initializeSDL() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
            std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
            return false;
        }
        
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        
        SDL_DisplayMode displayMode;
        if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
            std::cerr << "Failed to get display mode: " << SDL_GetError() << std::endl;
            return false;
        }
        
        if (config.ipcMode || !config.fullscreen) {
            window = SDL_CreateWindow(config.windowTitle, 
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                config.windowWidth, config.windowHeight, SDL_WINDOW_OPENGL);
        } else {
            window = SDL_CreateWindow(config.windowTitle, 
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                displayMode.w, displayMode.h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
        }
        
        if (!window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            return false;
        }
        
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            return false;
        }
        
        SDL_GL_SetSwapInterval(1);
        SDL_ShowCursor(SDL_DISABLE);
        
        return true;
    }
    
    bool initializeOpenGL() {
        if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }
        return true;
    }
    
    bool initializeControllers() {
        int numControllers = SDL_NumJoysticks();
        std::cout << "Found " << numControllers << " controllers" << std::endl;
        
        for (int i = 0; i < numControllers && i < 4; i++) {
            SDL_GameController* controller = SDL_GameControllerOpen(i);
            if (controller) {
                gameControllers.push_back(controller);
                std::cout << "Controller " << i << ": " << SDL_GameControllerName(controller) << std::endl;
            }
        }
        
        return true;
    }
    

    
    // IPC functionality
    bool initializeIPCMode() {
#ifdef ENABLE_IPC
        std::cout << "=== INITIALIZING IPC MODE ===" << std::endl;
        std::cout << "Grid size: " << config.gridWidth << "x" << config.gridHeight << std::endl;
        std::cout << "Grid data size: " << (config.gridWidth * config.gridHeight) << " bytes" << std::endl;
        
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
#else
        return true;
#endif
    }
    
    void writeIPCSlotData(const char* gridData, char lastButton) {
#ifdef ENABLE_IPC
        if (!circularBuffer) return;
        
        // IPC slot data structure (using config grid size)
        struct IPCSlotData {
            char grid[32 * 20];  // Normal grid representation (640 bytes)
            char last_button;    // Last button pressed (1 byte)
            char padding[383];   // Pad to slot size (1024 - 640 - 1 = 383 bytes)
        };
        
        IPCSlotData slotData = {};
        
        // Copy grid data
        int gridSize = config.gridWidth * config.gridHeight;
        memcpy(slotData.grid, gridData, std::min(gridSize, 32 * 20));
        
        // Set last button pressed
        slotData.last_button = lastButton;
        
        // Write to circular buffer
        if (!circularBuffer->write_slot(&slotData, sizeof(IPCSlotData))) {
            std::cout << "⚠️  Failed to write to circular buffer!" << std::endl;
        }
#endif
    }
    
    void cleanupIPCMode() {
#ifdef ENABLE_IPC
        if (circularBuffer) {
            circularBuffer->cleanup();
            delete circularBuffer;
            circularBuffer = nullptr;
            std::cout << "IPC mode cleaned up" << std::endl;
        }
#endif
    }
    
    bool loadShaders() {
        // Load shader source
        std::string vertexShaderSource = loadShaderFromFile("shaders/vertex.vs");
        std::string fragmentShaderSource = loadShaderFromFile("shaders/fragment.fs");
        
        if (vertexShaderSource.empty() || fragmentShaderSource.empty()) {
            std::cerr << "Failed to load shader files!" << std::endl;
            return false;
        }
        
        GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER, "Vertex");
        GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER, "Fragment");
        
        if (vertexShader == 0 || fragmentShader == 0) {
            return false;
        }
        
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        
        // Get uniforms
        u_offset = glGetUniformLocation(shaderProgram, "u_offset");
        u_color = glGetUniformLocation(shaderProgram, "u_color");
        u_scale = glGetUniformLocation(shaderProgram, "u_scale");
        u_shape_type = glGetUniformLocation(shaderProgram, "u_shape_type");
        u_inner_radius = glGetUniformLocation(shaderProgram, "u_inner_radius");
        u_texture = glGetUniformLocation(shaderProgram, "u_texture");
        u_use_texture = glGetUniformLocation(shaderProgram, "u_use_texture");
        u_aspect_ratio = glGetUniformLocation(shaderProgram, "u_aspect_ratio");
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        return true;
    }
    
    bool setupRenderResources() {
        // Setup VAO/VBO
        float squareVertices[] = {
            0.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f
        };
        
        GLuint indices[] = {0, 1, 2, 2, 3, 0};
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        GLuint EBO;
        glGenBuffers(1, &EBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        return true;
    }
    
    void updateTimers() {
        float newTime = SDL_GetTicks() / 1000.0f;
        deltaTime = newTime - lastFrameTime;
        lastFrameTime = newTime;
        currentTime = newTime;
    }
    
    void handleSDLEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    publishExitEvent();
                    break;
                case SDL_KEYDOWN:
                    publishKeyboardEvent(event.key.keysym.sym, true);
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    publishGamepadButtonEvent(event.cbutton.which, event.cbutton.button, true);
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    publishGamepadAxisEvent(event.caxis.which, event.caxis.axis, event.caxis.value);
                    break;
                default:
                    break;
            }
        }
    }
    
    void publishExitEvent() {
        Event exitEvent;
        exitEvent.type = EventType::GAME_EXIT;
        exitEvent.timestamp = currentTime;
        eventSystem->publish(exitEvent);
        running = false;
    }
    
    void publishKeyboardEvent(int keyCode, bool pressed) {
        Event inputEvent;
        inputEvent.type = EventType::INPUT_KEYBOARD;
        inputEvent.timestamp = currentTime;
        inputEvent.input.keyCode = keyCode;
        inputEvent.input.isPressed = pressed;
        eventSystem->publish(inputEvent);
    }
    
    void publishGamepadButtonEvent(int controllerId, int buttonId, bool pressed) {
        Event inputEvent;
        inputEvent.type = EventType::INPUT_GAMEPAD_BUTTON;
        inputEvent.timestamp = currentTime;
        inputEvent.input.controllerId = controllerId;
        inputEvent.input.buttonId = buttonId;
        inputEvent.input.isPressed = pressed;
        eventSystem->publish(inputEvent);
    }
    
    void publishGamepadAxisEvent(int controllerId, int axis, float value) {
        Event inputEvent;
        inputEvent.type = EventType::INPUT_GAMEPAD_AXIS;
        inputEvent.timestamp = currentTime;
        inputEvent.input.controllerId = controllerId;
        inputEvent.input.buttonId = axis; // Reuse buttonId for axis
        inputEvent.input.axisValue = value;
        eventSystem->publish(inputEvent);
    }
    
    void cleanup() {
        // Cleanup IPC mode
        cleanupIPCMode();
        
        for (auto controller : gameControllers) {
            if (controller) {
                SDL_GameControllerClose(controller);
            }
        }
        gameControllers.clear();
        
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(shaderProgram);
        
        if (appleTexture != 0) {
            glDeleteTextures(1, &appleTexture);
        }
        
        if (glContext) {
            SDL_GL_DeleteContext(glContext);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        
        SDL_Quit();
    }
    
    // Utility functions
    std::string loadShaderFromFile(const char* filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return "";
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    GLuint compileShader(const std::string& source, GLenum shaderType, const char* shaderName) {
        GLuint shader = glCreateShader(shaderType);
        const char* sourcePtr = source.c_str();
        glShaderSource(shader, 1, &sourcePtr, NULL);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "Shader compilation failed: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        
        return shader;
    }
    
    // Texture loading functions
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
        
        glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format,
                     GL_UNSIGNED_BYTE, surface->pixels);
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        SDL_FreeSurface(surface);
        
        std::cout << "Loaded texture: " << filename << " (ID: " << texture << ")" << std::endl;
        return texture;
    }
    
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
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, appleData);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        std::cout << "Created procedural apple bitmap (ID: " << texture << ")" << std::endl;
        return texture;
    }
    
    bool loadAppleTexture() {
        // Try to load apple texture from different formats
        appleTexture = loadTexture("apple.bmp"); // Try BMP first (always supported)
        if (appleTexture == 0) {
            appleTexture = loadTexture("apple.png"); // Try PNG (requires SDL2_image)
        }
        if (appleTexture == 0) {
            appleTexture = loadTexture("apple.jpg"); // Try JPG (requires SDL2_image)
        }
        if (appleTexture == 0) {
            std::cout << "No apple image found, creating procedural apple bitmap..." << std::endl;
            appleTexture = createAppleBitmap();
        }
        
        return appleTexture != 0;
    }
};

// ===== SNAKE APP PUBLIC INTERFACE =====

SnakeApp::SnakeApp() : m_impl(std::make_unique<Impl>()) {}

SnakeApp::~SnakeApp() = default;

bool SnakeApp::initialize(const AppConfig& config) {
    m_impl->config = config;
    
    if (!m_impl->initializeSDL()) return false;
    if (!m_impl->initializeOpenGL()) return false;
    if (!m_impl->initializeControllers()) return false;
    if (!m_impl->loadShaders()) return false;
    if (!m_impl->setupRenderResources()) return false;
    if (!m_impl->loadAppleTexture()) return false;
    
    m_impl->running = true;
    m_impl->currentTime = SDL_GetTicks() / 1000.0f;
    m_impl->lastFrameTime = m_impl->currentTime;
    
    std::cout << "✅ Snake Application initialized successfully" << std::endl;
    return true;
}

void SnakeApp::run() {
    if (!m_impl->running) return;
    
    while (m_impl->running) {
        m_impl->updateTimers();
        m_impl->handleSDLEvents();
        
        // Publish game tick event
        Event tickEvent;
        tickEvent.type = EventType::GAME_TICK;
        tickEvent.timestamp = m_impl->currentTime;
        tickEvent.tick.deltaTime = m_impl->deltaTime;
        tickEvent.tick.currentTime = m_impl->currentTime;
        m_impl->eventSystem->publish(tickEvent);
        
        // Publish render event
        Event renderEvent;
        renderEvent.type = EventType::GAME_RENDER;
        renderEvent.timestamp = m_impl->currentTime;
        m_impl->eventSystem->publish(renderEvent);
        
        SDL_GL_SwapWindow(m_impl->window);
    }
}

void SnakeApp::shutdown() {
    if (!m_impl->running) return;
    
    m_impl->running = false;
    m_impl->cleanup();
    std::cout << "✅ Snake Application shut down" << std::endl;
}

// Getters
EventSystem* SnakeApp::getEventSystem() { return m_impl->eventSystem.get(); }
SDL_Window* SnakeApp::getWindow() const { return m_impl->window; }
GLuint SnakeApp::getShaderProgram() const { return m_impl->shaderProgram; }
GLuint SnakeApp::getVAO() const { return m_impl->VAO; }
GLuint SnakeApp::getAppleTexture() const { return m_impl->appleTexture; }
float SnakeApp::getCurrentTime() const { return m_impl->currentTime; }
float SnakeApp::getDeltaTime() const { return m_impl->deltaTime; }
const AppConfig& SnakeApp::getConfig() const { return m_impl->config; }
int SnakeApp::getNumControllers() const { return m_impl->gameControllers.size(); }

// Uniform getters
GLint SnakeApp::getOffsetUniform() const { return m_impl->u_offset; }
GLint SnakeApp::getColorUniform() const { return m_impl->u_color; }
GLint SnakeApp::getScaleUniform() const { return m_impl->u_scale; }
GLint SnakeApp::getShapeTypeUniform() const { return m_impl->u_shape_type; }
GLint SnakeApp::getInnerRadiusUniform() const { return m_impl->u_inner_radius; }
GLint SnakeApp::getTextureUniform() const { return m_impl->u_texture; }
GLint SnakeApp::getUseTextureUniform() const { return m_impl->u_use_texture; }
GLint SnakeApp::getAspectRatioUniform() const { return m_impl->u_aspect_ratio; }



// IPC functionality
bool SnakeApp::initializeIPC() { 
    return m_impl->initializeIPCMode(); 
}
void SnakeApp::writeIPCSlot(const char* gridData, char lastButton) { 
    m_impl->writeIPCSlotData(gridData, lastButton); 
}
void SnakeApp::cleanupIPC() { 
    m_impl->cleanupIPCMode(); 
}