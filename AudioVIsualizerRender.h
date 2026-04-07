#include "global.h"

// Vertex structure
struct Vertex {
    float x, y, z;      // Position
    float r, g, b, a;   // Color
    float u, v;         // UV (for background)
};

bool InitDirectX(HWND hWnd);
bool CreateShaders();
bool CreateBackgroundTexture(HWND hWnd);
void Render(HWND hWnd);
void CleanupDirectX();
