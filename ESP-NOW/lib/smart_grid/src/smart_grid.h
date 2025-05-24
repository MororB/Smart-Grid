#ifndef SMART_GRID_H
#define SMART_GRID_H

#include <ArduinoJson.h>
#include <esp_now.h>
#include <Arduino.h>
#include <WiFi.h>

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

bool jsonToSmartGrid(const JsonObject& json, SmartGridData* data);
void smartGridToJson(const SmartGridData* data, JsonObject& json);

bool sendSmartGridJson(const JsonDocument& doc, const uint8_t* receiverAddress);

void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc);

void printMacAddress();

bool initEspNow(bool printMac = false);
bool sendEspNowMessage(const uint8_t* macAddress, const uint8_t* data, size_t len);

#endif
