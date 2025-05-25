#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "smart_grid.h"


//static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// Modulkonfiguration
const ModuleType myModuleType = MODULE_WIND;  // Anpassen z. B. MODULE_WIND, MODULE_CAR etc.


// Daten-Structs
SmartGridData smartGridData;
JoinMessage joinMessage;
StaticJsonDocument<256> doc;

bool recived_mac = false; // Flag, ob eine MAC-Adresse empfangen wurde
uint8_t modulnumber = 0; // Zähler für die Anzahl der empfangenen Module

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000;  // Alle 2 Sekunden

void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {

    Serial.print("Empfange Daten von MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }



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

            modulnumber++; // Erhöhe den Zähler für die Anzahl der empfangenen Module
            Serial.print("Anzahl empfangener Module: ");
            Serial.println(modulnumber);

            if (modulnumber == 1) {
                // Wenn das erste Modul empfangen wurde, sende die MAC-Liste an den neuen Teilnehmer
                sendMacListToNewPeer(mac);
            } else {
                // Wenn weitere Module empfangen wurden, sende die MAC-Liste nicht erneut
                Serial.println("Weitere Module empfangen, keine MAC-Liste gesendet.");
            }
            
            break;
        }
        case sizeof(ModuleRegistry): {
            // Wenn die Länge der empfangenen Daten der Größe von MacListMessage entspricht
            Serial.println("Empfange MAC-Liste...");
            waitForPeerList(incomingData);
            printKnownPeers(); // Zeige die bekannten Peers an
            recived_mac = true; // Setze Flag, dass eine MAC-Adresse empfangen wurde
            break;
        }
        
        case sizeof(ControlCommand): {
            ControlCommand command;
            memcpy(&command, incomingData, sizeof(ControlCommand));

            Serial.print("Empfange ControlCommand test ");
            handleControlCommand(mac,command);
            Serial.println("ControlCommand verarbeitet.");

            break;
        }
    }
}


void setup() {
    Serial.begin(115200);

    if (!initEspNow(true)) {
        Serial.println("ESP-NOW Init fehlgeschlagen, stoppe...");
        while (true) delay(1000);
    }

    esp_now_register_recv_cb(onReceiveCallback);

    Serial.println("Client bereit");

    // Join-Nachricht senden (einmalig)
    sendJoinMessage(myModuleType);

    unsigned long startMillis = millis();
    while ((recived_mac == false) && (millis()-startMillis < 3000)) {
        // Warten, bis eine MAC-Adresse empfangen wurde
        Serial.println("Warte auf MAC-Adresse...");
        delay(10);
    }

    
    
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
        doc["coordinates"]["x"] = 1;  // Beispielkoordinate, anpassen je nach Bedarf
        doc["coordinates"]["y"] = 5;  // Beispielkoordinate, anpassen je nach Bedarf
        doc["error"] = 0;  // Beispielwert, anpassen je nach Bedarf
        lastSendTime = now;
        jsonToSmartGrid(doc, &smartGridData);  // Konvertiere JSON zu SmartGridData
        //sendSmartGridJson(doc, BROADCAST_MAC);
        //Serial.println("SmartGrid-Daten gesendet");
    }
}
