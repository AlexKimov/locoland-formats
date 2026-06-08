#include "MyDirectDrawSurface.h"
#include "Utils.h"
#include "Settings.h"
#include "D3D9Renderer.h"
#include "SpriteDebugger.h"
#include "Overlay.h" 
#include <vector>
#include <fstream>
#include <cstring>
#include <windowsx.h>

static int s_NextSurfaceId = 1;

MyDirectDrawSurface::MyDirectDrawSurface(IDirectDrawSurface* realSurface, bool isFakePrimary) {
    m_realSurface = realSurface;
    m_refCount = 1;
    m_isFakePrimary = isFakePrimary;
    m_surfaceId = s_NextSurfaceId++;

    DDSURFACEDESC ddsd = { sizeof(ddsd) };
    if (SUCCEEDED(m_realSurface->GetSurfaceDesc(&ddsd))) {
        m_width = (ddsd.dwFlags & DDSD_WIDTH) ? ddsd.dwWidth : 0;
        m_height = (ddsd.dwFlags & DDSD_HEIGHT) ? ddsd.dwHeight : 0;
    }
    else {
        m_width = m_height = 0;
    }

    g_SurfaceMap[this] = m_realSurface;
    LogMessage("Surface Created: ID=%d, Size=%dx%d, IsPrimary=%d", m_surfaceId, m_width, m_height, m_isFakePrimary);
}

MyDirectDrawSurface::~MyDirectDrawSurface() {
    g_SurfaceMap.erase(this);
}

HRESULT __stdcall MyDirectDrawSurface::QueryInterface(REFIID riid, LPVOID* ppv) {
    HRESULT hr = m_realSurface->QueryInterface(riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv && (riid == IID_IDirectDrawSurface || riid == IID_IUnknown)) {
        *ppv = this; AddRef();
    }
    return hr;
}

ULONG __stdcall MyDirectDrawSurface::AddRef() {
    m_refCount++;
    return m_realSurface->AddRef();
}

ULONG __stdcall MyDirectDrawSurface::Release() {
    ULONG count = m_realSurface->Release();
    if (--m_refCount == 0) delete this;
    return count;
}

HRESULT __stdcall MyDirectDrawSurface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrc, LPRECT lpSrcRect, DWORD dwTrans) {
    return m_realSurface->BltFast(dwX, dwY, UnwrapSurface(lpDDSrc), lpSrcRect, dwTrans);
}

HRESULT __stdcall MyDirectDrawSurface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrc, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    return m_realSurface->Blt(lpDestRect, UnwrapSurface(lpDDSrc), lpSrcRect, dwFlags, lpDDBltFx);
}

HRESULT __stdcall MyDirectDrawSurface::Unlock(LPVOID lpSurfaceData) {
    return m_realSurface->Unlock(lpSurfaceData);
}

