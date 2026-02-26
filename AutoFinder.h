#pragma once
#include "PatternScanner.h"
#include "ETS2Signatures.h"
#include <iostream>

// ================================================================
//  AutoFinder: Pattern tarayıp tüm adresleri otomatik bulur
//  Kullanım:
//    AutoFinder finder(processHandle);
//    finder.ScanAll();
//    float speed = finder.ReadSpeed();
// ================================================================

class AutoFinder {
public:
    HANDLE hProcess;

    // Bulunan adresler
    uintptr_t truckBaseAddr = 0;  // Truck pointer'ın tutulduğu global adres
    uintptr_t speedAddr     = 0;
    uintptr_t rpmAddr       = 0;
    uintptr_t fuelAddr      = 0;
    uintptr_t damageAddr    = 0;

    bool isReady = false;

    AutoFinder(HANDLE handle) : hProcess(handle) {}

    // ----------------------------------------------------------------
    //  Ana tarama — başlangıçta bir kez çağır
    //  Birkaç saniye sürer (memory boyutuna göre)
    // ----------------------------------------------------------------
    bool ScanAll() {
        std::cout << "[AutoFinder] Tarama başlıyor...\n";
        std::cout << "[AutoFinder] Bu 3-10 saniye sürebilir.\n\n";

        // 1. Önce truck pointer'ını bul (en kritik adres)
        std::cout << "[1/4] Truck pointer aranıyor...  ";
        uintptr_t ptrInstr = PatternScanner::ScanAllRegions(hProcess, ETS2Sig::TRUCK_PTR);

        if (ptrInstr == 0) {
            std::cout << "BAŞARISIZ ✗\n";
            std::cout << "      → Harita yüklenmiş mi? Araçta mısın?\n";
            return false;
        }

        // RIP-relative pointer'ı çöz → gerçek truck adresi
        truckBaseAddr = PatternScanner::ResolveRIP(hProcess, ptrInstr);
        std::cout << "BULUNDU ✓  (0x" << std::hex << truckBaseAddr << ")\n" << std::dec;

        // 2. Hız adresi
        std::cout << "[2/4] Hız adresi aranıyor...     ";
        speedAddr = PatternScanner::ScanAllRegions(hProcess, ETS2Sig::SPEED);
        if (speedAddr) std::cout << "BULUNDU ✓\n";
        else           std::cout << "BAŞARISIZ ✗\n";

        // 3. RPM adresi
        std::cout << "[3/4] RPM adresi aranıyor...     ";
        rpmAddr = PatternScanner::ScanAllRegions(hProcess, ETS2Sig::RPM);
        if (rpmAddr) std::cout << "BULUNDU ✓\n";
        else         std::cout << "BAŞARISIZ ✗\n";

        // 4. Yakıt + Hasar
        std::cout << "[4/4] Yakıt/Hasar aranıyor...   ";
        fuelAddr   = PatternScanner::ScanAllRegions(hProcess, ETS2Sig::FUEL);
        damageAddr = PatternScanner::ScanAllRegions(hProcess, ETS2Sig::DAMAGE);
        if (fuelAddr && damageAddr) std::cout << "BULUNDU ✓\n\n";
        else                        std::cout << "KISMEN ✗\n\n";

        isReady = (truckBaseAddr != 0);
        return isReady;
    }

    // ----------------------------------------------------------------
    //  Truck pointer'dan gerçek veri adresini hesapla
    // ----------------------------------------------------------------
    uintptr_t GetTruckObject() {
        uintptr_t obj = 0;
        ReadProcessMemory(hProcess, (LPCVOID)truckBaseAddr, &obj, sizeof(obj), nullptr);
        return obj;
    }

    // ----------------------------------------------------------------
    //  Değer okuma fonksiyonları
    // ----------------------------------------------------------------
    float ReadSpeed() {
        uintptr_t truck = GetTruckObject();
        if (!truck) return 0.f;
        float val = 0.f;
        ReadProcessMemory(hProcess, (LPCVOID)(truck + 0x160), &val, 4, nullptr);
        return abs(val * 3.6f);  // m/s → km/h
    }

    float ReadRPM() {
        uintptr_t truck = GetTruckObject();
        if (!truck) return 0.f;
        float val = 0.f;
        ReadProcessMemory(hProcess, (LPCVOID)(truck + 0x164), &val, 4, nullptr);
        return val;
    }

    float ReadFuel() {
        uintptr_t truck = GetTruckObject();
        if (!truck) return 0.f;
        float val = 0.f;
        ReadProcessMemory(hProcess, (LPCVOID)(truck + 0x250), &val, 4, nullptr);
        return val;
    }

    float ReadDamage() {
        uintptr_t truck = GetTruckObject();
        if (!truck) return 0.f;
        float val = 0.f;
        ReadProcessMemory(hProcess, (LPCVOID)(truck + 0x2A4), &val, 4, nullptr);
        return val * 100.f;
    }

    // ----------------------------------------------------------------
    //  Değer yazma (No Damage, RPM Boost için)
    // ----------------------------------------------------------------
    void WriteNoDamage() {
        uintptr_t truck = GetTruckObject();
        if (!truck) return;
        float zero = 0.f;
        WriteProcessMemory(hProcess, (LPVOID)(truck + 0x2A4), &zero, 4, nullptr);
    }

    void WriteRPM(float rpm) {
        uintptr_t truck = GetTruckObject();
        if (!truck) return;
        WriteProcessMemory(hProcess, (LPVOID)(truck + 0x164), &rpm, 4, nullptr);
    }

    // ----------------------------------------------------------------
    //  Oyun güncellemesi gelince yeniden tara
    //  Pattern değişirse bu fonksiyon false döner → log'a yaz
    // ----------------------------------------------------------------
    bool Rescan() {
        std::cout << "[AutoFinder] Yeniden taranıyor...\n";
        isReady = false;
        truckBaseAddr = speedAddr = rpmAddr = fuelAddr = damageAddr = 0;
        return ScanAll();
    }
};
