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
    msg.join.is_joining = true;
    WiFi.macAddress(msg.join.mac);
    msg.join.module_type = myModuleType;
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
    for (int i = 0; i < msg.registry.count && i < MAX_MODULES; ++i) {
        addPeerIfNew(msg.registry.modules.mac, msg.registry.modules.type);
        computeNetworkStatus();
    }
}

void SmartGrid::handleJoinMessage(const JoinMessageWithType& joinMsg) {
    newPeerCount++; // Zähler erhöhen
    addPeerIfNew(joinMsg.join.mac, static_cast<ModuleType>(joinMsg.type));
}

void SmartGrid::handleControlCommand(const uint8_t* macAddress, ControlCommand command) {
    // Implementiere nach Bedarf
    Serial.print("Empfange ControlCommand");

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
        if (jsonToSmartGrid(doc, &command.statusOverride)) {
            setSmartGridData(command.statusOverride);
            dataChanged = true; // Markiere, dass sich die Daten geändert haben
            Serial.println("SmartGridData erfolgreich gesetzt");
        } else {
            Serial.println("Fehler beim Setzen von SmartGridData");
        }
        break;    
    
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
            computeNetworkStatus();
            found = true;
            newData = true; // Markiere, dass neue Daten empfangen wurden
            break;
        }
    }

    if (!found) {
        Serial.println("Empfangene Daten von unbekanntem Modul!");
    }

}

