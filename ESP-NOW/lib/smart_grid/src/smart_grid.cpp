#include "smart_grid.h"

SmartGrid::SmartGrid(ModuleType myType)
    : myModuleType(myType)
    ,display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) // -1 für Reset-Pin, da nicht verwendet
{
    moduleRegistry.count = 0;
}

bool SmartGrid::initEspNow(bool printMac) {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init fehlgeschlagen!");
        return false;
    }
    // Peer für Broadcast hinzufügen (Workaround für manche ESP32-Versionen)
    esp_now_peer_info_t peerInfo = {};
    memset(&peerInfo, 0, sizeof(peerInfo));
    for (int i = 0; i < 6; ++i) peerInfo.peer_addr[i] = 0xFF;
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    Serial.println("ESP-NOW Init erfolgreich!");
    if (printMac) printMacAddress();
    return true;
}

void SmartGrid::printMacAddress() const {
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

void SmartGrid::sendJoinMessage() {
    Serial.println("Sende Join-Message...");
    JoinMessageWithType msg;
    msg.type = MSG_JOIN;
    WiFi.macAddress(msg.join.mac);
    msg.join.data = getSmartGridData(); // Aktuelle SmartGrid-Daten anhängen
    esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::sendModuleRegistryToPeer(const uint8_t* receiverMac) {
    SingleModuleRegistryMessage msg;
    for( uint8_t i = 0; i < moduleRegistry.count && i < MAX_MODULES; ++i) {
        //memcpy(msg.registry.modules, moduleRegistry.modules[i], sizeof(ModuleState));
        msg.registry.modules = moduleRegistry.modules[i];
        msg.registry.count = moduleRegistry.count;
        esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
        delay(100);
    }
}

void SmartGrid::sendControlCommand(const uint8_t* receiverMac, const ControlCommand& command) {
    ControlCommandMessage msg;
    msg.type = MSG_CONTROL_COMMAND;
    msg.command = command;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::handleReceivedModuleRegistry(const uint8_t* incomingData) {
    registryReceived = true;
    SingleModuleRegistryMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));
    //const ModuleRegistry* receivedRegistry = (const ModuleRegistry*)incomingData;
    addPeerIfNew(msg.registry.modules.mac, msg.registry.modules.data);
    computeNetworkStatus();
    
}

void SmartGrid::handleJoinMessage(const JoinMessageWithType& joinMsg) {
    newPeerCount++; // Zähler erhöhen
    addPeerIfNew(joinMsg.join.mac, joinMsg.join.data);
}

void SmartGrid::handleControlCommand(const uint8_t* macAddress, ControlCommand command) {
    // Implementiere nach Bedarf
    Serial.print("Empfange ControlCommand");
    Serial.print(" ID");
    Serial.print(command.type);

    switch (command.type)  // Beispiel für die Verarbeitung von ControlCommand
    {
    case ControlCommandType::SET_MODE:
        Serial.print("Setze Modus auf: ");
        Serial.println(command.mode);
        setCurrentMode(command.mode);
        break;
    case ControlCommandType::REQUEST_STATUS:
        Serial.println("Status angefordert");
        sendSmartGridData(macAddress);
        Serial.println("Sende SmartGridData an anfragendes Modul");
        break;
    case ControlCommandType::SET_STATUS:
        Serial.println("Setze SmartGridData Status");
        setSmartGridData(command.statusOverride);
        Serial.print("Neue SmartGridData: ");
        Serial.print("Timestamp: ");
        Serial.println(command.statusOverride.timestamp);
        Serial.print("ID: ");
        Serial.println(command.statusOverride.id);
        Serial.print("Module: ");
        Serial.println(command.statusOverride.module);
        Serial.print("Current Consumption: ");
        Serial.println(command.statusOverride.current_consumption);
        Serial.print("Current Generation: ");
        Serial.println(command.statusOverride.current_generation);
        Serial.print("Current Storage: ");
        Serial.println(command.statusOverride.current_storage);
        Serial.print("Coordinates: ");
        Serial.print(command.statusOverride.coordinates.x);
        Serial.print(", ");
        Serial.println(command.statusOverride.coordinates.y);
        Serial.print("Error: ");
        Serial.println(command.statusOverride.error);
        //dataChanged = true; // Markiere, dass sich die Daten geändert haben
        Serial.println("SmartGridData erfolgreich gesetzt");
        break;    
    
    case ControlCommandType::MODE_NEXT_STEP:
        Serial.println("Nächster Schritt im Modus");
        modeMakeStep = true; // Setze Flag, dass der Modus einen Schritt machen soll
        break;

    case ControlCommandType::MODIFY_MODE:{
        Serial.println("Modifiziere Modus");

        uint8_t npoint = command.profile.interpolationpoints;
        profileSize = npoint; // Setze die Größe des Profils
        command.profile.nAnchorPoints = 24;

        if (command.profile.consOrGen == true) {
            Serial.println("Verbrauchsprofil empfangen");
            for (size_t i = 0; i < command.profile.nAnchorPoints; ++i) {
                Serial.print(command.profile.anchorPoints[i], 1);
                Serial.print(" ");
            }
            Serial.println();
            consAnchors.assign(command.profile.anchorPoints, command.profile.anchorPoints + command.profile.nAnchorPoints);
        Serial.print("Empfangenes Verbrauchsprofil: ");
        for (const auto& val : consAnchors) {
            Serial.print(val, 1);
            Serial.print(" ");
        }
        Serial.println();
            setDailyProfiles(consAnchors, npoint, cycleDurationMs,1);
        } else {
            Serial.println("Erzeugungsprofil empfangen");
            genAnchors.assign(command.profile.anchorPoints, command.profile.anchorPoints + command.profile.nAnchorPoints);
                    Serial.print("Empfangenes Verbrauchsprofil: ");
        for (const auto& val : genAnchors) {
            Serial.print(val, 1);
            Serial.print(" ");
        }
            setDailyProfiles(genAnchors, npoint, cycleDurationMs,0);
        }

        Serial.print("Anzahl Ankerpunkte: ");
        Serial.println(command.profile.nAnchorPoints);
        Serial.print("Anzahl Interpolationspunkte: ");
        Serial.println(command.profile.interpolationpoints);
        Serial.print("Zyklusdauer: ");
        Serial.println(command.profile.cycleDuration);
        break;
    }
    default:
        Serial.println("Unbekannter ControlCommand-Typ!");
        break;
    }
}

// Funktion: Empfangene Rohdaten in JSON umwandeln und ausgeben
void SmartGrid::handleReceivedSmartGridDataRaw(const uint8_t* rawData, int len, JsonDocument& doc) {
    if (len != sizeof(SmartGridData)) {
        Serial.printf("Fehlerhafte Länge: %d erwartet, %d erhalten\n", sizeof(SmartGridData), len);
        return;
    }

    SmartGridData data;
    memcpy(&data, rawData, sizeof(SmartGridData));

    JsonDocument obj = doc;
    smartGridToJson(&data, obj);

    serializeJsonPretty(doc, Serial);
    Serial.println();
}

void SmartGrid::handleRecivedSmartGridData(const uint8_t* mac, const uint8_t* incomingData, int len ) {
    if (incomingData == nullptr) {
        Serial.println("Empfangene Daten sind leer!");
        return;
    }

    SmartGridDataMessage msg;
    memcpy(&msg, incomingData, sizeof(SmartGridDataMessage));

    // Prüfe, ob das sendende Modul in der Registry ist
    bool found = false;
    for (int i = 0; i < moduleRegistry.count; ++i) {
        if (memcmp(moduleRegistry.modules[i].mac, mac, 6) == 0) {
            // Update die Daten des Moduls
            moduleRegistry.modules[i].data = msg.data;
            found = true;
            newData = true; // Markiere, dass neue Daten empfangen wurden
            break;
        }
    }

    if (!found) {
        Serial.println("Empfangene Daten von unbekanntem Modul!");
    }

}

void SmartGrid::onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {
    Serial.print("Empfange Daten von MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    if (len < sizeof(MessageType)) {
        Serial.println("Nachricht zu kurz!");
        return;
    }

    MessageType type = static_cast<MessageType>(incomingData[0]);

    switch (type) {
        case MSG_SMARTGRID_DATA:
            //handleReceivedSmartGridDataRaw(incomingData, len, doc);
            Serial.println("Empfange SmartGridData...");
            handleRecivedSmartGridData(mac, incomingData, len);
            break;
        case MSG_JOIN: {
            JoinMessageWithType joinMsg;
            memcpy(&joinMsg, incomingData, sizeof(JoinMessageWithType));
            handleJoinMessage(joinMsg);
            printKnownPeers();
            break;
        }
        case MSG_SINGLE_MODULE_REGISTRY:
        Serial.println("Empfange Module Registry...");
            handleReceivedModuleRegistry(incomingData);
            printKnownPeers();
            break;
        case MSG_CONTROL_COMMAND: {
            if (len == sizeof(ControlCommandMessage)) {
                ControlCommandMessage msg;
                memcpy(&msg, incomingData, sizeof(msg));
                handleControlCommand(mac, msg.command);
            }
            break;
        }
        case MSG_REGISTRY_REQUEST: {
            Serial.println("Empfange RegistryRequest...");
            RegistryRequestMessage req;
            memcpy(&req, incomingData, sizeof(req));
            receivedRegistryRequests++; // Zähle mit

            Serial.print("Anzahl empfangener RegistryRequests: ");
            Serial.println(receivedRegistryRequests);

            esp_now_peer_info_t peerInfo{};
            memcpy(peerInfo.peer_addr, mac, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (!esp_now_is_peer_exist(mac)) {
                esp_now_add_peer(&peerInfo);
            }


            uint8_t myMac[6];
            WiFi.macAddress(myMac);

            Serial.print("Meine MAC: ");
            for (int i = 0; i < 6; ++i) {
                Serial.printf("%02X", myMac[i]);
                if (i < 5) Serial.print(":");
            }
            Serial.println();

            Serial.print("Requester MAC: ");
            for (int i = 0; i < 6; ++i) {
                Serial.printf("%02X", req.requesterMac[i]);
                if (i < 5) Serial.print(":");
            }
            Serial.println();

            if(newPeerCount - receivedRegistryRequests == 0){
                sendModuleRegistryToPeer(req.requesterMac);
                Serial.println("Sende Registry an anfragendes Modul");
            }
            else {
                Serial.println("Keine Registry gesendet, da ich nicht an der der Reihe bin");
            }
            break;
        }
        default:
            Serial.println("Unbekannter Nachrichtentyp!");
            break;
    }
}

void SmartGrid::printKnownPeers() const {
    Serial.println("Bekannte Module:");
    for (int i = 0; i < moduleRegistry.count; i++) {
        Serial.print("Modul ");
        Serial.print(moduleRegistry.modules[i].data.id);
        Serial.print(" (Typ ");
        Serial.print(moduleRegistry.modules[i].data.module);
        Serial.print("): ");
        for (int j = 0; j < 6; j++) {
            Serial.printf("%02X", moduleRegistry.modules[i].mac[j]);
            if (j < 5) Serial.print(":");
        }
        Serial.println();
    }
}

bool SmartGrid::addPeerIfNew(const uint8_t* macAddress, SmartGridData data) {
    // 1) Schon drin?
    for (int i = 0; i < moduleRegistry.count; ++i) {
        if (memcmp(moduleRegistry.modules[i].mac, macAddress, 6) == 0) {
            return false;
        }
    }
    // 2) Platz prüfen
    if (moduleRegistry.count >= MAX_MODULES) {
        Serial.println("Maximale Peer-Anzahl erreicht!");
        return false;
    }

    // 3) Neuen Eintrag anlegen
    Serial.print("ADD TYPE:");
    Serial.println(data.module);
    uint8_t newId = moduleRegistry.count + 1;
    ModuleState& m = moduleRegistry.modules[moduleRegistry.count++];
    memcpy(m.mac, macAddress, 6);
    moduleRegistry.modules[moduleRegistry.count].data = data;
    moduleRegistry.modules[moduleRegistry.count].data.id = newId; // ID setzen



    // 4) ESP-NOW Peer eintragen
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(macAddress)) {
        esp_now_add_peer(&peerInfo);
    }

    // 5) Debug-Output
    Serial.print("Neues Modul: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(':');
    }
    Serial.print("  Typ=");
    Serial.println(data.module);
    Serial.print("  ID=");
    Serial.println(newId);

    // 6) Erstes Peer → Registry senden
    if (newPeerCount == 1) {
        sendModuleRegistryToPeer(macAddress);
    }

    return true;
}


SmartGridData SmartGrid::getSmartGridData() const {
    return smartGridData;
}

void SmartGrid::setSmartGridData(const SmartGridData& data) {
    smartGridData = data;
    dataChanged = true; // Markiere, dass sich die Daten geändert haben
    //die modulr registry des eigenen Moduls soll aktualisiert werden
    for (int i = 0; i < moduleRegistry.count; ++i) {
        if (memcmp(moduleRegistry.modules[i].mac, own_mac, 6) == 0) {
            moduleRegistry.modules[i].data = data;
            break; // Modul gefunden und aktualisiert, Schleife beenden
        }
    }
    
}

const ModuleRegistry& SmartGrid::getModuleRegistry() const {
    return moduleRegistry;
}


// Funktion zum Konvertieren von JSON in SmartGridData
bool SmartGrid::jsonToSmartGrid(const JsonDocument& json, SmartGridData* data) {
    data->timestamp = json["timestamp"].as<uint32_t>();
    data->id = json["id"].as<uint8_t>();
    data->module = static_cast<ModuleType>(json["module"].as<uint8_t>());
    data->error = json["error"].as<uint8_t>();

    data->current_consumption = json["current_consumption"] | 0.0f;
    data->current_generation = json["current_generation"] | 0.0f;
    data->current_storage = json["current_storage"] | 0.0f;

    return true;
}

// Funktion zum Konvertieren von SmartGridData in JSON
void SmartGrid::smartGridToJson(const SmartGridData* data, JsonDocument& json) {
    json["timestamp"] = data->timestamp;
    json["id"] = data->id;
    json["module"] = data->module;
    json["error"] = data->error;
    json["current_consumption"] = data->current_consumption;
    json["current_generation"] = data->current_generation;
    json["current_storage"] = data->current_storage;
}

void SmartGrid::sendSmartGridData(const uint8_t* receiverMac) {
    SmartGridDataMessage msg;
    msg.type = MSG_SMARTGRID_DATA;
    msg.data = smartGridData;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

ModuleMode SmartGrid::getCurrentMode() const {
    return currentMode;
}

void SmartGrid::setCurrentMode(ModuleMode mode) {
    currentMode = mode;
}

void SmartGrid::computeNetworkStatus() {
    // Berechne den Netzstatus für jedes Modul
    Serial.println("Berechne Netzstatus...");
    float maxAbs = 0;

    int ownIndex = -1;
    for (int j = 0; j < moduleRegistry.count && j < MAX_MODULES; ++j) {
        if (memcmp(moduleRegistry.modules[j].mac, own_mac, 6) == 0) {
            ownIndex = j;
            break;
        }
    }
    if (ownIndex == -1) {
        Serial.println("Eigenes Modul nicht gefunden!");
        return; // Eigenes Modul nicht gefunden, Abbruch
    }

    smartGridData.id = moduleRegistry.modules[ownIndex].data.id; // Setze ID des eigenen Moduls
    //smartGridData.module = moduleRegistry.modules[ownIndex].type; // Setze Modultyp des eigenen Moduls


    for (int i = 0; i < moduleRegistry.count; ++i) {
        auto& s = moduleRegistry.modules[i];
        s.net = s.data.current_generation - s.data.current_consumption;
        maxAbs = max(maxAbs, fabs(s.net));
    }

    // Berechne nur für das eigene Modul die Werte für brightness und color
    if (ownIndex != -1) {
        auto& s = moduleRegistry.modules[ownIndex];
        if (s.net == 0) {
            brightness = 0;
            motorPwm = 0; // Motor aus
            //motorForward = true; // Vorwärts, wenn Netzstatus 0
        } else {
            brightness = (uint8_t)((fabs(s.net) / maxAbs) * 255);
            motorPwm = (uint8_t)((fabs(s.net) / maxAbs) * 255);
            //motorForward = (s.net >= 0); // Vorwärts, wenn net positiv
        }

        Serial.print("Eigenes Modul - Netzstatus: ");
        Serial.print(s.net);
        Serial.print("  Helligkeit: ");
        Serial.print(brightness);
        Serial.print("  Farbe: ");

        color = (s.net >= 0) ? CRGB::Green : CRGB::Red;
        if (s.net >= 0) {
            Serial.println("Grün");
        } else {
            Serial.println("Rot");
        }
    }
}

bool SmartGrid::checkForChanges() {
    bool changed = false;

    if (newData) {
        // Aktualisiere Netzstatus und LED-Anzeige
        // computeNetworkStatus();
        // updateDisplay();
        // updateLED();
        computeNetworkStatus();
        newData = false; // Nach der Aktualisierung zurücksetzen
        changed = true;
    }

    if (dataChanged) {
        sendNewSmartGridData();
        computeNetworkStatus();
        dataChanged = false; // Nach dem Senden zurücksetzen
        changed = true; // Daten haben sich geändert
    }

    return changed; // Am Ende wird entschieden, ob sich etwas geändert hat
}

void SmartGrid::sendNewSmartGridData() {
    // Sende SmartGridData an alle bekannten Peers
    for (int i = 0; i < moduleRegistry.count; ++i) {
        sendSmartGridData(moduleRegistry.modules[i].mac);
        delay(10);
    }
}



void SmartGrid::updateDisplay() {
    // 1) Display vorbereiten
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    // 2) Daten abrufen
    const SmartGridData& d = smartGridData;

    // 3) Zeilenweise ausgeben
    display.setCursor(0, 0);
    display.print(F("Time: "));
    display.println(d.timestamp);

    display.setCursor(0, 8); // erste Spalte: y = 0 + 8 = 8, hier z.B. 8
    display.print(F("ID: "));
    display.print(d.id);
    display.print(F("  Mod: "));
    display.println(d.module);

    display.setCursor(0, 16);
    display.print(F("Cons: "));
    display.print(d.current_consumption, 1);  // 1 Nachkommastelle
    display.print(F(" W"));

    display.setCursor(0,  24); // zweite Spalte: y = 10 + 8 = 18, hier z.B. 18
    display.print(F("Gen: "));
    display.print(d.current_generation, 1);
    display.print(F(" W"));

    display.setCursor(0, 32);
    display.print(F("Stor: "));
    display.print(d.current_storage, 1);
    display.print(F(" W"));

    display.setCursor(0, 40);
    display.print(F("X: "));
    display.print(d.coordinates.x);
    display.print(F(" Y: "));
    display.print(d.coordinates.y);

    display.setCursor(0, 48);
    display.print(F("Err: "));
    display.println(d.error);

    // 4) Tatsächlich anzeigen
    display.display();
}


void SmartGrid::updateLED() {
    // Suche eigenen Modul-Eintrag nur einmal
    int ownIndex = -1;
    for (int j = 0; j < moduleRegistry.count && j < MAX_MODULES; ++j) {
        if (memcmp(moduleRegistry.modules[j].mac, own_mac, 6) == 0) {
            ownIndex = j;
            break;
        }
    }
    //Serial.print("Eigenes Modul gefunden bei Index: ");
    //Serial.println(ownIndex);
    if (ownIndex == -1) return; // Eigenes Modul nicht gefunden

    //Serial.print("FARBE SET:");
    //Serial.print(color);

    // Setze alle LEDs entsprechend
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
        leds[i].nscale8_video(brightness);
    }
    FastLED.show();
}

void SmartGrid::updateMotor() {
    // Vorwärts → IN1 = PWM, IN2 = 0
    if (motorForward) {
        ledcWrite(MOTOR_PWM_CH_A, motorPwm);
        ledcWrite(MOTOR_PWM_CH_B, 0);
    }
    // Rückwärts → IN1 = 0, IN2 = PWM
    else {
        ledcWrite(MOTOR_PWM_CH_A, 0);
        ledcWrite(MOTOR_PWM_CH_B, motorPwm);
    }

    Serial.printf("updateMotor: IN1=%u, IN2=%u\n",
                  motorForward ? motorPwm : 0,
                  motorForward ? 0        : motorPwm);
}


void SmartGrid::readSolarcell() {
    Serial.println("Aktualisiere Solarmodul...");
    // Hier könnte Logik für das Auslesen des Solarmoduls implementiert werden
}

void SmartGrid::update() {
    switch (currentMode) {
        case MODE_WAIT_FOR_REGISTRY:
            runWaitForRegistry();
            break;
        case MODE_AUTOMATIK:
            runAutomatik();
            break;
        case MODE_TAGESZYKLUS:
            runTageszyklus();
            break;
        case MODE_NACHTZYKLUS:
            runNachtzyklus();
            break;
        case MODE_TAGNACHTZYKLUS:
            runTagNachtzyklus();
            break;
        case MODE_INTERAKTIV:
            runInteraktiv();
            break;
        case MODE_PAUSE:
            runPause();
            break;
        default:
            // Optional: Fehlerbehandlung
            break;
    }
}

// Beispiel für eine modusspezifische Funktion
void SmartGrid::runInteraktiv() {

    switch (myModuleType)
    {
    case MODULE_WIND:
        // Logik für Windmodul
        //updateMotor();
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        //readSolarcell();
        break;
    case MODULE_BATTERY:
        // Logik für Batteriespeicher
        break;
    case MODULE_HYDRO:
        // Logik für Wasserkraftmodul
        break;
    case MODULE_ELECTROLYZER:
        // Logik für Elektrolyseur
        break;
    case MODULE_HYDROGEN:
        // Logik für Wasserstoffmodul
        break;
    case MODULE_PUMP_STORAGE:
        // Logik für Pumpspeicherkraftwerk
        break;
    case MODULE_HOUSE:
        // Logik für Hausmodul
        break;  
    case MODULE_FACTORY:
        // Logik für Fabrikmodul
        break;
    case MODULE_CAR:
        // Logik für Automodul
        break;
    case MODULE_SUBSTATION:
        // Logik für Umspannwerk
        break;
    case MODULE_MASTER:
        // Logik für Master-Modul
        break;
    default:
        break;
    }
    unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateSystemTime();
        updateDisplay();
        updateLED();
        last_update=millis();
    }
}

