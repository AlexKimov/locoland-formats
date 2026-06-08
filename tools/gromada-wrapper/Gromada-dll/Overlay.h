#pragma once
#include <d3d9.h>
#include <windows.h>

// Глобальная переменная для FPS
extern float g_CurrentFPS;

// Функции оверлея
void AddOverlayMessage(const char* text);
void RenderOverlay(IDirect3DSurface9* pBackBuffer, int screenW, int screenH);