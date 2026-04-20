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
    y = (v.y / v.z) * f + h/2 */
	double px = (vx / vz) * focalLength + static_cast<double>(screenWidth) / 2.0;
	double py = (vy / vz) * focalLength + static_cast<double>(screenHeight) / 2.0;

	return Coordinate{ static_cast<int>(std::round(px)), static_cast<int>(std::round(py)) };
}

// Draw a pixel in the client area
static void DrawPixelInClient(HWND hWnd, Coordinate coord, INT8 red = 255, INT8 green = 0, INT8 blue = 0) {
    HDC hdc = GetDC(hWnd); // Get DC for the window
    SetPixel(hdc, coord.x, coord.y, RGB(red, green, blue));
    ReleaseDC(hWnd, hdc); // Always release the DC
}

// Draw a filled circle centered in the client area
static void DrawCircleInClient(HDC hdc, int cx, int cy, int radius, INT8 red = 255, INT8 green = 255, INT8 blue = 255)
{
    HBRUSH brush = CreateSolidBrush(RGB(red, green, blue)); // white
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    // Draw filled circle as ellipse
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

//static void DrawPlanet(HWND hwnd, Coordinate centre, const RECT& rect) {
//
//}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        // Clear background to black
        HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        int cx = (rc.right + rc.left) / 2;
        int cy = (rc.bottom + rc.top) / 2;
        int radius = 50;
        DrawCircleInClient(hdc, cx, cy, radius, 120, 120, 200);
        DrawPixelInClient(hwnd, Coordinate(100, 100));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
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

    if (!RegisterClass(&wc))
    {
        std::cerr << "RegisterClass failed.\n";
        return 1;
    }

    // Create an 800x600 window (client area approximate)
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
