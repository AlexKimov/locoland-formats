#include "MyDirectDraw.h"
#include "MyDirectDrawSurface.h"
#include "Utils.h"
#include "Settings.h"
#include "WindowManager.h"
#include "D3D9Renderer.h"
#include "SpriteDebugger.h"
#include "WidescreenFix.h"
#include <cstdio>
#include "FpsUncapFix.h"

MyDirectDraw::MyDirectDraw(IDirectDraw* realDD) {
    m_realDD = realDD; m_refCount = 1; m_hWnd = nullptr;
    LogMessage("MyDirectDraw created");
}
MyDirectDraw::~MyDirectDraw() { LogMessage("MyDirectDraw destroyed"); }

HRESULT __stdcall MyDirectDraw::QueryInterface(REFIID riid, LPVOID* ppv) { return m_realDD->QueryInterface(riid, ppv); }
ULONG __stdcall MyDirectDraw::AddRef() { m_refCount++; return m_realDD->AddRef(); }
ULONG __stdcall MyDirectDraw::Release() { ULONG c = m_realDD->Release(); if (--m_refCount == 0) delete this; return c; }

HRESULT __stdcall MyDirectDraw::Compact() { return m_realDD->Compact(); }
HRESULT __stdcall MyDirectDraw::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) { return m_realDD->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter); }
HRESULT __stdcall MyDirectDraw::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter) { return m_realDD->CreatePalette(dwFlags, lpDDColorArray, lplpDDPalette, pUnkOuter); }

HRESULT __stdcall MyDirectDraw::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter) {
    DWORD caps = (lpDDSurfaceDesc && (lpDDSurfaceDesc->dwFlags & DDSD_CAPS)) ? lpDDSurfaceDesc->ddsCaps.dwCaps : 0;
    LogMessage("CreateSurface called: %dx%d %dbpp, Caps=0x%08X",
        (lpDDSurfaceDesc && (lpDDSurfaceDesc->dwFlags & DDSD_WIDTH)) ? lpDDSurfaceDesc->dwWidth : 0,
        (lpDDSurfaceDesc && (lpDDSurfaceDesc->dwFlags & DDSD_HEIGHT)) ? lpDDSurfaceDesc->dwHeight : 0,
        (lpDDSurfaceDesc && (lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)) ? lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount : 0,
        caps);

    if (caps & DDSCAPS_PRIMARYSURFACE) {
        LogMessage("Intercepting Primary Surface -> Creating Fake Primary");
        DDSURFACEDESC ddsd = *lpDDSurfaceDesc;
        ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

        ddsd.dwWidth = g_ActiveWidth;
        ddsd.dwHeight = g_ActiveHeight;

        if (!(ddsd.dwFlags & DDSD_PIXELFORMAT)) {
            ddsd.dwFlags |= DDSD_PIXELFORMAT;
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
            ddsd.ddpfPixelFormat.dwRBitMask = 0xF800;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x07E0;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x001F;
        }

        HRESULT hr = m_realDD->CreateSurface(&ddsd, lplpDDSurface, pUnkOuter);
        if (SUCCEEDED(hr)) {
            *lplpDDSurface = new MyDirectDrawSurface(*lplpDDSurface, true);
            LogMessage("SUCCESS: Fake primary surface created (%dx%d)", ddsd.dwWidth, ddsd.dwHeight);
        }
        return hr;
    }

    DDSURFACEDESC ddsd = *lpDDSurfaceDesc;
    if ((ddsd.dwFlags & DDSD_CAPS) && (ddsd.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN)) {
        if (!(ddsd.dwFlags & DDSD_PIXELFORMAT)) {
            ddsd.dwFlags |= DDSD_PIXELFORMAT;
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
            ddsd.ddpfPixelFormat.dwRBitMask = 0xF800;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x07E0;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x001F;
        }
    }

    HRESULT hr = m_realDD->CreateSurface(&ddsd, lplpDDSurface, pUnkOuter);
    if (SUCCEEDED(hr)) {
        *lplpDDSurface = new MyDirectDrawSurface(*lplpDDSurface, false);
    }
    return hr;
}

HRESULT __stdcall MyDirectDraw::DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE* lplpDupDDSurface) { return m_realDD->DuplicateSurface(UnwrapSurface(lpDDSurface), lplpDupDDSurface); }
HRESULT __stdcall MyDirectDraw::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) { return m_realDD->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback); }
HRESULT __stdcall MyDirectDraw::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) { return m_realDD->EnumSurfaces(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumSurfacesCallback); }
HRESULT __stdcall MyDirectDraw::FlipToGDISurface() { return m_realDD->FlipToGDISurface(); }
HRESULT __stdcall MyDirectDraw::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) { return m_realDD->GetCaps(lpDDDriverCaps, lpDDHELCaps); }
HRESULT __stdcall MyDirectDraw::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) { return m_realDD->GetDisplayMode(lpDDSurfaceDesc); }
HRESULT __stdcall MyDirectDraw::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) { return m_realDD->GetFourCCCodes(lpNumCodes, lpCodes); }
HRESULT __stdcall MyDirectDraw::GetGDISurface(LPDIRECTDRAWSURFACE* lplpGDIDDSurface) { return m_realDD->GetGDISurface(lplpGDIDDSurface); }
HRESULT __stdcall MyDirectDraw::GetMonitorFrequency(LPDWORD lpdwFrequency) { return m_realDD->GetMonitorFrequency(lpdwFrequency); }
HRESULT __stdcall MyDirectDraw::GetScanLine(LPDWORD lpdwScanLine) { return m_realDD->GetScanLine(lpdwScanLine); }
HRESULT __stdcall MyDirectDraw::GetVerticalBlankStatus(LPBOOL lpbIsInVB) { return m_realDD->GetVerticalBlankStatus(lpbIsInVB); }
HRESULT __stdcall MyDirectDraw::Initialize(GUID* lpGUID) { return m_realDD->Initialize(lpGUID); }
HRESULT __stdcall MyDirectDraw::RestoreDisplayMode() { return m_realDD->RestoreDisplayMode(); }