void SmartGrid::sendJsonStatusToPi_(const SmartGridData& d) {
    StaticJsonDocument<256> doc;
    doc["cmd"] = "status_response";

    // optional: kennzeichne Peer‑MAC, falls nötig
    // char macBuf[18];
    // formatMac(currentPeerMac, macBuf);
    // doc["mac"] = macBuf;

    JsonObject js = doc.createNestedObject("data");
    js["timestamp"]          = d.timestamp;
    js["id"]                 = d.id;
    js["module"]             = d.module;
    js["current_consumption"]= d.current_consumption;
    js["current_generation"] = d.current_generation;
    js["current_storage"]    = d.current_storage;
    JsonObject coords = js.createNestedObject("coordinates");
    coords["x"]             = d.coordinates.x;
    coords["y"]             = d.coordinates.y;
    js["error"]             = d.error;

    String out;
    serializeJson(doc, out);
    Serial2.println(out);   // an Pi über UART2
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
        case MSG_SMARTGRID_DATA:{
            //handleReceivedSmartGridDataRaw(incomingData, len, doc);
            Serial.println("Empfange SmartGridData...");
            //handleRecivedSmartGridData(mac, incomingData, len);
            SmartGridDataMessage data;
            memcpy(&data, incomingData, sizeof(SmartGridDataMessage));
            sendJsonStatusToPi_(data.data);
            break;
        }
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

            addPeerIfNew(req.requesterMac, MODULE_CAR);


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

bool SmartGrid::addPeerIfNew(const uint8_t* macAddress, ModuleType type) {
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
    ModuleState& m = moduleRegistry.modules[moduleRegistry.count++];
    memcpy(m.mac, macAddress, 6);
    m.type = type;
    // Modul-Daten auf 0 setzen (falls du Default-Werte möchtest)
    m.data = SmartGridData{ /* das leert alle Felder */ };

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
    Serial.println(type);

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
    data->module = json["module"].as<uint8_t>();
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
    for (int i = 0; i < moduleRegistry.count; ++i) {
        auto& s = moduleRegistry.modules[i];
        s.net = s.data.current_generation - s.data.current_consumption;
        maxAbs = max(maxAbs, fabs(s.net));
    }
    for (int i = 0; i < moduleRegistry.count; ++i) {
        auto& s = moduleRegistry.modules[i];
        s.brightness =  (uint8_t)((fabs(s.net) / maxAbs) * 255);
        Serial.print("Modul ");
        Serial.print(i);
        Serial.print(" - Netzstatus: ");
        Serial.print(s.net);
        Serial.print("  Helligkeit: ");
        Serial.print(s.brightness);
        Serial.print("  Farbe: ");

        s.color = (s.net >= 0) ? CRGB::Green : CRGB::Red;
        if(s.net >= 0) {
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
        newData = false; // Nach der Aktualisierung zurücksetzen
        changed = true;
    }

    if (dataChanged) {
        sendNewSmartGridData();
        dataChanged = false; // Nach dem Senden zurücksetzen
        changed = true; // Daten haben sich geändert
    }

    return changed; // Am Ende wird entschieden, ob sich etwas geändert hat
}

void SmartGrid::sendNewSmartGridData() {
    // Sende SmartGridData an alle bekannten Peers
    for (int i = 0; i < moduleRegistry.count; ++i) {
        sendSmartGridData(moduleRegistry.modules[i].mac);
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
    Serial.print("Eigenes Modul gefunden bei Index: ");
    Serial.println(ownIndex);
    if (ownIndex == -1) return; // Eigenes Modul nicht gefunden

    // Setze alle LEDs entsprechend
    for (int i = 0; i < NUM_LEDS; i++) {
        Serial.print("Setze LED ");
        Serial.print(i);
        Serial.print(" auf Farbe: ");
        Serial.print(moduleRegistry.modules[ownIndex].color.r);
        leds[i] = moduleRegistry.modules[ownIndex].color;
        Serial.print(" mit Helligkeit: ");
        Serial.println(moduleRegistry.modules[ownIndex].brightness);
        leds[i].nscale8_video(moduleRegistry.modules[ownIndex].brightness);
    }
    FastLED.show();
}

void SmartGrid::updateMotor() {
    Serial.println("Aktualisiere Motor...");
    motor_rpm = (motor_rpm + 100) % 1000; // Beispiel: RPM erhöhen und zurücksetzen bei 1000
    Serial.print("Motor RPM: ");
    Serial.println(motor_rpm);
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

// –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// Prozessiere eine JSON‑Zeile vom Pi und sende ControlCommands
// –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
void SmartGrid::processUartCommand_(const String &line) {
    StaticJsonDocument<256> doc;
    auto err = deserializeJson(doc, line);
    if (err) {
        Serial2.println(R"({"cmd":"error","why":"json_parse"})");
        return;
    }

    const char *cmd = doc["cmd"];
    ControlCommand cc{};
    uint8_t mac[6];

    if (strcmp(cmd, "set_mode") == 0) {
        // { "cmd":"set_mode", "mac":"AA:BB:CC:DD:EE:FF", "mode":5 }
        const char* macStr = doc["mac"];
        for (int i = 0; i < 6; ++i)
            mac[i] = strtoul(macStr + 3*i, nullptr, 16);
        cc.type = ControlCommandType::SET_MODE;
        cc.mode = static_cast<ModuleMode>(doc["mode"].as<int>());

        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"set_mode"})");
    }
    else if (strcmp(cmd, "request_status") == 0) {
        // { "cmd":"request_status", "mac":"AA:BB:CC:DD:EE:FF" }
        const char* macStr = doc["mac"];
        for (int i = 0; i < 6; ++i)
            mac[i] = strtoul(macStr + 3*i, nullptr, 16);
        cc.type = ControlCommandType::REQUEST_STATUS;

        masterRequest = true; // Setze Flag, dass Master den Status anfordert
        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"request_status"})");
    }
    else if (strcmp(cmd, "set_status") == 0) {
        // { "cmd":"set_status", "data":{…} }
        const char* macStr = doc["mac"];
        for (int i = 0; i < 6; ++i)
            mac[i] = strtoul(macStr + 3*i, nullptr, 16);
        SmartGridData d;
        d.timestamp = doc["timestamp"].as<uint32_t>();
        d.id = doc["id"].as<uint8_t>();
        d.module = doc["module"].as<uint8_t>();
        d.current_consumption = doc["consumption"].as<float>();
        d.current_generation = doc["generation"].as<float>();
        d.current_storage = doc["storage"].as<float>();
        d.coordinates.x = doc["x"].as<uint8_t>();
        d.coordinates.y = doc["y"].as<uint8_t>();
        d.error = doc["error"].as<uint8_t>();
            //setSmartGridData(d);
            Serial.println("Setze SmartGridData...");
            Serial.print("ID: ");
            Serial.println(d.id);
            Serial.print("Module: ");
            Serial.println(d.module);
            Serial.print("Current Consumption: ");
            Serial.println(d.current_consumption);
            Serial.print("Current Generation: ");
            Serial.println(d.current_generation);
            Serial.print("Current Storage: ");
            Serial.println(d.current_storage);
            Serial.print("Coordinates: (");
            Serial.print(d.coordinates.x);
            Serial.print(", ");
            Serial.print(d.coordinates.y);
            Serial.println(")");
            Serial.print("Error: ");
            Serial.println(d.error);
            cc.type = ControlCommandType::SET_STATUS;
            cc.statusOverride = d;
            // Beispiel: broadcast an alle Master-Peers
            sendControlCommand(mac, cc);
            Serial2.println(R"({"cmd":"ack","on":"set_status"})");
    }
    else {
        Serial2.println(R"({"cmd":"error","why":"unknown_cmd"})");
    }
}