void SmartGrid::runAutomatik() {
    unsigned long time = millis();
    switch (myModuleType)
    {
    case MODULE_WIND:
        if(checkForChanges() || (time-last_update)>5000) {
            updateMotor(); // Motorsteuerung für Windmodul
            Serial.println("Aktualisiere Windmodul...");
            //last_update=millis();
        }
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        break;
    case MODULE_BATTERY:
        // Logik für Batteriespeicher
        break;
    case MODULE_HYDRO:
        // Logik für Wasserkraftmodul
        break;
    case MODULE_ELECTROLYZER:
        // Logik für Elektrolyseur
        break;
    case MODULE_HYDROGEN:
        // Logik für Wasserstoffmodul
        break;
    case MODULE_PUMP_STORAGE:
        // Logik für Pumpspeicherkraftwerk
        break;
    case MODULE_HOUSE:
        // Logik für Hausmodul
        break;  
    case MODULE_FACTORY:
        // Logik für Fabrikmodul
        break;
    case MODULE_CAR:
        // Logik für Automodul
        break;
    case MODULE_SUBSTATION:
        // Logik für Umspannwerk
        break;
    case MODULE_MASTER:
        // Logik für Master-Modul
        break;
    default:
        break;
    }
    //unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateSystemTime();
        updateDisplay();
        updateLED();
        printKnownPeers();
        last_update=millis();
    }
}

