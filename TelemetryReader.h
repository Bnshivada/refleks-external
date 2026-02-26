#pragma once
#include <Windows.h>
#include <iostream>
#include <cstring>

// ================================================================
//  TelemetryReader.h
//  SCS SDK Plugin'in yazdığı shared memory'yi okur
//  
//  KURULUM:
//  1. scs-telemetry.dll → ETS2/bin/win_x64/plugins/ klasörüne koy
//  2. ETS2'yi aç, haritaya gir
//  3. Bu sınıf otomatik bağlanır
//
//  KAYNAK: https://github.com/RenCloud/scs-sdk-plugin
// ================================================================

// ---- SDK'nın shared memory adı ----
#define SCS_TELEMETRY_SHARED_MEMORY_NAME  "Local\\SCSTelemetry"

// ---- SDK struct layout (scs-sdk-plugin v1.12 / ETS2 1.58) ----
// Tüm değerler bu struct içinde sıralı duruyor
// Offset'ler SDK'nın kendi header'ından alındı

#pragma pack(push, 1)
struct SCSTelemetryStruct {

    // ----- GENEL -----
    uint32_t sdk_active;          // 0 = oyun kapalı, 1 = aktif
    uint32_t paused;              // 1 = oyun pauseda

    // ----- TRUCK -----
    // Motor
    uint8_t  engine_enabled;      // Motor açık mı?
    uint8_t  electric_enabled;    // Elektrik açık mı?
    uint8_t  _pad0[2];

    float    engine_rpm;          // Anlık RPM
    float    engine_rpm_max;      // Max RPM (genelde 2500)

    // Hız
    float    speed;               // m/s — 3.6 ile çarp → km/h

    // Yakıt
    float    fuel;                // Litre
    float    fuel_capacity;       // Max litre
    float    fuel_avg_consumption;// Ortalama tüketim

    // Hasar
    float    wear_engine;         // 0.0 - 1.0
    float    wear_transmission;
    float    wear_cabin;
    float    wear_chassis;
    float    wear_wheels;

    // Koordinatlar
    float    coord_x;
    float    coord_y;
    float    coord_z;

    // Rotasyon
    float    rot_heading;         // Yön (0-1, 0=kuzey)
    float    rot_pitch;
    float    rot_roll;

    // Vites
    int32_t  gear;                // Anlık vites (-1=geri, 0=nötr, 1-12=ileri)
    int32_t  gear_dash;           // Gösterge vitesi

    // Fren
    float    brake;               // 0.0 - 1.0
    float    throttle;            // 0.0 - 1.0
    float    clutch;              // 0.0 - 1.0
    float    steering;            // -1.0 - 1.0

    // Işıklar
    uint8_t  lights_parking;
    uint8_t  lights_beam_low;
    uint8_t  lights_beam_high;
    uint8_t  lights_beacon;
    uint8_t  lights_brake;
    uint8_t  lights_reverse;
    uint8_t  lights_hazard;
    uint8_t  _pad1;

    // Cruise control
    uint8_t  cruise_control;
    uint8_t  _pad2[3];
    float    cruise_control_speed; // km/h

    // ----- TAŞIMA (JOB) -----
    float    cargo_mass;           // kg
    char     cargo_id[64];         // Kargo adı
    char     city_dst[64];         // Hedef şehir
    char     company_dst[64];      // Hedef şirket

    // Kazanç
    int64_t  income;               // XP kazancı

    uint32_t _reserved[32];        // Gelecek güncellemeler için boş alan
};
#pragma pack(pop)


// ================================================================
//  TelemetryReader sınıfı
// ================================================================
class TelemetryReader {
public:
    HANDLE    hMapFile  = nullptr;
    SCSTelemetryStruct* pData = nullptr;
    bool      isReady   = false;

