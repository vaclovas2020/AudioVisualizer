#include "global.h"

HINSTANCE hInst;

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