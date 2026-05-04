#pragma once
// Declaration-only header. Implementations live in WindowDrawing.cpp.

#ifdef _WIN32
#include <windows.h>
#endif

#include <vector>
#include <unordered_map>

const int refreshRate = 16 * 100; // Start a timer to periodically invalidate the window (example: ~60Hz => 16ms)
const int maxRefreshRateFactor = 10; //This is the maximum number of times a velocity can be higher than the refresh rate

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

// Lighting parameters used for per-pixel shading
struct LightingBuffer {
	double ambient = 0.15;          // ambient light factor
	double diffuseStrength = 0.85;  // diffuse contribution scale
	double lx = -0.4;               // light direction x
	double ly = -0.3;               // light direction y
	double lz = 0.9;                // light direction z

	LightingBuffer() = default;
	LightingBuffer(double a, double d, double _lx, double _ly, double _lz)
		: ambient(a), diffuseStrength(d), lx(_lx), ly(_ly), lz(_lz) {}
};

// Representation of orbital body
struct OrbitalBodyRepresentation {
	Coordinate coordOnScreen;
	int index = 1;
	int radius = 1;
	RGBBuffer colourBuffer;
	
	OrbitalBodyRepresentation() = default;

	OrbitalBodyRepresentation(Coordinate coord, int pIndex, int pRadius, RGBBuffer buffer) {
		coordOnScreen = coord;
		index = pIndex;
		radius = pRadius;
		colourBuffer = buffer;
	}

	~OrbitalBodyRepresentation() {}
};

// Representation of planet
struct PlanetRepresentation {
	OrbitalBodyRepresentation planetaryBody;
	std::vector<OrbitalBodyRepresentation> moons;

	bool hasRings = false;
	UINT8 ringThickness = 0;
	RGBBuffer ringColourBuffer;
	// default constructor
	PlanetRepresentation() = default;

	// planet with no moons or rings
	PlanetRepresentation(OrbitalBodyRepresentation body) {
		planetaryBody = body;
		hasRings = false;
	}

	// planet with only moons
	PlanetRepresentation(OrbitalBodyRepresentation body, std::vector<OrbitalBodyRepresentation> moonList) {
		planetaryBody = body;
		moons = moonList;
		hasRings = false;
	}

	// planet with only rings
	PlanetRepresentation(OrbitalBodyRepresentation body, UINT8 thickness, RGBBuffer ringBuffer) {
		planetaryBody = body;
		ringThickness = thickness;
		ringColourBuffer = ringBuffer;
		hasRings = true;
	}

	// planet with moons and rings
	PlanetRepresentation(OrbitalBodyRepresentation body, std::vector<OrbitalBodyRepresentation> moonList, UINT8 thickness, RGBBuffer ringBuffer) {
		planetaryBody = body;
		moons = moonList;
		ringThickness = thickness;
		ringColourBuffer = ringBuffer;
		hasRings = true;
	}

	~PlanetRepresentation() {}
};

// Forward declaration to avoid including Physics.h in this header and creating include cycles.
struct Point;

// Projects a 3D Point into 2D screen coordinates.
// - fov is in degrees.
// - screenWidth and screenHeight are in pixels.
Coordinate Project3DTo2D(const Point &p, int fov, int screenWidth, int screenHeight);

int DrawWindow(int width, int height);