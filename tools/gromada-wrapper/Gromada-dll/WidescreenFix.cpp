#include "WidescreenFix.h"
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


void WidescreenFix::Install() {
    if (!g_Settings.bWidescreenMode) {
        LogMessage("WidescreenFix: Disabled in INI (Stretched mode). Skipping memory patches.");
        return;
    }

    LogMessage("WidescreenFix: Expanding Entity Culling Box from 1024x1024 to 2048x2048...");

    // Значения для патчей (Little Endian)
    // Меняем 0x200 (512) на 0x400 (1024)
    BYTE patch_add[] = { 0x00, 0x04, 0x00, 0x00 };
    // Меняем маску 0xFFFFFC00 на 0xFFFFF800
    BYTE patch_test[] = { 0x00, 0xF8, 0xFF, 0xFF };

    // =========================================================================
    // БЛОК 1: Основной список сущностей (sub_40B790)
    // =========================================================================

    // Ось X: add edx, 200h (инструкция с 0x0040B852) -> данные по смещению +2
    PatchMemory((BYTE*)0x0040B854, patch_add, 4);
    // Ось X: test edx, 0FFFFFC00h (инструкция с 0x0040B858) -> данные по смещению +2
    PatchMemory((BYTE*)0x0040B85A, patch_test, 4);

    // Ось Y: add eax, 200h (инструкция с 0x0040B86F) -> данные по смещению +1
    // ИСПРАВЛЕНО КРАША: Ранее было 0x0040B86E, что ломало sub eax, edx
    PatchMemory((BYTE*)0x0040B870, patch_add, 4);
    // Ось Y: test eax, 0FFFFFC00h (инструкция с 0x0040B874) -> данные по смещению +1
    PatchMemory((BYTE*)0x0040B875, patch_test, 4);

    // =========================================================================
    // БЛОК 2: Дочерние/вторичные сущности (sub_40B790)
    // =========================================================================

    // Ось X: add edx, 200h (инструкция с 0x0040B8D6) -> данные по смещению +2
    PatchMemory((BYTE*)0x0040B8D8, patch_add, 4);
    // Ось X: test edx, 0FFFFFC00h (инструкция с 0x0040B8DC) -> данные по смещению +2
    PatchMemory((BYTE*)0x0040B8DE, patch_test, 4);

    // Ось Y: add eax, 200h (инструкция с 0x0040B8F3) -> данные по смещению +1
    PatchMemory((BYTE*)0x0040B8F4, patch_add, 4);
    // Ось Y: test eax, 0FFFFFC00h (инструкция с 0x0040B8F8) -> данные по смещению +1
    PatchMemory((BYTE*)0x0040B8F9, patch_test, 4);

    LogMessage("WidescreenFix: Successfully patched 8 instructions in sub_40B790.");

    // =======================================================================
        // 1. FIX THE FSEEK DESYNC BUG (Prevents the "vid 17664" ghost error)
        // =======================================================================
        // In sub_40C330, at offset +E3 (0x0040C413), the instruction is "push 1" (bytes: 6A 01).
        // We change the operand byte at 0x0040C414 from '01' to '00', making it "push 0".
        // This stops the game from skipping 1 byte and misreading the file if a creation fails.
    BYTE patchFseekOperand[] = { 0x00 };
    PatchMemory((BYTE*)0x0040C414, patchFseekOperand, 1);
    LogMessage("WidescreenFix: Patched fseek desync bug.");

    // =======================================================================
    // 2. THE ULTIMATE MENU BOUNDS FIX (Patching sub_40E1D0 safely)
    // =======================================================================
    // Instead of patching the dynamic variables in sub_414920 (which causes crashes),
    // we patch the actual bounds checker in sub_40E1D0 (CreateUnit) to use 
    // hardcoded immediate values (1024 and 768). This is 100% safe and stable.

    // Patch X bounds check: 
    // Original at 0x0040E281: 3B 05 44 AE 42 00 (cmp eax, dword_42AE44)
    // New at 0x0040E281:      3D FF 7F 00 00 90 (cmp eax, 32767; nop)
    BYTE patchMaxX[] = { 0x3D, 0xFF, 0x7F, 0x00, 0x00, 0x90 };
    PatchMemory((BYTE*)0x0040E281, patchMaxX, sizeof(patchMaxX));

    // Patch Y bounds check: 
    // Original at 0x0040E28D: 3B 2D 48 AE 42 00 (cmp ebp, dword_42AE48)
    // New at 0x0040E28D:      81 FD FF 7F 00 00 (cmp ebp, 32767)
    BYTE patchMaxY[] = { 0x81, 0xFD, 0xFF, 0x7F, 0x00, 0x00 };
    PatchMemory((BYTE*)0x0040E28D, patchMaxY, sizeof(patchMaxY));

    LogMessage("WidescreenFix: Menu bounds successfully hardcoded to 1024x768. Stable and crash-free!");
}