HRESULT __stdcall MyDirectDrawSurface::Flip(LPDIRECTDRAWSURFACE lpDDTarget, DWORD dwFlags) {
    // --- 1. MEASURE GAME LOGIC TIME ---
    DWORD gameLogicStart = GetTickCount();

    // --- FPS COUNTER & WINDOW CAPTION UPDATE ---
    static DWORD s_lastTime = GetTickCount();
    static DWORD s_frameCount = 0;
    s_frameCount++;
    DWORD currentTime = GetTickCount();
    DWORD elapsed = currentTime - s_lastTime;

    if (elapsed >= 1000 && elapsed > 0) {
        g_CurrentFPS = (s_frameCount * 1000.0f) / elapsed;
        s_frameCount = 0;
        s_lastTime = currentTime;

        // Обновляем заголовок окна ТОЛЬКО в оконном режиме
        if (g_Settings.bWindowed) {
            char title[256];
            if (g_Settings.bShowFPS) {
                // Если показ FPS включен: "Gromada - 1024x768 - FPS: 60.0"
                sprintf_s(title, "Gromada - %dx%d - FPS: %.1f", g_ActiveWidth, g_ActiveHeight, g_CurrentFPS);
            }
            else {
                // Если показ FPS выключен: "Gromada - 1024x768"
                sprintf_s(title, "Gromada - %dx%d", g_ActiveWidth, g_ActiveHeight);
            }

            HWND hWnd = GetActiveWindow();
            if (hWnd) SetWindowTextA(hWnd, title);
        }
    }

    // --- SCREENSHOTTER ---
    static DWORD s_lastScreenshotTime = 0;
    if (GetAsyncKeyState(g_Settings.screenshotKey) & 1) {
        DWORD now = GetTickCount();
        if (now - s_lastScreenshotTime > 500) {
            s_lastScreenshotTime = now;
            DDSURFACEDESC ddsd = { sizeof(ddsd) };
            if (SUCCEEDED(m_realSurface->Lock(nullptr, &ddsd, DDLOCK_WAIT | DDLOCK_NOSYSLOCK, nullptr))) {
                DWORD w = (ddsd.dwFlags & DDSD_WIDTH) ? ddsd.dwWidth : g_ActiveWidth;
                DWORD h = (ddsd.dwFlags & DDSD_HEIGHT) ? ddsd.dwHeight : g_ActiveHeight;
                char filename[128];
                SYSTEMTIME st; GetLocalTime(&st);
                sprintf_s(filename, "Gromada_%04d%02d%02d_%02d%02d%02d.bmp", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                SaveVRAMToBMP(filename, ddsd.lpSurface, ddsd.lPitch, w, h);
                m_realSurface->Unlock(nullptr);
                AddOverlayMessage("Screenshot Saved!");
            }
        }
    }

    // --- VISUAL DEBUGGER TOGGLE (F11) ---
    if (g_Settings.bVisualDebug && (GetAsyncKeyState(g_Settings.visualDebugKey) & 1)) {
        SpriteDebugger::Toggle();
        if (SpriteDebugger::IsActive()) {
            AddOverlayMessage("Visual Debugger: STARTED");
        }
        else {
            AddOverlayMessage("Visual Debugger: STOPPED");
        }
    }

    // --- FPS LIMITER ---
    if (g_Settings.fpsLimit > 0) {
        static LARGE_INTEGER s_frequency = { 0 }, s_lastFrame = { 0 };
        if (s_frequency.QuadPart == 0) { QueryPerformanceFrequency(&s_frequency); QueryPerformanceCounter(&s_lastFrame); }
        double target = 1.0 / g_Settings.fpsLimit; LONGLONG ticks = (LONGLONG)(target * s_frequency.QuadPart);
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (now.QuadPart - s_lastFrame.QuadPart < ticks) {
            double ms = ((ticks - (now.QuadPart - s_lastFrame.QuadPart)) * 1000.0) / s_frequency.QuadPart;
            if (ms > 2.0) Sleep((DWORD)(ms - 1));
            do { QueryPerformanceCounter(&now); } while (now.QuadPart - s_lastFrame.QuadPart < ticks);
        }
        s_lastFrame = now;
    }

    DWORD gameLogicEnd = GetTickCount();

    if (m_isFakePrimary && D3D9Renderer::IsReady()) {
        DDSURFACEDESC ddsd = { sizeof(ddsd) };
        HRESULT lockResult = m_realSurface->Lock(nullptr, &ddsd, DDLOCK_READONLY | DDLOCK_WAIT, nullptr);
        if (SUCCEEDED(lockResult) && ddsd.lpSurface) {
            // This function contains the actual s_pDevice->Present() call
            D3D9Renderer::PresentFromDD(ddsd.lpSurface, ddsd.lPitch, g_ActiveWidth, g_ActiveHeight, g_Settings.resolutionWidth, g_Settings.resolutionHeight);

            m_realSurface->Unlock(nullptr);
            return DD_OK;
        }
        if (SUCCEEDED(lockResult)) m_realSurface->Unlock(nullptr);
    }

    if (m_isFakePrimary && g_pRealPrimary) {
        DDSURFACEDESC ddsd = { sizeof(ddsd) };
        m_realSurface->GetSurfaceDesc(&ddsd);
        RECT rc = { 0, 0, (LONG)ddsd.dwWidth, (LONG)ddsd.dwHeight };
        return g_pRealPrimary->Blt(&rc, m_realSurface, &rc, DDBLT_WAIT, nullptr);
    }

    return m_realSurface->Flip(UnwrapSurface(lpDDTarget), dwFlags);
}

// =====================================================================
// STANDARD FORWARDING METHODS
// =====================================================================
HRESULT __stdcall MyDirectDrawSurface::GetAttachedSurface(LPDDSCAPS lpCaps, LPDIRECTDRAWSURFACE* lplpDDS) {
    if (m_isFakePrimary && lpCaps && (lpCaps->dwCaps & DDSCAPS_BACKBUFFER)) { *lplpDDS = this; AddRef(); return DD_OK; }
    HRESULT hr = m_realSurface->GetAttachedSurface(lpCaps, lplpDDS);
    if (SUCCEEDED(hr) && lplpDDS && *lplpDDS) *lplpDDS = new MyDirectDrawSurface(*lplpDDS);
    return hr;
}

HRESULT __stdcall MyDirectDrawSurface::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDS) { return m_realSurface->AddAttachedSurface(UnwrapSurface(lpDDS)); }
HRESULT __stdcall MyDirectDrawSurface::AddOverlayDirtyRect(LPRECT lpRect) { return m_realSurface->AddOverlayDirtyRect(lpRect); }
HRESULT __stdcall MyDirectDrawSurface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) { return m_realSurface->BltBatch(lpDDBltBatch, dwCount, dwFlags); }
HRESULT __stdcall MyDirectDrawSurface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDS) { return m_realSurface->DeleteAttachedSurface(dwFlags, UnwrapSurface(lpDDS)); }
HRESULT __stdcall MyDirectDrawSurface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnum) { return m_realSurface->EnumAttachedSurfaces(lpContext, lpEnum); }
HRESULT __stdcall MyDirectDrawSurface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnum) { return m_realSurface->EnumOverlayZOrders(dwFlags, lpContext, lpEnum); }
HRESULT __stdcall MyDirectDrawSurface::GetBltStatus(DWORD dwFlags) { return m_realSurface->GetBltStatus(dwFlags); }
HRESULT __stdcall MyDirectDrawSurface::GetCaps(LPDDSCAPS lpDDSCaps) { return m_realSurface->GetCaps(lpDDSCaps); }
HRESULT __stdcall MyDirectDrawSurface::GetClipper(LPDIRECTDRAWCLIPPER* lplpDDClipper) { return m_realSurface->GetClipper(lplpDDClipper); }
HRESULT __stdcall MyDirectDrawSurface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) { return m_realSurface->GetColorKey(dwFlags, lpDDColorKey); }
HRESULT __stdcall MyDirectDrawSurface::GetDC(HDC* lphDC) { return m_realSurface->GetDC(lphDC); }
HRESULT __stdcall MyDirectDrawSurface::GetFlipStatus(DWORD dwFlags) { return m_realSurface->GetFlipStatus(dwFlags); }
HRESULT __stdcall MyDirectDrawSurface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) { return m_realSurface->GetOverlayPosition(lplX, lplY); }
HRESULT __stdcall MyDirectDrawSurface::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) { return m_realSurface->GetPalette(lplpDDPalette); }
HRESULT __stdcall MyDirectDrawSurface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) { return m_realSurface->GetPixelFormat(lpDDPixelFormat); }
HRESULT __stdcall MyDirectDrawSurface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) { return m_realSurface->GetSurfaceDesc(lpDDSurfaceDesc); }
HRESULT __stdcall MyDirectDrawSurface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) { return m_realSurface->Initialize(lpDD, lpDDSurfaceDesc); }
HRESULT __stdcall MyDirectDrawSurface::IsLost() { return m_realSurface->IsLost(); }
HRESULT __stdcall MyDirectDrawSurface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) { return m_realSurface->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent); }
HRESULT __stdcall MyDirectDrawSurface::ReleaseDC(HDC hDC) { return m_realSurface->ReleaseDC(hDC); }
HRESULT __stdcall MyDirectDrawSurface::Restore() { return m_realSurface->Restore(); }
HRESULT __stdcall MyDirectDrawSurface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) { return m_realSurface->SetClipper(lpDDClipper); }
HRESULT __stdcall MyDirectDrawSurface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) { return m_realSurface->SetColorKey(dwFlags, lpDDColorKey); }
HRESULT __stdcall MyDirectDrawSurface::SetOverlayPosition(LONG lX, LONG lY) { return m_realSurface->SetOverlayPosition(lX, lY); }
HRESULT __stdcall MyDirectDrawSurface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) { return m_realSurface->SetPalette(lpDDPalette); }
HRESULT __stdcall MyDirectDrawSurface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDest, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) { return m_realSurface->UpdateOverlay(lpSrcRect, UnwrapSurface(lpDDDest), lpDestRect, dwFlags, lpDDOverlayFx); }
HRESULT __stdcall MyDirectDrawSurface::UpdateOverlayDisplay(DWORD dwFlags) { return m_realSurface->UpdateOverlayDisplay(dwFlags); }
HRESULT __stdcall MyDirectDrawSurface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSRef) { return m_realSurface->UpdateOverlayZOrder(dwFlags, UnwrapSurface(lpDDSRef)); }