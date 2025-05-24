#include "smart_grid.h"

static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static ModuleRegistry moduleRegistry = { {}, 0 };



//INIT

// Funktion zum Ausgeben der MAC-Adresse
void printMacAddress() {
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



// Join

void sendJoinMessage(uint8_t ModuleType) {
    JoinMessage joinMessage;
    memcpy(joinMessage.mac, WiFi.macAddress().c_str(), 6);  // Nur symbolisch, besser: WiFi.macAddress() manuell in Bytes aufteilen
    joinMessage.is_joining = true;
    joinMessage.module_type = ModuleType;

    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&joinMessage, sizeof(JoinMessage));
    if (result == ESP_OK) {
        Serial.println("JoinMessage gesendet");
    } else {
        Serial.printf("Fehler beim Senden der JoinMessage: %d\n", result);
    }
}

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

void handleJoinMessage(const JoinMessage& joinMsg) {
    Serial.print("Join-Nachricht empfangen von: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", joinMsg.mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    if (addPeerIfNew(joinMsg.mac, static_cast<ModuleType>(joinMsg.module_type))) {
        Serial.printf("Modultyp: %d\n", joinMsg.module_type);
        // Hier kannst du weitere Aktionen durchführen, z.B. das Modul initialisieren
    } else {
        Serial.println("Teilnehmer bereits bekannt.");
    }
}

void printKnownPeers() {
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





//MESSAGE

// Funktion zum Konvertieren von JSON in SmartGridData
bool jsonToSmartGrid(const JsonDocument& json, SmartGridData* data) {


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
void smartGridToJson(const SmartGridData* data, JsonObject& json) {
    json["timestamp"] = data->timestamp;
    json["id"] = data->id;
    json["module"] = data->module;
    json["error"] = data->error;
    json["current_consumption"] = data->current_consumption;
    json["current_generation"] = data->current_generation;
    json["current_storage"] = data->current_storage;
}



//ESP-NOW

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


// Funktion: Empfangene Rohdaten in JSON umwandeln und ausgeben
void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc) {
    if (len != sizeof(SmartGridData)) {
        Serial.printf("Fehlerhafte Länge: %d erwartet, %d erhalten\n", sizeof(SmartGridData), len);
        return;
    }

    SmartGridData data;
    memcpy(&data, rawData, sizeof(SmartGridData));

    JsonObject obj = doc.to<JsonObject>();
    smartGridToJson(&data, obj);

    serializeJsonPretty(doc, Serial);
    Serial.println();
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