HRESULT __stdcall MyDirectDraw::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {

    LogMessage("SetCooperativeLevel called, hWnd=%p, flags=0x%08X", hWnd, dwFlags);
    m_hWnd = hWnd;
    g_GameHwnd = hWnd;
    WindowManager::Attach(hWnd);

    int targetW = g_Settings.resolutionWidth;
    int targetH = g_Settings.resolutionHeight;

    LONG style;
    if (g_Settings.bWindowed) {
        // 1. Force a strict, clean windowed style (NO maximize, NO popup)
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;

        if (targetW == 0 || targetH == 0) {
            targetW = 640;
            targetH = 480;
        }
    }
    else {
        style = WS_POPUP | WS_VISIBLE;
        targetW = GetSystemMetrics(SM_CXSCREEN);
        targetH = GetSystemMetrics(SM_CYSCREEN);

        g_Settings.resolutionWidth = targetW;
        g_Settings.resolutionHeight = targetH;

        LogMessage("Fullscreen: Using native monitor resolution %dx%d", targetW, targetH);
    }

    // 3. Apply style and FORCE the window out of maximized/fullscreen state
    SetWindowLong(hWnd, GWL_STYLE, style);
    ShowWindow(hWnd, SW_SHOWNORMAL); // CRITICAL: Resets any maximize state
    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // 4. Calculate proper windowed dimensions
    RECT rc = { 0, 0, targetW, targetH };
    AdjustWindowRect(&rc, style, FALSE);

    int posX = 0, posY = 0;
    if (g_Settings.bWindowed) {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        posX = (screenW - (rc.right - rc.left)) / 2;
        posY = (screenH - (rc.bottom - rc.top)) / 2;
    }

    // 5. Apply the calculated size
    SetWindowPos(hWnd, HWND_NOTOPMOST, posX, posY, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_FRAMECHANGED);

    g_ActiveWidth = 640;
    g_ActiveHeight = 480;


    D3D9Renderer::Initialize(hWnd, targetW, targetH);
    SpriteDebugger::InstallHook();
    WidescreenFix::Install();
    //FpsUncapFix::Install();

    RECT wndRect, cliRect;
    GetWindowRect(hWnd, &wndRect);
    GetClientRect(hWnd, &cliRect);
    LogMessage("Window Initialized: Outer Rect -> W:%d H:%d | Client Area -> W:%d H:%d",
        wndRect.right - wndRect.left, wndRect.bottom - wndRect.top, cliRect.right, cliRect.bottom);

    // Always pass DDSCL_NORMAL to prevent the real DirectDraw from trying to go exclusive
    return m_realDD->SetCooperativeLevel(hWnd, DDSCL_NORMAL);
}

HRESULT __stdcall MyDirectDraw::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) {
    LogMessage("SetDisplayMode called: %dx%d %dbpp", dwWidth, dwHeight, dwBPP);

    g_ActiveWidth = dwWidth;
    g_ActiveHeight = dwHeight;

    if (g_Settings.bWindowed && g_Settings.resolutionWidth == 0 && g_Settings.resolutionHeight == 0) {
        // DYNAMIC WINDOW RESIZING
        RECT rc = { 0, 0, (LONG)dwWidth, (LONG)dwHeight };
        LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
        AdjustWindowRect(&rc, style, FALSE);

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int posX = (screenW - (rc.right - rc.left)) / 2;
        int posY = (screenH - (rc.bottom - rc.top)) / 2;

        SetWindowPos(m_hWnd, HWND_NOTOPMOST, posX, posY, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_FRAMECHANGED);
        D3D9Renderer::Reset(dwWidth, dwHeight);
    }
    else {
        // STRETCH MODE or FULLSCREEN
        D3D9Renderer::ResizeSource(dwWidth, dwHeight);
    }

    int winW = (g_Settings.bWindowed && g_Settings.resolutionWidth > 0) ? g_Settings.resolutionWidth : g_ActiveWidth;
    int winH = (g_Settings.bWindowed && g_Settings.resolutionHeight > 0) ? g_Settings.resolutionHeight : g_ActiveHeight;

    LogMessage("Internal resolution changed to %dx%d. Window is %dx%d.", dwWidth, dwHeight, winW, winH);
    return DD_OK;
}

HRESULT __stdcall MyDirectDraw::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    if (g_Settings.bUnlockFPS) {
        return DD_OK;
    }

    return m_realDD->WaitForVerticalBlank(dwFlags, hEvent);
}