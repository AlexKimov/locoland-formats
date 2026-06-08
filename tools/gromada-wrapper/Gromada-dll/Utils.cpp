#include "Utils.h"
#include "Settings.h"
#include <vector>
#include <fstream>
#include <cstdarg>
#include <ctime>
#include <windows.h>

std::map<IDirectDrawSurface*, IDirectDrawSurface*> g_SurfaceMap;
LPDIRECTDRAWSURFACE g_pRealPrimary = nullptr;
LPDIRECTDRAWCLIPPER g_pClipper = nullptr;
HWND g_GameHwnd = nullptr;

DWORD g_ActiveWidth = 640;
DWORD g_ActiveHeight = 480;

static char g_logFilename[MAX_PATH] = { 0 };
static bool g_loggingInitialized = false;

void LogSystemEnvironment() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    char cwd[MAX_PATH] = { 0 };
    GetCurrentDirectoryA(MAX_PATH, cwd);

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int monitors = GetSystemMetrics(SM_CMONITORS);

    int dpiX = 96, dpiY = 96;
    HDC hdc = GetDC(NULL);
    if (hdc) {
        dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(NULL, hdc);
    }

    // ОБЪЕДИНЕНО: Вся информация теперь в одном вызове LogMessage с одним временным штампом
    LogMessage("System Info: EXE='%s' | DIR='%s' | CPU=%d cores (Arch:%d) | Monitor=%dx%d (x%d) | DPI=%d,%d (100%%=%d)",
        exePath, cwd, sysInfo.dwNumberOfProcessors, sysInfo.wProcessorArchitecture,
        screenW, screenH, monitors, dpiX, dpiY, 96);
}

void InitLogging() {
    if (!g_Settings.bEnableLogging || g_loggingInitialized) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf_s(g_logFilename, "ddraw_log_%04d%02d%02d_%02d%02d%02d.log",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    g_loggingInitialized = true;

    LogSystemEnvironment();
}

void LogMessage(const char* format, ...) {
    if (!g_Settings.bEnableLogging) return;
    if (!g_loggingInitialized) InitLogging();
    if (!g_loggingInitialized) return;

    FILE* f;
    fopen_s(&f, g_logFilename, "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

IDirectDrawSurface* UnwrapSurface(LPDIRECTDRAWSURFACE surf) {
    if (!surf) return nullptr;
    auto it = g_SurfaceMap.find(surf);
    return (it != g_SurfaceMap.end()) ? it->second : surf;
}

void SaveVRAMToBMP(const char* filename, void* surfacePtr, int pitch, int width, int height) {
    CreateDirectoryA(g_Settings.screenshotFolder, nullptr);
    char fullPath[MAX_PATH];
    sprintf_s(fullPath, "%s\\%s", g_Settings.screenshotFolder, filename);

    std::ofstream file(fullPath, std::ios::binary);
    if (!file) return;

    BITMAPFILEHEADER bfh = { 0 };
    BITMAPINFOHEADER bih = { 0 };
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = (LONG)width;
    bih.biHeight = -(LONG)height;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    int rowSize = ((width * 3 + 3) / 4) * 4;
    int imageSize = rowSize * height;
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    file.write(reinterpret_cast<char*>(&bfh), sizeof(bfh));
    file.write(reinterpret_cast<char*>(&bih), sizeof(bih));

    std::vector<uint8_t> rowBuffer(rowSize, 0);
    uint16_t* src = static_cast<uint16_t*>(surfacePtr);
    int pitchPixels = pitch / 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel = src[x];
            rowBuffer[x * 3 + 0] = (pixel & 0x1F) << 3;
            rowBuffer[x * 3 + 1] = ((pixel >> 5) & 0x3F) << 2;
            rowBuffer[x * 3 + 2] = ((pixel >> 11) & 0x1F) << 3;
        }
        file.write(reinterpret_cast<char*>(rowBuffer.data()), rowSize);
        src += pitchPixels;
    }
}

void SaveSurfaceToBMP(IDirectDrawSurface* surf, const char* folder, const char* filename) {
    if (!surf) return;
    DDSURFACEDESC ddsd = { sizeof(ddsd) };
    if (surf->Lock(nullptr, &ddsd, DDLOCK_WAIT | DDLOCK_READONLY, nullptr) != DD_OK) return;

    DWORD w = (ddsd.dwFlags & DDSD_WIDTH) ? ddsd.dwWidth : 640;
    DWORD h = (ddsd.dwFlags & DDSD_HEIGHT) ? ddsd.dwHeight : 480;

    CreateDirectoryA(folder, nullptr);
    char fullPath[MAX_PATH];
    sprintf_s(fullPath, "%s\\%s", folder, filename);

    std::ofstream file(fullPath, std::ios::binary);
    if (file) {
        BITMAPFILEHEADER bfh = { 0 };
        BITMAPINFOHEADER bih = { 0 };
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = (LONG)w;
        bih.biHeight = -(LONG)h;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;

        int rowSize = ((w * 3 + 3) / 4) * 4;
        int imageSize = rowSize * h;
        bfh.bfType = 0x4D42;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bfh.bfSize = bfh.bfOffBits + imageSize;

        file.write(reinterpret_cast<char*>(&bfh), sizeof(bfh));
        file.write(reinterpret_cast<char*>(&bih), sizeof(bih));

        std::vector<uint8_t> rowBuffer(rowSize, 0);
        uint16_t* src = static_cast<uint16_t*>(ddsd.lpSurface);
        int pitchPixels = ddsd.lPitch / 2;

        for (DWORD y = 0; y < h; y++) {
            for (DWORD x = 0; x < w; x++) {
                uint16_t pixel = src[x];
                rowBuffer[x * 3 + 0] = (pixel & 0x1F) << 3;
                rowBuffer[x * 3 + 1] = ((pixel >> 5) & 0x3F) << 2;
                rowBuffer[x * 3 + 2] = ((pixel >> 11) & 0x1F) << 3;
            }
            file.write(reinterpret_cast<char*>(rowBuffer.data()), rowSize);
            src += pitchPixels;
        }
    }
    surf->Unlock(nullptr);
}