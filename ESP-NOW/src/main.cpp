#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "smart_grid.h"


static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// Modulkonfiguration
const ModuleType myModuleType = MODULE_SOLAR;  // Anpassen z. B. MODULE_WIND, MODULE_CAR etc.

// Empfängeradresse (für Einzelversand). Bei Broadcast: alle 0xFF
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Daten-Structs
SmartGridData smartGridData;
JoinMessage joinMessage;
StaticJsonDocument<256> doc;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000;  // Alle 2 Sekunden






void setup() {
    Serial.begin(115200);

    if (!initEspNow(true)) {
        Serial.println("ESP-NOW Init fehlgeschlagen, stoppe...");
        while (true) delay(1000);
    }


    Serial.println("Client bereit");

    // Join-Nachricht senden (einmalig)
    sendJoinMessage(myModuleType);
}

void loop() {
    unsigned long now = millis();
    if (now - lastSendTime > sendInterval) {
        doc["timestamp"] = now;
        doc["id"] = 1;  // Beispiel-ID, anpassen je nach Bedarf
        doc["module"] = myModuleType;  // Aktueller Modultyp
        doc["current_consumption"] = 0.0f;  // Beispielwert, anpassen je nach Bedarf
        doc["current_generation"] = 5.0f;  // Beispielwert, anpassen je nach Bedarf
        doc["current_storage"] = 10.0f;  // Beispielwert, anpassen je nach Bedarf
        doc["coordinates"]["x"] = 0;  // Beispielkoordinate, anpassen je nach Bedarf
        doc["coordinates"]["y"] = 0;  // Beispielkoordinate, anpassen je nach Bedarf
        doc["error"] = 0;  // Beispielwert, anpassen je nach Bedarf
        lastSendTime = now;
        sendSmartGridJson(doc, broadcastAddress);
        Serial.println("SmartGrid-Daten gesendet");
    }
}
