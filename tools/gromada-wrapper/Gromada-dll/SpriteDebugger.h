#pragma once
#include <windows.h>

class SpriteDebugger {
public:
    static void InstallHook();
    static void Toggle();
    static bool IsActive() { return s_bActive; }

    // Сделаны public, чтобы extern "C" callback-функция хука могла их читать и изменять
    static bool s_bActive;
    static int s_CaptureCount;
};

// C-compatible callback для naked-хука
extern "C" void __cdecl OnRLESpriteDrawn(void* framebuffer, int pitchPixels);