#pragma once
// Declaration-only header. Implementations live in WindowDrawing.cpp.

#ifdef _WIN32
#include <windows.h>
#endif

// Simple 2D coordinate for projected points
struct Coordinate {
	int x = 0;
	int y = 0;
};

// Forward declaration to avoid including Physics.h in this header and creating include cycles.
struct Point;

// Projects a 3D Point into 2D screen coordinates.
// - fov is in degrees.
// - screenWidth and screenHeight are in pixels.
Coordinate Project3DTo2D(const Point &p, int fov, int screenWidth, int screenHeight);

int DrawWindow(int width, int height);