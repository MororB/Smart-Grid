#ifndef SMART_GRID_H
#define SMART_GRID_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FastLED.h>
#include "smart_grid_types.h"
#include <vector>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define NUM_LEDS 22
#define DATA_PIN 27

#define MAX_ATTEMPTS 3
#define INTERVAL_BETWEEN_REQUESTS 2000 // alle 2 Sekunden


#define DEBUG_FULL 0    // Detaillierte Debug-Ausgaben 0 = OFF, 1 = ON

#define SOLAR_PIN 34 // Pin für das Solarmodul
# define SMOKE_PIN 13 // Pin für die Rauchmaschine



class SmartGrid {

public:
    SmartGrid(ModuleType myType);

    // Methoden
    bool initEspNow(bool printMac = true);
    void printSmartGridData(SmartGridData data);
    void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printKnownPeers() const;
    void printMacAddress() const;
    void printRegistry();
    bool addPeerIfNew(const uint8_t* macAddress, SmartGridData data);

    void sendJoinMessage();
    void sendModuleRegistryToPeer(const uint8_t* receiverMac);
    void sendControlCommand(const uint8_t* receiverMac, const ControlCommand& command);
    void sendSmartGridData(const uint8_t* receiverMac);
    void sendRegistryRequest();
    void sendLeaveNetworkMessage();

    void handleReceivedModuleRegistry(const uint8_t* incomingData);
    void handleJoinMessage(const JoinMessageWithType& joinMsg);
    void handleControlCommand(const uint8_t* macAddress, ControlCommand command);
    void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc);
    void handleRecivedSmartGridData(const uint8_t* mac, const uint8_t* incomingData, int len);
    void handleRegistryRequest(RegistryRequestMessage req);
    void handleLeaveNetworkMessage(const uint8_t* macAddress);

    bool jsonToSmartGrid(const JsonDocument& json, SmartGridData* data);
    void smartGridToJson(const SmartGridData* data, JsonDocument& json);
    void tryRequestRegistry();
    void begin();
    void runWaitForRegistry();

    void update();

    // Getter/Setter für SmartGridData
    SmartGridData getSmartGridData() const;
    void setSmartGridData(const SmartGridData& data);

    // Zugriff auf ModuleRegistry
    const ModuleRegistry& getModuleRegistry() const;
    ModuleMode getCurrentMode() const;
    void setCurrentMode(ModuleMode mode);

    void setSystemTime(uint32_t);
    uint32_t getSystemTime();
    void updateSystemTime();

