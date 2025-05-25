#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include "smart_grid_types.h"
#include "utils.h" 
#include "messages.h"
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFi.h>


bool initEspNow(bool printMac);
void sendJoinMessage(uint8_t ModuleType);
bool sendSmartGridJson(const JsonDocument& doc, const uint8_t* receiverAddress);
bool sendEspNowMessage(const uint8_t* macAddress, const uint8_t* data, size_t len);
bool sendEspNowBroadcast(const uint8_t* data, size_t len);

#endif