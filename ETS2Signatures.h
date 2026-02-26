#pragma once
#include <string>

// ================================================================
//  ETS2 1.58 Pattern'leri
//
//  Bu pattern'ler ETS2'nin update kodlarından alınmıştır.
//  Oyun her hız/rpm güncellemesinde bu byte dizilerini çalıştırır.
//  Biz de bu kodların hemen yanındaki pointer'ı okuruz.
//
//  [VİDEODA ANLAT]: Bu byte'lar nereden geliyor?
//  → IDA Pro / Ghidra ile exe analiz edilir
//  → Hız yazma fonksiyonu bulunur
//  → Etrafındaki benzersiz byte'lar pattern olarak alınır
// ================================================================

namespace ETS2Sig {

    // --- Hız (Speed) ---
    // Assembly: movss xmm0, [rcx+160h]  (hız okuma kodu)
    // Pattern:  F3 0F 10 81 60 01 00 00
    // Sonrasındaki pointer truck base'i gösterir
    const std::string SPEED =
        "F3 0F 10 81 60 01 00 00";

    // --- RPM ---
    // Assembly: movss xmm1, [rcx+164h]
    const std::string RPM =
        "F3 0F 10 81 64 01 00 00";

    // --- Yakıt (Fuel) ---
    // Assembly: movss xmm0, [rax+250h]
    const std::string FUEL =
        "F3 0F 10 80 50 02 00 00";

    // --- Truck Ana Pointer ---
    // Bu pattern truck nesnesinin adresini tutan global pointer'ı işaret eder
    // MOV RAX, [RIP+??] → truck pointer
    const std::string TRUCK_PTR =
        "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? F3 0F 10 40";

    // --- Hasar (Damage) ---
    const std::string DAMAGE =
        "F3 0F 11 80 A4 02 00 00";

    // --- Motor durumu ---
    const std::string ENGINE =
        "88 81 88 01 00 00";

    // --- Yakıt kapasitesi ---
    const std::string FUEL_MAX =
        "F3 0F 10 80 54 02 00 00";
}
