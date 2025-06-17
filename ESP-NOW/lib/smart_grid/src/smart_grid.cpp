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
    JoinMessage joinMsg;
    joinMsg.is_joining = true;
    WiFi.macAddress(joinMsg.mac);
    joinMsg.module_type = myModuleType;
    esp_now_send(BROADCAST_MAC, (uint8_t*)&joinMsg, sizeof(joinMsg));
}

void SmartGrid::sendModuleRegistryToPeer(const uint8_t* receiverMac) {
    esp_now_send(receiverMac, (uint8_t*)&moduleRegistry, sizeof(ModuleRegistry));
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
    // Beispiel: Unterscheidung nach Nachrichtentyp
    // Hier kannst du anhand des Inhalts von incomingData entscheiden, was zu tun ist
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

// ... weitere Methoden analog ...






