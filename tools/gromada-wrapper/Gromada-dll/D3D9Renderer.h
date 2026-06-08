#pragma once
#include <d3d9.h>
#include <windows.h>

class D3D9Renderer {
public:
    static bool Initialize(HWND hWnd, int windowW, int windowH);
    static void Reset(int windowW, int windowH);
    static void ResizeSource(int srcW, int srcH);
    static void Shutdown();
    static bool PresentFromDD(void* ddSurfacePtr, int ddPitch, int srcW, int srcH, int dstW, int dstH);
    static bool IsReady() { return s_pDevice != nullptr; }

private:
    static HWND s_hWnd;
    static D3DPRESENT_PARAMETERS s_d3dpp; // НОВОЕ: Храним параметры создания устройства
    static IDirect3D9* s_pD3D;
    static IDirect3DDevice9* s_pDevice;
    static IDirect3DSurface9* s_pGameSurface;
    static IDirect3DSurface9* s_pRenderSurface;
};