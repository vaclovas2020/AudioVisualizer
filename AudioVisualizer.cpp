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
HBITMAP hBmp;

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

       HRESULT instanceResult = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
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

   SetTimer(hWnd, 1, 16, NULL);

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
    case WM_CREATE:
        {
            hBmp = (HBITMAP)LoadImage(
                ((LPCREATESTRUCT)lParam)->hInstance,
                MAKEINTRESOURCE(IDB_BITMAP1),
                IMAGE_BITMAP,
                0, 0,
                LR_CREATEDIBSECTION
            );
        }
        break;
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        //
        // === DOUBLE BUFFER SETUP ===
        //
        HDC backDC = CreateCompatibleDC(hdc);
        HBITMAP backBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBackBmp = (HBITMAP)SelectObject(backDC, backBmp);

        //
        // === DRAW BACKGROUND BITMAP INTO backDC ===
        //
        HDC memDC = CreateCompatibleDC(hdc);
        SelectObject(memDC, hBmp);

        BITMAP bm{};
        GetObject(hBmp, sizeof(bm), &bm);

        // Draw bitmap at its original size
        BitBlt(backDC,
            0, 0,
            bm.bmWidth, bm.bmHeight,
            memDC,
            0, 0,
            SRCCOPY);

        DeleteDC(memDC);

        //
        // === DRAW AUDIO BARS INTO backDC ===
        //
        EnterCriticalSection(&g_cs);
        std::vector<float> copy = g_audioBuffer;
        LeaveCriticalSection(&g_cs);

        HBRUSH hBrush = CreateSolidBrush(RGB(103, 254, 77));
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(76, 103, 254));

        HBRUSH oldBrush = (HBRUSH)SelectObject(backDC, hBrush);
        HPEN oldPen = (HPEN)SelectObject(backDC, hPen);

        if (!copy.empty())
        {
            const int bars = 64;
            int width = (rc.right - rc.left) / bars;
            int samplesPerBar = (int)copy.size() / bars;

            for (int i = 0; i < bars; i++)
            {
                float sum = 0;
                for (int j = 0; j < samplesPerBar; j++)
                {
                    float v = copy[i * samplesPerBar + j];
                    sum += v * v;
                }

                float rms = sqrt(sum / samplesPerBar);
                int barHeight = (int)(rms * 720);

                Rectangle(backDC,
                    i * width,
                    rc.bottom - barHeight,
                    i * width + width - 2,
                    rc.bottom);
            }
        }

        SelectObject(backDC, oldBrush);
        SelectObject(backDC, oldPen);
        DeleteObject(hBrush);
        DeleteObject(hPen);

        //
        // === FINAL BLIT TO SCREEN ===
        //
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, backDC, 0, 0, SRCCOPY);

        //
        // === CLEANUP ===
        //
        SelectObject(backDC, oldBackBmp);
        DeleteObject(backBmp);
        DeleteDC(backDC);

        EndPaint(hWnd, &ps);
    }
    break;

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
        DeleteObject(hBmp);
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
