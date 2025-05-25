#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>
#include "smart_grid_types.h" 
#include "communication.h"
#include "utils.h"
#include <ArduinoJson.h>

void handleControlCommand(const uint8_t* macAddress,ControlCommand command);

#endif