#pragma warning(disable : 28251)
#pragma warning(disable : 26812)
#include <initguid.h>
#include <windows.h>
#include <ddraw.h>
#include "Settings.h"
#include "Utils.h"
#include "MyDirectDraw.h"
#include "WindowManager.h"

HMODULE g_hRealDDraw = nullptr;
HWND g_GlobalhWnd = nullptr;

typedef HRESULT(WINAPI* DirectDrawCreateProc)(GUID*, LPDIRECTDRAW*, IUnknown*);
DirectDrawCreateProc RealDirectDrawCreate = nullptr;

extern "C" HRESULT WINAPI DirectDrawCreate(GUID * lpGUID, LPDIRECTDRAW * lplpDD, IUnknown * pUnkOuter) {
    if (!g_hRealDDraw) {
        g_Settings.Load();
        InitLogging();
        LogAllSettings();

        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\ddraw.dll");
        g_hRealDDraw = LoadLibraryA(sysPath);
        if (g_hRealDDraw) RealDirectDrawCreate = (DirectDrawCreateProc)GetProcAddress(g_hRealDDraw, "DirectDrawCreate");
    }

    if (!RealDirectDrawCreate) return DDERR_GENERIC;
    HRESULT hr = RealDirectDrawCreate(lpGUID, lplpDD, pUnkOuter);
    if (SUCCEEDED(hr) && lplpDD && *lplpDD) {
        *lplpDD = new MyDirectDraw(*lplpDD);
        LogMessage("DirectDraw wrapper successfully injected");
    }
    return hr;
}

extern "C" HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext) { return DDERR_GENERIC; }
extern "C" HRESULT WINAPI DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext) { return DDERR_GENERIC; }
extern "C" HRESULT WINAPI DllCanUnloadNow() { return S_FALSE; }
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID * ppv) { return CLASS_E_CLASSNOTAVAILABLE; }
extern "C" HRESULT WINAPI DirectDrawCreateEx(GUID * lpGuid, LPVOID * lplpDD, REFIID iid, IUnknown * pUnkOuter) { return DDERR_GENERIC; }
extern "C" HRESULT WINAPI DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER * lplpDDClipper, IUnknown * pUnkOuter) { return DDERR_GENERIC; }

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        WindowManager::Detach(); // Restore original WndProc
    }
    return TRUE;
}