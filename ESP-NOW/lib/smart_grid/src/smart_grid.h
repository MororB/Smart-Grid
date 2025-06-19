#ifndef SMART_GRID_H
#define SMART_GRID_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "smart_grid_types.h"

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
    void handleJoinMessage(const JoinMessage& joinMsg);
    void handleControlCommand(const uint8_t* macAddress, ControlCommand command);
    void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc);
    bool jsonToSmartGrid(const JsonDocument& json, SmartGridData* data);
    void smartGridToJson(const SmartGridData* data, JsonDocument& json);
    void sendControlCommand(const uint8_t* receiverMac, const ControlCommand& command);
    void sendSmartGridData(const uint8_t* receiverMac);

    void update();

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
    ModuleMode currentMode = MODE_AUTOMATIK; // Standardmodus

    void runAutomatik();
    void runTageszyklus();
    void runNachtzyklus();
    void runTagNachtzyklus();
    void runInteraktiv();
    void runPause();
};

#endif
