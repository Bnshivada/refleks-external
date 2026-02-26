#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <iostream>

// ================================================================
//  AutoScanner: ETS2'yi otomatik bulur ve bağlanır
//  Oyun açık değilse bekler, açılınca bağlanır
// ================================================================

class AutoScanner {
public:
    HANDLE   processHandle = nullptr;
    DWORD    processId     = 0;
    uintptr_t baseAddress  = 0;
    bool     isConnected   = false;

    // Hedef process adı
    const std::wstring TARGET_PROCESS = L"eurotrucks2.exe";

    // ----------------------------------------------------------------
    // Ana bağlantı fonksiyonu
    // Oyun bulunana kadar her 2 saniyede bir tekrar dener
    // ----------------------------------------------------------------
    void WaitAndConnect() {
        std::cout << "[AutoScanner] ETS2 aranıyor...\n";
        std::cout << "[AutoScanner] Oyunu açık unutmadıysan bekle!\n\n";

        while (!isConnected) {
            processId = FindProcess(TARGET_PROCESS);

            if (processId != 0) {
                // Process bulundu, handle aç
                processHandle = OpenProcess(
                    PROCESS_ALL_ACCESS,  // Okuma + Yazma yetkisi
                    FALSE,
                    processId
                );

                if (processHandle != nullptr) {
                    baseAddress = GetModuleBase(processId, TARGET_PROCESS);

                    if (baseAddress != 0) {
                        isConnected = true;
                        std::cout << "[✓] ETS2 BAĞLANDI!\n";
                        std::cout << "[✓] Process ID  : " << processId << "\n";
                        std::cout << "[✓] Base Adres  : 0x" 
                                  << std::hex << baseAddress << std::dec << "\n\n";
                    }
                }
            }

            if (!isConnected) {
                std::cout << "[...] ETS2 bulunamadı, 2 saniye sonra tekrar denenecek\r";
                Sleep(2000);
            }
        }
    }

    // ----------------------------------------------------------------
    // Oyunun hâlâ açık olup olmadığını kontrol et
    // Ana loop'ta her frame çağır
    // ----------------------------------------------------------------
    bool IsStillRunning() {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processHandle, &exitCode)) {
            return exitCode == STILL_ACTIVE;
        }
        return false;
    }

    // ----------------------------------------------------------------
    // Bağlantıyı temizle (oyun kapanınca çağırılır)
    // ----------------------------------------------------------------
    void Disconnect() {
        if (processHandle) {
            CloseHandle(processHandle);
            processHandle = nullptr;
        }
        isConnected = false;
        processId   = 0;
        baseAddress = 0;
        std::cout << "[!] ETS2 kapandı, bağlantı kesildi.\n";
    }

private:
    // Process ID bul
    DWORD FindProcess(const std::wstring& name) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (name == entry.szExeFile) {
                    CloseHandle(snapshot);
                    return entry.th32ProcessID; // Bulundu!
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return 0; // Bulunamadı
    }

    // Module base adresini al (ASLR bypass için şart)
    uintptr_t GetModuleBase(DWORD pid, const std::wstring& modName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W entry;
        entry.dwSize = sizeof(entry);

        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (modName == entry.szModule) {
                    CloseHandle(snapshot);
                    return (uintptr_t)entry.modBaseAddr;
                }
            } while (Module32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return 0;
    }
};