    // ----------------------------------------------------------------
    //  Shared memory'ye bağlan
    //  ETS2 açık ve plugin yüklü olmalı
    // ----------------------------------------------------------------
    bool Connect() {
        hMapFile = OpenFileMappingA(
            FILE_MAP_READ,                    // Sadece okuma
            FALSE,
            SCS_TELEMETRY_SHARED_MEMORY_NAME  // "Local\SCSTelemetry"
        );

        if (!hMapFile) {
            std::cout << "[Telemetry] Baglanti HATASI!\n";
            std::cout << "  → ETS2 acik mi?\n";
            std::cout << "  → scs-telemetry.dll plugins klasorunde mi?\n";
            std::cout << "  → Haritaya girdin mi?\n";
            return false;
        }

        pData = (SCSTelemetryStruct*)MapViewOfFile(
            hMapFile,
            FILE_MAP_READ,
            0, 0,
            sizeof(SCSTelemetryStruct)
        );

        if (!pData) {
            std::cout << "[Telemetry] MapViewOfFile HATASI!\n";
            CloseHandle(hMapFile);
            hMapFile = nullptr;
            return false;
        }

        isReady = true;
        std::cout << "[Telemetry] Baglanti OK!\n";
        return true;
    }

    // ----------------------------------------------------------------
    //  SDK aktif mi? (oyun açık ve haritada mı?)
    // ----------------------------------------------------------------
    bool IsGameActive() {
        if (!pData) return false;
        return pData->sdk_active == 1;
    }

    bool IsPaused() {
        if (!pData) return false;
        return pData->paused == 1;
    }

    // ----------------------------------------------------------------
    //  Veri okuma — direkt struct'tan, hiç syscall yok
    // ----------------------------------------------------------------
    float GetSpeed()     { return pData ? abs(pData->speed * 3.6f) : 0.f; }
    float GetRPM()       { return pData ? pData->engine_rpm         : 0.f; }
    float GetRPMMax()    { return pData ? pData->engine_rpm_max     : 2500.f; }
    float GetFuel()      { return pData ? pData->fuel               : 0.f; }
    float GetFuelMax()   { return pData ? pData->fuel_capacity      : 400.f; }
    float GetThrottle()  { return pData ? pData->throttle           : 0.f; }
    float GetBrake()     { return pData ? pData->brake              : 0.f; }
    int   GetGear()      { return pData ? pData->gear               : 0;   }
    bool  GetEngineOn()  { return pData ? pData->engine_enabled == 1: false;}

    // Ortalama hasar (tüm bileşenlerin ortalaması)
    float GetDamage() {
        if (!pData) return 0.f;
        float avg = (
            pData->wear_engine       +
            pData->wear_transmission +
            pData->wear_cabin        +
            pData->wear_chassis      +
            pData->wear_wheels
        ) / 5.f;
        return avg * 100.f;  // 0-100%
    }

    // Motor hasarı ayrı
    float GetEngineDamage() {
        return pData ? pData->wear_engine * 100.f : 0.f;
    }

    // Koordinatlar
    float GetPosX() { return pData ? pData->coord_x : 0.f; }
    float GetPosZ() { return pData ? pData->coord_z : 0.f; }

    // Kargo bilgisi
    const char* GetCargoName()   { return pData ? pData->cargo_id    : ""; }
    const char* GetDestCity()    { return pData ? pData->city_dst    : ""; }
    float       GetCargoMass()   { return pData ? pData->cargo_mass  : 0.f; }

    // ----------------------------------------------------------------
    //  YAZMA — telemetry read-only, yazma için hala RPM/damage
    //  için ayrı bir memory writer gerekli (MemoryManager)
    //  Ama No Damage için oyunun kendi "repair" sistemini kullanabiliriz
    // ----------------------------------------------------------------

    // ----------------------------------------------------------------
    //  Bağlantıyı kapat
    // ----------------------------------------------------------------
    void Disconnect() {
        if (pData)    { UnmapViewOfFile(pData); pData = nullptr; }
        if (hMapFile) { CloseHandle(hMapFile);  hMapFile = nullptr; }
        isReady = false;
        std::cout << "[Telemetry] Baglanti kesildi.\n";
    }

    ~TelemetryReader() { Disconnect(); }
};
