#pragma once

#include "snake_dep.h"
#include <vector>

// A* pathfinding structures and functions
struct AStarNode {
  Point pos;
  int g_cost; // Distance from start
  int h_cost; // Heuristic distance to target
  int f_cost; // g_cost + h_cost
  Point parent;
  
  AStarNode(Point position, int g, int h, Point par) 
    : pos(position), g_cost(g), h_cost(h), f_cost(g + h), parent(par) {}
  
  AStarNode() : pos(Point(-1, -1)), g_cost(0), h_cost(0), f_cost(0), parent(Point(-1, -1)) {}
};

// Callback function type for checking if a position is occupied
typedef bool (*IsPositionOccupiedCallback)(const Point& pos, void* context);

// A* pathfinding helper functions
int manhattanDistance(const Point& a, const Point& b);

std::vector<Point> getNeighbors(const Point& pos, int gridWidth, int gridHeight, 
                                IsPositionOccupiedCallback isOccupied, void* context);

// A* pathfinding algorithm
std::vector<Point> findPathAStar(const Point& start, const Point& goal, 
                                int gridWidth, int gridHeight,
                                IsPositionOccupiedCallback isOccupied, void* context);

// Naive pathfinding direction calculation (greedy approach)
Point calculateNaivePathDirection(const Point& start, const Point& target, 
                                 int gridWidth, int gridHeight,
                                 IsPositionOccupiedCallback isOccupied, void* context,
                                 const Point& currentDirection);

// A* pathfinding direction calculation
Point calculateAStarPathDirection(const Point& start, const Point& target, 
                                 int gridWidth, int gridHeight,
                                 IsPositionOccupiedCallback isOccupied, void* context);

// Greedy pathfinding with axis prioritization (prioritizes larger distance axis first)
Point calculateGreedyAxisPathDirection(const Point& start, const Point& target, 
                                      int gridWidth, int gridHeight,
                                      IsPositionOccupiedCallback isOccupied, void* context); 