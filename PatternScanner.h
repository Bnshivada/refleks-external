#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

// ================================================================
//  PatternScanner — AOB (Array of Bytes) tarayıcı
//  Cheat Engine kullanmadan memory'den adres bulur
//  ETS2 1.58 için optimize edilmiştir
// ================================================================

class PatternScanner {
public:

    // ----------------------------------------------------------------
    //  Pattern string'i parse et
    //  Örnek: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
    //  ?? = wildcard (herhangi bir byte)
    // ----------------------------------------------------------------
    static std::vector<int> ParsePattern(const std::string& pattern) {
        std::vector<int> bytes;
        std::istringstream stream(pattern);
        std::string token;

        while (stream >> token) {
            if (token == "??") {
                bytes.push_back(-1);  // Wildcard
            } else {
                bytes.push_back(std::stoi(token, nullptr, 16));
            }
        }
        return bytes;
    }

    // ----------------------------------------------------------------
    //  Bellek bölgesinde pattern ara
    //  handle    : OpenProcess ile alınan handle
    //  start     : tarama başlangıç adresi
    //  size      : kaç byte taranacak
    //  pattern   : aranacak byte dizisi ("48 8B 05 ?? ??")
    //  returns   : bulunan adres, 0 ise bulunamadı
    // ----------------------------------------------------------------
    static uintptr_t FindPattern(
        HANDLE handle,
        uintptr_t start,
        size_t size,
        const std::string& pattern)
    {
        auto patBytes = ParsePattern(pattern);
        if (patBytes.empty()) return 0;

        // Belleği chunk'lar halinde oku (4MB'lık parçalar)
        const size_t CHUNK = 0x400000;
        std::vector<BYTE> buffer(CHUNK);

        for (size_t offset = 0; offset < size; offset += CHUNK) {
            SIZE_T bytesRead = 0;
            size_t toRead = min(CHUNK, size - offset);

            if (!ReadProcessMemory(
                handle,
                (LPCVOID)(start + offset),
                buffer.data(),
                toRead,
                &bytesRead))
            {
                continue;  // Okuma hatası → sonraki chunk
            }

            // Bu chunk içinde pattern ara
            for (size_t i = 0; i + patBytes.size() <= bytesRead; i++) {
                bool found = true;

                for (size_t j = 0; j < patBytes.size(); j++) {
                    // Wildcard (-1) her byte ile eşleşir
                    if (patBytes[j] != -1 && buffer[i + j] != (BYTE)patBytes[j]) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    return start + offset + i;  // Adres bulundu!
                }
            }
        }

        return 0;  // Bulunamadı
    }

    // ----------------------------------------------------------------
    //  Tüm process memory bölgelerini tara
    //  (En kapsamlı tarama — yavaş ama kesin)
    // ----------------------------------------------------------------
    static uintptr_t ScanAllRegions(HANDLE handle, const std::string& pattern) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        uintptr_t addr = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
        uintptr_t maxAddr = (uintptr_t)sysInfo.lpMaximumApplicationAddress;

        while (addr < maxAddr) {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQueryEx(handle, (LPCVOID)addr, &mbi, sizeof(mbi))) {
                addr += 0x1000;
                continue;
            }

            // Sadece okunabilir, committed, private/image bölgeleri tara
            bool readable = (mbi.State == MEM_COMMIT) &&
                           !(mbi.Protect & PAGE_NOACCESS) &&
                           !(mbi.Protect & PAGE_GUARD);

            if (readable) {
                uintptr_t result = FindPattern(handle, addr, mbi.RegionSize, pattern);
                if (result != 0) return result;
            }

            addr += mbi.RegionSize;
        }

        return 0;
    }

    // ----------------------------------------------------------------
    //  RIP-relative adres çöz
    //  x64'te çoğu global pointer şu şekilde erişilir:
    //  MOV RAX, [RIP + offset]  → 48 8B 05 [4 byte offset]
    //  Pattern bulununca bu fonksiyon gerçek adresi verir
    // ----------------------------------------------------------------
    static uintptr_t ResolveRIP(HANDLE handle, uintptr_t instrAddr, int opcodeLen = 7) {
        // instruction'dan sonraki 4 byte'ı oku (relative offset)
        int32_t relOffset = 0;
        ReadProcessMemory(handle, (LPCVOID)(instrAddr + opcodeLen - 4),
                         &relOffset, sizeof(relOffset), nullptr);

        // Gerçek adres = instruction sonu + relative offset
        return instrAddr + opcodeLen + relOffset;
    }
};
