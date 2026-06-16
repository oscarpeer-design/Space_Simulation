#include "WindowDrawing.h"
#include "Physics.h"

#include <windows.h>
#include <windowsx.h> // Required for GET_X_LPARAM and GET_Y_LPARAM
#include <cstring> // Required for strncpy_s
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <cwchar> // for wcscpy_s when UNICODE build

// Definition of the SolarSystemConfig declared in the header.
// Centralizes the previously scattered magic literals.
SolarSystemConfig g_solarConfig = []()->SolarSystemConfig {
	SolarSystemConfig cfg;

	// Sun visual template (position zeroed; will be positioned at runtime)
	// Previously: SunRepresentation(Coordinate(cx, cy), 0, 30, RGBBuffer(240, 200, 20), 6);
	cfg.sunTemplate = SunRepresentation(Coordinate(0, 0), 0, 30, RGBBuffer(240, 200, 20), 6);

	// physics
	cfg.sunMassKg = 1.9885e30;
	cfg.sunPhysicalRadiusMeters = 6.9634e8;

	// lighting & rendering factors (these replaced ad-hoc multipliers)
	cfg.outerAmbientFactor = 0.45;
	cfg.outerDiffuseFactor = 0.9;
	cfg.coreAmbientFactor = 0.9;
	cfg.coreDiffuseFactor = 1.05;
	cfg.sunHaloCoefficient = 1.2;

	// base orbit layout ratio used in RecomputeOrbitRadii
	cfg.baseOrbitRatio = 0.45;

	// orbit path colour
	cfg.orbitColour = RGBBuffer(160, 160, 220);

	// Planets (previous raw addPlanet calls are captured here)
	// name, semi-major axis (m), mass (kg), visual size px, colour, orbit ratio, init angle, ang speed (rad/s), ellipse factor, rings?, ringThickness, ringColour
	cfg.planets.push_back(PlanetConfig("Mercury", 5.79e10, 3.3011e23, 4, RGBBuffer(180, 180, 180), 0.13, 0.0, 1.2, 0));
	cfg.planets.push_back(PlanetConfig("Venus", 1.082e11, 4.8675e24, 7, RGBBuffer(210, 160, 100), 0.20, 0.9, 0.8, 0));
	cfg.planets.push_back(PlanetConfig("Earth", 1.496e11, 5.97237e24, 8, RGBBuffer(100, 140, 240), 0.28, 2.0, 0.65, 0));
	cfg.planets.push_back(PlanetConfig("Mars", 2.279e11, 6.4171e23, 6, RGBBuffer(210, 100, 80), 0.36, 1.5, 0.48, 0));
	cfg.planets.push_back(PlanetConfig("Jupiter", 7.785e11, 1.8982e27, 14, RGBBuffer(200, 160, 120), 0.49, 0.7, 0.35, 0));
	cfg.planets.push_back(PlanetConfig("Saturn", 1.433e12, 5.6834e26, 12, RGBBuffer(200, 180, 140), 0.64, 0.4, 0.22, 0, true, 3, RGBBuffer(200, 200, 180)));
	cfg.planets.push_back(PlanetConfig("Uranus", 2.872e12, 8.6810e25, 11, RGBBuffer(160, 200, 220), 0.78, 1.1, 0.18, 0, true, 3, RGBBuffer(150, 220, 240)));
	cfg.planets.push_back(PlanetConfig("Neptune", 4.495e12, 1.02413e26, 11, RGBBuffer(120, 140, 220), 0.90, 1.9, 0.15, 0));

	return cfg;
}();

// Projects a 3D Point into 2D screen coordinates using a simple pinhole camera model.
// Algorithm based on the provided Python snippet: normalize vector, avoid z<=0, compute focal length from FOV.
Coordinate Project3DTo2D(const Point& p, int fov, int screenWidth, int screenHeight)
{
	// Copy components
	double vx = p.x;
	double vy = p.y;
	double vz = p.z;
	// Normalize vector to avoid scale-dependence
	double len = std::sqrt(vx * vx + vy * vy + vz * vz);
	if (len == 0.0) {
		// avoid division by zero; treat as small forward vector
		len = 1e-6;
	}
	vx /= len;
	vy /= len;
	vz /= len;
	// If point is behind or at the camera plane, push it slightly forward to avoid division blow-up.
	if (vz <= 0.0) {
		vz = 1e-3;
	}
	// Focal length from field of view (fov in degrees)
	double fovRad = (static_cast<double>(fov) / 2.0) * (Pi / 180.0); // Pi from Physics.h
	double focalLength = (static_cast<double>(screenWidth) / 2.0) / std::tan(fovRad);
	/*
	Use 3D-2D projection formula:
	x = (v.x / v.z) * f + w/2
	y = (v.y / v.z) * f + h/2
	*/
	double px = (vx / vz) * focalLength + static_cast<double>(screenWidth) / 2.0;
	double py = (vy / vz) * focalLength + static_cast<double>(screenHeight) / 2.0;
	// Return the projected coordinate
	return Coordinate{ static_cast<int>(std::round(px)), static_cast<int>(std::round(py)) };
}