void SmartGrid::runTageszyklus() {
     switch (myModuleType)
    {
    case MODULE_WIND:
        // Logik für Windmodul
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        break;
    case MODULE_BATTERY:
        // Logik für Batteriespeicher
        break;
    case MODULE_HYDRO:
        // Logik für Wasserkraftmodul
        break;
    case MODULE_ELECTROLYZER:
        // Logik für Elektrolyseur
        break;
    case MODULE_HYDROGEN:
        // Logik für Wasserstoffmodul
        break;
    case MODULE_PUMP_STORAGE:
        // Logik für Pumpspeicherkraftwerk
        break;
    case MODULE_HOUSE:
        // Logik für Hausmodul
        break;  
    case MODULE_FACTORY:
        // Logik für Fabrikmodul
        break;
    case MODULE_CAR:
        // Logik für Automodul
        break;
    case MODULE_SUBSTATION:
        // Logik für Umspannwerk
        break;
    case MODULE_MASTER:
        // Logik für Master-Modul
        break;
    default:
        break;
    }
     uint32_t now = millis();
  if (modeMakeStep) {
    // nächsten Profil‑Punkt
    SmartGridData data;
    data = getSmartGridData();
    data.current_consumption = consProfile[cycleIndex];
    data.current_generation = genProfile[cycleIndex];
    setSmartGridData(data);

    // Ausgabe zu Debug
    Serial.printf("Step %u: cons=%.1f, gen=%.1f\n",
                  cycleIndex,
                  data.current_consumption,
                  data.current_generation);

    // Index und Zeit updaten
    cycleIndex = (cycleIndex + 1) % profileSize;
    modeMakeStep = false; // Schritt wurde gemacht
  }
    unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateSystemTime();
        updateDisplay();
        updateLED();
        last_update=millis();
    }
}

