#include "control.h"


void handleControlCommand(const uint8_t* macAddress, ControlCommand command){
    Serial.print("ControlCommand empfangen");

    switch (command.type) {
        case SET_MODE:
            Serial.printf("SET_MODE empfangen, Modus: %d\n", command.mode);
            
            break;
        case REQUEST_STATUS: {
            Serial.println("REQUEST_STATUS empfangen");

            StaticJsonDocument<256> doc_int;
            smartGridToJson(&smartGridData, doc_int);
            sendSmartGridJson(doc_int, macAddress);
            break;
        }

        case SET_STATUS:
            Serial.println("SET_STATUS empfangen");
            // Hier Logik für SET_STATUS implementieren
            break;
        default:
            Serial.println("Unbekannter ControlCommand-Typ empfangen");
            break;
    }
}