// Convert radius from scientific notation to pixels
static int RadiusInPixels(double radius, double conversionRatio, int screenWidth, int screenHeight) {
	// each pixel on screen represents a converstion ratio. We choose 200km for most cases - the moon is approximately 1800 km
	// TODO: consider non-scientific notation (km instead of metres)
	double radiusConverted = radius / conversionRatio;
	double screenRatio = static_cast<double>(screenWidth) / static_cast<double>(screenHeight);
	int radiusPixels = static_cast<int>(radiusConverted * screenRatio);
	return radiusPixels;
}

// Draw helpers (pixel, line, circle, ring) - kept from original implementation but cleaned up.
static void DrawPixelInClient(HDC hdc, Coordinate coord, RGBBuffer buffer) {
	SetPixel(hdc, coord.x, coord.y, RGB(buffer.red, buffer.green, buffer.blue));
}

static void DrawLineInClient(HDC hdc, Coordinate start, Coordinate end, RGBBuffer buffer, UINT8 thickness) {
	HPEN hpen = CreatePen(PS_SOLID, thickness, RGB(buffer.red, buffer.green, buffer.blue));
	HPEN oldPen = (HPEN)SelectObject(hdc, hpen);
	MoveToEx(hdc, start.x, start.y, NULL);
	LineTo(hdc, end.x, end.y);
	SelectObject(hdc, oldPen);
	DeleteObject(hpen);
}

static void DrawCircleInClient(HDC hdc, Coordinate centre, int radius, RGBBuffer buffer, const LightingBuffer& light)
{
	if (radius <= 0) return;

	double lx = light.lx;
	double ly = light.ly;
	double lz = light.lz;
	double llen = std::sqrt(lx * lx + ly * ly + lz * lz);
	if (llen == 0.0) llen = 1e-6;
	lx /= llen; ly /= llen; lz /= llen;

	int y0 = centre.y - radius;
	int y1 = centre.y + radius;
	for (int py = y0; py <= y1; ++py) {
		int dy = py - centre.y;
		double dy2 = static_cast<double>(dy) * static_cast<double>(dy);
		double r2 = static_cast<double>(radius) * static_cast<double>(radius);
		if (dy2 > r2) continue;
		int halfWidth = static_cast<int>(std::floor(std::sqrt(r2 - dy2)));
		int x0 = centre.x - halfWidth;
		int x1 = centre.x + halfWidth;
		for (int px = x0; px <= x1; ++px) {
			int dx = px - centre.x;
			double nx = static_cast<double>(dx) / static_cast<double>(radius);
			double ny = static_cast<double>(dy) / static_cast<double>(radius);
			double nz2 = 1.0 - (nx * nx + ny * ny);
			if (nz2 < 0.0) nz2 = 0.0;
			double nz = std::sqrt(nz2);
			double dot = nx * lx + ny * ly + nz * lz;
			double lambert = (std::max)(0.0, dot);
			double intensity = light.ambient + light.diffuseStrength * lambert;
			intensity = (std::min)(1.0, (std::max)(0.0, intensity));
			int r = static_cast<int>(std::round(buffer.red * intensity));
			int g = static_cast<int>(std::round(buffer.green * intensity));
			int b = static_cast<int>(std::round(buffer.blue * intensity));
			r = (std::min)(255, (std::max)(0, r));
			g = (std::min)(255, (std::max)(0, g));
			b = (std::min)(255, (std::max)(0, b));
			SetPixel(hdc, px, py, RGB(r, g, b));
		}
	}
}

