#pragma once
#include <windows.h>
#include <ddraw.h>
#include <map>
#include <string>

extern std::map<IDirectDrawSurface*, IDirectDrawSurface*> g_SurfaceMap;
extern LPDIRECTDRAWSURFACE g_pRealPrimary;
extern LPDIRECTDRAWCLIPPER g_pClipper;
extern HWND g_GameHwnd;

extern DWORD g_ActiveWidth;
extern DWORD g_ActiveHeight;

IDirectDrawSurface* UnwrapSurface(LPDIRECTDRAWSURFACE surf);
void SaveVRAMToBMP(const char* filename, void* surfacePtr, int pitch, int width, int height);
void SaveSurfaceToBMP(IDirectDrawSurface* surf, const char* folder, const char* filename);
void LogMessage(const char* format, ...);
void InitLogging();
void LogSystemEnvironment(); 