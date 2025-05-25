#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>
#include "smart_grid_types.h" 
#include <ArduinoJson.h>
#include "registry.h"


bool jsonToSmartGrid(const JsonDocument& json, SmartGridData* data);
void smartGridToJson(const SmartGridData* data, JsonDocument& json);
void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc);
void handleJoinMessage(const JoinMessage& joinMsg);

#endif