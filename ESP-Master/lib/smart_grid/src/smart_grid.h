#ifndef SMART_GRID_H
#define SMART_GRID_H

#include <ArduinoJson.h>
#include <esp_now.h>
#include <Arduino.h>
#include <WiFi.h>

enum ModuleType : uint8_t {
    MODULE_SOLAR = 1,
    MODULE_WIND = 2,
    MODULE_HYDRO = 3,
    MODULE_ELECTROLYZER = 4,
    MODULE_BATTERY = 5,
    MODULE_HYDROGEN = 6,
    MODULE_PUMP_STORAGE = 7,
    MODULE_HOUSE = 8,
    MODULE_FACTORY = 9,
    MODULE_CAR = 10,
    MODULE_SUBSTATION = 11
};

typedef struct {
    uint8_t x;                  // 1 Byte
    uint8_t y;                  // 1 Byte
} Coordinat;

typedef struct {
    uint32_t timestamp;             // 4 Byte
    uint8_t id;                     // 1 Byte
    uint8_t module;                 // 1 Byte
    float current_consumption;     // 4 Byte
    float current_generation;      // 4 Byte
    float current_storage;         // 4 Byte
    Coordinat coordinates;         // 2 Byte
    uint8_t error;                 // 1 Byte
} SmartGridData;

struct JoinMessage {
    bool is_joining;
    uint8_t mac[6];
    uint8_t module_type; 
};

bool jsonToSmartGrid(const JsonObject& json, SmartGridData* data);
void smartGridToJson(const SmartGridData* data, JsonObject& json);

bool sendSmartGridJson(const JsonDocument& doc, const uint8_t* receiverAddress);

void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc);

void printMacAddress();


void handleJoinMessage(const JoinMessage& joinMsg);
void printKnownPeers();
bool initEspNow(bool printMac);
bool sendEspNowMessage(const uint8_t* macAddress, const uint8_t* data, size_t len);
bool sendEspNowBroadcast(const uint8_t* data, size_t len);
bool addPeerIfNew(const uint8_t* macAddress);

#endif
