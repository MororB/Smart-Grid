#include "control.h"


void handleControlCommand(ControlCommand command){
    Serial.print("ControlCommand empfangen");

    switch (command.type) {
        case SET_MODE:
            Serial.printf("SET_MODE empfangen, Modus: %d\n", command.mode);
            // Hier Logik für SET_MODE implementieren
            break;
        case REQUEST_STATUS:
            Serial.println("REQUEST_STATUS empfangen");
            // Hier Logik für REQUEST_STATUS implementieren
            break;
        case SET_STATUS:
            Serial.println("SET_STATUS empfangen");
            // Hier Logik für SET_STATUS implementieren
            break;
        default:
            Serial.println("Unbekannter ControlCommand-Typ empfangen");
            break;
    }
}




