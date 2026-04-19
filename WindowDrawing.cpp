#include <windows.h>
#include <iostream>

// Draw a filled circle centered in the client area
void DrawCircleInClient(HDC hdc, const RECT & rc)
{
    int cx = (rc.right + rc.left) / 2;
    int cy = (rc.bottom + rc.top) / 2;
    int radius = 50;
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255)); // white
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    // Draw filled circle as ellipse
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

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
        DrawCircleInClient(hdc, rc);
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