void SmartGrid::updateMaster() {

// 2) UART2 auf eingehende JSON‑Zeilen prüfen
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            Serial.println("Verarbeite UART-Befehl: " + uartBuf_);
            processUartCommand_(uartBuf_);
            uartBuf_.clear();
        } else {
            uartBuf_.concat(c);
        }
    }



}

// Beispiel für eine modusspezifische Funktion
void SmartGrid::runInteraktiv() {

    switch (myModuleType)
    {
    case MODULE_WIND:
        // Logik für Windmodul
        updateMotor();
        break;
    case MODULE_SOLAR:
        // Logik für Solarmodul
        readSolarcell();
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

    updateDisplay();
    updateLED();
}

void SmartGrid::runAutomatik() {
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
    if(checkForChanges()) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateDisplay();
        updateLED();
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
    const uint8_t MAX_ATTEMPTS = 3;
    const unsigned long INTERVAL = 2000; // alle 2 Sekunden

    if (registryRequestAttempts < MAX_ATTEMPTS && now - lastRegistryRequestTime > INTERVAL) {
        sendRegistryRequest();
        registryRequestAttempts++;
        lastRegistryRequestTime = now;
        Serial.println("RegistryRequest gesendet.");
    }

    if (!registryReceived && registryRequestAttempts >= MAX_ATTEMPTS) {
        Serial.println("Ich bin das erste Modul im Netzwerk.");
        // Hier ggf. spezielle Initialisierung
    }
}

void SmartGrid::begin() {
    // Reset relevanter Variablen
    registryReceived = false;
    registryRequestAttempts = 0;
    receivedRegistryRequests = 0;
    newPeerCount = 0;
    lastRegistryRequestTime = millis();

    WiFi.macAddress(own_mac);
    Serial.print("Eigene MAC-Adresse: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", own_mac[i]);
        if (i < 5) Serial.print(":");
    }
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    }
    
    display.clearDisplay();

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

    // Sende Join-Message
    sendJoinMessage();

    Serial.println("SmartGrid Initialisierung gestartet. Warte auf Registry...");

    //delay(1000); // Kurze Pause, um sicherzustellen, dass das System bereit ist

}

void SmartGrid::runWaitForRegistry() {
    tryRequestRegistry();
    if (registryReceived) {
        Serial.println("Registry erfolgreich empfangen!");
        setCurrentMode(MODE_AUTOMATIK); // oder dein gewünschter Startmodus
    } else if (registryRequestAttempts >= 3) {
        Serial.println("Keine Registry erhalten. Ich bin das erste Modul im Netzwerk.");
        setCurrentMode(MODE_AUTOMATIK); // oder dein gewünschter Startmodus
    }
}