private:
    ModuleType myModuleType;
    SmartGridData smartGridData;
    ModuleRegistry moduleRegistry;
    StaticJsonDocument<256> doc;
    ModuleMode currentMode = MODE_WAIT_FOR_REGISTRY; // Standardmodus
    ModuleMode lastMode = MODE_WAIT_FOR_REGISTRY; // Für Moduswechsel
    uint8_t newPeerCount = 0;                // Wie bisher: zählt neue Peers
    uint8_t receivedRegistryRequests = 0;     // Zählt empfangene RegistryRequests
    unsigned long lastRegistryRequestTime = 0; // Für Timing
    uint8_t registryRequestAttempts = 0;      // Wie oft schon versucht
    bool registryReceived = false;            // Wurde eine Registry empfangen?
    bool checkRegistry = false; // Soll die Registry geprüft werden?
    uint8_t motor_rpm = 0; // Motor RPM, initialisiert auf 0
    Adafruit_SSD1306 display;
    CRGB leds[NUM_LEDS]; // LED-Array für FastLED
    bool newData = true;
    bool dataChanged = false; // Flag, ob sich die Daten geändert haben
    uint8_t own_mac[6];
    uint8_t own_registry_number = 255; // Eigene Registry-Nummer
    unsigned long last_update = 0;
    unsigned long systemTimeStartMs = 0; // Startzeitpunkt in ms


    std::vector<uint16_t> consAnchors;    // Anker für Verbrauch
    std::vector<uint16_t> genAnchors;     // Anker für Erzeugung

    std::vector<uint16_t> consProfile;    // Interpoliertes Profil
    std::vector<uint16_t> genProfile;    
    uint16_t profileSize = 0; // Größe des Profils 

    uint32_t cycleDurationMs    = 20000;  // 24 h in ms  
    uint32_t time_per_step   = 0;      // Startzeitpunkt des Zyklus in ms                     
    uint32_t lastStepTimeMs = 0;
    uint32_t lastStepMs      = 0;
    uint8_t  cycleIndex = 0;
    bool     cycleEnabled = true;
    bool modeMakeStep = false; // Flag, ob der Modus einen Schritt machen soll

    // HW354 braucht zwei Eingänge: IN1 und IN2
    static constexpr int MOTOR_IN1_PIN     = 25;  
    static constexpr int MOTOR_IN2_PIN     = 26;  

    // PWM-Setup
    static constexpr int MOTOR_PWM_FREQ    = 5000; // 5 kHz
    static constexpr int MOTOR_PWM_RES     = 8;    // 8-Bit Auflösung
    static constexpr int MOTOR_PWM_CH_A    = 0;    // Kanal 0 → IN1
    static constexpr int MOTOR_PWM_CH_B    = 1;    // Kanal 1 → IN2

    uint8_t       brightness; // 0…255
    CRGB          color;      // Grün bei Überschuss, Rot bei Defizit
    uint8_t motorPwm       = 0;    // 0…255
    bool    motorForward   = true; // true=IN1 aktiviert, false=IN2

    bool checkAnimation = false;
    volatile LedAnimation currentAnimation = LedAnimation::NONE;

    int   smokePin = 13;    // anpassen
    bool  smokeIsOn = false;
    float smokeDuty = 0.0f; // 0..1

    const unsigned long SMOKE_CYCLE_MS      = 4000;
    const unsigned long SMOKE_PULSE_HIGH_MS = 50;   // HIGH-Pulsdauer
    const unsigned long SMOKE_PULSE_GAP_MS  = 200;   // Lücke zwischen den 2 ON-Pulsen
    const unsigned long SMOKE_MIN_WIN_MS    = 500;  // min. Fenster gegen „Flattern“

    unsigned long smokeCycleStart = 0;

    void startAnimation(LedAnimation t);
    void endAnimation();
    void updateLedAnimation();
    void updateLED();

    void updateDisplay();
    void updateMotor();
    void readSolarcell();
    void computeNetworkStatus();
    bool checkForChanges();
    void sendNewSmartGridData();
    bool checkForOwnModuleinRegistry();
    uint8_t nextFreeId();


    void runAutomatik();
    void runTageszyklus();
    void runNachtzyklus();
    void runTagNachtzyklus();
    void runInteraktiv();
    void runPause();
    void runLeaveNetwork();
    void runShutdown();

    void tickSmokeSimple();
    void setSmokeDuty(float duty01);
    void pulseHigh(unsigned long ms);
    void smokeTurnOn(); 
    void smokeTurnOff();
    void renderStorageBarForOwnModule();

    void generateInterpolatedProfile(const std::vector<uint16_t>& anchors,
                                 std::vector<uint16_t>& profile);
    void setDailyProfiles(const std::vector<uint16_t>& anchors,
                        size_t outPoints,
                        uint32_t durationMs,
                        bool isConsumption);

    void delet_single_registry(int reg_num);
    void print_single_registry(int reg_num);
    void leave_network();
    int indexOfMac(const uint8_t mac[6]);


    static inline bool isStorage(ModuleType t) {
    return t == MODULE_BATTERY || t == MODULE_PUMP_STORAGE || t == MODULE_HYDROGEN;
    }

    static StorageLimits getStorageLimits(ModuleType t) {
        switch (t) {
            case MODULE_BATTERY:      return { 200.0f, 200.0f, 2000.0f };
            case MODULE_PUMP_STORAGE: return { 400.0f, 400.0f, 5000.0f };
            case MODULE_HYDROGEN:     return { 250.0f, 250.0f, 4000.0f };
            default:                  return {   0.0f,   0.0f,    0.0f };
        }
    }


};


#endif
