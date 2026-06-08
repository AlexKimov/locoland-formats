#include "Settings.h"
#include "Utils.h"
#include <cstring>
#include <cstdlib>

WrapperSettings g_Settings = {
    true, 1280, 720, true, false, 60, true, true, true, true, // Added false for bVerboseLogging, false for bUnlockFPS
    "screenshots", VK_SNAPSHOT, "PrintScreen",
    VK_RCONTROL, "RightCtrl",
    false, VK_F11, "F11", "visual_debug"
};

int StringToVK(const char* name) {
    if (!name || !name[0]) return VK_SNAPSHOT;
    if (_stricmp(name, "F1") == 0) return VK_F1; if (_stricmp(name, "F2") == 0) return VK_F2;
    if (_stricmp(name, "F3") == 0) return VK_F3; if (_stricmp(name, "F4") == 0) return VK_F4;
    if (_stricmp(name, "F5") == 0) return VK_F5; if (_stricmp(name, "F6") == 0) return VK_F6;
    if (_stricmp(name, "F7") == 0) return VK_F7; if (_stricmp(name, "F8") == 0) return VK_F8;
    if (_stricmp(name, "F9") == 0) return VK_F9; if (_stricmp(name, "F10") == 0) return VK_F10;
    if (_stricmp(name, "F11") == 0) return VK_F11; if (_stricmp(name, "F12") == 0) return VK_F12;
    if (_stricmp(name, "PrintScreen") == 0 || _stricmp(name, "PrtSc") == 0) return VK_SNAPSHOT;
    if (_stricmp(name, "Insert") == 0) return VK_INSERT;
    if (_stricmp(name, "Home") == 0) return VK_HOME;
    if (_stricmp(name, "End") == 0) return VK_END;
    if (_stricmp(name, "RightCtrl") == 0) return VK_RCONTROL;
    if (_stricmp(name, "RightAlt") == 0) return VK_RMENU;
    if (_stricmp(name, "LeftCtrl") == 0) return VK_LCONTROL;
    if (_stricmp(name, "LeftAlt") == 0) return VK_LMENU;
    if (_stricmp(name, "RightShift") == 0) return VK_RSHIFT;
    if (_stricmp(name, "LeftShift") == 0) return VK_LSHIFT;
    if (strlen(name) == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return 'A' + (c - 'a');
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }
    return VK_SNAPSHOT;
}

void WrapperSettings::Load() {
    char iniPath[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, iniPath);
    strcat_s(iniPath, "\\ddraw.ini");

    g_Settings.bWindowed = GetPrivateProfileIntA("Settings", "Windowed", 1, iniPath) != 0;
    g_Settings.resolutionWidth = GetPrivateProfileIntA("Settings", "ResolutionWidth", 1280, iniPath);
    g_Settings.resolutionHeight = GetPrivateProfileIntA("Settings", "ResolutionHeight", 720, iniPath);
    g_Settings.bMaintainAspectRatio = GetPrivateProfileIntA("Settings", "MaintainAspectRatio", 1, iniPath) != 0;
    g_Settings.bWidescreenMode = GetPrivateProfileIntA("Settings", "WidescreenMode", 0, iniPath) != 0;
    g_Settings.fpsLimit = GetPrivateProfileIntA("Settings", "FPSLimit", 60, iniPath);
    g_Settings.bShowFPS = GetPrivateProfileIntA("Settings", "ShowFPS", 1, iniPath) != 0;
    g_Settings.bEnableLogging = GetPrivateProfileIntA("Settings", "EnableLogging", 1, iniPath) != 0;

    g_Settings.bVerboseLogging = GetPrivateProfileIntA("Settings", "VerboseLogging", 0, iniPath) != 0;
    g_Settings.bUnlockFPS = GetPrivateProfileIntA("Settings", "UnlockFPS", 0, iniPath) != 0;

    GetPrivateProfileStringA("Settings", "ScreenshotFolder", "screenshots", g_Settings.screenshotFolder, 256, iniPath);
    GetPrivateProfileStringA("Settings", "ScreenshotKey", "PrintScreen", g_Settings.screenshotKeyName, 32, iniPath);
    g_Settings.screenshotKey = StringToVK(g_Settings.screenshotKeyName);

    GetPrivateProfileStringA("Settings", "MouseTrapReleaseKey", "RightCtrl", g_Settings.mouseReleaseKeyName, 32, iniPath);
    g_Settings.mouseReleaseKey = StringToVK(g_Settings.mouseReleaseKeyName);

    g_Settings.bVisualDebug = GetPrivateProfileIntA("Settings", "VisualDebug", 0, iniPath) != 0;
    GetPrivateProfileStringA("Settings", "VisualDebugKey", "F11", g_Settings.visualDebugKeyName, 32, iniPath);
    g_Settings.visualDebugKey = StringToVK(g_Settings.visualDebugKeyName);
    GetPrivateProfileStringA("Settings", "VisualDebugFolder", "visual_debug", g_Settings.visualDebugFolder, 256, iniPath);
}

void LogAllSettings() {
    LogMessage("========================================");
    LogMessage("Gromada DirectDraw Wrapper Settings");
    LogMessage("========================================");
    LogMessage("  Windowed Mode: %s", g_Settings.bWindowed ? "ON" : "OFF");
    LogMessage("  Target Window Size: %dx%d (0x0 means dynamic/stretch)", g_Settings.resolutionWidth, g_Settings.resolutionHeight);
    LogMessage("  Maintain Aspect Ratio: %s", g_Settings.bMaintainAspectRatio ? "ON" : "OFF");
    LogMessage("  Widescreen Mode: %s", g_Settings.bWidescreenMode ? "ON" : "OFF");
    LogMessage("  FPS Limit: %d", g_Settings.fpsLimit);
    LogMessage("  Show FPS Overlay: %s", g_Settings.bShowFPS ? "ON" : "OFF");
    LogMessage("  Logging: %s", g_Settings.bEnableLogging ? "ON" : "OFF");
    LogMessage("  Screenshot Folder: %s", g_Settings.screenshotFolder);
    LogMessage("  Screenshot Key: %s (VK=0x%02X)", g_Settings.screenshotKeyName, g_Settings.screenshotKey);
    LogMessage("  Mouse Trap Release: %s (VK=0x%02X)", g_Settings.mouseReleaseKeyName, g_Settings.mouseReleaseKey);
    LogMessage("  Visual Debug: %s (Key: %s, VK=0x%02X)", g_Settings.bVisualDebug ? "ON" : "OFF", g_Settings.visualDebugKeyName, g_Settings.visualDebugKey);
    LogMessage("========================================");
}