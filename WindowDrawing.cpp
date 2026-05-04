#include "WindowDrawing.h"
#include "Physics.h"

#include <windows.h>
#include <iostream>
#include <cmath>
#include <algorithm>

// Projects a 3D Point into 2D screen coordinates using a simple pinhole camera model.
// Algorithm based on the provided Python snippet: normalize vector, avoid z<=0, compute focal length from FOV.
Coordinate Project3DTo2D(const Point &p, int fov, int screenWidth, int screenHeight)
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
    //each pixel on screen represents a converstion ratio. We choose 200km for most cases - the moon is approximately 1800 km
    //TODO: consider non-scientific notation (km instead of metres)
    double radiusConverted = radius / conversionRatio;
    double screenRatio = static_cast<double>(screenWidth / screenHeight);
    int radiusPixels = static_cast<int>(radiusConverted * screenRatio);
    return radiusPixels;
}

//Convert orbital velocities to milliseconds
static int OrbitalVelocityInMilliseconds(double velocity) {
    //The Earth spins at 30km/s or 30,000 m/s
    velocity = std::abs(velocity); //invalidate negative values
    //convert velocity from m/s to km per second if greater than 1000
    if (velocity > 1000)
        velocity /= 1000;
    //continue further reducing velocity until it is less than a specified number of times the refresh rate
    int limit = maxRefreshRateFactor * refreshRate;
    if (velocity >= limit) {
        double factorToReduce = velocity / limit;
        velocity /= factorToReduce;
    }
    int velocityInMilliseconds = static_cast<int>(velocity / refreshRate);
    return velocityInMilliseconds;
}

// Draw a pixel into provided HDC (use HDC from BeginPaint during WM_PAINT)
static void DrawPixelInClient(HDC hdc, Coordinate coord, RGBBuffer buffer) {
    SetPixel(hdc, coord.x, coord.y, RGB(buffer.red, buffer.green, buffer.blue));
}

// Draw a line into provided HDC given coordinates (use HDC from BeginPaint during WM_PAINT)
static void DrawLineInClient(HDC hdc, Coordinate start, Coordinate end, RGBBuffer buffer, UINT8 thickness) {
    // Create pen, select it into DC, draw, then restore and delete
    HPEN hpen = CreatePen(PS_SOLID, thickness, RGB(buffer.red, buffer.green, buffer.blue));
    HPEN oldPen = (HPEN)SelectObject(hdc, hpen);
    // Draw theline from start to finish
    MoveToEx(hdc, start.x, start.y, NULL);
    LineTo(hdc, end.x, end.y);
    // Restore old pen and delete created pen
    SelectObject(hdc, oldPen);
    DeleteObject(hpen);
}

// Draw a filled circle into provided HDC (use HDC from BeginPaint during WM_PAINT)
// Uses per-pixel Lambertian shading using provided LightingBuffer.
static void DrawCircleInClient(HDC hdc, Coordinate centre, int radius, RGBBuffer buffer, const LightingBuffer& light)
{
    if (radius <= 0) return;

    // normalize light direction
    double lx = light.lx;
    double ly = light.ly;
    double lz = light.lz;
    double llen = std::sqrt(lx*lx + ly*ly + lz*lz);
    if (llen == 0.0) llen = 1e-6;
    lx /= llen; ly /= llen; lz /= llen;

    // iterate over bounding box of circle
    int y0 = centre.y - radius;
    int y1 = centre.y + radius;
    for (int py = y0; py <= y1; ++py) {
        int dy = py - centre.y;
        // compute horizontal half-width for this scanline
        double dy2 = static_cast<double>(dy) * static_cast<double>(dy);
        double r2 = static_cast<double>(radius) * static_cast<double>(radius);
        if (dy2 > r2) continue;
        int halfWidth = static_cast<int>(std::floor(std::sqrt(r2 - dy2)));
        int x0 = centre.x - halfWidth;
        int x1 = centre.x + halfWidth;
        for (int px = x0; px <= x1; ++px) {
            int dx = px - centre.x;
            // compute normal on unit sphere surface (approximation)
            double nx = static_cast<double>(dx) / static_cast<double>(radius);
            double ny = static_cast<double>(dy) / static_cast<double>(radius);
            double nz2 = 1.0 - (nx*nx + ny*ny);
            if (nz2 < 0.0) nz2 = 0.0;
            double nz = std::sqrt(nz2);

            // Lambertian diffuse (dot product of normal and light)
            double dot = nx * lx + ny * ly + nz * lz;
            double lambert = (std::max)(0.0, dot);

            double intensity = light.ambient + light.diffuseStrength * lambert;
            // clamp intensity
            intensity = (std::min)(1.0, (std::max)(0.0, intensity));

            // apply intensity to base colour
            int r = static_cast<int>(std::round(buffer.red * intensity));
            int g = static_cast<int>(std::round(buffer.green * intensity));
            int b = static_cast<int>(std::round(buffer.blue * intensity));

            // ensure valid range
            r = (std::min)(255, (std::max)(0, r));
            g = (std::min)(255, (std::max)(0, g));
            b = (std::min)(255, (std::max)(0, b));

            SetPixel(hdc, px, py, RGB(r, g, b));
        }
    }
}

