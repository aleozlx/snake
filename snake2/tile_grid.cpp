#include "snake_dep.h"
#include <iostream>
#include <iomanip>

// TileGrid Implementation
TileGrid::TileGrid(int width, int height) : m_width(width), m_height(height), m_grid(nullptr) {
    allocateGrid();
    clear();
    setBorder();
}

TileGrid::~TileGrid() {
    deallocateGrid();
}

void TileGrid::allocateGrid() {
    m_grid = new TileContent*[m_width];
    for (int x = 0; x < m_width; x++) {
        m_grid[x] = new TileContent[m_height];
    }
}

void TileGrid::deallocateGrid() {
    if (m_grid) {
        for (int x = 0; x < m_width; x++) {
            delete[] m_grid[x];
        }
        delete[] m_grid;
        m_grid = nullptr;
    }
}

void TileGrid::clear() {
    if (!m_grid) return;
    
    for (int x = 0; x < m_width; x++) {
        for (int y = 0; y < m_height; y++) {
            m_grid[x][y] = TileContent::EMPTY;
        }
    }
}

void TileGrid::setBorder() {
    if (!m_grid) return;
    
    for (int x = 0; x < m_width; x++) {
        for (int y = 0; y < m_height; y++) {
            if (x == 0 || x == m_width - 1 || y == 0 || y == m_height - 1) {
                m_grid[x][y] = TileContent::BORDER;
            }
        }
    }
}

TileContent TileGrid::getTile(int x, int y) const {
    if (!isValidPosition(x, y)) return TileContent::BORDER;
    return m_grid[x][y];
}

void TileGrid::setTile(int x, int y, TileContent content) {
    if (isValidPosition(x, y)) {
        m_grid[x][y] = content;
    }
}

bool TileGrid::isOccupied(int x, int y) const {
    if (!isValidPosition(x, y)) return true;
    TileContent tile = m_grid[x][y];
    return tile != TileContent::EMPTY && tile != TileContent::FOOD;
}

bool TileGrid::isValidPosition(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

void TileGrid::updateFromGameState(const std::vector<Snake>& playerSnakes, 
                                 const std::vector<Snake>& aiSnakes,
                                 const Point& food, 
                                 bool pacmanActive, const Point& pacman) {
    // Clear all non-border tiles
    for (int x = 0; x < m_width; x++) {
        for (int y = 0; y < m_height; y++) {
            if (m_grid[x][y] != TileContent::BORDER) {
                m_grid[x][y] = TileContent::EMPTY;
            }
        }
    }
    
    // Place food
    if (isValidPosition(food.x, food.y)) {
        setTile(food.x, food.y, TileContent::FOOD);
    }
    
    // Place pacman if active
    if (pacmanActive && isValidPosition(pacman.x, pacman.y)) {
        setTile(pacman.x, pacman.y, TileContent::PACMAN);
    }
    
    // Place player snakes (body first, then heads to overwrite)
    for (const auto& snake : playerSnakes) {
        for (size_t i = 0; i < snake.body.size(); i++) {
            const Point& segment = snake.body[i];
            if (isValidPosition(segment.x, segment.y)) {
                if (i == 0) {
                    setTile(segment.x, segment.y, TileContent::SNAKE_HEAD);
                } else {
                    setTile(segment.x, segment.y, TileContent::SNAKE_BODY);
                }
            }
        }
    }
    
    // Place AI snakes (body first, then heads to overwrite)
    for (const auto& aiSnake : aiSnakes) {
        for (size_t i = 0; i < aiSnake.body.size(); i++) {
            const Point& segment = aiSnake.body[i];
            if (isValidPosition(segment.x, segment.y)) {
                if (i == 0) {
                    setTile(segment.x, segment.y, TileContent::AI_SNAKE_HEAD);
                } else {
                    setTile(segment.x, segment.y, TileContent::AI_SNAKE_BODY);
                }
            }
        }
    }
}

void TileGrid::createIPCGrid(char* gridData) const {
    // Initialize with spaces (empty)
    for (int i = 0; i < m_width * m_height; i++) {
        gridData[i] = ' ';
    }
    
    // Convert tile grid to IPC format
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            int index = y * m_width + x;
            gridData[index] = tileToIPCChar(m_grid[x][y], x, y);
        }
    }
}

char TileGrid::tileToIPCChar(TileContent tile, int x, int y) const {
    switch (tile) {
        case TileContent::EMPTY:
            return ' ';
        case TileContent::BORDER:
            // Corner markers with special characters
            if (x == 0 && y == 0) return 'Y';  // Yellow corner (bottom-left)
            if (x == m_width - 1 && y == 0) return 'C';  // Cyan corner (bottom-right)
            if (x == 0 && y == m_height - 1) return 'M';  // Magenta corner (top-left)
            if (x == m_width - 1 && y == m_height - 1) return 'W';  // White corner (top-right)
            return '#';  // Regular border
        case TileContent::SNAKE_HEAD:
            return 'S';  // Snake head
        case TileContent::SNAKE_BODY:
            return 's';  // Snake body
        case TileContent::AI_SNAKE_HEAD:
            return 'I';  // AI snake head
        case TileContent::AI_SNAKE_BODY:
            return 'i';  // AI snake body
        case TileContent::PACMAN:
            return 'P';  // Pacman
        case TileContent::FOOD:
            return 'F';  // Food
        default:
            return '?';  // Unknown
    }
}

bool TileGrid::isPathBlocked(const Point& pos) const {
    return isOccupied(pos.x, pos.y);
}

void TileGrid::debugPrint() const {
    if (!m_grid) {
        std::cout << "TileGrid is null!" << std::endl;
        return;
    }
    
    std::cout << "TileGrid (" << m_width << "x" << m_height << "):" << std::endl;
    for (int y = m_height - 1; y >= 0; y--) {  // Print top to bottom
        std::cout << std::setw(2) << y << ": ";
        for (int x = 0; x < m_width; x++) {
            char c = tileToIPCChar(m_grid[x][y], x, y);
            std::cout << c;
        }
        std::cout << std::endl;
    }
    
    // Print x-axis labels
    std::cout << "    ";
    for (int x = 0; x < m_width; x++) {
        std::cout << (x % 10);
    }
    std::cout << std::endl;
} 