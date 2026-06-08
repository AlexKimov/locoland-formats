#include "D3D9Renderer.h"
#include "Utils.h"
#include "Settings.h"
#include "Overlay.h"
#include <cstring>

HWND D3D9Renderer::s_hWnd = nullptr;
D3DPRESENT_PARAMETERS D3D9Renderer::s_d3dpp = { 0 }; // НОВОЕ: Инициализация
IDirect3D9* D3D9Renderer::s_pD3D = nullptr;
IDirect3DDevice9* D3D9Renderer::s_pDevice = nullptr;
IDirect3DSurface9* D3D9Renderer::s_pGameSurface = nullptr;
IDirect3DSurface9* D3D9Renderer::s_pRenderSurface = nullptr;

static void Convert16To32(const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int width, int height) {
    for (int y = 0; y < height; y++) {
        const uint16_t* srcRow = (const uint16_t*)(src + y * srcPitch);
        uint32_t* dstRow = (uint32_t*)(dst + y * dstPitch);
        for (int x = 0; x < width; x++) {
            uint16_t pixel = srcRow[x];
            uint32_t r = ((pixel >> 11) & 0x1F) << 3;
            uint32_t g = ((pixel >> 5) & 0x3F) << 2;
            uint32_t b = (pixel & 0x1F) << 3;
            dstRow[x] = (r << 16) | (g << 8) | b;
        }
    }
}

bool D3D9Renderer::Initialize(HWND hWnd, int windowW, int windowH) {
    if (s_pDevice) {
        if (s_d3dpp.BackBufferWidth == (UINT)windowW &&
            s_d3dpp.BackBufferHeight == (UINT)windowH &&
            s_d3dpp.hDeviceWindow == hWnd) {
            return true;
        }
        Shutdown();
    }

    s_hWnd = hWnd;
    s_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!s_pD3D) {
        LogMessage("D3D9: FAILED to initialize Direct3D 9.");
        return false;
    }

    D3DADAPTER_IDENTIFIER9 adapterId;
    if (SUCCEEDED(s_pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId))) {
        LogMessage("D3D9: GPU Adapter -> %s", adapterId.Description);
    }

    // Clean initialization using the global s_d3dpp
    ZeroMemory(&s_d3dpp, sizeof(s_d3dpp));
    s_d3dpp.Windowed = TRUE;
    s_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    s_d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    s_d3dpp.BackBufferWidth = windowW;
    s_d3dpp.BackBufferHeight = windowH;
    s_d3dpp.hDeviceWindow = hWnd;

    // If UnlockFPS is ON, disable VSync. If OFF, let Windows handle VSync (Recommended for smooth gameplay).
    if (g_Settings.bUnlockFPS) {
        s_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }
    else {
        s_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    }

    HRESULT hr = s_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &s_d3dpp, &s_pDevice);
    if (FAILED(hr) || !s_pDevice) {
        LogMessage("D3D9: FAILED to create device (HR=0x%08X).", hr);
        s_pD3D->Release(); s_pD3D = nullptr;
        return false;
    }

    LogMessage("D3D9: Device created successfully.");
    ResizeSource(640, 480);
    return true;
}

void D3D9Renderer::Reset(int windowW, int windowH) {
    if (!s_pDevice) return;

    // НОВОЕ: Изменяем наши сохраненные параметры
    s_d3dpp.BackBufferWidth = windowW;
    s_d3dpp.BackBufferHeight = windowH;

    // Вызываем Reset с обновленной структурой
    HRESULT hr = s_pDevice->Reset(&s_d3dpp);
    if (SUCCEEDED(hr)) {
        LogMessage("D3D9: Device reset successfully to %dx%d", windowW, windowH);
        ResizeSource(g_ActiveWidth, g_ActiveHeight);
    }
    else {
        LogMessage("D3D9: Failed to reset device (HR=0x%08X). Recreating...", hr);
        Shutdown();
        Initialize(s_hWnd, windowW, windowH);
    }
}

void D3D9Renderer::ResizeSource(int srcW, int srcH) {
    if (!s_pDevice) return;
    if (s_pRenderSurface) { s_pRenderSurface->Release(); s_pRenderSurface = nullptr; }
    if (s_pGameSurface) { s_pGameSurface->Release(); s_pGameSurface = nullptr; }

    s_pDevice->CreateOffscreenPlainSurface(srcW, srcH, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &s_pGameSurface, nullptr);
    s_pDevice->CreateOffscreenPlainSurface(srcW, srcH, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &s_pRenderSurface, nullptr);
    LogMessage("D3D9: Source surfaces resized to %dx%d", srcW, srcH);
}

void D3D9Renderer::Shutdown() {
    if (s_pRenderSurface) { s_pRenderSurface->Release(); s_pRenderSurface = nullptr; }
    if (s_pGameSurface) { s_pGameSurface->Release(); s_pGameSurface = nullptr; }
    if (s_pDevice) { s_pDevice->Release(); s_pDevice = nullptr; }
    if (s_pD3D) { s_pD3D->Release(); s_pD3D = nullptr; }
}

bool D3D9Renderer::PresentFromDD(void* ddSurfacePtr, int ddPitch, int srcW, int srcH, int dstW, int dstH) {
    if (!s_pDevice || !s_pGameSurface || !s_pRenderSurface) return false;

    D3DLOCKED_RECT d3dLock;
    if (FAILED(s_pGameSurface->LockRect(&d3dLock, nullptr, 0))) return false;
    Convert16To32(static_cast<uint8_t*>(ddSurfacePtr), ddPitch, static_cast<uint8_t*>(d3dLock.pBits), d3dLock.Pitch, srcW, srcH);
    s_pGameSurface->UnlockRect();

    RenderOverlay(s_pGameSurface, srcW, srcH);

    s_pDevice->UpdateSurface(s_pGameSurface, nullptr, s_pRenderSurface, nullptr);

    IDirect3DSurface9* pBackBuffer = nullptr;
    if (FAILED(s_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))) return false;

    s_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    RECT srcRect = { 0, 0, srcW, srcH };
    RECT dstRect = { 0, 0, dstW, dstH };

    if (g_Settings.bMaintainAspectRatio) {
        float srcAR = (float)srcW / (float)srcH;
        float dstAR = (float)dstW / (float)dstH;
        int newW = dstW, newH = dstH;
        if (srcAR > dstAR) newH = (int)(dstW / srcAR);
        else newW = (int)(dstH * srcAR);
        int offsetX = (dstW - newW) / 2;
        int offsetY = (dstH - newH) / 2;
        dstRect.left = offsetX; dstRect.top = offsetY;
        dstRect.right = offsetX + newW; dstRect.bottom = offsetY + newH;
    }

    HRESULT hr = s_pDevice->StretchRect(s_pRenderSurface, &srcRect, pBackBuffer, &dstRect, D3DTEXF_LINEAR);
    pBackBuffer->Release();

    return SUCCEEDED(hr) && SUCCEEDED(s_pDevice->Present(nullptr, nullptr, nullptr, nullptr));
}