#pragma once
#include <ddraw.h>

class MyDirectDraw : public IDirectDraw {
private:
    IDirectDraw* m_realDD;
    ULONG m_refCount;
    HWND m_hWnd;


public:
    MyDirectDraw(IDirectDraw* realDD);
    virtual ~MyDirectDraw();

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, LPVOID* ppv) override;
    ULONG __stdcall AddRef() override;
    ULONG __stdcall Release() override;

    // IDirectDraw
    HRESULT __stdcall Compact() override;
    HRESULT __stdcall CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) override;
    HRESULT __stdcall CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter) override;
    HRESULT __stdcall CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter) override;
    HRESULT __stdcall DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE* lplpDupDDSurface) override;
    HRESULT __stdcall EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) override;
    HRESULT __stdcall EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) override;
    HRESULT __stdcall FlipToGDISurface() override;
    HRESULT __stdcall GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) override;
    HRESULT __stdcall GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override;
    HRESULT __stdcall GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) override;
    HRESULT __stdcall GetGDISurface(LPDIRECTDRAWSURFACE* lplpGDIDDSurface) override;
    HRESULT __stdcall GetMonitorFrequency(LPDWORD lpdwFrequency) override;
    HRESULT __stdcall GetScanLine(LPDWORD lpdwScanLine) override;
    HRESULT __stdcall GetVerticalBlankStatus(LPBOOL lpbIsInVB) override;
    HRESULT __stdcall Initialize(GUID* lpGUID) override;
    HRESULT __stdcall RestoreDisplayMode() override;
    HRESULT __stdcall SetCooperativeLevel(HWND hWnd, DWORD dwFlags) override;
    HRESULT __stdcall SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) override;
    HRESULT __stdcall WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) override;
};