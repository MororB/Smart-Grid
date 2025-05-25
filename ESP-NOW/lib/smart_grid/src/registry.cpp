#include "registry.h"

// Funktion zum Hinzufügen eines neuen Teilnehmers, wenn er noch nicht bekannt ist
bool addPeerIfNew(const uint8_t* macAddress, ModuleType type = MODULE_SOLAR) {
    for (int i = 0; i < moduleRegistry.count; i++) {
        if (memcmp(moduleRegistry.modules[i].mac, macAddress, 6) == 0) {
            Serial.println("Peer bereits bekannt, überspringe Speicherung.");
            return false; // Peer ist schon gespeichert
        }
    }

    if (moduleRegistry.count >= MAX_MODULES) {
        Serial.println("Maximale Peer-Anzahl erreicht!");
        return false;
    }

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

    Serial.print("Neuer Teilnehmer gespeichert: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    return true;
}

// Funktion zum Senden der ModuleRegistry an einen neuen Teilnehmer
void sendMacListToNewPeer(const uint8_t* receiverMac) {
    sendEspNowMessage(receiverMac, (uint8_t*)&moduleRegistry, sizeof(ModuleRegistry));
}

// Funktion zum Verarbeiten der empfangenen ModuleRegistry
void waitForPeerList(const uint8_t* incomingData) {
    const ModuleRegistry* receivedRegistry = (const ModuleRegistry*)incomingData;
    Serial.println("Empfange ModuleRegistry von Peer:");

    for (int i = 0; i < receivedRegistry->count && i < MAX_MODULES; ++i) {
        addPeerIfNew(receivedRegistry->modules[i].mac, receivedRegistry->modules[i].type);
    }

    Serial.println("ModuleRegistry verarbeitet.");
}
