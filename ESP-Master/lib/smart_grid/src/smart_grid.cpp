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

// Hilfsfunktion: Index eines MAC-Eintrags finden, -1 wenn nicht vorhanden
int SmartGrid::indexOfMac(const uint8_t mac[6]) {
    for (int i = 0; i < moduleRegistry.count; ++i) {
        if (memcmp(moduleRegistry.modules[i].mac, mac, 6) == 0) return i;
    }
    return -1;
}

void SmartGrid::print_single_registry(int reg_num) {
    if (reg_num < 0 || reg_num >= moduleRegistry.count) {
        Serial.println("print_single_registry: Index out of bounds");
        return;
    }
    const ModuleState& m = moduleRegistry.modules[reg_num];

    Serial.print("Registry["); Serial.print(reg_num); Serial.println("]");
    Serial.print("  MAC: ");
    for (int j = 0; j < 6; ++j) {
        Serial.printf("%02X", m.mac[j]);
        if (j < 5) Serial.print(":");
    }
    Serial.println();
    Serial.print("  ID: ");     Serial.println(m.data.id);
    Serial.print("  Module: "); Serial.println(m.data.module);
    Serial.print("  Cons: ");   Serial.println(m.data.current_consumption, 1);
    Serial.print("  Gen : ");   Serial.println(m.data.current_generation, 1);
    Serial.print("  Stor: ");   Serial.println(m.data.current_storage, 1);
    Serial.print("  Err : ");   Serial.println(m.data.error);
}

void SmartGrid::delet_single_registry(int reg_num){

    uint8_t macToRemove[6];
    memcpy(macToRemove, moduleRegistry.modules[reg_num].mac, 6);

    if (esp_now_is_peer_exist(macToRemove)) {
        esp_now_del_peer(macToRemove);
    }

    moduleRegistry.count = (moduleRegistry.count > 0) ? moduleRegistry.count - 1 : 0;
    ModuleState tempModules;
    tempModules.data.id = 0;;
    moduleRegistry.modules[reg_num] = tempModules;
}

