#include "AudioVisualizerMainWindow.h"
#include "AudioVIsualizerRender.h"
#include "global.h"

AudioVisualizerRender g_RenderObj{};

int AudioVisualizerMainWindow::InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_AUDIOVISUALIZER, szWindowClass, MAX_LOADSTRING);

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AUDIOVISUALIZER));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM AudioVisualizerMainWindow::MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AUDIOVISUALIZER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_AUDIOVISUALIZER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL AudioVisualizerMainWindow::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 1280, 720, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

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
        // Release COM objects...
        if (pCapture) pCapture->Release();
        if (pClient) pClient->Release();
        if (pDevice) pDevice->Release();
        if (pEnum) pEnum->Release();
        CoUninitialize();
        }).detach();

    // Initialize DirectX 11
    if (!g_RenderObj.InitDirectX(hWnd))
    {
        DestroyWindow(hWnd);
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    SetTimer(hWnd, 1, 16, NULL);  // ~60 FPS

    return TRUE;
}

LRESULT CALLBACK AudioVisualizerMainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
        // Background texture is created in InitInstance
        break;

    case WM_TIMER:
        if (wParam == 1)
            g_RenderObj.Render(hWnd);
        break;

    case WM_SIZE:
        // Optional: recreate swap chain and render target on resize
        break;

    case WM_DESTROY:
        g_running = false;
        g_RenderObj.CleanupDirectX();
        DeleteCriticalSection(&g_cs);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK AudioVisualizerMainWindow::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
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