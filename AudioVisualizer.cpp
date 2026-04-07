// AudioVisualizer.cpp : Defines the entry point for the application.
//

#include "AudioVisualizer.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AudioVisualizerMainWindow mainWindow{};

    return mainWindow.InitWindow(hInstance, nCmdShow);
}
