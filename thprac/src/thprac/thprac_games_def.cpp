#include <Windows.h>
#include <string.h>
#include "thprac_games_def.h"
#include <cstdint>
#include <metrohash128.h>

namespace THPrac {

inline bool HashCompare(uint32_t hash1[4], uint32_t hash2[4])
{
    for (int i = 0; i < 4; ++i) {
        if (hash1[i] != hash2[i]) {
            return false;
        }
    }
    return true;
}
inline bool OepCompare(uint32_t oepCode1[10], uint32_t opeCode2[10])
{
    for (int i = 0; i < 10; ++i) {
        if (oepCode1[i] != opeCode2[i]) {
            return false;
        }
    }
    return true;
}
bool ReadMemory(void* buffer, void* addr, size_t size)
{
    SIZE_T bytesRead = 0;
    ReadProcessMemory(GetCurrentProcess(), addr, buffer, size, &bytesRead);
    return bytesRead == size;
}
bool GetExeInfo(void* exeBuffer, size_t exeSize, ExeSig& exeSigOut)
{
    if (exeSize < 128)
        return false;

    IMAGE_DOS_HEADER dosHeader;
    if (!ReadMemory(&dosHeader, exeBuffer, sizeof(IMAGE_DOS_HEADER)) || dosHeader.e_magic != 0x5a4d)
        return false;
    IMAGE_NT_HEADERS ntHeader;
    if (!ReadMemory(&ntHeader, (void*)((DWORD)exeBuffer + dosHeader.e_lfanew), sizeof(IMAGE_NT_HEADERS)) || ntHeader.Signature != 0x00004550)
        return false;

    exeSigOut.timeStamp = ntHeader.FileHeader.TimeDateStamp;
    exeSigOut.textSize = 0;
    for (auto& codeBlock : exeSigOut.oepCode) {
        codeBlock = 0;
    }
    for (auto& hashBlock : exeSigOut.metroHash) {
        hashBlock = 0;
    }

    PIMAGE_SECTION_HEADER pSection = (PIMAGE_SECTION_HEADER)((ULONG_PTR)((LONG)exeBuffer + dosHeader.e_lfanew) + ((LONG)(LONG_PTR) & (((IMAGE_NT_HEADERS*)0)->OptionalHeader)) + ntHeader.FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < ntHeader.FileHeader.NumberOfSections; i++, pSection++) {
        IMAGE_SECTION_HEADER section;
        if (!ReadMemory(&section, pSection, sizeof(IMAGE_SECTION_HEADER)))
            continue;
        if (!strcmp(".text", (char*)section.Name)) {
            exeSigOut.textSize = section.SizeOfRawData;
        }
        DWORD pOepCode = ntHeader.OptionalHeader.AddressOfEntryPoint;
        if (pOepCode >= section.VirtualAddress && pOepCode <= (section.VirtualAddress + section.Misc.VirtualSize)) {
            pOepCode -= section.VirtualAddress;
            pOepCode += section.PointerToRawData;
            pOepCode += (DWORD)exeBuffer;

            uint16_t oepCode[10];
            if (!ReadMemory(&oepCode, (void*)pOepCode, sizeof(oepCode)))
                continue;

            for (unsigned int j = 0; j < 10; ++j) {
                exeSigOut.oepCode[j] = (uint32_t) * (oepCode + j);
                exeSigOut.oepCode[j] ^= (j + 0x41) | ((j + 0x41) << 8);
            }
        }
    }

    if (exeSize < (1 << 23)) {
        MetroHash128::Hash((uint8_t*)exeBuffer, exeSize, (uint8_t*)exeSigOut.metroHash);
    }

    return true;
}
bool GetExeInfoEx(uintptr_t hProcess, uintptr_t base, ExeSig& exeSigOut)
{
    DWORD bytesRead;
    HANDLE hProc = (HANDLE)hProcess;

    IMAGE_DOS_HEADER dosHeader;
    if (!ReadProcessMemory(hProc, (void*)base, &dosHeader, sizeof(IMAGE_DOS_HEADER), &bytesRead)) {
        return false;
    }
    IMAGE_NT_HEADERS ntHeader;
    if (!ReadProcessMemory(hProc, (void*)(base + dosHeader.e_lfanew), &ntHeader, sizeof(IMAGE_NT_HEADERS), &bytesRead)) {
        return false;
    }

    exeSigOut.timeStamp = ntHeader.FileHeader.TimeDateStamp;
    PIMAGE_SECTION_HEADER pSection = (PIMAGE_SECTION_HEADER)((ULONG_PTR)(base + dosHeader.e_lfanew) + ((LONG)(LONG_PTR) & (((IMAGE_NT_HEADERS*)0)->OptionalHeader)) + ntHeader.FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < ntHeader.FileHeader.NumberOfSections; i++, pSection++) {
        IMAGE_SECTION_HEADER section;
        if (!ReadProcessMemory(hProc, (void*)(pSection), &section, sizeof(IMAGE_SECTION_HEADER), &bytesRead)) {
            return false;
        }
        if (!strcmp(".text", (char*)section.Name)) {
            exeSigOut.textSize = section.SizeOfRawData;
            return true;
        }
    }

    return false;
}

};
