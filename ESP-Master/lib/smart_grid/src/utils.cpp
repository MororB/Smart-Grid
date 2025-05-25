#include "utils.h"



// Funktion zum Ausgeben der bekannten Peers
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

bool parseMac(const char* str, uint8_t* mac) {
    return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}






