#include "WindowManager.h"
#include "Settings.h"
#include "Utils.h"
#include <windowsx.h>

WNDPROC WindowManager::s_OriginalProc = nullptr;
HWND WindowManager::s_TargetHwnd = nullptr;
static bool s_bMouseTrapped = false;

void WindowManager::Attach(HWND hWnd) {
    if (!hWnd || s_TargetHwnd) return;
    s_TargetHwnd = hWnd;
    s_OriginalProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
    LogMessage("Subclassed game window (Original WndProc: %p)", s_OriginalProc);
}

void WindowManager::Detach() {
    if (s_TargetHwnd && s_OriginalProc) {
        SetWindowLongPtr(s_TargetHwnd, GWLP_WNDPROC, (LONG_PTR)s_OriginalProc);
        s_TargetHwnd = nullptr; s_OriginalProc = nullptr;
    }
}

static LPARAM TranslateMouseCoords(HWND hWnd, LPARAM lParam) {
    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    ScreenToClient(hWnd, &pt);

    // If INI is 0x0, dst matches src (1:1 scale). Otherwise, it scales to the fixed INI size.
    int dstW = (g_Settings.resolutionWidth > 0) ? g_Settings.resolutionWidth : g_ActiveWidth;
    int dstH = (g_Settings.resolutionHeight > 0) ? g_Settings.resolutionHeight : g_ActiveHeight;
    int srcW = g_ActiveWidth;
    int srcH = g_ActiveHeight;

    int offsetX = 0, offsetY = 0, scaledW = dstW, scaledH = dstH;

    if (g_Settings.bMaintainAspectRatio && dstW > 0 && dstH > 0) {
        float srcAR = (float)srcW / (float)srcH;
        float dstAR = (float)dstW / (float)dstH;
        if (srcAR > dstAR) scaledH = (int)(dstW / srcAR);
        else scaledW = (int)(dstH * srcAR);
        offsetX = (dstW - scaledW) / 2;
        offsetY = (dstH - scaledH) / 2;
    }

    pt.x -= offsetX; pt.y -= offsetY;
    float scaleX = (float)srcW / (float)scaledW;
    float scaleY = (float)srcH / (float)scaledH;
    pt.x = (LONG)(pt.x * scaleX); pt.y = (LONG)(pt.y * scaleY);

    if (pt.x < 0) pt.x = 0; if (pt.x >= (LONG)srcW) pt.x = srcW - 1;
    if (pt.y < 0) pt.y = 0; if (pt.y >= (LONG)srcH) pt.y = srcH - 1;
    return MAKELPARAM(pt.x, pt.y);
}

static void TrapMouse(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
    ClientToScreen(hWnd, &tl); ClientToScreen(hWnd, &br);
    RECT clipRect = { tl.x, tl.y, br.x, br.y };
    ClipCursor(&clipRect); s_bMouseTrapped = true;
}

static void ReleaseMouse() { ClipCursor(NULL); s_bMouseTrapped = false; }

