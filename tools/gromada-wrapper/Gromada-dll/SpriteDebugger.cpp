#include "SpriteDebugger.h"
#include "Settings.h"
#include "Utils.h"
#include <stdio.h>
#include <vector>

// Static member definitions (Ensure these are public in SpriteDebugger.h)
bool SpriteDebugger::s_bActive = false;
int SpriteDebugger::s_CaptureCount = 0;

extern "C" void __cdecl OnRLESpriteDrawn(void* framebuffer, int pitchPixels) {
    if (!SpriteDebugger::s_bActive || !framebuffer || pitchPixels <= 0) return;

    // Auto-stop after 500 captures (~1-2 frames) to prevent infinite disk filling
    if (SpriteDebugger::s_CaptureCount >= 500) {
        SpriteDebugger::Toggle();
        return;
    }

    int w = g_Settings.resolutionWidth;
    int h = g_Settings.resolutionHeight;

    CreateDirectoryA(g_Settings.visualDebugFolder, nullptr);
    char fname[256];
    sprintf_s(fname, "%s\\Step_%05d.bmp", g_Settings.visualDebugFolder, SpriteDebugger::s_CaptureCount++);

    // Use fopen_s to satisfy MSVC security checks (fixes C4996)
    FILE* f = nullptr;
    if (fopen_s(&f, fname, "wb") != 0 || !f) return;

    BITMAPFILEHEADER bfh = { 0 };
    BITMAPINFOHEADER bih = { 0 };
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = w;
    bih.biHeight = -h; // Top-down
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    int rowSize = ((w * 3 + 3) / 4) * 4;
    bfh.bfSize = bfh.bfOffBits + (rowSize * h);

    fwrite(&bfh, sizeof(bfh), 1, f);
    fwrite(&bih, sizeof(bih), 1, f);

    uint8_t* src = static_cast<uint8_t*>(framebuffer);
    std::vector<uint8_t> buf(rowSize, 0);

    // Pitch from 0x1A50 is in PIXELS. Multiply by 2 for byte stride (16-bit color).
    int byteStride = pitchPixels * 2;

    for (int y = 0; y < h; y++) {
        uint16_t* row = reinterpret_cast<uint16_t*>(src + y * byteStride);
        for (int x = 0; x < w; x++) {
            uint16_t p = row[x];
            buf[x * 3 + 0] = (p & 0x1F) << 3;         // B
            buf[x * 3 + 1] = ((p >> 5) & 0x3F) << 2;  // G
            buf[x * 3 + 2] = ((p >> 11) & 0x1F) << 3; // R
        }
        fwrite(buf.data(), 1, rowSize, f);
    }
    fclose(f);
}

__declspec(naked) void Hook_RLEDecoder_Entry() {
    __asm {
        pushfd
        pushad

        // ecx holds VideoManager* (this pointer)
        mov eax, ecx
        mov ebx, [eax + 0x1F9B]   // ebx = framebuffer pointer
        mov ecx, [eax + 0x1A50]   // ecx = pitch (in pixels)

        push ecx
        push ebx
        call OnRLESpriteDrawn
        add esp, 8

        popad
        popfd

        // Execute original overwritten instructions (9 bytes)
        // 0x00402020: push ebp
        // 0x00402021: mov ebp, esp
        // 0x00402023: sub esp, 828h
        push ebp
        mov ebp, esp
        sub esp, 828h

        // Jump back to 0x00402029 (push ebx)
        push 0x00402029
        ret
    }
}

void SpriteDebugger::InstallHook() {
    DWORD oldProtect;
    VirtualProtect((void*)0x00402020, 9, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Write JMP to our hook (5 bytes)
    BYTE jmp[5] = { 0xE9 };
    DWORD offset = (DWORD)Hook_RLEDecoder_Entry - 0x00402020 - 5;
    memcpy(jmp + 1, &offset, 4);

    memcpy((void*)0x00402020, jmp, 5);

    // NOP the remaining 4 bytes of the original 9-byte instruction block
    memset((void*)(0x00402020 + 5), 0x90, 4);

    VirtualProtect((void*)0x00402020, 9, oldProtect, &oldProtect);
    LogMessage("Sprite Debugger: Hooked sub_402020 (0x00402020)");
}

void SpriteDebugger::Toggle() {
    s_bActive = !s_bActive;
    if (s_bActive) {
        s_CaptureCount = 0;
        LogMessage("=== Step-by-Step RLE Capture STARTED ===");
    }
    else {
        LogMessage("=== Step-by-Step RLE Capture STOPPED. Saved %d steps. ===", s_CaptureCount);
    }
}