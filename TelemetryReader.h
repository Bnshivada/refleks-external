#pragma once
#include <Windows.h>
#include <iostream>
#include <cstring>

// ================================================================
//  TelemetryReader.h
//  nlhans/ets2-sdk-plugin shared memory okuyucu
//  KAYNAK: https://github.com/nlhans/ets2-sdk-plugin
// ================================================================

#define SCS_TELEMETRY_SHARED_MEMORY_NAME  "Local\\ETStelemetry"

#pragma pack(push, 4)
struct SCSTelemetryStruct {
    uint32_t time;
    uint32_t paused;
    bool     engine_enabled;
    bool     trailer_attached;
    uint8_t  _pad[2];
    float    speed;
    float    accelerationX;
    float    accelerationY;
    float    accelerationZ;
    float    coordX;
    float    coordY;
    float    coordZ;
    float    rotationX;
    float    rotationY;
    float    rotationZ;
    int32_t  gear;
    int32_t  gears;
    int32_t  gearRanges;
    int32_t  gearRangeActive;
    float    engineRpm;
    float    engineRpmMax;
    float    fuel;
    float    fuelCapacity;
    float    fuelRate;
    float    fuelAvgConsumption;
    float    userSteer;
    float    userThrottle;
    float    userBrake;
    float    userClutch;
    float    gameSteer;
    float    gameThrottle;
    float    gameBrake;
    float    gameClutch;
    float    truckSpeed;
    float    wearEngine;
    float    wearTransmission;
    float    wearCabin;
    float    wearChassis;
    float    wearWheels;
    float    truckOdometer;
    float    cruiseControlSpeed;
    float    truckLat;
    float    truckLong;
    float    navigationDistance;
    float    navigationTime;
    float    navigationSpeedLimit;
    char     truckMake[64];
    char     truckMakeId[64];
    char     truckModel[64];
    char     load[64];
    char     cityDst[64];
    char     compDst[64];
    char     citySrc[64];
    char     compSrc[64];
    uint32_t onJob;
    uint32_t jobFinished;
    float    jobIncome;
    float    jobDeadline;
    bool     lightsParking;
    bool     lightsBrake;
    bool     lightsReverse;
    bool     lightsHazard;
    bool     lightsBeamLow;
    bool     lightsBeamHigh;
    bool     lightsBeacon;
    bool     lightsBlinkerLeft;
    bool     lightsBlinkerRight;
    uint8_t  _pad2[3];
};
#pragma pack(pop)

class TelemetryReader {
public:
    HANDLE              hMapFile = nullptr;
    SCSTelemetryStruct* pData    = nullptr;
    bool                isReady  = false;