// Draw a shaded filled semicircle oriented by (dirx,diry).
static void DrawSemiCircleInClient(HDC hdc, Coordinate centre, int radius, RGBBuffer buffer, const LightingBuffer& light, double dirx, double diry, bool frontHalf)
{
	if (radius <= 0) return;

	double lx = light.lx;
	double ly = light.ly;
	double lz = light.lz;
	double llen = std::sqrt(lx * lx + ly * ly + lz * lz);
	if (llen == 0.0) llen = 1e-6;
	lx /= llen; ly /= llen; lz /= llen;

	double dlen = std::sqrt(dirx * dirx + diry * diry);
	if (dlen == 0.0) { dirx = 1.0; diry = 0.0; dlen = 1.0; }
	dirx /= dlen; diry /= dlen;

	int y0 = centre.y - radius;
	int y1 = centre.y + radius;
	for (int py = y0; py <= y1; ++py) {
		int dy = py - centre.y;
		double dy2 = static_cast<double>(dy) * static_cast<double>(dy);
		double r2 = static_cast<double>(radius) * static_cast<double>(radius);
		if (dy2 > r2) continue;
		int halfWidth = static_cast<int>(std::floor(std::sqrt(r2 - dy2)));
		int x0 = centre.x - halfWidth;
		int x1 = centre.x + halfWidth;
		for (int px = x0; px <= x1; ++px) {
			int dx = px - centre.x;

			// orientation test: only draw the requested half of the circle
			double proj = static_cast<double>(dx) * dirx + static_cast<double>(dy) * diry;
			if (frontHalf) {
				if (proj < 0.0) continue;
			}
			else {
				if (proj > 0.0) continue;
			}

			double nx = static_cast<double>(dx) / static_cast<double>(radius);
			double ny = static_cast<double>(dy) / static_cast<double>(radius);
			double nz2 = 1.0 - (nx * nx + ny * ny);
			if (nz2 < 0.0) nz2 = 0.0;
			double nz = std::sqrt(nz2);

			double dot = nx * lx + ny * ly + nz * lz;
			double lambert = (std::max)(0.0, dot);

			double intensity = light.ambient + light.diffuseStrength * lambert;
			intensity = (std::min)(1.0, (std::max)(0.0, intensity));

			int r = static_cast<int>(std::round(buffer.red * intensity));
			int g = static_cast<int>(std::round(buffer.green * intensity));
			int b = static_cast<int>(std::round(buffer.blue * intensity));

			r = (std::min)(255, (std::max)(0, r));
			g = (std::min)(255, (std::max)(0, g));
		 b = (std::min)(255, (std::max)(0, b));

			SetPixel(hdc, px, py, RGB(r, g, b));
		}
	}
}

// Draw ring (kept simple and robust)
static void DrawRingInClient(HDC hdc, Coordinate start, Coordinate end, UINT8 thickness, RGBBuffer buffer, const LightingBuffer& light, bool semicircleCaps = false, bool halfSizeCaps = false)
{
	if (thickness == 0) return;

	double lx = light.lx;
	double ly = light.ly;
	double lz = light.lz;
	double llen = std::sqrt(lx * lx + ly * ly + lz * lz);
	if (llen == 0.0) llen = 1e-6;
	lx /= llen; ly /= llen; lz /= llen;

	double dx = static_cast<double>(end.x - start.x);
	double dy = static_cast<double>(end.y - start.y);
	double segLen = std::sqrt(dx * dx + dy * dy);
	if (segLen < 1e-6) {
		// Degenerate: draw a vertical band centered at start
		int half = (thickness + 1) / 2;
		for (int oy = -half; oy <= half; ++oy) {
			// approximate normal varying across thickness
			double offsetNorm = (half == 0) ? 0.0 : static_cast<double>(oy) / static_cast<double>(half);
			double nx = 0.0;
			double ny = offsetNorm;
			double nlen2 = nx * nx + ny * ny;
			double nz = 0.0;
			if (nlen2 < 1.0) nz = std::sqrt(1.0 - nlen2);

			double dot = nx * lx + ny * ly + nz * lz;
			double lambert = (std::max)(0.0, dot);
			double intensity = light.ambient + light.diffuseStrength * lambert;
			intensity = (std::min)(1.0, (std::max)(0.0, intensity));

			int r = static_cast<int>(std::round(buffer.red * intensity));
			int g = static_cast<int>(std::round(buffer.green * intensity));
			int b = static_cast<int>(std::round(buffer.blue * intensity));
			r = (std::min)(255, (std::max)(0, r));
			g = (std::min)(255, (std::max)(0, g));
			b = (std::min)(255, (std::max)(0, b));
			SetPixel(hdc, start.x, start.y + oy, RGB(r, g, b));
		}
		// draw rounded caps for degenerate case
		int capRadius = (thickness + 1) / 2;
		if (halfSizeCaps) capRadius = static_cast<int>(std::ceil(static_cast<double>(capRadius) * 0.5));
		if (capRadius > 0) {
			if (semicircleCaps) {
				// degenerate: use up-facing semicircle as a reasonable default (dir = 0,1)
				DrawSemiCircleInClient(hdc, start, capRadius, buffer, light, 0.0, 1.0, true);
			}
			else {
				DrawCircleInClient(hdc, start, capRadius, buffer, light);
			}
		}
		return;
	}

	// unit direction along the segment
	double dirx = dx / segLen;
	double diry = dy / segLen;
	// perpendicular unit vector (points "out" of the band)
	double perpX = -diry;
	double perpY = dirx;

	// number of samples along line - use segment length for smoothness
	int samples = static_cast<int>(std::ceil(segLen));
	if (samples < 1) samples = 1;

	double halfThickness = static_cast<double>(thickness) * 0.5;
	// iterate along segment
	for (int i = 0; i <= samples; ++i) {
		double t = static_cast<double>(i) / static_cast<double>(samples);
		double fx = static_cast<double>(start.x) + dx * t;
		double fy = static_cast<double>(start.y) + dy * t;

		// for each offset across thickness
		int intHalf = static_cast<int>(std::ceil(halfThickness));
		for (int oy = -intHalf; oy <= intHalf; ++oy) {
			// offset in pixels across band
			double offset = static_cast<double>(oy);
			// sample position
			int sx = static_cast<int>(std::round(fx + perpX * offset));
			int sy = static_cast<int>(std::round(fy + perpY * offset));

			// approximate normal for ring face-on: vary normal across thickness so edges catch light
			// map offset to [-1,1] using halfThickness
			double offsetNorm = (halfThickness > 0.0) ? (offset / halfThickness) : 0.0;
			// normal has in-plane component across perp direction and out-of-plane z component
			double nx = perpX * offsetNorm;
			double ny = perpY * offsetNorm;
			double nlen2 = nx * nx + ny * ny;
			double nz = 0.0;
			if (nlen2 < 1.0) nz = std::sqrt(1.0 - nlen2);
			else {
				// fallback normalization
				double inv = 1.0 / std::sqrt(nlen2);
				nx *= inv; ny *= inv; nz = 0.0;
			}

			double dot = nx * lx + ny * ly + nz * lz;
			double lambert = (std::max)(0.0, dot);

			double intensity = light.ambient + light.diffuseStrength * lambert;
			intensity = (std::min)(1.0, (std::max)(0.0, intensity));

			int r = static_cast<int>(std::round(buffer.red * intensity));
			int g = static_cast<int>(std::round(buffer.green * intensity));
			int b = static_cast<int>(std::round(buffer.blue * intensity));

			r = (std::min)(255, (std::max)(0, r));
			g = (std::min)(255, (std::max)(0, g));
			b = (std::min)(255, (std::max)(0, b));
			SetPixel(hdc, sx, sy, RGB(r, g, b));
		}
	}

	// Draw end caps.
	int capRadius = static_cast<int>(std::ceil(halfThickness));
	if (halfSizeCaps) capRadius = static_cast<int>(std::ceil(static_cast<double>(capRadius) * 0.5));
	if (capRadius > 0) {
		if (semicircleCaps) {
			// For the start cap, draw the semicircle on the side toward the segment (frontHalf = true).
			DrawSemiCircleInClient(hdc, start, capRadius, buffer, light, dirx, diry, true);
			// For the end cap, draw the semicircle on the side toward the segment (frontHalf = false)
			// but the orientation is the same vector; invert frontHalf so the flat edge sits against the band.
			DrawSemiCircleInClient(hdc, end, capRadius, buffer, light, dirx, diry, false);
		}
		else {
			DrawCircleInClient(hdc, start, capRadius, buffer, light);
			DrawCircleInClient(hdc, end, capRadius, buffer, light);
		}
	}
}

