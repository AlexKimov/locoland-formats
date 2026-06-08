#pragma once
#include <windows.h>

struct WrapperSettings {
    bool bWindowed;
    int resolutionWidth;
    int resolutionHeight;
    bool bMaintainAspectRatio;
    bool bWidescreenMode;   
    int fpsLimit;
    bool bShowFPS;
    bool bEnableLogging;

    bool bVerboseLogging;
    bool bUnlockFPS; 

    char screenshotFolder[256];
    int screenshotKey;
    char screenshotKeyName[32];

    int mouseReleaseKey;
    char mouseReleaseKeyName[32];

    bool bVisualDebug;
    int visualDebugKey;
    char visualDebugKeyName[32];
    char visualDebugFolder[256];

    void Load();
};

extern WrapperSettings g_Settings;
int StringToVK(const char* name);
void LogAllSettings();