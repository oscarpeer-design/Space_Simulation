#include "WindowDrawing.h"
#include "Physics.h"

#include <windows.h>
#include <iostream>
#include <cmath>

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
static void DrawCircleInClient(HDC hdc, Coordinate centre, int radius, RGBBuffer buffer)
{
    HBRUSH brush = CreateSolidBrush(RGB(buffer.red, buffer.green, buffer.blue));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, centre.x - radius, centre.y - radius, centre.x + radius, centre.y + radius);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

// Draw an orbital body on the screen
static void DrawPlanet(HDC hdc, const PlanetRepresentation& planet) {
    RGBBuffer colourBuffer = planet.planetaryBody.colourBuffer;
    Coordinate centre = planet.planetaryBody.coordOnScreen;
    int radius = planet.planetaryBody.radius;
    DrawCircleInClient(hdc, centre, radius, colourBuffer);
    if (planet.hasRings) {
        Coordinate start(centre.x - radius, centre.y);
        Coordinate end(centre.x + radius, centre.y);
        RGBBuffer ringColourBuffer = planet.ringColourBuffer;
        UINT8 thickness = planet.ringThickness;
        DrawLineInClient(hdc, start, end, ringColourBuffer, thickness);
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

        PlanetRepresentation planet( OrbitalBodyRepresentation(Coordinate(cx, cy), 1, radius, RGBBuffer(120, 120, 200)), 5, RGBBuffer(200, 150, 150));
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
