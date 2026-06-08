#include "FpsUncapFix.h"
#include "Settings.h"
#include "Utils.h"
#include <windows.h>

static void PatchMemory(BYTE* address, BYTE* newBytes, size_t size) {
    DWORD oldProtect;
    if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(address, newBytes, size);
        VirtualProtect(address, size, oldProtect, &oldProtect);
    }
}

void FpsUncapFix::Install() {
    if (!g_Settings.bUnlockFPS) {
        LogMessage("FpsUncapFix: Disabled in INI. Internal frame limiter remains active.");
        return;
    }

    LogMessage("FpsUncapFix: Patching main loop frame limiter to uncap FPS...");

    // =========================================================================
    // PATCH 1: Bypass the frame skipper logic entirely.
    // At sub_414150+EF (0x0041423F), the game does:
    //   cmp edx, ecx
    //   jbe short loc_414293  (Original bytes: 76 52)
    // 
    // We change this to:
    //   jmp loc_414287        (New bytes: E9 43 00 00 00)
    // 
    // This forces the game to always execute loc_414287, which sets 
    // dword_4444AC = 1 and then jumps to the rendering phase, completely 
    // ignoring the 20ms time check.
    // =========================================================================
    BYTE* patch1Address = (BYTE*)0x0041423F;
    BYTE patch1Bytes[] = { 0xE9, 0x43, 0x00, 0x00, 0x00 };
    PatchMemory(patch1Address, patch1Bytes, 5);

    // =========================================================================
    // PATCH 2: Disable the busy-wait loop (Redundant if Patch 1 works, but safe to have)
    // At sub_414150+133 (0x00414283), the game does:
    //   jnb short loc_414268  (Original bytes: 73 E3)
    // We change this to:
    //   jmp short loc_4142AB  (New bytes: EB 26)
    // =========================================================================
    BYTE* patch2Address = (BYTE*)0x00414283;
    BYTE patch2Bytes[] = { 0xEB, 0x26 };
    PatchMemory(patch2Address, patch2Bytes, 2);

    // Verification
    BYTE verify1[5];
    memcpy(verify1, patch1Address, 5);
    LogMessage("FpsUncapFix: Verification 1 at 0x%08X -> Bytes: %02X %02X %02X %02X %02X (Expected: E9 43 00 00 00)",
        (DWORD)patch1Address, verify1[0], verify1[1], verify1[2], verify1[3], verify1[4]);

    if (verify1[0] == 0xE9 && verify1[1] == 0x43) {
        LogMessage("FpsUncapFix: Frame skipper bypass CONFIRMED active in memory.");
    }
    else {
        LogMessage("FpsUncapFix: WARNING! Patch 1 was overwritten or failed to apply.");
    }
}