// Draw an orbital body on the screen
static void DrawPlanet(HDC hdc, const PlanetRepresentation& planet) {
	RGBBuffer colourBuffer = planet.planetaryBody.colourBuffer;
	Coordinate centre = planet.planetaryBody.coordOnScreen;
	int radius = planet.planetaryBody.radius;

	LightingBuffer lighting;
	DrawCircleInClient(hdc, centre, radius, colourBuffer, lighting);
	if (planet.hasRings) {
		UINT8 thickness = planet.ringThickness;
		if (thickness == 0) thickness = 1;
		// clamp ring thickness to a reasonable upper bound to avoid very thick visual rings
		const UINT8 maxRingThickness = 4;
		if (thickness > maxRingThickness) thickness = maxRingThickness;
		RGBBuffer ringColourBuffer = planet.ringColourBuffer;
		LightingBuffer ringLighting = lighting;
		// use named factors from configuration instead of magic literal
		ringLighting.ambient = lighting.ambient * 0.5;
		ringLighting.diffuseStrength = lighting.diffuseStrength * 1.25;
		int extension = (std::max)(2, static_cast<int>(thickness));
		Coordinate start = Coordinate(centre.x - radius - extension, centre.y);
		Coordinate end = Coordinate(centre.x + radius + extension, centre.y);
		// draw ring after planet so it overlays correctly
		DrawRingInClient(hdc, start, end, thickness, ringColourBuffer, ringLighting);
	}
}

// --- Simple solar-system scene state and helpers ---
static SunRepresentation g_sun;
static std::vector<PlanetRepresentation> g_planets;
static std::vector<int> g_planetOrbitRadii;   // pixels
static std::vector<double> g_planetOrbitRatios; // ratios relative to available half-min-dimension
static std::vector<double> g_planetAngles;    // radians
static std::vector<double> g_planetAngularSpeeds; // radians per second
static std::vector<int> g_planetEllipseFactors;
static bool g_sceneInitialized = false;

// Hover state for mouse-over info
static int g_hoveredPlanetIndex = -1;
static POINT g_mousePos = { 0, 0 };

// Add near the other static scene state variables:
static bool g_hoveringSun = false;

