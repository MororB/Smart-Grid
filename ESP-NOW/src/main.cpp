#include <Arduino.h>
#include "smart_grid.h"

const ModuleType myModuleType = MODULE_WIND;
SmartGrid smartGrid(myModuleType);

void onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {
    smartGrid.onReceiveCallback(mac, incomingData, len);
}

void setup() {
    Serial.begin(115200);

    if (!smartGrid.initEspNow(true)) {
        Serial.println("ESP-NOW Init fehlgeschlagen, stoppe...");
        while (true) delay(1000);
    }

    esp_now_register_recv_cb(onReceiveCallback);

    smartGrid.begin();
    Serial.println("SmartGrid initialisiert und bereit.");
}

void loop() {
    //SmartGridData data = smartGrid.getSmartGridData();
    smartGrid.update();
    delay(1000); // Update alle 1 Sekunde
    
}
