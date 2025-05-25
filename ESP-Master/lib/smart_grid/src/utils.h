#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "smart_grid_types.h" 
#include <WiFi.h>
#include <esp_now.h>


void printMacAddress();
void printKnownPeers();
bool parseMac(const char* str, uint8_t* mac);

#endif