// Recompute pixel orbit radii based on current client size and stored ratios.
static void RecomputeOrbitRadii(int clientWidth, int clientHeight)
{
	if (g_planetOrbitRatios.empty()) return;
	int minDim = (std::min)(clientWidth, clientHeight);
	// reserve a small margin so orbits aren't clipped; use half-min-dimension as base radius
	// use configured base ratio instead of magic literal
	int base = static_cast<int>(std::floor(static_cast<double>(minDim) * g_solarConfig.baseOrbitRatio)); // replaced hard-coded 0.45
	if (base < 10) base = 10;
	g_planetOrbitRadii.resize(g_planetOrbitRatios.size());
	for (size_t i = 0; i < g_planetOrbitRatios.size(); ++i) {
		double ratio = g_planetOrbitRatios[i];
		int r = static_cast<int>(std::round(ratio * static_cast<double>(base)));
		if (r < 1) r = 1;
		g_planetOrbitRadii[i] = r;
	}
}

// Reposition planets' screen coordinates using current client centre and computed orbit radii.
static void RepositionPlanets(int clientWidth, int clientHeight)
{
	if (g_planets.empty() || g_planetOrbitRadii.empty()) return;
	int cx = clientWidth / 2;
	int cy = clientHeight / 2;
	Coordinate centre(cx, cy);

	// update sun centre
	g_sun.sunBody.coordOnScreen = centre;

	for (size_t i = 0; i < g_planets.size(); ++i) {
		double angle = g_planetAngles[i];
		int r = g_planetOrbitRadii[i];
		int elip = g_planetEllipseFactors[i];
		int yRadius = (elip > 1) ? (r / elip) : r;
		int x = centre.x + static_cast<int>(std::round(r * std::cos(angle)));
		int y = centre.y + static_cast<int>(std::round(yRadius * std::sin(angle)));
		g_planets[i].planetaryBody.coordOnScreen = Coordinate(x, y);
	}
}

// Simple orbit-path draw (thin outline) using sampled points.
// Improved: brighter colour and slightly larger dot size for visibility.
static void DrawOrbitPath(HDC hdc, Coordinate centre, int orbitRadius, RGBBuffer colour) {
	if (orbitRadius <= 0) return;
	const int stepDeg = 4; // sampling step; smaller => smoother orbit
	const int dotSize = 1; // draws a (2*dotSize+1)^2 block for visibility
	for (int a = 0; a < 360; a += stepDeg) {
		double rad = a * (Pi / 180.0);
		int x = centre.x + static_cast<int>(std::round(orbitRadius * std::cos(rad)));
		int y = centre.y + static_cast<int>(std::round(orbitRadius * std::sin(rad)));
		// draw small bright block to make dots more visible
		for (int oy = -dotSize; oy <= dotSize; ++oy) {
			for (int ox = -dotSize; ox <= dotSize; ++ox) {
				int px = x + ox;
				int py = y + oy;
				const UINT8 maxBuffer = 255;
				SetPixel(hdc, px, py, RGB((std::min)(maxBuffer, colour.red), (std::min)(maxBuffer, colour.green), (std::min)(maxBuffer, colour.blue)));
			}
		}
	}
}

// Initialize a basic solar system layout (lazy, uses current client centre)
static void InitializeSolarSystemIfNeeded(HWND hwnd, int clientWidth, int clientHeight) {
	if (g_sceneInitialized) return;

	int cx = clientWidth / 2;
	int cy = clientHeight / 2;
	Coordinate centre(cx, cy);

	// Sun (visual) - use configured template and position it
	g_sun = g_solarConfig.sunTemplate;
	g_sun.sunBody.coordOnScreen = centre;

	// Central mass (Sun) in kg: use configured value
	const double sunMassKg = g_solarConfig.sunMassKg;

	// store the sun's physical mass so the UI popup can display it
	g_sun.sunBody.massKg = sunMassKg;
	// optionally set a nominal physical radius for gravitational calculations (meters)
	g_sun.sunBody.physicalRadiusMeters = g_solarConfig.sunPhysicalRadiusMeters;

	g_planets.clear();
	g_planetOrbitRadii.clear();
	g_planetOrbitRatios.clear();
	g_planetAngles.clear();
	g_planetAngularSpeeds.clear();
	g_planetEllipseFactors.clear();

	// Helper lambda to create planet entries.
	auto addPlanet = [&](const PlanetConfig& cfg) {
		// compute approximate orbital speed using central sun mass and physical semi-major axis
		double orbitalSpeed = 0.0;
		if (cfg.semiMajorAxisMeters > 0.0) {
			orbitalSpeed = ScalarOrbitalVelocity(sunMassKg, cfg.semiMajorAxisMeters); // sqrt(G*M / r)
			if (orbitalSpeed < 0.0) orbitalSpeed = 0.0;
		}

		g_planetOrbitRatios.push_back(cfg.orbitRatio);
		Coordinate pos(centre.x + 10, centre.y);
		OrbitalBodyRepresentation body(pos, 1, static_cast<int>(cfg.visualRadiusPx), cfg.colour, cfg.name, cfg.massKg, orbitalSpeed);

		if (cfg.rings) {
			PlanetRepresentation p(body, cfg.ringThickness, cfg.ringColour);
			g_planets.push_back(p);
		}
		else {
			PlanetRepresentation p(body);
			g_planets.push_back(p);
		}

		g_planetAngles.push_back(cfg.initialAngle);
		g_planetAngularSpeeds.push_back(cfg.angularSpeed);
		g_planetEllipseFactors.push_back(cfg.ellipseFactor);
	};

	// Populate planets from the central configuration (replaces inline raw addPlanet calls)
	for (const auto& p : g_solarConfig.planets) {
		addPlanet(p);
	}

	// compute pixel radii and initial screen positions
	RecomputeOrbitRadii(clientWidth, clientHeight);
	RepositionPlanets(clientWidth, clientHeight);

	g_sceneInitialized = true;
}

