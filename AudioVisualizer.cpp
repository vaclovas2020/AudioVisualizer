// AudioVisualizer.cpp : Defines the entry point for the application.
//
#include "framework.h"
#include "AudioVisualizer.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <atomic>
#include <thread>
#include <wrl/client.h>   // For ComPtr (recommended)

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define MAX_LOADSTRING 100
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

// Global Variables
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

std::atomic<bool> g_running = true;
std::vector<float> g_audioBuffer;
CRITICAL_SECTION g_cs;

HBITMAP hBmp = nullptr;  // We still load it temporarily to convert to DX texture

// DirectX 11 globals
Microsoft::WRL::ComPtr<ID3D11Device>           g_pDevice;
Microsoft::WRL::ComPtr<ID3D11DeviceContext>    g_pContext;
Microsoft::WRL::ComPtr<IDXGISwapChain>         g_pSwapChain;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_pRenderTargetView;

Microsoft::WRL::ComPtr<ID3D11VertexShader>     g_pVertexShader;
Microsoft::WRL::ComPtr<ID3D11PixelShader>      g_pPixelShader;
Microsoft::WRL::ComPtr<ID3D11InputLayout>      g_pInputLayout;

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> g_pBackgroundSRV;
Microsoft::WRL::ComPtr<ID3D11SamplerState>       g_pSamplerState;

// For drawing bars (dynamic)
Microsoft::WRL::ComPtr<ID3D11Buffer>           g_pBarVertexBuffer;
UINT                                           g_barVertexCount = 0;

// Vertex structure
struct Vertex {
    float x, y, z;      // Position
    float r, g, b, a;   // Color
    float u, v;         // UV (for background)
};

// Forward declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

bool InitDirectX(HWND hWnd);
bool CreateShaders();
bool CreateBackgroundTexture(HWND hWnd);
void Render(HWND hWnd);
void CleanupDirectX();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

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

ATOM MyRegisterClass(HINSTANCE hInstance)
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

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
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
    if (!InitDirectX(hWnd))
    {
        DestroyWindow(hWnd);
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    SetTimer(hWnd, 1, 16, NULL);  // ~60 FPS

    return TRUE;
}

// Initialize DirectX 11 device, swap chain, shaders and background
bool InitDirectX(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &g_pSwapChain, &g_pDevice, &featureLevel, &g_pContext);

    if (FAILED(hr)) return false;

    // Render target view
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_pRenderTargetView);

    g_pContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), nullptr);

    // Viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_pContext->RSSetViewports(1, &vp);

    if (!CreateShaders()) return false;
    if (!CreateBackgroundTexture(hWnd)) return false;

    // Create sampler state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    g_pDevice->CreateSamplerState(&sampDesc, &g_pSamplerState);

    return true;
}

// Compile shaders from embedded strings
bool CreateShaders()
{
    const char* vsCode = R"(
    struct VS_INPUT {
        float4 Pos : POSITION;
        float4 Color : COLOR;
        float2 Tex : TEXCOORD;
    };
    struct PS_INPUT {
        float4 Pos : SV_POSITION;
        float4 Color : COLOR;
        float2 Tex : TEXCOORD;
    };
    PS_INPUT VS(VS_INPUT input) {
        PS_INPUT output;
        output.Pos = input.Pos;
        output.Color = input.Color;
        output.Tex = input.Tex;
        return output;
    })";

    const char* psCode = R"(
    Texture2D txDiffuse : register(t0);
    SamplerState samLinear : register(s0);

    struct PS_INPUT {
        float4 Pos : SV_POSITION;
        float4 Color : COLOR;
        float2 Tex : TEXCOORD;
    };

    float4 PS(PS_INPUT input) : SV_Target {
        float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    
        // If texture has alpha > 0 (background is bound), use texture * vertex color (white)
        // Otherwise (bars), just use the vertex color
        if (texColor.r > 0.7f || texColor.g > 0.7f || texColor.b > 0.7f)
            return texColor;     // input.Color is usually white (1,1,1,1) for background
        else
            return input.Color;                // pure color for bars
    }
    )";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, &errorBlob);

    g_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pDevice->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    return true;
}

// Load background bitmap resource and convert to DX11 texture
bool CreateBackgroundTexture(HWND hWnd)
{
    // Load the bitmap resource (IDB_BITMAP1)
    hBmp = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    if (!hBmp) return false;

    BITMAP bm;
    GetObject(hBmp, sizeof(BITMAP), &bm);

    // Get pixel data
    HDC hdc = GetDC(hWnd);
    HDC memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hBmp);

    std::vector<UINT32> pixels(bm.bmWidth * bm.bmHeight);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    GetDIBits(memDC, hBmp, 0, bm.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS);

    DeleteDC(memDC);
    ReleaseDC(hWnd, hdc);

    // Create texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = bm.bmWidth;
    desc.Height = bm.bmHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = bm.bmWidth * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
    g_pDevice->CreateTexture2D(&desc, &initData, &pTexture);
    g_pDevice->CreateShaderResourceView(pTexture.Get(), nullptr, &g_pBackgroundSRV);

    DeleteObject(hBmp);  // no longer needed
    return true;
}