// --- NEW helper: draw a shaded filled semicircle oriented by (dirx,diry).
static void DrawSemiCircleInClient(HDC hdc, Coordinate centre, int radius, RGBBuffer buffer, const LightingBuffer& light, double dirx, double diry, bool frontHalf)
{
	if (radius <= 0) return;

	// normalize light direction
	double lx = light.lx;
	double ly = light.ly;
	double lz = light.lz;
	double llen = std::sqrt(lx*lx + ly*ly + lz*lz);
	if (llen == 0.0) llen = 1e-6;
	lx /= llen; ly /= llen; lz /= llen;

	// normalize orientation vector
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
			} else {
				if (proj > 0.0) continue;
			}

			// compute normal on unit sphere surface (approximation)
			double nx = static_cast<double>(dx) / static_cast<double>(radius);
			double ny = static_cast<double>(dy) / static_cast<double>(radius);
			double nz2 = 1.0 - (nx*nx + ny*ny);
			if (nz2 < 0.0) nz2 = 0.0;
			double nz = std::sqrt(nz2);

			// Lambertian diffuse (dot product of normal and light)
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

// Modified: allow choosing cap style. If semicircleCaps==true, end caps are semicircles
// oriented along the ring direction; if halfSizeCaps==true the caps are half the usual radius.
static void DrawRingInClient(HDC hdc, Coordinate start, Coordinate end, UINT8 thickness, RGBBuffer buffer, const LightingBuffer& light, bool semicircleCaps = false, bool halfSizeCaps = false)
{
	if (thickness == 0) return;

	// normalize light direction
	double lx = light.lx;
	double ly = light.ly;
	double lz = light.lz;
	double llen = std::sqrt(lx*lx + ly*ly + lz*lz);
	if (llen == 0.0) llen = 1e-6;
	lx /= llen; ly /= llen; lz /= llen;

	// Vector from start to end
	double dx = static_cast<double>(end.x - start.x);
	double dy = static_cast<double>(end.y - start.y);
	double segLen = std::sqrt(dx*dx + dy*dy);
	if (segLen < 1e-6) {
		// Degenerate: draw a vertical band centered at start
		int half = (thickness + 1) / 2;
		for (int oy = -half; oy <= half; ++oy) {
			// approximate normal varying across thickness
			double offsetNorm = (half == 0) ? 0.0 : static_cast<double>(oy) / static_cast<double>(half);
			double nx = 0.0;
			double ny = offsetNorm;
			double nlen2 = nx*nx + ny*ny;
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
			} else {
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
			double nlen2 = nx*nx + ny*ny;
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
		} else {
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

    // Default lighting for this draw call (can be replaced with a scene/global lighting instance)
    LightingBuffer lighting; // uses defaults from header

    DrawCircleInClient(hdc, centre, radius, colourBuffer, lighting);
    if (planet.hasRings) {
        // Place ring so it crosses the planet's disc, producing a visible central band.
        // Drawn after the planet so the band appears on top.
        UINT8 thickness = planet.ringThickness;
        if (thickness == 0) thickness = 1;
        RGBBuffer ringColourBuffer = planet.ringColourBuffer;

        // Prepare a ring-specific lighting buffer. Rings often scatter light differently
        // so reduce ambient and increase diffuse slightly to make the band read better.
        LightingBuffer ringLighting = lighting;
        ringLighting.ambient = lighting.ambient * 0.5;
        ringLighting.diffuseStrength = lighting.diffuseStrength * 1.25;

        // Extend the ring slightly past the planetary disc so it "sticks out" at the edges,
        // and draw rounded end caps so the ring doesn't look clipped.
        int extension = (std::max)(2, static_cast<int>(thickness)); // tweakable: increase for larger overhang
        Coordinate start = Coordinate(centre.x - radius - extension, centre.y);
        Coordinate end   = Coordinate(centre.x + radius + extension, centre.y);
        DrawRingInClient(hdc, start, end, thickness, ringColourBuffer, ringLighting);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps); // Use the HDC from BeginPaint for all draw calls
        RECT rc;
        GetClientRect(hwnd, &rc);
        // Clear background to black
        HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        int cx = (rc.right + rc.left) / 2;
        int cy = (rc.bottom + rc.top) / 2;
        int radius = 30;

        PlanetRepresentation planet( OrbitalBodyRepresentation(Coordinate(cx, cy), 1, radius, RGBBuffer(120, 120, 200)), 3, RGBBuffer(200, 150, 150));
        DrawPlanet(hdc, planet);
        DrawPixelInClient(hdc, Coordinate(100, 100), RGBBuffer(255, 0,0));
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        // Invalidate entire client area and request erase (TRUE). This triggers WM_PAINT (and WM_ERASEBKGND).
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_DESTROY:
        // Stop timer (if set) and quit
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
