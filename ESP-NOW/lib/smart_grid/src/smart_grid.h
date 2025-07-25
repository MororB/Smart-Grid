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



class SmartGrid {

public:
    SmartGrid(ModuleType myType);

    // Methoden
    bool initEspNow(bool printMac = true);
    void sendJoinMessage();
    void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printKnownPeers() const;
    void printMacAddress() const;
    bool addPeerIfNew(const uint8_t* macAddress, ModuleType type = MODULE_SOLAR);
    void sendModuleRegistryToPeer(const uint8_t* receiverMac);
    void handleReceivedModuleRegistry(const uint8_t* incomingData);
    void handleJoinMessage(const JoinMessageWithType& joinMsg);
    void handleControlCommand(const uint8_t* macAddress, ControlCommand command);
    void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc);
    void handleRecivedSmartGridData(const uint8_t* mac, const uint8_t* incomingData, int len);
    bool jsonToSmartGrid(const JsonDocument& json, SmartGridData* data);
    void smartGridToJson(const SmartGridData* data, JsonDocument& json);
    void sendControlCommand(const uint8_t* receiverMac, const ControlCommand& command);
    void sendSmartGridData(const uint8_t* receiverMac);
    void sendRegistryRequest();
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
    uint8_t newPeerCount = 0;                // Wie bisher: zählt neue Peers
    uint8_t receivedRegistryRequests = 0;     // Zählt empfangene RegistryRequests
    unsigned long lastRegistryRequestTime = 0; // Für Timing
    uint8_t registryRequestAttempts = 0;      // Wie oft schon versucht
    bool registryReceived = false;            // Wurde eine Registry empfangen?
    uint8_t motor_rpm = 0; // Motor RPM, initialisiert auf 0
    Adafruit_SSD1306 display;
    CRGB leds[NUM_LEDS]; // LED-Array für FastLED
    bool newData = true;
    bool dataChanged = false; // Flag, ob sich die Daten geändert haben
    uint8_t own_mac[6];
    unsigned long last_update = 0;
    unsigned long systemTimeStartMs = 0; // Startzeitpunkt in ms


    std::vector<float> consAnchors;    // Anker für Verbrauch
    std::vector<float> genAnchors;     // Anker für Erzeugung

    std::vector<float> consProfile;    // Interpoliertes Profil
    std::vector<float> genProfile;    
    uint16_t profileSize = 0; // Größe des Profils 

    uint32_t cycleDurationMs    = 20000;  // 24 h in ms                       
    uint32_t lastStepTimeMs = 0;
    uint32_t lastStepMs      = 0;
    uint8_t  cycleIndex = 0;
    bool     cycleEnabled = true;
    bool modeMakeStep = false; // Flag, ob der Modus einen Schritt machen soll


    void updateDisplay();
    void updateLED();
    void updateMotor();
    void readSolarcell();
    void computeNetworkStatus();
    bool checkForChanges();
    void sendNewSmartGridData();

    void runAutomatik();
    void runTageszyklus();
    void runNachtzyklus();
    void runTagNachtzyklus();
    void runInteraktiv();
    void runPause();

    void generateInterpolatedProfile(const std::vector<float>& anchors,
                                 std::vector<float>& profile);
    void setDailyProfiles(const std::vector<float>& consAnch,
                      const std::vector<float>& genAnch,
                      size_t outPoints,
                      uint32_t durationMs);
};

#endif