void SmartGrid::runNachtzyklus() {
     switch (myModuleType)
    {
    case MODULE_WIND:
        // Logik für Windmodul
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        break;
    case MODULE_BATTERY:
        // Logik für Batteriespeicher
        break;
    case MODULE_HYDRO:
        // Logik für Wasserkraftmodul
        break;
    case MODULE_ELECTROLYZER:
        // Logik für Elektrolyseur
        break;
    case MODULE_HYDROGEN:
        // Logik für Wasserstoffmodul
        break;
    case MODULE_PUMP_STORAGE:
        // Logik für Pumpspeicherkraftwerk
        break;
    case MODULE_HOUSE:
        // Logik für Hausmodul
        break;  
    case MODULE_FACTORY:
        // Logik für Fabrikmodul
        break;
    case MODULE_CAR:
        // Logik für Automodul
        break;
    case MODULE_SUBSTATION:
        // Logik für Umspannwerk
        break;
    case MODULE_MASTER:
        // Logik für Master-Modul
        break;
    default:
        break;
    }
    unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateDisplay();
        updateLED();
        last_update=millis();
    }
}

void SmartGrid::runTagNachtzyklus() {
     switch (myModuleType)
    {
    case MODULE_WIND:
        // Logik für Windmodul
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        break;
    case MODULE_BATTERY:
        // Logik für Batteriespeicher
        break;
    case MODULE_HYDRO:
        // Logik für Wasserkraftmodul
        break;
    case MODULE_ELECTROLYZER:
        // Logik für Elektrolyseur
        break;
    case MODULE_HYDROGEN:
        // Logik für Wasserstoffmodul
        break;
    case MODULE_PUMP_STORAGE:
        // Logik für Pumpspeicherkraftwerk
        break;
    case MODULE_HOUSE:
        // Logik für Hausmodul
        break;  
    case MODULE_FACTORY:
        // Logik für Fabrikmodul
        break;
    case MODULE_CAR:
        // Logik für Automodul
        break;
    case MODULE_SUBSTATION:
        // Logik für Umspannwerk
        break;
    case MODULE_MASTER:
        // Logik für Master-Modul
        break;
    default:
        break;
    }
    unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateDisplay();
        updateLED();
        last_update=millis();
    }
}

