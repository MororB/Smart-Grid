#include "smart_grid.h"

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
bool initEspNow(bool printMac = false) {

    WiFi.mode(WIFI_STA);  // Setze WiFi in den Station-Modus 

    Serial.println("Initialisiere ESP-NOW...");
    if (printMac) {
        printMacAddress();
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("Fehler: ESP-NOW konnte nicht initialisiert werden.");
        return false;
    }

    Serial.println("ESP-NOW erfolgreich initialisiert.");
    return true;
}

// Funktion zum Konvertieren von JSON in SmartGridData
bool jsonToSmartGrid(const JsonObject& json, SmartGridData* data) {
    if (!json.containsKey("timestamp") ||
        !json.containsKey("id") ||
        !json.containsKey("module") ||
        !json.containsKey("error"))
        return false;

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


// Funktion: JSON → Struct → Senden per ESP-NOW
bool sendSmartGridJson(const JsonDocument& doc, const uint8_t* receiverAddress) {
    SmartGridData data;
    if (!jsonToSmartGrid(doc.as<JsonObject>(), &data)) {
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
