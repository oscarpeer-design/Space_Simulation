// Space_Simulation.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "WindowDrawing.h"
#include "Physics.h"

int main()
{
    int width = 800;
    int height = 750;
    int iErr = DrawWindow(width, height);
    return iErr;
}
