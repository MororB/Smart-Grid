#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "smart_grid.h"

#define SEND_INTERVAL 2000  // alle 2 Sekunden senden

uint8_t receiverAddress[] = {0x1C, 0x69, 0x20, 0x31, 0x5C, 0x08};
esp_now_peer_info_t peerInfo;

// Struct für kompakte Übertragung
SmartGridData smartGridData;

// JSON-Dokument (dynamisch genug für unsere Daten)
StaticJsonDocument<256> doc;

unsigned long lastSendTime = 0;

void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {
    handleReceivedSmartGridDataRaw(incomingData, len, doc);
}

void setup() {
    Serial.begin(115200);
    if (!initEspNow(true)) {
        Serial.println("Programmabbruch wegen ESP-NOW Fehler.");
        while (true) {  	// Endlosschleife im Fehlerfall
            Serial.println("ESP-NOW Initialisierung fehlgeschlagen");
            Serial.println("Warte auf Neustart...");
            delay(1000);  
        }
    }

    esp_now_register_recv_cb(onReceiveCallback);

    Serial.println("ESP-NOW bereit");
}

void loop() {
    unsigned long now = millis();

    if (now - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = now;

        // Simulierte JSON-Daten (hier könntest du sie auch aus Sensorwerten generieren)
        doc["timestamp"] = "2025-05-18T12:00:00Z";
        doc["id"] = "device_001";
        doc["module_name"] = "solar_module_1";
        doc["current_consumption"] = 1.5;
        doc["current_generation"] = 2.0;
        doc["current_storage"] = 0.8;
        doc["error_message"] = "";

        sendSmartGridJson(doc, receiverAddress);
    }
}
