#pragma once
#include "resource.h"
#include "framework.h"

class AudioVisualizerMainWindow {
private:
	public: 
		int InitWindow(HINSTANCE, int);
		ATOM MyRegisterClass(HINSTANCE);
		BOOL InitInstance(HINSTANCE, int);
		static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
};