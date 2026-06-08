#include "Overlay.h"
#include "Settings.h"
#include "Utils.h"
#include <vector>
#include <string>
#include <cstring>

float g_CurrentFPS = 0.0f;

struct OverlayMsg {
    char text[256];
    DWORD expireTime;
};

static std::vector<OverlayMsg> g_Messages;
static HFONT g_hFont = nullptr;
static int g_lastFontHeight = 0;

void AddOverlayMessage(const char* text) {
    if (!text) return;
    OverlayMsg msg;
    strncpy_s(msg.text, text, 255);
    msg.text[255] = '\0';
    msg.expireTime = GetTickCount() + 3000;
    g_Messages.push_back(msg);
}

void RenderOverlay(IDirect3DSurface9* pSurface, int screenW, int screenH) {
    if (!pSurface) return;

    HDC hdc;
    HRESULT hrDC = pSurface->GetDC(&hdc);

    if (FAILED(hrDC)) {
        static bool loggedErr = false;
        if (!loggedErr) {
            LogMessage("Overlay: GetDC FAILED (HR=0x%08X).", hrDC);
            loggedErr = true;
        }
        return;
    }

    static bool loggedOk = false;
    if (!loggedOk) {
        LogMessage("Overlay: GetDC SUCCESS on GameSurface (%dx%d). Rendering overlay.", screenW, screenH);
        loggedOk = true;
    }

    int fontHeight = screenH / 30;
    if (fontHeight < 12) fontHeight = 12;

    if (!g_hFont || g_lastFontHeight != fontHeight) {
        if (g_hFont) DeleteObject(g_hFont);
        // Fallback на Arial, так как он гарантированно есть во всех Windows
        g_hFont = CreateFontA(fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        g_lastFontHeight = fontHeight;
    }

    HFONT oldFont = (HFONT)SelectObject(hdc, g_hFont);
    SetBkMode(hdc, TRANSPARENT);

    // 1. Отрисовка FPS (ТОЛЬКО в Fullscreen, в правом верхнем углу)
    if (g_Settings.bShowFPS && !g_Settings.bWindowed) {
        SetTextColor(hdc, RGB(0, 255, 0));
        char fpsText[64];
        sprintf_s(fpsText, "%.1f | %dx%d", g_CurrentFPS, screenW, screenH);

        SIZE textSize;
        GetTextExtentPoint32A(hdc, fpsText, (int)strlen(fpsText), &textSize);

        int textX = screenW - textSize.cx - 15;
        int textY = 15;

        TextOutA(hdc, textX, textY, fpsText, (int)strlen(fpsText));
    }

    // 2. Отрисовка сообщений от инструментов (Слева сверху)
    SetTextColor(hdc, RGB(255, 255, 0));
    int yOffset = 15;
    DWORD now = GetTickCount();

    for (auto it = g_Messages.begin(); it != g_Messages.end(); ) {
        if (now > it->expireTime) {
            it = g_Messages.erase(it);
        }
        else {
            TextOutA(hdc, 15, yOffset, it->text, (int)strlen(it->text));
            yOffset += fontHeight + 5;
            ++it;
        }
    }

    SelectObject(hdc, oldFont);
    pSurface->ReleaseDC(hdc);
}