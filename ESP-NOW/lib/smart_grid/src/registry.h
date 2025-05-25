#ifndef REGISTRY_H
#define REGISTRY_H

#include <Arduino.h>
#include "smart_grid.h"  // z.B. SmartGridData, ModuleType etc.

extern ModuleRegistry moduleRegistry;

bool addPeerIfNew(const uint8_t* macAddress, ModuleType type);
void sendMacListToNewPeer(const uint8_t* receiverMac);
void waitForPeerList(const uint8_t* incomingData);

#endif