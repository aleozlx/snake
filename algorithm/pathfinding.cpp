#include "pathfinding.h"
#include <algorithm>
#include <cmath>

// A* pathfinding helper functions
int manhattanDistance(const Point& a, const Point& b) {
  return abs(a.x - b.x) + abs(a.y - b.y);
}

std::vector<Point> getNeighbors(const Point& pos, int gridWidth, int gridHeight, 
                                IsPositionOccupiedCallback isOccupied, void* context) {
  std::vector<Point> neighbors;
  std::vector<Point> directions = {Point(0, 1), Point(0, -1), Point(1, 0), Point(-1, 0)};
  
  for (const auto& dir : directions) {
    Point neighbor = Point(pos.x + dir.x, pos.y + dir.y);
    if (!isOccupied(neighbor, context)) {
      neighbors.push_back(neighbor);
    }
  }
  
  return neighbors;
}

// A* pathfinding algorithm
std::vector<Point> findPathAStar(const Point& start, const Point& goal, 
                                int gridWidth, int gridHeight,
                                IsPositionOccupiedCallback isOccupied, void* context) {
  std::vector<Point> path;
  
  // Early exit if goal is occupied or unreachable
  if (isOccupied(goal, context) || start == goal) {
    return path;
  }
  
  std::vector<AStarNode> openList;
  std::vector<AStarNode> closedList; // Store actual nodes for path reconstruction
  
  // Add starting node to open list
  AStarNode startNode(start, 0, manhattanDistance(start, goal), Point(-1, -1));
  openList.push_back(startNode);
  
  while (!openList.empty()) {
    // Find node with lowest f_cost
    int currentIndex = 0;
    for (int i = 1; i < openList.size(); i++) {
      if (openList[i].f_cost < openList[currentIndex].f_cost ||
          (openList[i].f_cost == openList[currentIndex].f_cost && openList[i].h_cost < openList[currentIndex].h_cost)) {
        currentIndex = i;
      }
    }
    
    AStarNode currentNode = openList[currentIndex];
    openList.erase(openList.begin() + currentIndex);
    closedList.push_back(currentNode);
    
    // Check if we reached the goal
    if (currentNode.pos == goal) {
      // Reconstruct path by backtracking through parents
      path.push_back(goal);
      Point current = currentNode.parent;
      
      while (!(current == Point(-1, -1)) && !(current == start)) {
        path.push_back(current);
        
        // Find parent of current node in closed list
        bool foundParent = false;
        for (const auto& node : closedList) {
          if (node.pos == current) {
            current = node.parent;
            foundParent = true;
            break;
          }
        }
        
        if (!foundParent) break;
      }
      
      if (!(current == Point(-1, -1))) {
        path.push_back(start);
      }
      
      // Reverse path to get start->goal direction
      std::reverse(path.begin(), path.end());
      return path;
    }
    
    // Check neighbors
    std::vector<Point> neighbors = getNeighbors(currentNode.pos, gridWidth, gridHeight, isOccupied, context);
    for (const auto& neighbor : neighbors) {
      // Skip if in closed list
      bool inClosedList = false;
      for (const auto& closedNode : closedList) {
        if (neighbor == closedNode.pos) {
          inClosedList = true;
          break;
        }
      }
      if (inClosedList) continue;
      
      int newG = currentNode.g_cost + 1;
      int newH = manhattanDistance(neighbor, goal);
      
      // Check if neighbor is already in open list with better cost
      bool inOpenList = false;
      for (auto& openNode : openList) {
        if (openNode.pos == neighbor) {
          if (newG < openNode.g_cost) {
            openNode.g_cost = newG;
            openNode.f_cost = newG + openNode.h_cost;
            openNode.parent = currentNode.pos;
          }
          inOpenList = true;
          break;
        }
      }
      
      // Add to open list if not present
      if (!inOpenList) {
        AStarNode newNode(neighbor, newG, newH, currentNode.pos);
        openList.push_back(newNode);
      }
    }
    
    // Prevent infinite loops by limiting search depth
    if (closedList.size() > (gridWidth * gridHeight) / 2) {
      break;
    }
  }
  
  return path; // Empty path if no solution found
}

// Naive pathfinding direction calculation (greedy approach)
Point calculateNaivePathDirection(const Point& start, const Point& target, 
                                 int gridWidth, int gridHeight,
                                 IsPositionOccupiedCallback isOccupied, void* context,
                                 const Point& currentDirection) {
  // Calculate direction toward target
  int dx = target.x - start.x;
  int dy = target.y - start.y;

  // Try to move toward target, prioritizing the axis with greater distance
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
    Point newHead = Point(start.x + move.x, start.y + move.y);
    if (!isOccupied(newHead, context)) {
      return move;
    }
  }

  // If no preferred move is valid, try any valid move (avoid going backwards)
  std::vector<Point> allMoves = {Point(1, 0), Point(-1, 0), Point(0, 1), Point(0, -1)};
  Point oppositeDir = Point(-currentDirection.x, -currentDirection.y);
  
  for (const auto &move : allMoves) {
    // Don't go backwards unless absolutely necessary
    if (move == oppositeDir) continue;
    
    Point newHead = Point(start.x + move.x, start.y + move.y);
    if (!isOccupied(newHead, context)) {
      return move;
    }
  }

  // If no forward moves are valid, try going backwards as last resort
  Point newHead = Point(start.x + oppositeDir.x, start.y + oppositeDir.y);
  if (!isOccupied(newHead, context)) {
    return oppositeDir;
  }

  // No valid moves - maintain current direction (will be blocked)
  return currentDirection;
}

// A* pathfinding direction calculation
Point calculateAStarPathDirection(const Point& start, const Point& target, 
                                 int gridWidth, int gridHeight,
                                 IsPositionOccupiedCallback isOccupied, void* context) {
  // Find path using A*
  std::vector<Point> path = findPathAStar(start, target, gridWidth, gridHeight, isOccupied, context);
  
  if (path.size() >= 2) {
    // Get next step in path
    Point nextStep = path[1]; // path[0] is current position
    Point direction = Point(nextStep.x - start.x, nextStep.y - start.y);
    
    // Validate the move
    Point newHead = Point(start.x + direction.x, start.y + direction.y);
    if (!isOccupied(newHead, context)) {
      return direction;
    }
  }
  
  // Fallback to naive algorithm if A* fails
  return calculateNaivePathDirection(start, target, gridWidth, gridHeight, isOccupied, context, Point(0, 0));
} 