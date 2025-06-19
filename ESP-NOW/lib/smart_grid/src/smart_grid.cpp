#include "smart_grid.h"

SmartGrid::SmartGrid(ModuleType myType)
    : myModuleType(myType)
{
    moduleRegistry.count = 0;
}

bool SmartGrid::initEspNow(bool printMac) {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init fehlgeschlagen!");
        return false;
    }
    if (printMac) printMacAddress();
    return true;
}

void SmartGrid::printMacAddress() const {
    String mac = WiFi.macAddress();
    Serial.print("MAC-Address (String): ");
    Serial.println(mac);
    Serial.print("MAC-Address (formatted): {");
    int index = 0;
    for (int i = 0; i < 6; i++) {
        Serial.print("0x");
        Serial.print(mac.substring(index, index + 2));
        if (i < 5) Serial.print(", ");
        index += 3;
    }
    Serial.println("}");
}

void SmartGrid::sendJoinMessage() {
    JoinMessageWithType msg;
    msg.type = MSG_JOIN;
    msg.join.is_joining = true;
    WiFi.macAddress(msg.join.mac);
    msg.join.module_type = myModuleType;
    esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::sendModuleRegistryToPeer(const uint8_t* receiverMac) {
    ModuleRegistryMessage msg;
    msg.type = MSG_MODULE_REGISTRY;
    msg.registry = moduleRegistry;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::sendControlCommand(const uint8_t* receiverMac, const ControlCommand& command) {
    ControlCommandMessage msg;
    msg.type = MSG_CONTROL_COMMAND;
    msg.command = command;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::handleReceivedModuleRegistry(const uint8_t* incomingData) {
    const ModuleRegistry* receivedRegistry = (const ModuleRegistry*)incomingData;
    for (int i = 0; i < receivedRegistry->count && i < MAX_MODULES; ++i) {
        addPeerIfNew(receivedRegistry->modules[i].mac, receivedRegistry->modules[i].type);
    }
}

void SmartGrid::handleJoinMessage(const JoinMessage& joinMsg) {
    addPeerIfNew(joinMsg.mac, static_cast<ModuleType>(joinMsg.module_type));
}

void SmartGrid::handleControlCommand(const uint8_t* macAddress, ControlCommand command) {
    // Implementiere nach Bedarf
}

void SmartGrid::onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {
    Serial.print("Empfange Daten von MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    if (len < sizeof(MessageHeader)) {
        Serial.println("Nachricht zu kurz!");
        return;
    }

    MessageType type = static_cast<MessageType>(incomingData[0]);

    switch (type) {
        case MSG_SMARTGRID_DATA:
            handleReceivedSmartGridDataRaw(incomingData, len, doc);
            break;
        case MSG_JOIN: {
            JoinMessage joinMsg;
            memcpy(&joinMsg, incomingData, sizeof(JoinMessage));
            handleJoinMessage(joinMsg);
            printKnownPeers();
            break;
        }
        case MSG_MODULE_REGISTRY:
            handleReceivedModuleRegistry(incomingData);
            printKnownPeers();
            break;
        case MSG_CONTROL_COMMAND: {
            if (len == sizeof(ControlCommandMessage)) {
                ControlCommandMessage msg;
                memcpy(&msg, incomingData, sizeof(msg));
                handleControlCommand(mac, msg.command);
            }
            break;
        }
        default:
            Serial.println("Unbekannter Nachrichtentyp!");
            break;
    }
}

void SmartGrid::printKnownPeers() const {
    Serial.println("Bekannte Module:");
    for (int i = 0; i < moduleRegistry.count; i++) {
        Serial.print("Modul ");
        Serial.print(i + 1);
        Serial.print(" (Typ ");
        Serial.print(moduleRegistry.modules[i].type);
        Serial.print("): ");
        for (int j = 0; j < 6; j++) {
            Serial.printf("%02X", moduleRegistry.modules[i].mac[j]);
            if (j < 5) Serial.print(":");
        }
        Serial.println();
    }
}

bool SmartGrid::addPeerIfNew(const uint8_t* macAddress, ModuleType type) {
    for (int i = 0; i < moduleRegistry.count; i++) {
        if (memcmp(moduleRegistry.modules[i].mac, macAddress, 6) == 0) {
            return false;
        }
    }
    if (moduleRegistry.count >= MAX_MODULES) return false;

    memcpy(moduleRegistry.modules[moduleRegistry.count].mac, macAddress, 6);
    moduleRegistry.modules[moduleRegistry.count].type = type;
    moduleRegistry.count++;

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(macAddress)) {
        esp_now_add_peer(&peerInfo);
    }
    return true;
}

SmartGridData SmartGrid::getSmartGridData() const {
    return smartGridData;
}

void SmartGrid::setSmartGridData(const SmartGridData& data) {
    smartGridData = data;
}

const ModuleRegistry& SmartGrid::getModuleRegistry() const {
    return moduleRegistry;
}

// Funktion: Empfangene Rohdaten in JSON umwandeln und ausgeben
void SmartGrid::handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc) {
    if (len != sizeof(SmartGridData)) {
        Serial.printf("Fehlerhafte Länge: %d erwartet, %d erhalten\n", sizeof(SmartGridData), len);
        return;
    }

    SmartGridData data;
    memcpy(&data, rawData, sizeof(SmartGridData));

    JsonDocument obj = doc;
    smartGridToJson(&data, obj);

    serializeJsonPretty(doc, Serial);
    Serial.println();
}

// Funktion zum Konvertieren von JSON in SmartGridData
bool SmartGrid::jsonToSmartGrid(const JsonDocument& json, SmartGridData* data) {
    data->timestamp = json["timestamp"].as<uint32_t>();
    data->id = json["id"].as<uint8_t>();
    data->module = json["module"].as<uint8_t>();
    data->error = json["error"].as<uint8_t>();

    data->current_consumption = json["current_consumption"] | 0.0f;
    data->current_generation = json["current_generation"] | 0.0f;
    data->current_storage = json["current_storage"] | 0.0f;

    return true;
}

// Funktion zum Konvertieren von SmartGridData in JSON
void SmartGrid::smartGridToJson(const SmartGridData* data, JsonDocument& json) {
    json["timestamp"] = data->timestamp;
    json["id"] = data->id;
    json["module"] = data->module;
    json["error"] = data->error;
    json["current_consumption"] = data->current_consumption;
    json["current_generation"] = data->current_generation;
    json["current_storage"] = data->current_storage;
}

void SmartGrid::sendSmartGridData(const uint8_t* receiverMac) {
    SmartGridDataMessage msg;
    msg.type = MSG_SMARTGRID_DATA;
    msg.data = smartGridData;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

ModuleMode SmartGrid::getCurrentMode() const {
    return currentMode;
}

void SmartGrid::setCurrentMode(ModuleMode mode) {
    currentMode = mode;
}

void SmartGrid::update() {
    switch (currentMode) {
        case MODE_AUTOMATIK:
            runAutomatik();
            break;
        case MODE_TAGESZYKLUS:
            runTageszyklus();
            break;
        case MODE_NACHTZYKLUS:
            runNachtzyklus();
            break;
        case MODE_TAGNACHTZYKLUS:
            runTagNachtzyklus();
            break;
        case MODE_INTERAKTIV:
            runInteraktiv();
            break;
        case MODE_PAUSE:
            runPause();
            break;
        default:
            // Optional: Fehlerbehandlung
            break;
    }
}

// Beispiel für eine modusspezifische Funktion
void SmartGrid::runInteraktiv() {
    //int analogValue = analogRead(A0); // Beispiel: Wert einlesen
    //smartGridData.current_consumption = analogValue * 0.01f; // Beispiel: Wert umrechnen
    // ...weitere Logik...
}

void SmartGrid::runAutomatik() {
    // TODO: Automatik-Modus-Logik hier implementieren
}

void SmartGrid::runTageszyklus() {
    // TODO: Tageszyklus-Modus-Logik hier implementieren
}

void SmartGrid::runNachtzyklus() {
    // TODO: Nachtzyklus-Modus-Logik hier implementieren
}

void SmartGrid::runTagNachtzyklus() {
    // TODO: TagNachtZyklus-Modus-Logik hier implementieren
}

void SmartGrid::runPause() {
    // TODO: Pause-Modus-Logik hier implementieren
}






