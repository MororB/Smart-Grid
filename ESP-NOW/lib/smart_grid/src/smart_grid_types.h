#ifndef SMART_GRID_TYPES_H
#define SMART_GRID_TYPES_H

#include <Arduino.h>
#include "smart_grid_types.h" 


const uint8_t MAX_MODULES = 20; // Maximale Anzahl an Modulen

enum ModuleType : uint8_t {
    MODULE_MASTER = 0, // Master-Modul, z. B. der zentrale Controller
    // Weitere Module, die im Smart Grid verwendet werden können
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

enum ModuleMode : uint8_t {
    MODE_WAIT_FOR_REGISTRY,
    MODE_AUTOMATIK,
    MODE_TAGESZYKLUS,
    MODE_NACHTZYKLUS,
    MODE_TAGNACHTZYKLUS,
    MODE_INTERAKTIV,
    MODE_PAUSE
};

struct ControlCommand {
    uint8_t targetMac[6];
    ControlCommandType type;
    ModuleMode mode; // statt uint8_t mode
    SmartGridData statusOverride;
};

// struct ModuleInfo {
//     uint8_t mac[6];
//     ModuleType type;
// };

// struct ModuleRegistry {
//     ModuleInfo modules[MAX_MODULES];
//     uint8_t count;
// };

struct ModuleState {
    uint8_t       mac[6];               // Peer-MAC
    ModuleType    type;                 // Peer-Typ
    SmartGridData data;                 // zuletzt empfangene Messwerte

        // Netz‑Status (wird von computeNetworkStatus() befüllt)
    float         net;        // data.generation - data.consumption
    uint8_t       brightness; // 0…255
    CRGB          color;      // Grün bei Überschuss, Rot bei Defizit
};

struct ModuleRegistry {
    ModuleState modules[MAX_MODULES];
    uint8_t     count;
};

struct SingleModuleRegistry {
    ModuleState modules;
    uint8_t     count;
};

enum MessageType : uint8_t {
    MSG_SMARTGRID_DATA = 1,
    MSG_JOIN = 2,
    MSG_MODULE_REGISTRY = 3,
    MSG_CONTROL_COMMAND = 4,
    MSG_REGISTRY_REQUEST = 5,
    MSG_SINGLE_MODULE_REGISTRY = 6
};


static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//static ModuleRegistry moduleRegistry = { {}, 0 };
extern ModuleRegistry moduleRegistry;
extern SmartGridData smartGridData;

struct SmartGridDataMessage {
    MessageType type = MSG_SMARTGRID_DATA;
    SmartGridData data;
};

struct JoinMessageWithType {
    MessageType type = MSG_JOIN;
    JoinMessage join;
};

struct ModuleRegistryMessage {
    MessageType type = MSG_MODULE_REGISTRY;
    ModuleRegistry registry;
};

struct ControlCommandMessage {
    MessageType type = MSG_CONTROL_COMMAND;
    ControlCommand command;
};

struct RegistryRequestMessage {
    MessageType type = MSG_REGISTRY_REQUEST;
    uint8_t requesterMac[6];
};

struct SingleModuleRegistryMessage {
    MessageType type = MSG_SINGLE_MODULE_REGISTRY;
    SingleModuleRegistry registry;
};



#endif