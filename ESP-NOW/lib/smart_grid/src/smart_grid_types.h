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

enum SmartGridError : uint8_t {
    ERR_NONE             = 0,   ///< kein Fehler
    ERR_COMMUNICATION    = 1,   ///< keine Verbindung / Daten verloren
    ERR_COMM_TIMEOUT     = 2,   ///< Zeitüberschreitung bei Kommunikation
    ERR_DUPLICATE_ID     = 3,   ///< doppelte Modul-ID erkannt
    ERR_REGISTRY         = 4,   ///< Registry fehlerhaft/inkonsistent

    ERR_OVERLOAD         = 10,  ///< Überlast
    ERR_BATTERY_EMPTY    = 11,  ///< Batterie leer
    ERR_BATTERY_FULL     = 12,  ///< Batterie voll
    ERR_NEGATIVE_POWER   = 13,  ///< unplausible Leistungswerte

    ERR_OVERHEAT         = 20,  ///< Temperatur zu hoch
    ERR_SENSOR_FAIL      = 21,  ///< Sensorfehler
    ERR_CALIBRATION      = 22,  ///< Kalibrierung fehlt
    ERR_MOTOR_STALL      = 23,  ///< Motor blockiert

    ERR_MODE_INVALID     = 30,  ///< ungültiger Modus
    ERR_PROFILE_INVALID  = 31,  ///< fehlerhaftes Profil
    ERR_NOT_ALLOWED      = 32,  ///< Aktion nicht erlaubt
    ERR_MEMORY           = 33,  ///< Speicher voll

    ERR_SHUTDOWN         = 99,  ///< Modul im Shutdown
    ERR_EMERGENCY_STOP   = 100, ///< Not-Aus aktiv
    ERR_CRITICAL_FAILURE = 255, ///< Schwerwiegender Fehler
};


enum ControlCommandType {
    SET_MODE,
    REQUEST_STATUS,
    SET_STATUS,
    MODE_NEXT_STEP,
    MODIFY_MODE,
    LEAVE_NETWORK,
    SHUTDOWN_SINGLE_MODULE,
    START_SINGLE_MODULE,
    SHUTDOWN_ALL_MODULES,
    START_ALL_MODULES,
};

enum ModuleMode : uint8_t {
    MODE_WAIT_FOR_REGISTRY,
    MODE_AUTOMATIK,
    MODE_TAGESZYKLUS,
    MODE_NACHTZYKLUS,
    MODE_TAGNACHTZYKLUS,
    MODE_INTERAKTIV,
    MODE_PAUSE,
    MODE_LEAVE_NETWORK,
    MODE_SHUTDOWN
};

enum MessageType : uint8_t {
    MSG_SMARTGRID_DATA = 1,
    MSG_JOIN = 2,
    MSG_MODULE_REGISTRY = 3,
    MSG_CONTROL_COMMAND = 4,
    MSG_REGISTRY_REQUEST = 5,
    MSG_SINGLE_MODULE_REGISTRY = 6,
    MSG_CONTROL_COMMAND_STEP = 7,
    MSG_LEAVE_NETWORK = 8
};

enum LedAnimation: uint8_t {
  NONE,              // keine Animation, LEDs aus
  STARTUP,           // Grün hochfaden (0 -> 255), dann aus
  SHUTDOWN,          // Rot runterfaden (255 -> 0), dann aus
  CONNECTED,         // Blau sanft pulsieren (hoch/runter), dann aus
  ERROR,             // Rotes Blinken (z. B. 3×), dann aus
  NEW_PEER,          // Kurzes Weiß-Aufblitzen, dann aus
  MODE_CHANGE,       // Grün pulsieren (einmal hoch & runter), dann aus
  RECEIVED_NEW_DATA,  // Kurzes Weiß-Aufblitzen (schneller als NEW_PEER), dann aus
  SET_NEW_TRAJECTORY // Blaues Aufblitzen, dann aus
};


typedef struct {
    int8_t x;                  // 1 Byte
    int8_t y;                  // 1 Byte
} Coordinat;

typedef struct {
    uint32_t timestamp;             // 4 Byte
    uint8_t id;                     // 1 Byte
    ModuleType module;                 // 1 Byte
    float current_consumption;     // 4 Byte
    float current_generation;      // 4 Byte
    float current_storage;         // 4 Byte
    Coordinat coordinates;         // 2 Byte
    uint8_t error;                 // 1 Byte
} SmartGridData;

struct StorageLimits {
    float maxChargeW;     // +W laden
    float maxDischargeW;  // +W entladen
    float capacityWh;     // für Grenzprüfung/Anzeige
};

struct JoinMessage {
    uint8_t mac[6];
    SmartGridData data; // Enthält die SmartGrid-Daten des Moduls 
};

#define MAX_ANCHOR_POINTS 25 // Maximale Anzahl an Ankerpunkten für Profile

struct profileMessage {
    //ModuleMode mode; // statt uint8_t mode
    bool consOrGen; // true = Verbrauch, false = Erzeugung
    uint8_t nAnchorPoints; // Anzahl der Ankerpunkte
    uint16_t anchorPoints[MAX_ANCHOR_POINTS]; // Ankerpunkte (max. 10)
    uint8_t interpolationpoints; // Anzahl der Interpolationspunkte
    uint16_t cycleDuration; // Zyklusdauer in Sekunden
};

struct ControlCommand {
    uint8_t targetMac[6];
    ControlCommandType type;
    ModuleMode mode; // statt uint8_t mode
    SmartGridData statusOverride;
    profileMessage profile; // Profilinformationen
};

struct ControlCommandStep {
    uint8_t targetMac[6];
    ControlCommandType type; // MODE_NEXT_STEP
    uint8_t cycleIndex;
};

struct ModuleState {
    uint8_t       mac[6];               // Peer-MAC
    SmartGridData data;                 // zuletzt empfangene Messwerte

        // Netz‑Status (wird von computeNetworkStatus() befüllt)
    float         net;        // data.generation - data.consumption
};

struct ModuleRegistry {
    ModuleState modules[MAX_MODULES];
    uint8_t     count;
};

struct SingleModuleRegistry {
    ModuleState modules;
    uint8_t     count;
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

struct LeaveNetworkMessage {
    MessageType type = MSG_LEAVE_NETWORK;
    uint8_t mac[6];
};

struct ModuleRegistryMessage {
    MessageType type = MSG_MODULE_REGISTRY;
    ModuleRegistry registry;
};

struct ControlCommandMessage {
    MessageType type = MSG_CONTROL_COMMAND;
    ControlCommand command;
};

struct ControlCommandStepMessage {
    MessageType       type = MSG_CONTROL_COMMAND_STEP;   // == MessageType::MSG_CONTROL_COMMAND_STEP
    ControlCommandStep step;
};

struct RegistryRequestMessage {
    MessageType type = MSG_REGISTRY_REQUEST;
    uint8_t requesterMac[6];
};

struct SingleModuleRegistryMessage {
    MessageType type = MSG_SINGLE_MODULE_REGISTRY;
    SingleModuleRegistry registry;
};


struct {
  LedAnimation type = LedAnimation::NONE;
  uint32_t start_ms = 0;
  int step = 0;             // Fortschritt/Zustand der Animation
  bool active = false;
} anim;


#endif