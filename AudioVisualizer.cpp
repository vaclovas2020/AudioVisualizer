// AudioVisualizer.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "AudioVisualizer.h"

#define MAX_LOADSTRING 100
#define WS_OVERLAPPEDWINDOW_MY_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU)

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


std::atomic<bool> g_running = true;
std::vector<float> g_audioBuffer;   // raw PCM samples
CRITICAL_SECTION g_cs;              // protects g_audioBuffer

HBRUSH hBrushBackground;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_AUDIOVISUALIZER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AUDIOVISUALIZER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    hBrushBackground = CreateSolidBrush(RGB(254, 249, 183));


    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AUDIOVISUALIZER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = hBrushBackground;
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_AUDIOVISUALIZER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW_MY_STYLE,
      CW_USEDEFAULT, 0, 1280, 720, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   InitializeCriticalSection(&g_cs);

   std::thread([]() {
       HRESULT result = CoInitialize(nullptr);

       IMMDeviceEnumerator* pEnum = nullptr;
       IMMDevice* pDevice = nullptr;

       CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
           CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

       // Use microphone:
       pEnum->GetDefaultAudioEndpoint(eCapture, eCommunications, &pDevice);

       IAudioClient* pClient = nullptr;
       pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pClient);

       WAVEFORMATEX* wf = nullptr;
       pClient->GetMixFormat(&wf);

       pClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
           AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
           0, 0, wf, NULL);

       IAudioCaptureClient* pCapture = nullptr;
       pClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCapture);

       HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

       if (!hEvent) {
           return;
       }


       pClient->SetEventHandle(hEvent);

       pClient->Start();

       while (g_running) {
           WaitForSingleObject(hEvent, 500);

           UINT32 packetFrames = 0;
           pCapture->GetNextPacketSize(&packetFrames);
           while (packetFrames > 0) {
               BYTE* data;
               UINT32 numFrames;
               DWORD flags;

               pCapture->GetBuffer(&data, &numFrames, &flags, NULL, NULL);

               float* fdata = (float*)data;
               size_t count = numFrames * wf->nChannels;

               if (!g_running) {
                   return;
               }

               EnterCriticalSection(&g_cs);
               g_audioBuffer.insert(g_audioBuffer.end(), fdata, fdata + count);
               if (g_audioBuffer.size() > 48000)     // limit buffer
                   g_audioBuffer.erase(g_audioBuffer.begin(),
                       g_audioBuffer.begin() + 24000);
               LeaveCriticalSection(&g_cs);

               pCapture->ReleaseBuffer(numFrames);
               pCapture->GetNextPacketSize(&packetFrames);
           }
       }

       pClient->Stop();
       pCapture->Release();
       pClient->Release();
       pDevice->Release();
       pEnum->Release();
       CoUninitialize();
       }).detach();


   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   SetTimer(hWnd, 1, 1, NULL);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // ==== AUDIO VISUALIZATION START ====

            RECT rc;
            GetClientRect(hWnd, &rc);

            EnterCriticalSection(&g_cs);
            std::vector<float> copy = g_audioBuffer;
            LeaveCriticalSection(&g_cs);

            if (!copy.empty()) {
                // Draw simple bars: take N sample chunks and compute RMS
                const int bars = 128;
                int width = (rc.right - rc.left) / bars;
                int samplesPerBar = (int)copy.size() / bars;

                for (int i = 0; i < bars; i++) {
                    float sum = 0;
                    for (int j = 0; j < samplesPerBar; j++) {
                        float v = copy[i * samplesPerBar + j];
                        sum += v * v;
                    }
                    float rms = sqrt(sum / samplesPerBar);
                    int barHeight = (int)(rms * 400);

                    Rectangle(hdc,
                        i * width,
                        rc.bottom - barHeight,
                        i * width + width - 2,
                        rc.bottom);
                }
            }

            // ==== AUDIO VISUALIZATION END ====

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_TIMER:
        RECT rc;
        GetClientRect(hWnd, &rc);
        InvalidateRect(hWnd, &rc, TRUE);
        break;
    case WM_DESTROY:
        g_running = false;
        DeleteCriticalSection(&g_cs);
        DeleteObject(hBrushBackground);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