LRESULT CALLBACK WindowManager::NewWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {

    if (g_Settings.bWindowed && Msg == WM_NCCALCSIZE && g_Settings.resolutionWidth > 0 && g_Settings.resolutionHeight > 0) {
        LRESULT res = CallWindowProc(s_OriginalProc, hWnd, Msg, wParam, lParam);
        if (wParam == TRUE && lParam) {
            NCCALCSIZE_PARAMS* pParams = (NCCALCSIZE_PARAMS*)lParam;
            pParams->rgrc[0].right = pParams->rgrc[0].left + g_Settings.resolutionWidth;
            pParams->rgrc[0].bottom = pParams->rgrc[0].top + g_Settings.resolutionHeight;
        }
        return res;
    }

    if (Msg == WM_NCHITTEST) {
        LPARAM fixedLParam = TranslateMouseCoords(hWnd, lParam);

        if (g_Settings.bVerboseLogging) {
            static int hitTestCounter = 0;
            if (++hitTestCounter % 50 == 0) {
                int origX = GET_X_LPARAM(lParam);
                int origY = GET_Y_LPARAM(lParam);
                int scaledX = GET_X_LPARAM(fixedLParam);
                int scaledY = GET_Y_LPARAM(fixedLParam);
                LogMessage("MOUSE HITTEST: Orig(%d,%d) -> Scaled(%d,%d) | Active: %dx%d | Window: %dx%d",
                    origX, origY, scaledX, scaledY, g_ActiveWidth, g_ActiveHeight, g_Settings.resolutionWidth, g_Settings.resolutionHeight);
            }
        }

        CallWindowProc(s_OriginalProc, hWnd, Msg, wParam, fixedLParam);

        if (g_Settings.bWindowed) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rc; GetWindowRect(hWnd, &rc);
            if (pt.y >= rc.top && pt.y < rc.top + 30) {
                int btnW = GetSystemMetrics(SM_CXSIZE);
                if (pt.x >= rc.right - btnW) return HTCLOSE;
                if (pt.x >= rc.right - 2 * btnW && pt.x < rc.right - btnW) return HTMINBUTTON;
                if (pt.x < rc.right - 2 * btnW) return HTCAPTION;
            }
        }
        return HTCLIENT;
    }

    switch (Msg) {
    case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP: case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP: case WM_NCMBUTTONDOWN: case WM_NCMBUTTONUP: {
        if (s_bMouseTrapped) ReleaseMouse();
        return DefWindowProc(hWnd, Msg, wParam, lParam);
    }
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: {
        bool isSafeZone = false;
        if (g_Settings.bWindowed) {
            POINT cursorPos; GetCursorPos(&cursorPos);
            RECT windowRect; GetWindowRect(hWnd, &windowRect);
            isSafeZone = (cursorPos.y >= windowRect.top && cursorPos.y < windowRect.top + 30);
        }
        if (!s_bMouseTrapped && !isSafeZone) TrapMouse(hWnd);
        LPARAM fixedLParam = TranslateMouseCoords(hWnd, lParam);
        return CallWindowProc(s_OriginalProc, hWnd, Msg, wParam, fixedLParam);
    }
    case WM_MOUSEMOVE: case WM_LBUTTONUP: case WM_RBUTTONUP: {
        LPARAM fixedLParam = TranslateMouseCoords(hWnd, lParam);
        return CallWindowProc(s_OriginalProc, hWnd, Msg, wParam, fixedLParam);
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        if (GetAsyncKeyState(g_Settings.mouseReleaseKey) & 0x8000 && s_bMouseTrapped) ReleaseMouse();
        break;
    }
    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE && s_bMouseTrapped) ReleaseMouse();
        break;
    }
    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS* wp = (WINDOWPOS*)lParam;
        wp->flags &= ~SWP_NOMOVE;
        if (!(wp->flags & SWP_NOMOVE) && wp->x == 0 && wp->y == 0) {
            RECT rc; GetWindowRect(hWnd, &rc);
            if (rc.left != 0 || rc.top != 0) { wp->x = rc.left; wp->y = rc.top; }
        }
        break;
    }
    case WM_MOVE: {
        if (s_bMouseTrapped) TrapMouse(hWnd);
        return DefWindowProc(hWnd, Msg, wParam, lParam);
    }
    case WM_SYSCOMMAND: {
        UINT cmd = wParam & 0xFFF0;
        if (cmd == SC_CLOSE || cmd == SC_MINIMIZE || cmd == SC_RESTORE) return DefWindowProc(hWnd, Msg, wParam, lParam);
        if (cmd == SC_MAXIMIZE) return 0;
        break;
    }
    case WM_SETCURSOR: {
        if (g_Settings.bWindowed) {
            POINT pt; GetCursorPos(&pt);
            RECT rc; GetWindowRect(hWnd, &rc);
            if (pt.y >= rc.top && pt.y < rc.top + 30) {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                return TRUE;
            }
        }
        break;
    }
    }
    return CallWindowProc(s_OriginalProc, hWnd, Msg, wParam, lParam);
}