#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "smart_grid.h"


// Struct für kompakte Übertragung
SmartGridData smartGridData;

// JSON-Dokument 
StaticJsonDocument<256> doc;

void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {

    switch (len) {
        case sizeof(SmartGridData): {
            // Wenn die Länge der empfangenen Daten der Größe von SmartGridData entspricht
            Serial.println("Empfange SmartGridData...");
            handleReceivedSmartGridDataRaw(incomingData, len, doc);
            break;
        }
        case sizeof(JoinMessage): {

            JoinMessage joinMsg;
            memcpy(&joinMsg, incomingData, sizeof(JoinMessage));

            handleJoinMessage(joinMsg);

            printKnownPeers();
            
            break;
        }
    }
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





}