// Update positions for each planet (called on timer). Uses client centre as orbit centre.
static void UpdatePlanetPositions(HWND hwnd) {
	if (g_planets.empty()) return;

	RECT rc;
	GetClientRect(hwnd, &rc);
	int cx = (rc.right + rc.left) / 2;
	int cy = (rc.bottom + rc.top) / 2;
	Coordinate centre(cx, cy);

	// delta time in seconds
	double dt = static_cast<double>(refreshRate) / 1000.0;
	for (size_t i = 0; i < g_planets.size(); ++i) {
		// advance angle
		g_planetAngles[i] += g_planetAngularSpeeds[i] * dt;
		// keep angles within reasonable range
		if (g_planetAngles[i] > 2.0 * Pi) g_planetAngles[i] = std::fmod(g_planetAngles[i], 2.0 * Pi);

		double angle = g_planetAngles[i];
		int r = g_planetOrbitRadii[i];
		int elip = g_planetEllipseFactors[i];
		int yRadius = (elip > 1) ? (r / elip) : r;
		int x = centre.x + static_cast<int>(std::round(r * std::cos(angle)));
		int y = centre.y + static_cast<int>(std::round(yRadius * std::sin(angle)));
		g_planets[i].planetaryBody.coordOnScreen = Coordinate(x, y);
	}
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			RECT rc;
			GetClientRect(hwnd, &rc);

			// --- Create a 32bpp top-down DIB section for crisp ClearType text and consistent rendering ---
			HDC memDC = CreateCompatibleDC(hdc);

			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = rc.right - rc.left;
			bmi.bmiHeader.biHeight = -(rc.bottom - rc.top); // top-down
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			// After creating memBM, check for failure before using SelectObject
			void* dibBits = nullptr;
			HBITMAP memBM = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &dibBits, NULL, 0);
			if (memBM == NULL) {
				// Handle error: fallback to direct painting or abort paint
				DeleteDC(memDC);
				EndPaint(hwnd, &ps);
				return 0;
			}
			HBITMAP oldBM = static_cast<HBITMAP>(SelectObject(memDC, memBM));

			// Clear background on memDC
			HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
			FillRect(memDC, &rc, bg);
			DeleteObject(bg);

			int cx = (rc.right + rc.left) / 2;
			int cy = (rc.bottom + rc.top) / 2;

			InitializeSolarSystemIfNeeded(hwnd, rc.right - rc.left, rc.bottom - rc.top);

			Coordinate centre(cx, cy);

			// Draw sun as two concentric filled spheres: outer orange halo, inner yellow core.
			LightingBuffer sunLight; // base lighting
			// Outer (orange) sphere - use configured lighting multipliers
			LightingBuffer outerLight = sunLight;
			outerLight.ambient = sunLight.ambient * g_solarConfig.outerAmbientFactor;
			outerLight.diffuseStrength = sunLight.diffuseStrength * g_solarConfig.outerDiffuseFactor;
			double coefficient = g_solarConfig.sunHaloCoefficient;
			int outerRadius = g_sun.sunBody.radius + static_cast<int>((std::max)(6.0, static_cast<int>(g_sun.thickness) * coefficient));
			DrawCircleInClient(memDC, g_sun.sunBody.coordOnScreen, outerRadius, RGBBuffer(240, 140, 40), outerLight);

			// Inner (yellow) core, drawn on top
			LightingBuffer coreLight = sunLight;
			coreLight.ambient = sunLight.ambient * g_solarConfig.coreAmbientFactor;
			coreLight.diffuseStrength = sunLight.diffuseStrength * g_solarConfig.coreDiffuseFactor;
			DrawCircleInClient(memDC, g_sun.sunBody.coordOnScreen, g_sun.sunBody.radius, g_sun.sunBody.colourBuffer, coreLight);

			// Draw orbit paths (memDC) - use configured orbit colour
			RGBBuffer orbitColour = g_solarConfig.orbitColour; // replaced raw literal
			for (size_t i = 0; i < g_planets.size(); ++i) {
				DrawOrbitPath(memDC, centre, g_planetOrbitRadii[i], orbitColour);
			}

			// Draw planets (memDC)
			for (const auto& p : g_planets) {
				DrawPlanet(memDC, p);
			}

			// If hovering over a planet, draw an info box near the mouse cursor
			if (g_hoveredPlanetIndex >= 0 && static_cast<size_t>(g_hoveredPlanetIndex) < g_planets.size()) {
				const OrbitalBodyRepresentation& b = g_planets[g_hoveredPlanetIndex].planetaryBody;
				std::ostringstream ss;
				ss << std::fixed << std::setprecision(3);
				ss << b.name;
				ss << "\nMass: ";
				if (b.massKg >= 1e24) {
					ss << std::setprecision(3) << (b.massKg / 1e24) << "e24 kg";
				}
				else {
					ss << std::setprecision(3) << b.massKg << " kg";
				}
				ss << "\nv: ";
				if (b.velocityMs >= 1000.0) {
					ss << std::setprecision(1) << (b.velocityMs / 1000.0) << " km/s";
				}
				else {
					ss << std::setprecision(1) << b.velocityMs << " m/s";
				}
			 std::string text = ss.str();

				// Create a Clear/Opaque tooltip with black text on white background.
				LOGFONT lf = {};
				lf.lfHeight = -12; // pixel height; adjust as needed for DPI
				lf.lfWeight = FW_NORMAL;
				lf.lfCharSet = DEFAULT_CHARSET;
				lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
				lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
				// Prefer grayscale antialiasing for DIB rendering to avoid ClearType subpixel greying.
				lf.lfQuality = ANTIALIASED_QUALITY;

				// Copy face name correctly for ANSI/UNICODE builds
#ifdef UNICODE
				wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Segoe UI");
#else
				strcpy_s(lf.lfFaceName, LF_FACESIZE, "Segoe UI");
#endif

				HFONT hClearFont = CreateFontIndirect(&lf);
				HFONT oldFont = (HFONT)SelectObject(memDC, hClearFont);

				// measure text extents using DrawText with DT_CALCRECT
				RECT textRc = { 0,0,0,0 };
				DrawTextA(memDC, text.c_str(), static_cast<int>(text.size()), &textRc, DT_LEFT | DT_CALCRECT | DT_NOPREFIX);

				// position box with slight offset from cursor, and clamp to client rect
				int padding = 6;
				int bx = g_mousePos.x + 12;
				int by = g_mousePos.y + 12;
				if (bx + (textRc.right - textRc.left) + padding * 2 > rc.right) bx = rc.right - (textRc.right - textRc.left) - padding * 2;
				if (by + (textRc.bottom - textRc.top) + padding * 2 > rc.bottom) by = rc.bottom - (textRc.bottom - textRc.top) - padding * 2;
				if (bx < 0) bx = 0;
				if (by < 0) by = 0;

				RECT box = { bx, by, bx + (textRc.right - textRc.left) + padding * 2, by + (textRc.bottom - textRc.top) + padding * 2 };

				// draw white background
				HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
				FillRect(memDC, &box, brush);
				DeleteObject(brush);

				// draw border (leave as subtle grey)
				HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
				HPEN oldPen2 = (HPEN)SelectObject(memDC, pen);
				Rectangle(memDC, box.left, box.top, box.right, box.bottom);
				SelectObject(memDC, oldPen2);
				DeleteObject(pen);

				// draw text in black on opaque white background
				SetBkMode(memDC, OPAQUE);
				SetBkColor(memDC, RGB(255, 255, 255));
				SetTextColor(memDC, RGB(0, 0, 0));
				RECT textDraw = { box.left + padding, box.top + padding, box.right - padding, box.bottom - padding };
				DrawTextA(memDC, text.c_str(), static_cast<int>(text.size()), &textDraw, DT_LEFT | DT_NOPREFIX);

				// restore and clean up font
				SelectObject(memDC, oldFont);
				DeleteObject(hClearFont);
			}
			else if (g_hoveringSun) {
				// Build tooltip for sun: show mass and surface gravitational acceleration
				std::ostringstream ss;
				ss << std::fixed;
				ss << "Sun";
				ss << "\nMass: ";
				// show mass in scientific notation for readability
				ss << std::setprecision(3) << std::scientific << g_sun.sunBody.massKg << " kg";

				// Compute gravitational acceleration at sun radius (m/s^2)
				double gravity = ScalarGravitationalAroundBody(g_sun.sunBody.massKg, g_sun.sunBody.physicalRadiusMeters);
				ss << "\ng: ";
				ss << std::setprecision(2) << std::fixed << gravity << " m/s^2";

				std::string text = ss.str();

				LOGFONT lf = {};
				lf.lfHeight = -12;
				lf.lfWeight = FW_NORMAL;
				lf.lfCharSet = DEFAULT_CHARSET;
				lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
				lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
				lf.lfQuality = ANTIALIASED_QUALITY;
				#ifdef UNICODE
					wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Segoe UI");
				#else
					strcpy_s(lf.lfFaceName, LF_FACESIZE, "Segoe UI");
				#endif
				HFONT hClearFont = CreateFontIndirect(&lf);
				HFONT oldFont = (HFONT)SelectObject(memDC, hClearFont);

				RECT textRc = { 0,0,0,0 };
				DrawTextA(memDC, text.c_str(), static_cast<int>(text.size()), &textRc, DT_LEFT | DT_CALCRECT | DT_NOPREFIX);

				int padding = 6;
				int bx = g_mousePos.x + 12;
				int by = g_mousePos.y + 12;
				if (bx + (textRc.right - textRc.left) + padding * 2 > rc.right) bx = rc.right - (textRc.right - textRc.left) - padding * 2;
				if (by + (textRc.bottom - textRc.top) + padding * 2 > rc.bottom) by = rc.bottom - (textRc.bottom - textRc.top) - padding * 2;
				if (bx < 0) bx = 0;
				if (by < 0) by = 0;

				RECT box = { bx, by, bx + (textRc.right - textRc.left) + padding * 2, by + (textRc.bottom - textRc.top) + padding * 2 };
				HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
				FillRect(memDC, &box, brush);
				DeleteObject(brush);
				HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
				HPEN oldPen2 = (HPEN)SelectObject(memDC, pen);
				Rectangle(memDC, box.left, box.top, box.right, box.bottom);
				SelectObject(memDC, oldPen2);
				DeleteObject(pen);

				SetBkMode(memDC, OPAQUE);
				SetBkColor(memDC, RGB(255, 255, 255));
				SetTextColor(memDC, RGB(0, 0, 0));
				RECT textDraw = { box.left + padding, box.top + padding, box.right - padding, box.bottom - padding };
				DrawTextA(memDC, text.c_str(), static_cast<int>(text.size()), &textDraw, DT_LEFT | DT_NOPREFIX);

				SelectObject(memDC, oldFont);
				DeleteObject(hClearFont);
			}

			// Blit once to screen
			BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);

			// Cleanup double buffer
			SelectObject(memDC, oldBM);
			DeleteObject(memBM);
			DeleteDC(memDC);

			EndPaint(hwnd, &ps);
			return 0;
		}
	case WM_SIZE:
	{
		// Recompute orbit radii and reposition bodies so layout scales with window size.
		int width = LOWORD(lParam);
		int height = HIWORD(lParam);
		RecomputeOrbitRadii(width, height);
		RepositionPlanets(width, height);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		int mx = GET_X_LPARAM(lParam);
		int my = GET_Y_LPARAM(lParam);
		g_mousePos.x = mx;
		g_mousePos.y = my;

        // detect hover over any planet
        int found = -1;
        for (size_t i = 0; i < g_planets.size(); ++i) {
            const Coordinate& c = g_planets[i].planetaryBody.coordOnScreen;
            int r = g_planets[i].planetaryBody.radius;
            int dx = mx - c.x;
            int dy = my - c.y;
            if (dx * dx + dy * dy <= r * r) {
                found = static_cast<int>(i);
                break;
            }
        }

        // detect hover over sun only if not hovering a planet (planets take precedence)
        bool prevHoverSun = g_hoveringSun;
        if (found == -1) {
            const Coordinate& sc = g_sun.sunBody.coordOnScreen;
            int sr = g_sun.sunBody.radius; // visual radius in pixels
            int sdx = mx - sc.x;
            int sdy = my - sc.y;
            if (sdx * sdx + sdy * sdy <= sr * sr) {
                g_hoveringSun = true;
            } else {
                g_hoveringSun = false;
            }
        } else {
            g_hoveringSun = false;
        }

        // If hover state changed, invalidate to repaint tooltip
        if (found != g_hoveredPlanetIndex || g_hoveringSun != prevHoverSun) {
            g_hoveredPlanetIndex = found;
            InvalidateRect(hwnd, NULL, TRUE);
        }
		return 0;
	}
	case WM_MOUSELEAVE:
	{
		// Clear hover state
		if (g_hoveredPlanetIndex != -1 || g_hoveringSun) {
			g_hoveredPlanetIndex = -1;
			g_hoveringSun = false;
			InvalidateRect(hwnd, NULL, TRUE);
		}
		return 0;
	}
	case WM_TIMER:
		UpdatePlanetPositions(hwnd);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	case WM_DESTROY:
		KillTimer(hwnd, 1);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int DrawWindow(int width, int height) {
	HINSTANCE hInst = GetModuleHandle(NULL);
	const wchar_t CLASS_NAME[] = L"SimpleGDIWindowClass";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hbrBackground = NULL;

	if (!RegisterClass(&wc))
	{
		std::cerr << "RegisterClass failed.\n";
		return 1;
	}

	// Create an width x height window (client area approximate)
	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Win32 GDI Window",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInst,
		NULL);

	if (!hwnd)
	{
		std::cerr << "CreateWindowEx failed.\n";
		return 1;
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	SetTimer(hwnd, 1, refreshRate, NULL);

	// Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	std::cout << "Window closed.\n";
	return 0;
}