void SmartGrid::leave_network(){

    sendLeaveNetworkMessage();

    esp_now_deinit();
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void SmartGrid::sendLeaveNetworkMessage(){
    Serial.println("Sende Leave-Network-Message...");
    LeaveNetworkMessage msg;
    msg.type = MSG_LEAVE_NETWORK;
    memcpy(msg.mac, own_mac, sizeof(msg.mac));
    esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
    delay(500); // Kurze Verzögerung, um sicherzustellen, dass die Nachricht gesendet wird
    Serial.println("Verlasse Netzwerk und deaktiviere ESP-NOW.");
}

void SmartGrid::handleLeaveNetworkMessage(const uint8_t* macAddress) {
    Serial.print("Empfange Leave-Network-Message von MAC: ");
    for (int j = 0; j < 6; ++j) {
        Serial.printf("%02X", macAddress[j]);
        if (j < 5) Serial.print(":");
    }
    Serial.println();

    int idx = indexOfMac(macAddress);
    if (idx >= 0) {
        delet_single_registry(idx);
#if DEBUG_FULL
        Serial.println("Peer aus Registry entfernt.");
#endif
    } else {
#if DEBUG_FULL
        Serial.println("Peer nicht in Registry gefunden.");
#endif
    }
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

void SmartGrid::sendControlCommandStep(const uint8_t* receiverMac, const ControlCommandStep& step) {
    ControlCommandStepMessage msg;
    msg.type = MSG_CONTROL_COMMAND_STEP;
    msg.step = step;
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

void SmartGrid::sendJsonStatusToPi_(const SmartGridData& d, const uint8_t *mac) {
    StaticJsonDocument<256> doc;
    doc["cmd"] = "status_response";

    // optional: kennzeichne Peer‑MAC, falls nötig
    // char macBuf[18];
    // formatMac(currentPeerMac, macBuf);
    // doc["mac"] = macBuf;

    JsonObject js = doc.createNestedObject("data");
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    js["mac"] = macStr;
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

    // Serielle Ausgabe für jedes JSON-Element
    Serial.println("JSON status_response:");
    Serial.print("cmd: "); Serial.println(doc["cmd"].as<const char*>());
    Serial.print("mac: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
    Serial.print("timestamp: "); Serial.println(d.timestamp);
    Serial.print("id: "); Serial.println(d.id);
    Serial.print("module: "); Serial.println(d.module);
    Serial.print("current_consumption: "); Serial.println(d.current_consumption);
    Serial.print("current_generation: "); Serial.println(d.current_generation);
    Serial.print("current_storage: "); Serial.println(d.current_storage);
    Serial.print("coordinates.x: "); Serial.println(d.coordinates.x);
    Serial.print("coordinates.y: "); Serial.println(d.coordinates.y);
    Serial.print("error: "); Serial.println(d.error);

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
            sendJsonStatusToPi_(data.data, mac);
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
            #if DEBUG_FULL
                Serial.println("Empfange RegistryRequest...");
            #endif
            RegistryRequestMessage req;
            memcpy(&req, incomingData, sizeof(req));
            handleRegistryRequest(req);
            break;
        }
        case MSG_LEAVE_NETWORK: {
            #if DEBUG_FULL
                Serial.println("Empfange Leave-Network-Message...");
            #endif
            handleLeaveNetworkMessage(mac);
            printKnownPeers();
            break;
        }
        default:
            Serial.println("Unbekannter Nachrichtentyp!");
            break;
    }
}

void SmartGrid::handleRegistryRequest(RegistryRequestMessage req) {
            receivedRegistryRequests++; // Zähle mit
            lastRegistryRequestTime = millis();
            checkRegistry = true; // Setze Flag, dass Registry geprüft werden soll

            Serial.print("Anzahl empfangener RegistryRequests: ");
            Serial.println(receivedRegistryRequests);

            esp_now_peer_info_t peerInfo{};
            memcpy(peerInfo.peer_addr, req.requesterMac, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (!esp_now_is_peer_exist(req.requesterMac)) {
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
            Serial.print("Eigene Registry-Nummer: ");
            Serial.println(own_registry_number);
            Serial.print("Anzahl Module in Registry: ");
            Serial.println(moduleRegistry.count);
            Serial.print("Anzahl empfangener RegistryRequests: ");
            Serial.println(receivedRegistryRequests);

            if((own_registry_number - (moduleRegistry.count - receivedRegistryRequests - 1)) == 0){
                sendModuleRegistryToPeer(req.requesterMac);
                receivedRegistryRequests = 0; // Zähler zurücksetzen
                Serial.println("Sende Registry an anfragendes Modul");
            }
            else {
                Serial.println("Keine Registry gesendet, da ich nicht an der der Reihe bin");
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

uint8_t SmartGrid::nextFreeId(){
    bool used[MAX_MODULES + 1] = {false}; // Index 0 ungenutzt
    for (int i = 0; i < moduleRegistry.count; ++i) {
        uint8_t id = moduleRegistry.modules[i].data.id;
        if (id >= 1 && id <= MAX_MODULES) used[id] = true;
    }
    for (uint8_t id = 1; id <= MAX_MODULES; ++id) {
        if (!used[id]) return id;
    }
    return 0; // 0 = keine frei
}

bool SmartGrid::addPeerIfNew(const uint8_t* macAddress, SmartGridData data) {
#if DEBUG_FULL
    Serial.println("==== addPeerIfNew START ====");
    Serial.print("Prüfe, ob Modul schon bekannt ist: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
#endif

    // 1) Schon drin?
    for (int i = 0; i < moduleRegistry.count; ++i) {
#if DEBUG_FULL
        Serial.print("Vergleiche mit Registry-Eintrag ");
        Serial.print(i);
        Serial.print(": ");
        for (int j = 0; j < 6; ++j) {
            Serial.printf("%02X", moduleRegistry.modules[i].mac[j]);
            if (j < 5) Serial.print(":");
        }
        Serial.println();
#endif
        if (memcmp(moduleRegistry.modules[i].mac, macAddress, 6) == 0) {
#if DEBUG_FULL
            Serial.println("-> Modul existiert bereits, aktualisiere Daten.");
            Serial.print("Aktualisierte Daten: ID=");
            Serial.print(data.id);
            Serial.print(", Typ=");
            Serial.println(data.module);
            Serial.println("==== addPeerIfNew ENDE ====");
#endif
            uint8_t keepId = moduleRegistry.modules[i].data.id;  // ID sichern
            moduleRegistry.modules[i].data = data;
            moduleRegistry.modules[i].data.id = keepId; 
            return false;
        }
    }

    // 2) Platz prüfen
#if DEBUG_FULL
    Serial.print("Registry Count: ");
    Serial.println(moduleRegistry.count);
#endif
    if (moduleRegistry.count >= MAX_MODULES) {
#if DEBUG_FULL
        Serial.println("Maximale Peer-Anzahl erreicht!");
        Serial.println("==== addPeerIfNew ENDE ====");
#endif
        return false;
    }

    // 3) Neuen Eintrag anlegen
#if DEBUG_FULL
    Serial.println("Modul ist NEU, lege Eintrag an.");
    Serial.print("ADD TYPE: ");
    Serial.println(data.module);
#endif

    uint8_t newId = nextFreeId();
#if DEBUG_FULL
    Serial.print("Vergebe neue ID: ");
    Serial.println(newId);
#endif

    memcpy(moduleRegistry.modules[moduleRegistry.count].mac, macAddress, 6);
    moduleRegistry.modules[moduleRegistry.count].data = data;
    moduleRegistry.modules[moduleRegistry.count].data.id = newId; // Setze die ID für das neue Modul

#if DEBUG_FULL
    Serial.print("Eintrag im Registry-Array Index ");
    Serial.println(moduleRegistry.count);
    Serial.print("MAC: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
    Serial.print("Typ: ");
    Serial.println(data.module);
    Serial.print("ID: ");
    Serial.println(newId);
#endif

    moduleRegistry.count++; // Hochzählen nach erfolgreichem Eintrag

#if DEBUG_FULL
    Serial.print("Registry Count nach Hinzufügen: ");
    Serial.println(moduleRegistry.count);
#endif

    // 4) ESP-NOW Peer eintragen
#if DEBUG_FULL
    Serial.println("Prüfe, ob ESP-NOW Peer existiert...");
#endif
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(macAddress)) {
#if DEBUG_FULL
        Serial.println("Peer existiert NICHT, füge hinzu.");
#endif
        esp_now_add_peer(&peerInfo);
    } else {
#if DEBUG_FULL
        Serial.println("Peer existiert bereits.");
#endif
    }

    // 5) Debug-Output
#if DEBUG_FULL
    Serial.print("Neues Modul hinzugefügt: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(':');
    }
    Serial.print("  Typ=");
    Serial.println(data.module);
    Serial.print("  ID=");
    Serial.println(newId);
    Serial.println("==== addPeerIfNew ENDE ====");
#endif

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

    const char* macStr = doc["mac"];
        for (int i = 0; i < 6; ++i)
            mac[i] = strtoul(macStr + 3*i, nullptr, 16);
    
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_add_peer(&peerInfo);
    }

    if (strcmp(cmd, "set_mode") == 0) {
        // { "cmd":"set_mode", "mac":"AA:BB:CC:DD:EE:FF", "mode":5 }
        cc.type = ControlCommandType::SET_MODE;
        cc.mode = static_cast<ModuleMode>(doc["mode"].as<int>());

        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"set_mode"})");
    }
    else if (strcmp(cmd, "request_status") == 0) {
        // { "cmd":"request_status", "mac":"AA:BB:CC:DD:EE:FF" }
        cc.type = ControlCommandType::REQUEST_STATUS;

        masterRequest = true; // Setze Flag, dass Master den Status anfordert
        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"request_status"})");
    }
    else if (strcmp(cmd, "set_status") == 0) {
        // { "cmd":"set_status", "data":{…} }
        SmartGridData d;
        d.timestamp = doc["timestamp"].as<uint32_t>();
        d.id = doc["id"].as<uint8_t>();
        d.module =  static_cast<ModuleType>(doc["module"].as<uint8_t>());
        d.current_consumption = doc["consumption"].as<float>();
        d.current_generation = doc["generation"].as<float>();
        d.current_storage = doc["storage"].as<float>();
        d.coordinates.x = doc["x"].as<int8_t>();
        d.coordinates.y = doc["y"].as<int8_t>();
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
    else if(strcmp(cmd, "mode_next_step") == 0){

        Serial.println("Wechsel zum nächsten Modus...");
        cc.type = ControlCommandType::MODE_NEXT_STEP;
        sendControlCommand(BROADCAST_MAC, cc);

    }
    else if(strcmp(cmd,"start_cycle") == 0){

        if (cycleEnabled == false) {
            Serial.println("Starte Zyklus...");
            cycleEnabled = true;
            Serial2.println(R"({"cmd":"ack","on":"start_cycle"})");
        } else {
            cycleEnabled = false; // Zyklus beenden
            Serial2.println(R"({"cmd":"error","why":"cycle_already_started"})");
        }

    }
    else if(strcmp(cmd,"modify_mode") == 0){
        // { "cmd":"modify_mode", "mode":5 }
        cc.type = ControlCommandType::MODIFY_MODE;
        const char* macStr = doc["mac"];
        //cc.mode = 
        cc.profile.consOrGen = doc["type"].as<uint8_t>();
        cc.profile.nAnchorPoints = doc["n_anchor_points"].as<uint8_t>();
        cc.profile.cycleDuration = doc["cycle_duration"].as<uint16_t>();
        cycleDurationMs = cc.profile.cycleDuration * 1000; // in Millisekunden
        cc.profile.interpolationpoints = doc["interpolation_points"].as<uint8_t>();
        time_per_step = cycleDurationMs / cc.profile.interpolationpoints; // Zeit pro Schritt in Millisekunden
        profileSize = cc.profile.interpolationpoints;
        JsonArray arr = doc["anchor_points"].as<JsonArray>();
        size_t count = MAX_ANCHOR_POINTS;
        for (size_t i = 0; i < count; ++i) {
        // arr[i] kann float, int, etc. sein
            cc.profile.anchorPoints[i] = arr[i].as<float>();
        }

        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"modify_mode"})");
    }
    else if(strcmp(cmd,"leave_network") == 0){
        // { "cmd":"leave_network", "mac":"AA:BB:CC:DD:EE:FF" }
        cc.type = ControlCommandType::LEAVE_NETWORK;
        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"leave_network"})");
    }
    else if(strcmp(cmd,"leave_network_all") == 0){
        // { "cmd":"leave_network_all"}
        cc.type = ControlCommandType::LEAVE_NETWORK;
        sendControlCommand(BROADCAST_MAC, cc);
        Serial2.println(R"({"cmd":"ack","on":"leave_network_all"})");
    }
    else if(strcmp(cmd,"shutdown_single") == 0){
        // { "cmd":"shutdown_single", "mac":"AA:BB:CC:DD:EE:FF" }
        cc.type = ControlCommandType::SHUTDOWN_SINGLE_MODULE;
        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"shutdown_single"})");
    }
        else if(strcmp(cmd,"start_single") == 0){
        // { "cmd":"shutdown_single", "mac":"AA:BB:CC:DD:EE:FF" }
        cc.type = ControlCommandType::START_SINGLE_MODULE;
        sendControlCommand(mac, cc);
        Serial2.println(R"({"cmd":"ack","on":"start_single"})");
    }
        else if(strcmp(cmd,"shutdown_all") == 0){
        // { "cmd":"shutdown_all"}
        cc.type = ControlCommandType::SHUTDOWN_ALL_MODULES;
        sendControlCommand(BROADCAST_MAC, cc);
        Serial2.println(R"({"cmd":"ack","on":"shutdown_all"})");
    }
        else if(strcmp(cmd,"start_all") == 0){
        // { "cmd":"shutdown_all"}
        cc.type = ControlCommandType::START_ALL_MODULES;
        sendControlCommand(BROADCAST_MAC, cc);
        Serial2.println(R"({"cmd":"ack","on":"start_all"})");
    }
    else {
        Serial2.println(R"({"cmd":"error","why":"unknown_cmd"})");
    }
}


