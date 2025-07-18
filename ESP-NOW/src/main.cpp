#include <Arduino.h>
#include "smart_grid.h"

#ifndef MODULE_TYPE
  #define MODULE_TYPE MODULE_UNKNOWN
#endif

const ModuleType myModuleType = MODULE_TYPE;
SmartGrid smartGrid(myModuleType);

void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {
    //Serial.print("Hab was empfangen");
    smartGrid.onReceiveCallback(mac, incomingData, len);
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Kurze Pause für die serielle Verbindung
    Serial.println("module_type: " + String(myModuleType));

    if (!smartGrid.initEspNow(true)) {
        Serial.println("ESP-NOW Init fehlgeschlagen, stoppe...");
        while (true) delay(1000);
    }

    esp_now_register_recv_cb(onReceiveCallback);

    smartGrid.begin();
    delay(1000); // Kurze Pause, um sicherzustellen, dass alles initialisiert ist
    Serial.println("SmartGrid initialisiert und bereit.");
}

void loop() {
    // static unsigned long lastRandomUpdate = 0;
    // static unsigned long nextRandomInterval = 0;

    // unsigned long now = millis();

    // // Zufälliges Intervall zwischen 500 und 3000 ms
    // if (nextRandomInterval == 0) {
    //     nextRandomInterval = random(2000, 30000);
    //     lastRandomUpdate = now;
    // }

    // if (now - lastRandomUpdate > nextRandomInterval) {
    //     SmartGridData data;
    //     data.timestamp = now;
    //     data.id = random(1, 255);
    //     data.module = myModuleType;
    //     data.error = random(0, 2);
    //     data.current_consumption = random(0, 1000) / 10.0f;
    //     data.current_generation = random(0, 1000) / 10.0f;
    //     data.current_storage = random(0, 1000) / 10.0f;
    //     data.coordinates.x = random(0, 100);
    //     data.coordinates.y = random(0, 100);

    //     smartGrid.setSmartGridData(data);

    //     Serial.println("SmartGridData zufällig aktualisiert!");

    //     lastRandomUpdate = now;
    //     nextRandomInterval = random(2000, 30000);
    // }

    smartGrid.update();
    delay(10);  
}
