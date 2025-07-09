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
    ModuleRegistryMessage msg;
    msg.type = MSG_MODULE_REGISTRY;
    msg.registry = moduleRegistry;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::sendControlCommand(const uint8_t* receiverMac, const ControlCommand& command) {
    ControlCommandMessage msg;
    msg.type = MSG_CONTROL_COMMAND;
    msg.command = command;
    esp_now_send(receiverMac, (uint8_t*)&msg, sizeof(msg));
}

void SmartGrid::handleReceivedModuleRegistry(const uint8_t* incomingData) {
    registryReceived = true;
    ModuleRegistryMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));
    //const ModuleRegistry* receivedRegistry = (const ModuleRegistry*)incomingData;
    for (int i = 0; i < msg.registry.count && i < MAX_MODULES; ++i) {
        addPeerIfNew(msg.registry.modules[i].mac, msg.registry.modules[i].type);
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

void SmartGrid::onReceiveCallback(const uint8_t *mac, const uint8_t *incomingData, int len) {
    Serial.print("Empfange Daten von MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    if (len < sizeof(MessageHeader)) {
        Serial.println("Nachricht zu kurz!");
        return;
    }

    MessageType type = static_cast<MessageType>(incomingData[0]);

    switch (type) {
        case MSG_SMARTGRID_DATA:
            handleReceivedSmartGridDataRaw(incomingData, len, doc);
            break;
        case MSG_JOIN: {
            JoinMessageWithType joinMsg;
            memcpy(&joinMsg, incomingData, sizeof(JoinMessageWithType));
            handleJoinMessage(joinMsg);
            printKnownPeers();
            break;
        }
        case MSG_MODULE_REGISTRY:
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
    for (int i = 0; i < moduleRegistry.count; i++) {
        if (memcmp(moduleRegistry.modules[i].mac, macAddress, 6) == 0) {
            return false;
        }
    }
    if (moduleRegistry.count >= MAX_MODULES) return false;

    memcpy(moduleRegistry.modules[moduleRegistry.count].mac, macAddress, 6);
    moduleRegistry.modules[moduleRegistry.count].type = type;
    moduleRegistry.count++;

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(macAddress)) {
        esp_now_add_peer(&peerInfo);
    }

    Serial.print("New peer MAC: ");
    for (int i = 0; i < 6; ++i) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    // Wenn dies der erste neue Peer ist, Registry senden
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
}

const ModuleRegistry& SmartGrid::getModuleRegistry() const {
    return moduleRegistry;
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

void SmartGrid::updateDisplay() {
    Serial.println("Aktualisiere Display...");

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 10);
    // Display static text
    display.println("Hello world!");
    display.display(); 

    
}

void SmartGrid::updateLED() {
    Serial.println("Aktualisiere LED...");


    
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
    updateDisplay();
    updateLED();
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

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    }
    
    display.clearDisplay();

    // Sende Join-Message
    sendJoinMessage();

    Serial.println("SmartGrid Initialisierung gestartet. Warte auf Registry...");

    delay(1000); // Kurze Pause, um sicherzustellen, dass das System bereit ist

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






