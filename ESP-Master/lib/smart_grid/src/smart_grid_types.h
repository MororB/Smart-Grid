#ifndef SMART_GRID_TYPES_H
#define SMART_GRID_TYPES_H

#include <Arduino.h>
#include "smart_grid_types.h" 


const uint8_t MAX_MODULES = 20; // Maximale Anzahl an Modulen

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

enum ControlCommandType {
    SET_MODE,
    REQUEST_STATUS,
    SET_STATUS
};

struct ControlCommand {
    uint8_t targetMac[6];
    ControlCommandType type;
    uint8_t mode; // z. B. bei SET_MODE
    SmartGridData statusOverride; // bei SET_STATUS
};

struct ModuleInfo {
    uint8_t mac[6];
    ModuleType type;
};

struct ModuleRegistry {
    ModuleInfo modules[MAX_MODULES];
    uint8_t count;
};




static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static ModuleRegistry moduleRegistry = { {}, 0 };

#endif