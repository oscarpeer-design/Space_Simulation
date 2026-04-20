#pragma once
// Declaration-only header. Implementations live in WindowDrawing.cpp.

#ifdef _WIN32
#include <windows.h>
#endif

#include <unordered_map>

// Simple 2D coordinate for projected points
struct Coordinate {
	int x = 0;
	int y = 0;

	Coordinate() = default;

	Coordinate(int cx, int cy) {
		x = cx;
		y = cy;
	}

	~Coordinate() {}
};

// Representation of orbital body
struct OrbitalBodyRepresentation {
	Coordinate pointOnScreen;
	int index = 1;
	int radius = 1;
	INT8 red = 255;
	INT8 green = 255;
	INT8 blue = 255;
	
	OrbitalBodyRepresentation() = default;

	OrbitalBodyRepresentation(Coordinate coord, int pIndex, int pRadius, INT8 cred, INT8 cgreen, INT8 cblue) {
		pointOnScreen = coord;
		index = pIndex;
		radius = pRadius;
		red = cred;
		green = cgreen;
		blue = cblue;
	}

	~OrbitalBodyRepresentation() {}
};

// Forward declaration to avoid including Physics.h in this header and creating include cycles.
struct Point;

// Projects a 3D Point into 2D screen coordinates.
// - fov is in degrees.
// - screenWidth and screenHeight are in pixels.
Coordinate Project3DTo2D(const Point &p, int fov, int screenWidth, int screenHeight);

int DrawWindow(int width, int height);