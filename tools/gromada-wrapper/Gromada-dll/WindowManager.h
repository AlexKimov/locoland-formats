#pragma once
#include <windows.h>

class WindowManager {
public:
    static void Attach(HWND hWnd);
    static void Detach();

private:
    static LRESULT CALLBACK NewWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    // C++98/11 compatible static members (declared in header, defined in cpp)
    static WNDPROC s_OriginalProc;
    static HWND s_TargetHwnd;
};