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

// Struct to store RGB Colours
struct RGBBuffer {
	UINT8 red = 255;
	UINT8 green = 255;
	UINT8 blue = 255;

	RGBBuffer() = default;

	RGBBuffer(UINT8 r, UINT8 g, UINT8 b) {
		red = r;
		green = g;
		blue = b;
	}

	~RGBBuffer() {}
};

// Representation of orbital body
struct OrbitalBodyRepresentation {
	Coordinate coordOnScreen;
	int index = 1;
	int radius = 1;
	bool hasRings = false;
	UINT8 ringThickness = 0;

	RGBBuffer colourBuffer;
	RGBBuffer ringColourBuffer;
	
	OrbitalBodyRepresentation() = default;

	OrbitalBodyRepresentation(Coordinate coord, int pIndex, int pRadius, RGBBuffer buffer) {
		coordOnScreen = coord;
		index = pIndex;
		radius = pRadius;
		colourBuffer = buffer;
		hasRings = false;
	}

	OrbitalBodyRepresentation(Coordinate coord, int pIndex, int pRadius, RGBBuffer buffer, RGBBuffer ringColour, UINT8 thickness) {
		coordOnScreen = coord;
		index = pIndex;
		radius = pRadius;
		colourBuffer = buffer;
		hasRings = true;
		ringColourBuffer = ringColour;
		ringThickness = thickness;
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