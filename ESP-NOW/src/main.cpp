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


    smartGrid.update();
    delay(10);  
}
