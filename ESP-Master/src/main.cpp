#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "smart_grid.h"


//static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// Modulkonfiguration
const ModuleType myModuleType = MODULE_MASTER;  // Anpassen z. B. MODULE_WIND, MODULE_CAR etc.


// Daten-Structs
//SmartGridData smartGridData;
JoinMessage joinMessage;
StaticJsonDocument<256> doc;
uint8_t own_mac[6];
bool recived_mac = false; // Flag, ob eine MAC-Adresse empfangen wurde
uint8_t modulnumber = 0; // Zähler für die Anzahl der empfangenen Module

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000;  // Alle 2 Sekunden

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
            recived_mac = true; // Setze Flag, dass eine MAC-Adresse empfangen wurde
            printKnownPeers(); // Zeige die bekannten Peers an
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

    esp_wifi_get_mac(WIFI_IF_STA, own_mac);  // Hole die MAC-Adresse des Geräts
    
    
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.startsWith("SET_MODE")) {
            // Beispiel: SET_MODE AA:BB:CC:DD:EE:FF 1
            ControlCommand cmd;
            sscanf(input.c_str(), "SET_MODE %hhx:%hhx:%hhx:%hhx:%hhx:%hhx %hhu",
                &cmd.targetMac[0], &cmd.targetMac[1], &cmd.targetMac[2],
                &cmd.targetMac[3], &cmd.targetMac[4], &cmd.targetMac[5],
                &cmd.mode);

            cmd.type = SET_MODE;
            sendControlCommand(cmd.targetMac,cmd);
        }

        else if (input.startsWith("REQUEST_STATUS")) {
            // Beispiel: REQUEST_STATUS AA:BB:CC:DD:EE:FF
            ControlCommand cmd;
            sscanf(input.c_str(), "REQUEST_STATUS %hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &cmd.targetMac[0], &cmd.targetMac[1], &cmd.targetMac[2],
                &cmd.targetMac[3], &cmd.targetMac[4], &cmd.targetMac[5]);

            cmd.type = REQUEST_STATUS;
            //memcpy(cmd.targetMac, own_mac, 6);
            sendControlCommand(cmd.targetMac,cmd);
        }

        else if (input.startsWith("SET_STATUS")) {
            // Erweiterbar: Manuell z. B. aus JSON parsen (später)
            // z. B.: SET_STATUS AA:BB:... 100 50 20

            // Dummy-Implementierung für Beispiel
            ControlCommand cmd;
            sscanf(input.c_str(), "SET_STATUS %hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &cmd.targetMac[0], &cmd.targetMac[1], &cmd.targetMac[2],
                &cmd.targetMac[3], &cmd.targetMac[4], &cmd.targetMac[5]);

            cmd.type = SET_STATUS;
            //cmd.statusOverride = {/* z. B. Dummy-Daten */};
            sendControlCommand(cmd.targetMac,cmd);
        }

        else {
            Serial.println("Unbekannter Befehl.");
        }
    }
    delay(100); // Kurze Pause
}
