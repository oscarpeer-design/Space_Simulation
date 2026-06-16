#ifdef _WIN32
#include <windows.h>
#endif

#include <vector>
#include <unordered_map>
#include <string>

const int refreshRate = 16; // ms between frames (~60 FPS)
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
		: ambient(a), diffuseStrength(d), lx(_lx), ly(_ly), lz(_lz) {
	}
};

// Representation of orbital body
struct OrbitalBodyRepresentation {
	Coordinate coordOnScreen;
	int index = 1;
	int radius = 1;               // visual radius in pixels
	RGBBuffer colourBuffer;

	// New metadata for hover information and basic physics display
	std::string name;             // display name
	double massKg = 0.0;          // mass in kilograms (for display)
	double velocityMs = 0.0;      // orbital speed in m/s (for display)

	// Physical radius (meters) used for physics calculations — separate from visual `radius`.
	double physicalRadiusMeters = 0.0;

	// default constructor
	OrbitalBodyRepresentation() = default;

	// main constructor (keeps backward compatibility via defaults)
	OrbitalBodyRepresentation(Coordinate coord, int pIndex, int pRadius, RGBBuffer buffer,
		const std::string& pname = std::string(), double pmass = 0.0, double pvelocity = 0.0, double pPhysicalRadius = 0.0) {
		coordOnScreen = coord;
		index = pIndex;
		radius = pRadius;
		colourBuffer = buffer;
		name = pname;
		massKg = pmass;
		velocityMs = pvelocity;
		physicalRadiusMeters = pPhysicalRadius;
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

// Representation of a sun; a central star around which bodies revolve
struct SunRepresentation {
	OrbitalBodyRepresentation sunBody;
	UINT8 thickness = 1;

	// default constructor
	SunRepresentation() = default;

	// constructor with default orbital body, and a thickness level which represents a ring of light
	SunRepresentation(OrbitalBodyRepresentation body, UINT8 ambientThickness) {
		sunBody = body;
		thickness = ambientThickness;
	}

	// constructor with no orbital body
	SunRepresentation(Coordinate coordOnScreen, int index, int radius, RGBBuffer colourBuffer, UINT8 ambientThickness)
	{
		sunBody = OrbitalBodyRepresentation(coordOnScreen, index, radius, colourBuffer);
		thickness = ambientThickness;
	}

	~SunRepresentation() {}
};

// Representation of a background star in a fixed position
struct StarRepresentation {
	Coordinate coordOnScreen;
	RGBBuffer colourBuffer;

	// default constructor
	StarRepresentation() = default;

	// constructor for default (white) star
	StarRepresentation(Coordinate coord) {
		coordOnScreen = coord;
	}

	// constructor for colour star
	StarRepresentation(Coordinate coord, RGBBuffer buffer) {
		coordOnScreen = coord;
		colourBuffer = buffer;
	}

	~StarRepresentation() {}
};

// Forward declaration to avoid including Physics.h in this header and creating include cycles.
struct Point;

// Projects a 3D Point into 2D screen coordinates.
// - fov is in degrees.
// - screenWidth and screenHeight are in pixels.
Coordinate Project3DTo2D(const Point& p, int fov, int screenWidth, int screenHeight);

int DrawWindow(int width, int height);

// --- New: configuration structs to replace magic literals ---
// Planet definition used to create the scene; preserves values that were previously raw literals.
struct PlanetConfig {
	std::string name;
	double semiMajorAxisMeters = 0.0;
	double massKg = 0.0;
	int visualRadiusPx = 1;
	RGBBuffer colour = RGBBuffer();
	double orbitRatio = 0.0;
	double initialAngle = 0.0;
	double angularSpeed = 0.0;
	int ellipseFactor = 0;
	bool rings = false;
	UINT8 ringThickness = 0;
	RGBBuffer ringColour = RGBBuffer();

	PlanetConfig() = default;

	PlanetConfig(const std::string& n, double semiMajor, double mass, int vpx, RGBBuffer col, double ratio, double initA, double angSpd, int elip = 0, bool r = false, UINT8 rt = 0, RGBBuffer rc = RGBBuffer())
		: name(n), semiMajorAxisMeters(semiMajor), massKg(mass), visualRadiusPx(vpx), colour(col), orbitRatio(ratio),
		initialAngle(initA), angularSpeed(angSpd), ellipseFactor(elip), rings(r), ringThickness(rt), ringColour(rc)
	{
	}
};

// Solar system configuration struct to centralize values previously scattered as literals.
struct SolarSystemConfig {
	// Sun base parameters (visual and physics)
	SunRepresentation sunTemplate;         // sun representation (position will be set at runtime)
	double sunMassKg = 1.9885e30;         // default central mass
	double sunPhysicalRadiusMeters = 6.9634e8; // solar mean radius

	// Lighting and rendering factors (replace magic constants)
	double outerAmbientFactor = 0.45;
	double outerDiffuseFactor = 0.9;
	double coreAmbientFactor = 0.9;
	double coreDiffuseFactor = 1.05;
	double sunHaloCoefficient = 1.2;

	// Orbit layout base ratio (fraction of min client dimension used as base orbit radius)
	double baseOrbitRatio = 0.45;

	// Orbit and UI colours
	RGBBuffer orbitColour = RGBBuffer(160, 160, 220);

	// Planet entries (previously created inline with raw numbers)
	std::vector<PlanetConfig> planets;

	SolarSystemConfig() = default;
};

// Extern instance that the implementation will define and populate.
extern SolarSystemConfig g_solarConfig;