    bool Connect() {
        hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, SCS_TELEMETRY_SHARED_MEMORY_NAME);
        if (!hMapFile) {
            std::cout << "[Telemetry] HATA: OpenFileMapping basarisiz!\n";
            std::cout << "  -> ETS2 acik mi?\n";
            std::cout << "  -> ets2-telemetry.dll plugins klasorunde mi?\n";
            std::cout << "  -> Haritaya girdin mi?\n";
            return false;
        }
        pData = (SCSTelemetryStruct*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(SCSTelemetryStruct));
        if (!pData) {
            std::cout << "[Telemetry] HATA: MapViewOfFile basarisiz!\n";
            CloseHandle(hMapFile);
            hMapFile = nullptr;
            return false;
        }
        isReady = true;
        std::cout << "[Telemetry] OK! ETStelemetry baglandi.\n";
        return true;
    }

    bool IsGameActive() {
        if (!pData) return false;
        return pData->engine_enabled || pData->speed != 0.f;
    }

    bool IsPaused()     { return pData ? pData->paused == 1 : false; }

    float GetSpeed()    { return pData ? abs(pData->speed * 3.6f) : 0.f; }
    float GetRPM()      { return pData ? pData->engineRpm          : 0.f; }
    float GetRPMMax()   { return (pData && pData->engineRpmMax > 0.f) ? pData->engineRpmMax : 2500.f; }
    float GetFuel()     { return pData ? pData->fuel               : 0.f; }
    float GetFuelMax()  { return (pData && pData->fuelCapacity > 0.f) ? pData->fuelCapacity : 400.f; }
    float GetFuelRate() { return pData ? pData->fuelRate           : 0.f; }
    float GetThrottle() { return pData ? pData->userThrottle       : 0.f; }
    float GetBrake()    { return pData ? pData->userBrake          : 0.f; }
    float GetClutch()   { return pData ? pData->userClutch         : 0.f; }
    float GetSteering() { return pData ? pData->userSteer          : 0.f; }
    int   GetGear()     { return pData ? pData->gear               : 0;   }
    int   GetGearMax()  { return pData ? pData->gears              : 12;  }
    bool  GetEngineOn() { return pData ? pData->engine_enabled     : false; }
    bool  GetTrailer()  { return pData ? pData->trailer_attached   : false; }

    bool  GetCruiseOn()    { return pData ? pData->cruiseControlSpeed > 0.f : false; }
    float GetCruiseSpeed() { return pData ? pData->cruiseControlSpeed : 0.f; }

    float GetDamage() {
        if (!pData) return 0.f;
        return ((pData->wearEngine + pData->wearTransmission +
                 pData->wearCabin  + pData->wearChassis +
                 pData->wearWheels) / 5.f) * 100.f;
    }

    float GetWearEngine()       { return pData ? pData->wearEngine       * 100.f : 0.f; }
    float GetWearTransmission() { return pData ? pData->wearTransmission * 100.f : 0.f; }
    float GetWearCabin()        { return pData ? pData->wearCabin        * 100.f : 0.f; }
    float GetWearChassis()      { return pData ? pData->wearChassis      * 100.f : 0.f; }
    float GetWearWheels()       { return pData ? pData->wearWheels       * 100.f : 0.f; }

    float GetPosX()      { return pData ? pData->coordX    : 0.f; }
    float GetPosZ()      { return pData ? pData->coordZ    : 0.f; }
    float GetHeading()   { return pData ? pData->rotationX : 0.f; }

    float GetNavDistance()  { return pData ? pData->navigationDistance          : 0.f; }
    float GetNavTime()      { return pData ? pData->navigationTime              : 0.f; }
    float GetSpeedLimit()   { return pData ? pData->navigationSpeedLimit * 3.6f : 0.f; }

    const char* GetTruckMake()  { return pData ? pData->truckMake  : ""; }
    const char* GetTruckModel() { return pData ? pData->truckModel : ""; }
    const char* GetCargoName()  { return pData ? pData->load       : ""; }
    const char* GetDestCity()   { return pData ? pData->cityDst    : ""; }
    const char* GetSrcCity()    { return pData ? pData->citySrc    : ""; }
    const char* GetDestComp()   { return pData ? pData->compDst    : ""; }
    float       GetCargoMass()  { return 0.f; }  // nlhans pluginde yok
    bool        GetOnJob()      { return pData ? pData->onJob == 1 : false; }
    float       GetJobIncome()  { return pData ? pData->jobIncome  : 0.f; }

    bool GetLightsLow()    { return pData ? pData->lightsBeamLow      : false; }
    bool GetLightsHigh()   { return pData ? pData->lightsBeamHigh     : false; }
    bool GetHazard()       { return pData ? pData->lightsHazard       : false; }
    bool GetBlinkerLeft()  { return pData ? pData->lightsBlinkerLeft  : false; }
    bool GetBlinkerRight() { return pData ? pData->lightsBlinkerRight : false; }

    void Disconnect() {
        if (pData)    { UnmapViewOfFile(pData); pData    = nullptr; }
        if (hMapFile) { CloseHandle(hMapFile);  hMapFile = nullptr; }
        isReady = false;
        std::cout << "[Telemetry] Baglanti kesildi.\n";
    }

    ~TelemetryReader() { Disconnect(); }
};
