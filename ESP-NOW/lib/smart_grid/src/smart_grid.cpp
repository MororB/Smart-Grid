#include "smart_grid.h"

SmartGrid::SmartGrid(ModuleType myType)
    : myModuleType(myType)
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
    const ModuleRegistry* receivedRegistry = (const ModuleRegistry*)incomingData;
    for (int i = 0; i < receivedRegistry->count && i < MAX_MODULES; ++i) {
        addPeerIfNew(receivedRegistry->modules[i].mac, receivedRegistry->modules[i].type);
    }
}

void SmartGrid::handleJoinMessage(const JoinMessage& joinMsg) {
    addPeerIfNew(joinMsg.mac, static_cast<ModuleType>(joinMsg.module_type));
}

void SmartGrid::handleControlCommand(const uint8_t* macAddress, ControlCommand command) {
    // Implementiere nach Bedarf
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
            JoinMessage joinMsg;
            memcpy(&joinMsg, incomingData, sizeof(JoinMessage));
            handleJoinMessage(joinMsg);
            printKnownPeers();
            break;
        }
        case MSG_MODULE_REGISTRY:
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

            int responderIndex = moduleRegistry.count - receivedRegistryRequests;
            Serial.print("Berechneter responderIndex: ");
            Serial.println(responderIndex);

            if (responderIndex >= 0 && responderIndex < moduleRegistry.count) {
                Serial.print("Vergleiche MAC von Modul ");
                Serial.print(responderIndex + 1);
                Serial.print(": ");
                for (int i = 0; i < 6; ++i) {
                    Serial.printf("%02X", moduleRegistry.modules[responderIndex].mac[i]);
                    if (i < 5) Serial.print(":");
                }
                Serial.println();

                if (memcmp(moduleRegistry.modules[responderIndex].mac, myMac, 6) == 0 &&
                    memcmp(req.requesterMac, myMac, 6) != 0) {
                    Serial.printf("Modul %d antwortet auf RegistryRequest (Request #%d)\n", responderIndex + 1, receivedRegistryRequests);
                    sendModuleRegistryToPeer(req.requesterMac);
                } else {
                    Serial.println("Dieses Modul ist nicht an der Reihe zu antworten oder Anfrage kommt von mir selbst.");
                }
            } else {
                Serial.println("responderIndex außerhalb des gültigen Bereichs!");
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

    newPeerCount++; // Zähler erhöhen

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(macAddress)) {
        esp_now_add_peer(&peerInfo);
    }

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

void SmartGrid::update() {
    switch (currentMode) {
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
    //int analogValue = analogRead(A0); // Beispiel: Wert einlesen
    //smartGridData.current_consumption = analogValue * 0.01f; // Beispiel: Wert umrechnen
    // ...weitere Logik...
    Serial.println("Interaktiver Modus läuft...");
}

void SmartGrid::runAutomatik() {
    // TODO: Automatik-Modus-Logik hier implementieren
    Serial.println("Automatik-Modus läuft...");
}

void SmartGrid::runTageszyklus() {
    // TODO: Tageszyklus-Modus-Logik hier implementieren
    Serial.println("Tageszyklus-Modus läuft...");
}

void SmartGrid::runNachtzyklus() {
    // TODO: Nachtzyklus-Modus-Logik hier implementieren
    Serial.println("Nachtzyklus-Modus läuft...");
}

void SmartGrid::runTagNachtzyklus() {
    // TODO: TagNachtZyklus-Modus-Logik hier implementieren
    Serial.println("TagNachtZyklus-Modus läuft...");
}

void SmartGrid::runPause() {
    // TODO: Pause-Modus-Logik hier implementieren
    Serial.println("Pause-Modus läuft...");
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

    // Sende Join-Message
    sendJoinMessage();

    Serial.println("SmartGrid Initialisierung gestartet. Warte auf Registry...");

    // Warte, bis Registry empfangen wurde oder Modul als erstes erkannt wird
    while (!registryReceived && registryRequestAttempts < 3) {
        tryRequestRegistry();
        delay(100); // Kurze Pause, damit die Schleife nicht zu schnell läuft
    }

    if (registryReceived) {
        Serial.println("Registry erfolgreich empfangen!");
    } else {
        Serial.println("Keine Registry erhalten. Ich bin das erste Modul im Netzwerk.");
        // Hier ggf. spezielle Initialisierung
    }
}






