#include "messages.h"


// Funktion: Empfangene Rohdaten in JSON umwandeln und ausgeben
void handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc) {
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
void smartGridToJson(const SmartGridData* data, JsonDocument& json) {
    json["timestamp"] = data->timestamp;
    json["id"] = data->id;
    json["module"] = data->module;
    json["error"] = data->error;
    json["current_consumption"] = data->current_consumption;
    json["current_generation"] = data->current_generation;
    json["current_storage"] = data->current_storage;
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









