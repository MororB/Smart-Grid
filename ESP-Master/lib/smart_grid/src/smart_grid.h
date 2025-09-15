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

    void setSystemTime(uint32_t);
    uint32_t getSystemTime();
    void updateSystemTime();

    void processUartCommand_(const String &line); //MASTER
    void sendControlCommandStep(const uint8_t* receiverMac, const ControlCommandStep& step);

    void update();
    void updateMaster();        //MASTER

    // Getter/Setter für SmartGridData
    SmartGridData getSmartGridData() const;
    void setSmartGridData(const SmartGridData& data);

    // Zugriff auf ModuleRegistry
    const ModuleRegistry& getModuleRegistry() const;
    ModuleMode getCurrentMode() const;
    void setCurrentMode(ModuleMode mode);

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
    bool     cycleEnabled = false;
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


    String uartBuf_;
    bool masterRequest = false; // Flag, ob der Master den Status anfordert

    void updateDisplay();
    void updateLED();
    void updateMotor();
    void readSolarcell();
    void computeNetworkStatus();
    bool checkForChanges();
    void sendNewSmartGridData();
    bool checkForOwnModuleinRegistry();
    uint8_t nextFreeId();

    void sendJsonStatusToPi_(const SmartGridData& d,const uint8_t *mac); //MASTER

    void runAutomatik();
    void runTageszyklus();
    void runNachtzyklus();
    void runTagNachtzyklus();
    void runInteraktiv();
    void runPause();

    void delet_single_registry(int reg_num);
    void print_single_registry(int reg_num);
    void leave_network();
    int indexOfMac(const uint8_t mac[6]);
};

#endif
