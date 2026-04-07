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

// Global Variables
extern WCHAR szTitle[MAX_LOADSTRING];
extern WCHAR szWindowClass[MAX_LOADSTRING];

extern HINSTANCE hInst;

extern std::atomic<bool> g_running;
extern std::vector<float> g_audioBuffer;
extern CRITICAL_SECTION g_cs;

extern HBITMAP hBmp;  // We still load it temporarily to convert to DX texture

// DirectX 11 globals
extern Microsoft::WRL::ComPtr<ID3D11Device>           g_pDevice;
extern Microsoft::WRL::ComPtr<ID3D11DeviceContext>    g_pContext;
extern Microsoft::WRL::ComPtr<IDXGISwapChain>         g_pSwapChain;
extern Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_pRenderTargetView;

extern Microsoft::WRL::ComPtr<ID3D11VertexShader>     g_pVertexShader;
extern Microsoft::WRL::ComPtr<ID3D11PixelShader>      g_pPixelShader;
extern Microsoft::WRL::ComPtr<ID3D11InputLayout>      g_pInputLayout;

extern Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> g_pBackgroundSRV;
extern Microsoft::WRL::ComPtr<ID3D11SamplerState>       g_pSamplerState;

// For drawing bars (dynamic)
extern Microsoft::WRL::ComPtr<ID3D11Buffer>           g_pBarVertexBuffer;
extern UINT                                           g_barVertexCount;
