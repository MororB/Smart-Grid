#include "communication.h"

// Funktion zum Initialisieren von ESP-NOW
bool initEspNow(bool printMac) {
    esp_now_peer_info_t peerInfo;

    WiFi.mode(WIFI_STA);  // Setze WiFi in den Station-Modus 

    Serial.println("Initialisiere ESP-NOW...");
    if (printMac) {
        printMacAddress();
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("Fehler: ESP-NOW konnte nicht initialisiert werden.");
        return false;
    }

    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return false;
    }
    for (size_t i = 0; i < 6; i++)
    {
        moduleRegistry.modules[0].mac[i] = BROADCAST_MAC[i];
    }
    moduleRegistry.count = 1; // Setze die Anzahl der bekannten Peers auf 1 (Broadcast)

    Serial.println("ESP-NOW erfolgreich initialisiert.");
    return true;
}

void sendJoinMessage(uint8_t ModuleType) {
    JoinMessage joinMessage;
    esp_wifi_get_mac(WIFI_IF_STA, joinMessage.mac);  // Hole die MAC-Adresse des Geräts
    joinMessage.is_joining = true;
    joinMessage.module_type = ModuleType;

    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&joinMessage, sizeof(JoinMessage));
    if (result == ESP_OK) {
        Serial.println("JoinMessage gesendet");
    } else {
        Serial.printf("Fehler beim Senden der JoinMessage: %d\n", result);
    }
}

// Funktion: JSON → Struct → Senden per ESP-NOW
bool sendSmartGridJson(const JsonDocument& doc, const uint8_t* receiverAddress) {
    SmartGridData data;

    if (!jsonToSmartGrid(doc, &data)) {
        Serial.println("Fehler beim Konvertieren der JSON-Nachricht");
        return false;
    }

    esp_err_t result = esp_now_send(receiverAddress, (uint8_t*)&data, sizeof(SmartGridData));
    if (result == ESP_OK) {
        Serial.println("Nachricht erfolgreich gesendet");
        return true;
    } else {
        Serial.printf("Fehler beim Senden: %d\n", result);
        return false;
    }
}



bool sendEspNowMessage(const uint8_t* macAddress, const uint8_t* data, size_t len) {
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(macAddress)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("Fehler: Peer konnte nicht hinzugefügt werden.");
            return false;
        }
    }

    esp_err_t result = esp_now_send(macAddress, data, len);

    if (result == ESP_OK) {
        Serial.println("Nachricht erfolgreich gesendet.");
        return true;
    } else {
        Serial.printf("Fehler beim Senden: %d\n", result);
        return false;
    }
}

bool sendEspNowBroadcast(const uint8_t* data, size_t len) {
    return sendEspNowMessage(BROADCAST_MAC, data, len);
}

void sendControlCommand(const uint8_t* macAddress, const ControlCommand& command) {
    Serial.println("Sende ControlCommand...");
    Serial.printf("Ziel MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        command.targetMac[0], command.targetMac[1], command.targetMac[2],
        command.targetMac[3], command.targetMac[4], command.targetMac[5]);
    esp_err_t result = esp_now_send(macAddress, (uint8_t*)&command, sizeof(ControlCommand));
    
    if (result == ESP_OK) {
        Serial.println("ControlCommand gesendet.");
    } else {
        Serial.printf("Senden fehlgeschlagen: %d\n", result);
    }
}