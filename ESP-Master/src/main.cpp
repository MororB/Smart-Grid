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

    Serial2.begin(115200, SERIAL_8N1, 16, 17); // I2C Pins für ESP32
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

    // while (Serial2.available()) {
    //     uint8_t b = Serial2.read();
    //     Serial.write(b);
    //     Serial2.println(b); // Echo zurück an Serial2
    // }

    // static uint32_t next = millis() + 5000;
    // if ((int32_t)(millis() - next) >= 0) {
    //     const char* msg = "Hello from ESP32 UART2\n";
    //     Serial2.print(msg);
    //     Serial.println("Sent to Pi: " + String(msg));
    //     next = millis() + 5000;
    // }
    smartGrid.updateMaster();
    delay(10);  
}
