#pragma once
#include <ddraw.h>
#include "Overlay.h"

class MyDirectDrawSurface : public IDirectDrawSurface {
private:
    IDirectDrawSurface* m_realSurface;
    ULONG m_refCount;
    bool m_isFakePrimary;
    int m_surfaceId;
    DWORD m_width;
    DWORD m_height;

public:
    MyDirectDrawSurface(IDirectDrawSurface* realSurface, bool isFakePrimary = false);
    virtual ~MyDirectDrawSurface();

    HRESULT __stdcall QueryInterface(REFIID riid, LPVOID* ppv) override;
    ULONG __stdcall AddRef() override;
    ULONG __stdcall Release() override;

    HRESULT __stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDS) override;
    HRESULT __stdcall AddOverlayDirtyRect(LPRECT lpRect) override;
    HRESULT __stdcall Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrc, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) override;
    HRESULT __stdcall BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) override;
    HRESULT __stdcall BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrc, LPRECT lpSrcRect, DWORD dwTrans) override;
    HRESULT __stdcall DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDS) override;
    HRESULT __stdcall EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnum) override;
    HRESULT __stdcall EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnum) override;
    HRESULT __stdcall Flip(LPDIRECTDRAWSURFACE lpDDTarget, DWORD dwFlags) override;
    HRESULT __stdcall GetAttachedSurface(LPDDSCAPS lpCaps, LPDIRECTDRAWSURFACE* lplpDDS) override;
    HRESULT __stdcall GetBltStatus(DWORD dwFlags) override;
    HRESULT __stdcall GetCaps(LPDDSCAPS lpDDSCaps) override;
    HRESULT __stdcall GetClipper(LPDIRECTDRAWCLIPPER* lplpDDClipper) override;
    HRESULT __stdcall GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override;
    HRESULT __stdcall GetDC(HDC* lphDC) override;
    HRESULT __stdcall GetFlipStatus(DWORD dwFlags) override;
    HRESULT __stdcall GetOverlayPosition(LPLONG lplX, LPLONG lplY) override;
    HRESULT __stdcall GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) override;
    HRESULT __stdcall GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override;
    HRESULT __stdcall GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) override;
    HRESULT __stdcall Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) override;
    HRESULT __stdcall IsLost() override;
    HRESULT __stdcall Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) override;
    HRESULT __stdcall ReleaseDC(HDC hDC) override;
    HRESULT __stdcall Restore() override;
    HRESULT __stdcall SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) override;
    HRESULT __stdcall SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override;
    HRESULT __stdcall SetOverlayPosition(LONG lX, LONG lY) override;
    HRESULT __stdcall SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override;
    HRESULT __stdcall Unlock(LPVOID lpSurfaceData) override;
    HRESULT __stdcall UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDest, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) override;
    HRESULT __stdcall UpdateOverlayDisplay(DWORD dwFlags) override;
    HRESULT __stdcall UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSRef) override;
};