void SmartGrid::updateMaster() {

    updateSystemTime();

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

    if (cycleEnabled){
        
        uint32_t now = millis();
        if ((now - lastStepMs) > time_per_step) {

            ControlCommandStep cc{};

            cc.type = ControlCommandType::MODE_NEXT_STEP;
            cc.cycleIndex = cycleIndex;
            sendControlCommandStep(BROADCAST_MAC, cc);

            // Index und Zeit updaten
            cycleIndex = (cycleIndex + 1) % profileSize;
            //Serial.print("Aktueller Zyklusindex: ");
            //Serial.println(cycleIndex);
            lastStepMs = now; // Aktuelle Zeit als letzte Schrittzeit speichern
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
    lastRegistryRequestTime = millis();
    last_update = millis();
    systemTimeStartMs = last_update;


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

    moduleRegistry.count  = 0;
    memset(moduleRegistry.modules, 0, sizeof(moduleRegistry.modules));
    own_registry_number   = 255;  // „unbekannt“
    
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
        Serial.println("Keine Registry erhalten → ich bin erstes Modul.");
        SmartGridData me = getSmartGridData();
        addPeerIfNew(own_mac, me);      // ← vergibt ID=1 (kleinste frei)
        // own_registry_number setzen
        for (int i=0;i<moduleRegistry.count;++i){
            if (!memcmp(moduleRegistry.modules[i].mac, own_mac, 6)) {
                own_registry_number = i;
                break;
            }
        }
        setCurrentMode(MODE_AUTOMATIK);

    }
}







