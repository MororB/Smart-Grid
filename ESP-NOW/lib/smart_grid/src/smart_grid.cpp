#include "smart_grid.h"

SmartGrid::SmartGrid(ModuleType myType)
    : myModuleType(myType)
    ,display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) // -1 für Reset-Pin, da nicht verwendet
{
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

void SmartGrid::printSmartGridData(SmartGridData data){
    Serial.println("SmartGridData:");
    Serial.print("Timestamp: ");
    Serial.println(data.timestamp);
    Serial.print("ID: ");
    Serial.println(data.id);
    Serial.print("Module: ");
    Serial.println(data.module);
    Serial.print("Current Consumption: ");
    Serial.println(data.current_consumption);
    Serial.print("Current Generation: ");
    Serial.println(data.current_generation);
    Serial.print("Current Storage: ");
    Serial.println(data.current_storage);
    Serial.print("Coordinates: ");
    Serial.print(data.coordinates.x);
    Serial.print(", ");
    Serial.println(data.coordinates.y);
    Serial.print("Error: ");
    Serial.println(data.error);
}

void SmartGrid::printRegistry() {
    Serial.print("ModulRegistry.count:");
    Serial.println(moduleRegistry.count);
    Serial.println("Module Registry:");
    for (uint8_t i = 0; i < MAX_MODULES; ++i) {
        const ModuleState& module = moduleRegistry.modules[i];
        if (module.data.id != 0) {
            Serial.print("Module ");
            Serial.print(i);
            Serial.print(": MAC: ");
            for (int j = 0; j < 6; ++j) {
                Serial.printf("%02X", module.mac[j]);
                if (j < 5) Serial.print(":");
            }
            printSmartGridData(module.data);
        }
    }
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

void SmartGrid::printKnownPeers() const {
    Serial.println("Bekannte Peers:");
    for (uint8_t i = 0; i < moduleRegistry.count; ++i) {
        Serial.print("Peer ");
        Serial.print(i);
        Serial.print(": MAC: ");
        for (int j = 0; j < 6; ++j) {
            Serial.printf("%02X", moduleRegistry.modules[i].mac[j]);
            if (j < 5) Serial.print(":");
        }
        Serial.println();
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
        delay(300);
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
    //computeNetworkStatus();
    newData = true; // Markiere, dass neue Daten empfangen wurden
    startAnimation(LedAnimation::CONNECTED);
    //currentAnimation = LedAnimation::CONNECTED;
    //checkAnimation = true;
    // ID nur dann aus der *Nachricht* übernehmen, wenn die MAC deine ist
    if (memcmp(msg.registry.modules.mac, own_mac, 6) == 0) {
        SmartGridData me = getSmartGridData();
        if (me.id != msg.registry.modules.data.id) {
            me.id = msg.registry.modules.data.id;
            setSmartGridData(me);
            int idx = indexOfMac(own_mac);
            if (idx >= 0) {
                own_registry_number = idx;
            }

    #if DEBUG_FULL
                Serial.print("Eigene ID (aus eigener Nachricht) synchronisiert auf: ");
                Serial.println(me.id);
    #endif
            }
        }
}

void SmartGrid::handleJoinMessage(const JoinMessageWithType& joinMsg) {
    //newPeerCount++; // Zähler erhöhen
    addPeerIfNew(joinMsg.join.mac, joinMsg.join.data);
    startAnimation(LedAnimation::NEW_PEER);
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
        startAnimation(LedAnimation::MODE_CHANGE);
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
        startAnimation(LedAnimation::RECEIVED_NEW_DATA);
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
            setDailyProfiles(consAnchors, npoint, command.profile.cycleDuration * 1000,1);
        } 

        else {
            Serial.println("Erzeugungsprofil empfangen");
            genAnchors.assign(command.profile.anchorPoints, command.profile.anchorPoints + command.profile.nAnchorPoints);
                    Serial.print("Empfangenes Verbrauchsprofil: ");
        for (const auto& val : genAnchors) {
            Serial.print(val, 1);
            Serial.print(" ");
        }
            setDailyProfiles(genAnchors, npoint, command.profile.cycleDuration * 1000,0);
        }

        Serial.print("Anzahl Ankerpunkte: ");
        Serial.println(command.profile.nAnchorPoints);
        Serial.print("Anzahl Interpolationspunkte: ");
        Serial.println(command.profile.interpolationpoints);
        Serial.print("Zyklusdauer: ");
        Serial.println(command.profile.cycleDuration);
        startAnimation(LedAnimation::SET_NEW_TRAJECTORY);
        break;
    }
    case ControlCommandType::SHUTDOWN_SINGLE_MODULE: {
        Serial.println("Empfange Shutdown-Single-Module");
        lastMode   = currentMode;
        currentMode = ModuleMode::MODE_SHUTDOWN;

        // Konsum/Erzeugung auf 0, Storage behalten, Fehler setzen & announcen
        SmartGridData d = getSmartGridData();
        d.current_consumption = 0.0f;
        d.current_generation  = 0.0f;
        d.error               = ERR_SHUTDOWN;
        setSmartGridData(d);
        checkForChanges();
        startAnimation(LedAnimation::SHUTDOWN);
        break;
    }

    case ControlCommandType::START_SINGLE_MODULE: {
        Serial.println("Starte Single-Module");
        lastMode    = currentMode;
        currentMode = ModuleMode::MODE_AUTOMATIK;

        // Fehler zurücksetzen & announcen (Werte bleiben wie zuletzt)
        SmartGridData d = getSmartGridData();
        if (d.error != ERR_NONE) {
            d.error = ERR_NONE;
            setSmartGridData(d);
            checkForChanges();
        }
        startAnimation(LedAnimation::STARTUP);
        break;
    }

    case ControlCommandType::SHUTDOWN_ALL_MODULES: {
        Serial.println("Empfange Shutdown-All-Modules");
        lastMode    = currentMode;
        currentMode = ModuleMode::MODE_SHUTDOWN;

        SmartGridData d = getSmartGridData();
        d.current_consumption = 0.0f;
        d.current_generation  = 0.0f;
        d.error               = ERR_SHUTDOWN;
        setSmartGridData(d);
        checkForChanges();
        startAnimation(LedAnimation::SHUTDOWN);
        break;
    }

    case ControlCommandType::START_ALL_MODULES: {
        Serial.println("Starte All-Modules");
        lastMode    = currentMode;
        currentMode = ModuleMode::MODE_AUTOMATIK;

        SmartGridData d = getSmartGridData();
        if (d.error != ERR_NONE) {
            d.error = ERR_NONE;
            setSmartGridData(d);
            checkForChanges();
        }
        startAnimation(LedAnimation::STARTUP);
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

bool SmartGrid::checkForOwnModuleinRegistry() {    

    int ownIndex = -1;
    for (int j = 0; j < moduleRegistry.count && j < MAX_MODULES; ++j) {
        if (memcmp(moduleRegistry.modules[j].mac, own_mac, 6) == 0) {
            ownIndex = j;
            own_registry_number = j;

            return true; // Eigenes Modul gefunden, Abbruch
        }
    }
    if (ownIndex == -1) {
        Serial.println("Eigenes Modul nicht gefunden!");
        return false; // Eigenes Modul nicht gefunden, Abbruch
    }

    return false;
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

void SmartGrid::onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {

    #if DEBUG_FULL
        Serial.print("Empfange Daten von MAC: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", mac[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
    #endif


    if (len < sizeof(MessageType)) {
        Serial.println("Nachricht zu kurz!");
        return;
    }

    MessageType type = static_cast<MessageType>(incomingData[0]);

    switch (type) {
        case MSG_SMARTGRID_DATA:
            //handleReceivedSmartGridDataRaw(incomingData, len, doc);
            #if DEBUG_FULL
                Serial.println("Empfange SmartGridData...");
            #endif
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
            #if DEBUG_FULL
                Serial.println("Empfange Module Registry...");
            #endif
            handleReceivedModuleRegistry(incomingData);
            printKnownPeers();
            break;
        case MSG_CONTROL_COMMAND: {
            #if DEBUG_FULL
                Serial.println("Empfange ControlCommand...");
            #endif
            if (len == sizeof(ControlCommandMessage)) {
                ControlCommandMessage msg;
                memcpy(&msg, incomingData, sizeof(msg));
                handleControlCommand(mac, msg.command);
            }
            break;
        }
        case MessageType::MSG_CONTROL_COMMAND_STEP: {
        #if DEBUG_FULL
                    Serial.println("Empfange ControlCommandStep...");
        #endif
                    if (len < (int)sizeof(ControlCommandStepMessage)) {
        #if DEBUG_FULL
                        Serial.printf("CONTROL_COMMAND_STEP: Länge %d < erwartet %u\n", len, (unsigned)sizeof(ControlCommandStepMessage));
        #endif
                return;
            }
        Serial.println("Nächster Schritt im Modus");
        cycleIndex = ((ControlCommandStepMessage*)incomingData)->step.cycleIndex;
        modeMakeStep = true; // Setze Flag, dass der Modus einen Schritt machen soll
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

    if (memcmp(macAddress, own_mac, 6) == 0) {
        int idx = indexOfMac(macAddress);
        own_registry_number = idx;
        SmartGridData me = getSmartGridData();
        if (me.id != newId) {
            me.id = newId;
            setSmartGridData(me);   // spiegelt die vergebene ID ins eigene smartGridData
        }
}

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
    #if DEBUG_FULL
        printRegistry();
    #endif
    float maxAbs = 0;

    for (int i = 0; i < moduleRegistry.count; ++i) {
        auto& s = moduleRegistry.modules[i];
        s.net = s.data.current_generation - s.data.current_consumption;
        maxAbs = max(maxAbs, fabs(s.net));
    }

    Serial.print("Owm Registry Number: ");
    Serial.println(own_registry_number);

    // Berechne nur für das eigene Modul die Werte für brightness und color
    if (own_registry_number != 255) {
        auto& s = moduleRegistry.modules[own_registry_number];
        const float threshold = 10.0f;

        if (fabsf(s.net) <= threshold) {
            // Innerhalb Deadband → neutral
            brightness = 0;
            motorPwm   = 0;
            setSmokeDuty(0.0f);
            color = CRGB::Black; // LEDs aus oder neutral

            Serial.printf("[DEBUG] Modul-ID %u: NET=%.1f innerhalb Deadband (±%.1f) → Neutral\n",
                        s.data.id, s.net, threshold);
        } 
        else if (s.net > threshold) {
            // Überschuss
            float scale = (fabsf(s.net)) / (maxAbs);
            brightness = (uint8_t)(scale * 255);
            motorPwm   = brightness;
            setSmokeDuty(scale);
            color = CRGB::Green;

            Serial.printf("[DEBUG] Modul-ID %u: NET=%.1f Überschuss → Grün, Brightness=%u, Scale=%.2f\n",
                        s.data.id, s.net, brightness, scale);
        } 
        else { // s.net < -threshold
            // Unterdeckung
            float scale = (fabsf(s.net)) / (maxAbs);
            brightness = (uint8_t)(scale * 255);
            motorPwm   = brightness;
            setSmokeDuty(scale);
            color = CRGB::Red;

            Serial.printf("[DEBUG] Modul-ID %u: NET=%.1f Unterdeckung → Rot, Brightness=%u, Scale=%.2f\n",
                        s.data.id, s.net, brightness, scale);
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

     // NEU: Shutdown-Status anzeigen
    if (currentMode == MODE_SHUTDOWN) {
        display.setCursor(46, 48); // rechts daneben
        display.print(F(" SHUTDOWN"));
    }

    // 4) Tatsächlich anzeigen
    display.display();
}


void SmartGrid::updateLED() {

    // Setze alle LEDs entsprechend

    if (isStorage(myModuleType)) {
        renderStorageBarForOwnModule();
        return; // nichts anderes danach überschreiben lassen
    }

    fill_solid(leds, NUM_LEDS, color); // Alle LEDs ausschalten
    FastLED.setBrightness(brightness);
    // for (int i = 0; i < NUM_LEDS; i++) {
    //     leds[i] = color;
    //     leds[i].setBrightness(brightness);
    // }
    Serial.print("LED Color: ");
    Serial.print(color.r);
    Serial.print(", ");
    Serial.print(color.g);
    Serial.print(", ");
    Serial.print(color.b);
    Serial.print(" | Brightness: ");
    Serial.println(brightness);
    FastLED.show();
    //delay(500); 
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
    if(checkRegistry){
        if((millis() - lastRegistryRequestTime) > INTERVAL_BETWEEN_REQUESTS * 2) {
            receivedRegistryRequests = 0; // Zähler zurücksetzen
            checkRegistry = false; // Keine weiteren Anfragen senden
        }
    }

    updateLedAnimation();
    
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
        case MODE_LEAVE_NETWORK:
            // Optional: Logik zum Verlassen des Netzwerks
            break;
        case MODE_SHUTDOWN:
            runShutdown();
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
    case MODULE_SOLAR: {

        static float lastGen = -1.0f;  // merkt sich den letzten Wert
        const float thresholdW = 20.0f; // nur updaten, wenn Änderung > 5 W

        uint16_t adc_value = analogRead(SOLAR_PIN);

        // Spannung (0..1)
        float voltageNorm = (adc_value / 4095.0f); 

        const float maxSolarW = 200.0f; // anpassen
        float gen = voltageNorm * maxSolarW;

        // Nur updaten, wenn sich der Wert genug unterscheidet
        if (lastGen < 0 || fabs(gen - lastGen) > thresholdW) {
            SmartGridData d = getSmartGridData();
            d.current_generation = gen;
            setSmartGridData(d);

            Serial.print("Solar Gen aktualisiert: ");
            Serial.println(gen, 1);

            lastGen = gen;
        }
        break;
    }
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
        //Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        Serial.print(".");
        updateSystemTime();
        updateDisplay();
        updateLED();
        //printKnownPeers();
        Serial.println("--------------------------------------------------------------------------------------------------------------------------------------------");
        printRegistry();
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
    // Jitter: Wartezeit abhängig von eigener ID (z.B. 50ms pro ID)
    uint16_t jitterMs = 50 * smartGridData.id;
    Serial.print("Jitter-Delay für Modul-ID ");
    Serial.print(smartGridData.id);
    Serial.print(": ");
    Serial.print(jitterMs);
    Serial.println(" ms");
    delay(jitterMs);
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
    //cycleIndex = (cycleIndex + 1) % profileSize;
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
     uint32_t now = millis();
  if (now - last_update > time_per_step) {
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
    lastStepMs = now; // Aktuelle Zeit als letzte Schrittzeit speichern
  }
    unsigned long time = millis();
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateSystemTime();
        updateDisplay();
        updateLED();
        last_update = millis();
    }
}

void SmartGrid::runPause() {
    unsigned long time = millis();
    switch (smartGridData.module)       //myModuleType
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
        tickSmokeSimple();
        //Serial.println("F");
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
    if(checkForChanges() || (time-last_update)>5000) {
        // Wenn sich etwas geändert hat, aktualisiere die Anzeige und LEDs
        //sendNewSmartGridData();
        Serial.println("Daten haben sich geändert, aktualisiere Anzeige und LEDs...");
        updateDisplay();
        updateLED();
        last_update=millis();
    }
}

void SmartGrid::runLeaveNetwork() {
    static int pos = 0;
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate > 100) { // alle 100 ms weiter
        lastUpdate = millis();
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        leds[pos] = CRGB::Red;
        FastLED.show();

        pos = (pos + 1) % NUM_LEDS;
    }
}

void SmartGrid::runShutdown() {
    // --- 1) Aktorik aus ---
    ledcWrite(MOTOR_PWM_CH_A, 0);
    ledcWrite(MOTOR_PWM_CH_B, 0);

    // --- 3) Display
    updateDisplay();

    if(anim.active) {
        return;
    }

    // --- 4) LEDs: Herzschlag in rot (kurz-kurz-pause), non-blocking ---
    // Schema (in ms): ON(120), OFF(120), ON(120), OFF(900)
    // -> wiederholt sich
    static uint8_t  phase = 0;                 // 0..3
    static unsigned long t0 = 0;

    const unsigned long now = millis();
    const unsigned long durations[4] = {120, 120, 120, 900};  // ON, OFF, ON, PAUSE
    const bool          onState[4]    = { true, false, true, false };

    if (now - t0 >= durations[phase]) {
        phase = (phase + 1) & 0x03;           // 0..3
        t0 = now;
    }

    if (onState[phase]) {
        // kurze, knackige rote Blitze – volle Helligkeit
        FastLED.setBrightness(200);
        fill_solid(leds, NUM_LEDS, CRGB::Red);
    } else {
        FastLED.setBrightness(200);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
    FastLED.show();

#if DEBUG_FULL
    static unsigned long lastLog = 0;
    if (now - lastLog > 2000) {
        lastLog = now;
        Serial.println("System im SHUTDOWN (Heartbeat) ...");
    }
#endif
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

    pinMode(SOLAR_PIN, INPUT);
    pinMode(SMOKE_PIN, OUTPUT);
    digitalWrite(SMOKE_PIN, HIGH);
    smokeIsOn        = false;
    smokeDuty        = 0.0f;
    smokeCycleStart  = millis();


    

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

    startAnimation(LedAnimation::STARTUP);

    //currentAnimation = LedAnimation::STARTUP;
    //checkAnimation = true;

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
    time_per_step = durationMs / outPoints; // Dauer pro Schritt
    cycleDurationMs = durationMs;
    cycleIndex      = 0;
    lastStepMs      = millis();
}


void SmartGrid::startAnimation(LedAnimation t) {
  anim.type = t;
  anim.start_ms = millis();
  anim.step = 0;
  anim.active = true;

    if (t == LedAnimation::CONNECTED) {
    anim.step = 2; // Anzahl Pulse
  }
}

void SmartGrid::endAnimation() {
  FastLED.setBrightness(0);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  anim.active = false;
  anim.type   = LedAnimation::NONE;
}

void SmartGrid::updateLedAnimation() {
    if (!anim.active) return;
    uint32_t now = millis();

    Serial.print("LED Animation: ");
    Serial.println(anim.type);

  switch (anim.type) {

    case LedAnimation::STARTUP: {
        const uint32_t DUR = 2000;
        uint32_t el = now - anim.start_ms;
        if (el >= DUR) { endAnimation(); break; }
        uint8_t b = map(el, 0, DUR, 0, 255);
        fill_solid(leds, NUM_LEDS, CRGB::Green);
        FastLED.setBrightness(b);
        FastLED.show();
        break;
    }

    case LedAnimation::SHUTDOWN: {
        const uint32_t DUR = 2000;
        uint32_t el = now - anim.start_ms;
        if (el >= DUR) { endAnimation(); break; }
        uint8_t b = map(el, 0, DUR, 255, 0);
        fill_solid(leds, NUM_LEDS, CRGB::Red);
        FastLED.setBrightness(b);
        FastLED.show();
        break;
    }

    case LedAnimation::CONNECTED: {
        // Puls Blau (50 -> 200 -> 50) mit endlicher Anzahl Zyklen, danach AUS
        const uint32_t CYCLE = 1800;            // 0.9s hoch + 0.9s runter
        uint32_t el = now - anim.start_ms;

        if (el >= CYCLE) {                      // Zyklus fertig
            anim.start_ms = now;                  // nächsten Zyklus
            if (anim.step > 0) anim.step--;       // Pulse herunterzählen
            if (anim.step == 0) { endAnimation(); break; }
            el = 0;
        }

        uint8_t b = (el < 900)
            ? map(el, 0, 900, 50, 200)
            : map(el, 900, 1800, 200, 50);

        fill_solid(leds, NUM_LEDS, CRGB::Blue);
        FastLED.setBrightness(b);
        FastLED.show();
        break;
    }

    case LedAnimation::NEW_PEER: {
        // kurzes Weiß-Aufblitzen (0.6 s) und dann AUS
        const uint32_t DUR = 600;
        uint32_t el = now - anim.start_ms;
        if (el >= DUR) { endAnimation(); break; }
        uint8_t b = (el < 300) ? map(el, 0, 300, 0, 180)
                                : map(el, 300, 600, 180, 0);
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.setBrightness(b);
        FastLED.show();
        break;
    }

    case LedAnimation::MODE_CHANGE: {
      // Grün pulsieren (einmal hoch und runter, ca. 1.8 s)
      const uint32_t CYCLE = 1800;
      uint32_t el = now - anim.start_ms;
      if (el >= CYCLE) { endAnimation(); break; }

      uint8_t b = (el < 900)
        ? map(el, 0, 900, 0, 180)
        : map(el, 900, 1800, 180, 0);

      fill_solid(leds, NUM_LEDS, CRGB::Green);
      FastLED.setBrightness(b);
      FastLED.show();
      break;
    }

    case LedAnimation::RECEIVED_NEW_DATA: {
      // kurzes weißes Aufblitzen (0.4 s)
      const uint32_t DUR = 400;
      uint32_t el = now - anim.start_ms;
      if (el >= DUR) { endAnimation(); break; }

      uint8_t b = (el < 200) ? map(el, 0, 200, 0, 150)
                             : map(el, 200, 400, 150, 0);

      fill_solid(leds, NUM_LEDS, CRGB::White);
      FastLED.setBrightness(b);
      FastLED.show();
      break;
    }

    case LedAnimation::ERROR: {
      // rotes Blinken (step zählt Halbzyklen)
      const uint32_t INTERVAL = 250; // 0.25 s AN/AUS
      if (now - anim.start_ms >= INTERVAL) {
        anim.start_ms = now;
        anim.step--;
        if (anim.step <= 0) { endAnimation(); break; }

        bool on = anim.step % 2; // abwechselnd
        fill_solid(leds, NUM_LEDS, on ? CRGB::Red : CRGB::Black);
        FastLED.setBrightness(on ? 255 : 0);
        FastLED.show();
      }
      break;
    }

    case LedAnimation::SET_NEW_TRAJECTORY: {
        // kurzes blaues Aufleuchten (ca. 0.4 s)
        const uint32_t DUR = 400;
        uint32_t el = now - anim.start_ms;
        if (el >= DUR) { endAnimation(); break; }

        uint8_t b = (el < 200) ? map(el, 0, 200, 0, 180)   // hochfaden
                            : map(el, 200, 400, 180, 0); // runterfaden

        fill_solid(leds, NUM_LEDS, CRGB::Blue);
        FastLED.setBrightness(b);
        FastLED.show();
    break;
    }


    case LedAnimation::NONE: {
        // LEDs komplett ausschalten
        FastLED.setBrightness(0);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();

        anim.active = false;     // keine Animation läuft mehr
        anim.type   = LedAnimation::NONE;
        break;
    }

    default:
      endAnimation();
      break;
  }
}


void SmartGrid::setSmokeDuty(float duty01) {
    if (duty01 < 0) duty01 = 0;
    if (duty01 > 1) duty01 = 1;
    smokeDuty = duty01;
    Serial.print("Smoke Duty gesetzt auf ");
    Serial.println(smokeDuty, 3);
}

void SmartGrid::pulseHigh(unsigned long ms) {
    digitalWrite(smokePin, LOW);
    delay(ms);
    digitalWrite(smokePin, HIGH);
}

void SmartGrid::smokeTurnOn() {        // braucht 2 Pulse
    pulseHigh(SMOKE_PULSE_HIGH_MS);
    Serial.println("Rauch an");

    smokeIsOn = true;
}

void SmartGrid::smokeTurnOff() {       // braucht 1 Pulse
    Serial.println("Rauch aus");
    pulseHigh(SMOKE_PULSE_HIGH_MS);
    delay(SMOKE_PULSE_GAP_MS);
    pulseHigh(SMOKE_PULSE_HIGH_MS);
    smokeIsOn = false;
}


void SmartGrid::tickSmokeSimple() {
    // Fensterlängen aus Duty
    unsigned long onMs  = (unsigned long)(smokeDuty * SMOKE_CYCLE_MS);
    unsigned long offMs = SMOKE_CYCLE_MS - onMs;

    if (smokeDuty > 0.0f && onMs  < SMOKE_MIN_WIN_MS) onMs  = SMOKE_MIN_WIN_MS;
    if (smokeDuty < 1.0f && offMs < SMOKE_MIN_WIN_MS) offMs = SMOKE_MIN_WIN_MS;

    const unsigned long now     = millis();
    const unsigned long elapsed = now - smokeCycleStart;
    const unsigned long cycleMs = onMs + offMs;

    // Zyklus neu starten?
    if (elapsed >= cycleMs) {
        smokeCycleStart = now;
        // Start des neuen Zyklus: sicherstellen, dass der Zielzustand passt
        if (onMs > 0) {
            if (!smokeIsOn) smokeTurnOn();   // 2 Pulse
        } else {
            if (smokeIsOn)  smokeTurnOff();  // 1 Puls
        }
        return;
    }

    // Innerhalb des Zyklus:
    if (elapsed < onMs) {
        // ON-Fenster: sicherstellen, dass Modul an ist
        if (!smokeIsOn) smokeTurnOn();
    } else {
        // OFF-Fenster: sicherstellen, dass Modul aus ist
        if (smokeIsOn) smokeTurnOff();
    }

    // Pin bleibt sonst LOW
    digitalWrite(smokePin, HIGH);
}

void SmartGrid::renderStorageBarForOwnModule() {
    // nur für Speicher-Module; sonst raus
    if (!isStorage(myModuleType)) return;

    const float deadbandW = 10.0f; // ±10W ≙ Idle
    const StorageLimits lim = getStorageLimits(myModuleType);

    // aktuelle Werte des eigenen Moduls
    const SmartGridData d = getSmartGridData();

    // --- SOC → Anzahl LEDs ---
    float socPct = d.current_storage / lim.capacityWh;              // 0..1
    if (!isfinite(socPct)) socPct = 0.0f;
    socPct = fmaxf(0.0f, fminf(1.0f, socPct));

    int lit = (int)lroundf(socPct * NUM_LEDS);
    lit = fmax(0, fmin(NUM_LEDS, lit));
    if (socPct > 0.0f && lit == 0) lit = 1;                         // min. 1 LED, wenn >0%

    // --- Lade-/Entladerichtung & Intensität ---
    float netW = d.current_generation - d.current_consumption;      // >0 = entladen/abgeben, <0 = laden/aufnehmen

    CRGB col = CRGB::Blue;    // Idle
    uint8_t br = 60;          // Grundhelligkeit für Idle
    if (fabsf(netW) <= deadbandW) {
        // Idle
        col = CRGB::Blue;
        br  = 60;
    } else if (netW > deadbandW) {
        // ENTLADE-Intensität
        float scale = fminf(1.0f, (netW - deadbandW) / (lim.maxDischargeW + 1e-6f));
        col = CRGB::Red;
        br  = (uint8_t)fmaxf(30.0f, scale * 255.0f);
    } else { // netW < -deadbandW
        // LADE-Intensität
        float want = -netW; // positiv
        float scale = fminf(1.0f, (want - deadbandW) / (lim.maxChargeW + 1e-6f));
        col = CRGB::Green;
        br  = (uint8_t)fmaxf(30.0f, scale * 255.0f);
    }

    // --- LEDs setzen: erster 'lit' Bereich = Farbe/Helligkeit, Rest aus ---
        FastLED.setBrightness(br); // dynamisch
        fill_solid(leds, lit, col);
        fill_solid(leds+lit, NUM_LEDS - lit, CRGB::Black);
        FastLED.show();


    // Optionales Feedback, wenn voll/leer:
    // Voll → letzte LED kurz blitzen
    if (socPct >= 0.99f && NUM_LEDS > 0) {
        if (((millis()/400) % 2) == 0) {
            leds[NUM_LEDS-1] = CRGB::White;
            leds[NUM_LEDS-1].nscale8_video(180);
        }
    }
    // Leer → erste LED blitzen
    if (socPct <= 0.01f && NUM_LEDS > 0) {
        if (((millis()/400) % 2) == 0) {
            leds[0] = CRGB::White;
            leds[0].nscale8_video(180);
        }
    }

    FastLED.show();

#if DEBUG_FULL
    Serial.printf("[STORAGE] SOC=%.0f%%  netW=%.1fW  lit=%d  col=%s  br=%u\n",
                  socPct*100.0f, netW, lit,
                  (col==CRGB::Green?"Green":(col==CRGB::Red?"Red":"Blue")),
                  br);
#endif
}



