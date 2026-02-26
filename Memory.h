#pragma once
// ================================================
//   REFLEKS EXTERNAL v3.0
//   ETS2 1.58 | C++ | ImGui | External
//   Özellikler: RPM Boost, No Damage, Hız Göstergesi
//   Yöntem: ReadProcessMemory / WriteProcessMemory
// ================================================

#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <iostream>

// ================================================
//   POINTER ZİNCİRLERİ — ETS2 1.58
//   [VİDEODA ANLATILIR]: Cheat Engine ile bulma
//
//   NOT: Oyun güncellenince sadece BASE adresler
//   değişir, offset'ler genelde aynı kalır.
// ================================================

// RPM pointer zinciri
constexpr DWORD RPM_BASE         = 0x034D5F40;
constexpr DWORD RPM_OFFSETS[]    = { 0x8, 0x150, 0x10, 0x44 };

// Hasar (damage) pointer zinciri
constexpr DWORD DMG_BASE         = 0x034D5F40;
constexpr DWORD DMG_OFFSETS[]    = { 0x8, 0x150, 0x10, 0x78 };

// Hız pointer zinciri (m/s — 3.6 ile çarp → km/h)
constexpr DWORD SPD_BASE         = 0x034D5F40;
constexpr DWORD SPD_OFFSETS[]    = { 0x8, 0x150, 0x10, 0x50 };

// Yakıt pointer zinciri
constexpr DWORD FUEL_BASE        = 0x034D5F40;
constexpr DWORD FUEL_OFFSETS[]   = { 0x8, 0x150, 0x10, 0x6C };


// ================================================
//   MEMORY MANAGER SINIFI
//   Tüm okuma/yazma işlemleri buradan geçer
// ================================================
class MemoryManager {
public:
    HANDLE  hProcess  = nullptr;   // ETS2 process handle
    DWORD   processID = 0;         // ETS2 process ID
    uintptr_t baseAddress = 0;     // eurotrucks2.exe base adresi

    // --- Process'i bul ve bağlan ---
    bool Attach(const wchar_t* processName) {
        // Tüm çalışan process'leri tara
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnap, &pe32)) {
            do {
                if (wcscmp(pe32.szExeFile, processName) == 0) {
                    processID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);

        if (processID == 0) return false;

        // Process'e bağlan (okuma + yazma yetkisi)
        hProcess = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            FALSE,
            processID
        );

        if (!hProcess) return false;

        // Base adresi bul
        baseAddress = GetBaseAddress(processName);
        return baseAddress != 0;
    }

    // --- eurotrucks2.exe'nin bellekteki başlangıç adresi ---
    uintptr_t GetBaseAddress(const wchar_t* moduleName) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W me32{};
        me32.dwSize = sizeof(MODULEENTRY32W);

        uintptr_t result = 0;
        if (Module32FirstW(hSnap, &me32)) {
            do {
                if (wcscmp(me32.szModule, moduleName) == 0) {
                    result = (uintptr_t)me32.modBaseAddr;
                    break;
                }
            } while (Module32NextW(hSnap, &me32));
        }
        CloseHandle(hSnap);
        return result;
    }

    // --- Pointer zincirini çöz → hedef adresi döndür ---
    uintptr_t ResolvePointer(DWORD baseOffset, const DWORD* offsets, int offsetCount) {
        uintptr_t addr = 0;

        // İlk adresi oku: exe base + offset
        if (!ReadProcessMemory(hProcess,
            (LPCVOID)(baseAddress + baseOffset),
            &addr, sizeof(addr), nullptr))
            return 0;

        // Zinciri takip et (son offset hariç)
        for (int i = 0; i < offsetCount - 1; i++) {
            if (!ReadProcessMemory(hProcess,
                (LPCVOID)(addr + offsets[i]),
                &addr, sizeof(addr), nullptr))
                return 0;
        }

        // Son offset'i ekle → hedef adres
        return addr + offsets[offsetCount - 1];
    }

    // --- Float oku ---
    float ReadFloat(uintptr_t address) {
        float value = 0.0f;
        ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(float), nullptr);
        return value;
    }

    // --- Float yaz ---
    bool WriteFloat(uintptr_t address, float value) {
        return WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(float), nullptr);
    }

    // --- Bağlantıyı kapat ---
    void Detach() {
        if (hProcess) {
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
    }

    bool IsAttached() { return hProcess != nullptr; }
};


// ================================================
//   OYUN VERİSİ YAPISI
//   Her frame güncellenir
// ================================================
struct GameData {
    float rpm       = 0.0f;   // Motor devri
    float speedKmh  = 0.0f;   // Hız (km/h)
    float damage    = 0.0f;   // Hasar (0.0 = sağlam, 1.0 = tam hasar)
    float fuel      = 0.0f;   // Yakıt (litre)
    bool  valid     = false;  // Veri geçerli mi?
};