void SmartGrid::runPause() {
     switch (myModuleType)
    {
    case MODULE_WIND:
        // Logik für Windmodul
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        break;
    case MODULE_BATTERY:
        // Logik für Batteriespeicher
        break;
    case MODULE_HYDRO:
        // Logik für Wasserkraftmodul
        break;
    case MODULE_ELECTROLYZER:
        // Logik für Elektrolyseur
        break;
    case MODULE_HYDROGEN:
        // Logik für Wasserstoffmodul
        break;
    case MODULE_PUMP_STORAGE:
        // Logik für Pumpspeicherkraftwerk
        break;
    case MODULE_HOUSE:
        // Logik für Hausmodul
        break;  
    case MODULE_FACTORY:
        // Logik für Fabrikmodul
        break;
    case MODULE_CAR:
        // Logik für Automodul
        break;
    case MODULE_SUBSTATION:
        // Logik für Umspannwerk
        break;
    case MODULE_MASTER:
        // Logik für Master-Modul
        break;
    default:
        break;
    }
    unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateDisplay();
        updateLED();
        last_update=millis();
    }
}

void SmartGrid::sendRegistryRequest() {
    RegistryRequestMessage msg;
    msg.type = MSG_REGISTRY_REQUEST;
    WiFi.macAddress(msg.requesterMac);
    esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::tryRequestRegistry() {
    if (registryReceived) return; // Schon erhalten, nichts tun

    unsigned long now = millis();
    sendRegistryRequest();
    registryRequestAttempts++;
    lastRegistryRequestTime = now;
    Serial.println("RegistryRequest gesendet.");

}

void SmartGrid::begin() {
    // Reset relevanter Variablen
    registryReceived = false;
    registryRequestAttempts = 0;
    receivedRegistryRequests = 0;
    newPeerCount = 0;
    lastRegistryRequestTime = millis();
    last_update = millis();
    systemTimeStartMs = last_update;

    // PWM-Kanäle konfigurieren
    ledcSetup(MOTOR_PWM_CH_A, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    ledcAttachPin(MOTOR_IN1_PIN, MOTOR_PWM_CH_A);

    ledcSetup(MOTOR_PWM_CH_B, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    ledcAttachPin(MOTOR_IN2_PIN, MOTOR_PWM_CH_B);

    // Starte mit Motor aus
    ledcWrite(MOTOR_PWM_CH_A, 0);
    ledcWrite(MOTOR_PWM_CH_B, 0);

  // Beispiel‑Anker: zwei Verbrauchs‑Peaks, ein Erzeugungs‑Peak
  std::vector<uint16_t> ca = { 5, 12,  8, 20, 10 };
  std::vector<uint16_t> ga = { 0,  0, 25,  0,  0 };

  // Setze Tageszyklus: 96 Punkte (viertel‑Stunden), 24 h Dauer
  setDailyProfiles(ca, /*outPoints=*/20, /*duration=*/20000,1);
  setDailyProfiles(ga, /*outPoints=*/20, /*duration=*/20000,0);


    WiFi.macAddress(own_mac);
    Serial.print("Eigene MAC-Adresse: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", own_mac[i]);
        if (i < 5) Serial.print(":");
    }
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    }

    

    Serial.println("module_type: " + String(myModuleType));

    SmartGridData initialData;
    initialData.timestamp = 0;
    initialData.id = 0; // ID des Moduls
    initialData.module = myModuleType; // Setze Modultyp
    initialData.error = 0; // Kein Fehler
    initialData.current_consumption = 0.0f; // Aktueller Verbrauch
    initialData.current_generation = 0.0f; // Aktuelle Erzeugung
    initialData.current_storage = 0.0f; // Aktueller Speicherstand
    initialData.coordinates.x = 0; // X-Koordinate
    initialData.coordinates.y = 0; // Y-Koordinate
    setSmartGridData(initialData); // Setze initiale SmartGridData
    
    display.clearDisplay();

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

    // Sende Join-Message
    sendJoinMessage();

    Serial.println("SmartGrid Initialisierung gestartet. Warte auf Registry...");

    //delay(1000); // Kurze Pause, um sicherzustellen, dass das System bereit ist

}

void SmartGrid::runWaitForRegistry() {

    unsigned long now = millis();
    // Prüfe, ob Registry bereits empfangen wurde
    if(registryRequestAttempts < MAX_ATTEMPTS && now - lastRegistryRequestTime > INTERVAL_BETWEEN_REQUESTS) {
        // Sende RegistryRequest, wenn noch nicht zu oft versucht
        Serial.println("Sende RegistryRequest...");
        tryRequestRegistry();
    } else if (registryReceived) {
        Serial.println("Registry bereits empfangen.");
        setCurrentMode(MODE_AUTOMATIK);
    } else if (registryRequestAttempts >= MAX_ATTEMPTS) {
        Serial.println("Maximale Anzahl an RegistryRequests erreicht. Ich bin das erste Modul.");
        moduleRegistry.count = 1; // Setze auf 1, da wir das erste Modul sind
        moduleRegistry.modules[0].data.id = 0; // Setze ID des ersten Moduls
        moduleRegistry.modules[0].data.module = myModuleType; // Setze Modultyp des ersten Moduls
        SmartGridData data = getSmartGridData();
        memcpy(&moduleRegistry.modules[0].data, &data, sizeof(SmartGridData)); // Setze eigene SmartGridData

        memcpy(moduleRegistry.modules[0].mac, own_mac, 6); // Setze eigene MAC-Adresse

        setCurrentMode(MODE_AUTOMATIK);

    }
}

void SmartGrid::updateSystemTime() {
    // Berechne vergangene Sekunden seit Startzeitpunkt, robust gegen Überlauf
    unsigned long now = millis();
    uint32_t elapsed = (now - systemTimeStartMs) / 1000;
    setSystemTime(elapsed);
}

void SmartGrid::setSystemTime(uint32_t time){

    smartGridData.timestamp = time;
}


uint32_t SmartGrid::getSystemTime(){

    return smartGridData.timestamp;
}



// Generiert ein Profil der Länge `outLen` aus `nAnchors` Anker‑Werten.
// anchors[0…nAnchors-1] liegen gleichmäßig verteilt über den Zyklus;
// profile[0…outLen-1] wird linear dazwischen interpoliert.
void SmartGrid::generateInterpolatedProfile(const std::vector<uint16_t>& anchors,
                                 std::vector<uint16_t>& profile)
{
  size_t nAnch = anchors.size();
  size_t outLen = profile.size();
  if (nAnch < 2 || outLen == 0) return;

  for (size_t i = 0; i < outLen; i++) {
    float pos = float(i) * (nAnch - 1) / float(outLen - 1);
    size_t idx = size_t(floor(pos));
    float frac = pos - idx;

    if (idx + 1 < nAnch) {
      profile[i] = anchors[idx] * (1.0f - frac)
                 + anchors[idx+1] * frac;
    } else {
      profile[i] = anchors[nAnch - 1];
    }
  }
}


void SmartGrid::setDailyProfiles(const std::vector<uint16_t>& anchors,
                      size_t outPoints,
                      uint32_t durationMs,
                      bool isConsumption)
{
    // 1) Copy anchor definitions
    if (isConsumption) {
        consAnchors = anchors;
        consProfile.resize(outPoints);
        // 4) Erzeuge Verbrauchsprofil
        generateInterpolatedProfile(consAnchors, consProfile);

        Serial.print("Verbrauchsprofil: ");
        for (const auto& val : consProfile) {
            Serial.print(val, 1);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        genAnchors = anchors;
        genProfile.resize(outPoints);
        // 4) Erzeuge Erzeugungsprofil
        generateInterpolatedProfile(genAnchors, genProfile);

        Serial.print("Erzeugungsprofil: ");
        for (const auto& val : genProfile) {
            Serial.print(val, 1);
            Serial.print(" ");
        }
        Serial.println();
    }

    // 3) Setze Zyklusdauer
    cycleDurationMs = durationMs;
    cycleIndex      = 0;
    lastStepMs      = millis();
}