void Render(HWND hWnd)
{
    if (!g_pContext || !g_pRenderTargetView || !g_pContext.Get()) return;

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.1f };
    g_pContext->ClearRenderTargetView(g_pRenderTargetView.Get(), clearColor);

    g_pContext->IASetInputLayout(g_pInputLayout.Get());
    g_pContext->VSSetShader(g_pVertexShader.Get(), nullptr, 0);
    g_pContext->PSSetShader(g_pPixelShader.Get(), nullptr, 0);
    g_pContext->PSSetSamplers(0, 1, g_pSamplerState.GetAddressOf());

    RECT rc;
    GetClientRect(hWnd, &rc);
    float w = (float)(rc.right - rc.left);
    float h = (float)(rc.bottom - rc.top);

    // Unbind any texture at the beginning (safe practice)
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    g_pContext->PSSetShaderResources(0, 1, nullSRV);

    // 1. Draw background (full-screen quad)
    Vertex bgVerts[6] = {
        {-1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},  // white
        { 1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
        { 1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f}
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * 6;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = { bgVerts };

    Microsoft::WRL::ComPtr<ID3D11Buffer> pBgVB;
    HRESULT hr = g_pDevice->CreateBuffer(&bd, &initData, &pBgVB);
    if (SUCCEEDED(hr))
    {
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        g_pContext->IASetVertexBuffers(0, 1, pBgVB.GetAddressOf(), &stride, &offset);
        g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        g_pContext->PSSetShaderResources(0, 1, g_pBackgroundSRV.GetAddressOf());
        g_pContext->Draw(6, 0);
    }

    // 2. Draw audio bars
    EnterCriticalSection(&g_cs);
    std::vector<float> copy = g_audioBuffer;
    LeaveCriticalSection(&g_cs);

    if (!copy.empty())
    {
        const int bars = 64;
        int samplesPerBar = (int)copy.size() / bars;
        if (samplesPerBar < 1) samplesPerBar = 1;

        std::vector<Vertex> barVertices;
        barVertices.reserve(bars * 6);

        float barWidth = 2.0f / bars;

        for (int i = 0; i < bars; ++i)
        {
            float sum = 0.0f;
            int start = i * samplesPerBar;
            int end = min(start + samplesPerBar, (int)copy.size());

            for (int j = start; j < end; ++j)
                sum += copy[j] * copy[j];

            float rms = (end > start) ? sqrtf(sum / (end - start)) : 0.0f;
            float barHeight = min(rms * 3.5f, 1.8f);

            float left = -1.0f + i * barWidth;
            float right = left + barWidth * 0.85f;   // small gap between bars

            // Rainbow effect based on bar index
            float hue = (float)i / bars;                    // 0.0 to 1.0
            //float r = sinf(hue * 6.28f + 0.0f) * 0.5f + 0.5f;
            float r = 0.0f;
            float g = sinf(hue * 6.28f + 2.0f) * 0.5f + 0.5f;
            float b = sinf(hue * 6.28f + 4.0f) * 0.5f + 0.5f;
            //float b = 0.0f;

            // Quad
            barVertices.push_back({ left,  -1.0f + barHeight, 0, r,g,b,1, 0,0 });
            barVertices.push_back({ right, -1.0f + barHeight, 0, r,g,b,1, 0,0 });
            barVertices.push_back({ left,  -1.0f,            0, r,g,b,1, 0,0 });

            barVertices.push_back({ right, -1.0f + barHeight, 0, r,g,b,1, 0,0 });
            barVertices.push_back({ right, -1.0f,            0, r,g,b,1, 0,0 });
            barVertices.push_back({ left,  -1.0f,            0, r,g,b,1, 0,0 });
        }

        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.ByteWidth = sizeof(Vertex) * (UINT)barVertices.size();
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData = { barVertices.data() };

        Microsoft::WRL::ComPtr<ID3D11Buffer> pBarVB;
        hr = g_pDevice->CreateBuffer(&vbd, &vbData, &pBarVB);
        if (SUCCEEDED(hr))
        {
            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            g_pContext->IASetVertexBuffers(0, 1, pBarVB.GetAddressOf(), &stride, &offset);

            // Already unbound above, but we make sure again
            g_pContext->PSSetShaderResources(0, 1, nullSRV);

            g_pContext->Draw((UINT)barVertices.size(), 0);
        }
    }

    g_pSwapChain->Present(1, 0);   // VSync enabled
}

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
        // Background texture is created in InitInstance
        break;

    case WM_TIMER:
        if (wParam == 1)
            Render(hWnd);
        break;

    case WM_SIZE:
        // Optional: recreate swap chain and render target on resize
        break;

    case WM_DESTROY:
        g_running = false;
        CleanupDirectX();
        DeleteCriticalSection(&g_cs);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CleanupDirectX()
{
    // ComPtr will automatically release
    g_pRenderTargetView.Reset();
    g_pSwapChain.Reset();
    g_pContext.Reset();
    g_pDevice.Reset();
    // other objects will be released automatically
}

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