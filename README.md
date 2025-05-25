# Smart-Grid

## Übersicht
Es gibt einen Master im System. Der Master empfängt und verarbeitet Status- und Join-Nachrichten von Modulen, verwaltet die Teilnehmerliste und ermöglicht über die serielle Schnittstelle das gezielte Steuern und Abfragen einzelner Module im Smart-Grid-Netzwerk.

Es gibt mehrere Clients welche durch die einzelnen Module repräsentiert werden. Das Programm agiert als ESP-NOW-Client, der sich per Join-Nachricht im Smart-Grid anmeldet, andere Module erkennt, deren Informationen verwaltet und regelmäßig Sensordaten vorbereitet, um sie im Netzwerk zu versenden.


## Smart Grid Bibliothek – Funktionsübersicht

### Initialisierung & Kommunikation

- **`initEspNow`**  
  Initialisiert ESP-NOW und richtet das Gerät als Peer ein. Optional wird die eigene MAC-Adresse ausgegeben.

- **`sendEspNowMessage`**  
  Sendet beliebige Daten an eine bestimmte MAC-Adresse via ESP-NOW.

- **`sendEspNowBroadcast`**  
  Sendet Daten an alle Peers (Broadcast).

- **`sendJoinMessage`**  
  Sendet eine Join-Nachricht, um sich im Netzwerk anzumelden.

### Nachrichtenverarbeitung

- **`handleReceivedSmartGridDataRaw`**  
  Wandelt empfangene Rohdaten in JSON um und gibt sie aus.

- **`jsonToSmartGrid`**  
  Konvertiert ein JSON-Objekt in eine `SmartGridData`-Struktur.

- **`smartGridToJson`**  
  Konvertiert eine `SmartGridData`-Struktur in ein JSON-Objekt.

- **`handleJoinMessage`**  
  Verarbeitet eine empfangene Join-Nachricht und fügt neue Module hinzu.

### Registry & Peerverwaltung

- **`addPeerIfNew`**  
  Fügt ein neues Modul/Peer zur Registry hinzu, falls noch nicht vorhanden.

- **`sendMacListToNewPeer`**  
  Sendet die aktuelle Liste bekannter Module an einen neuen Peer.

- **`waitForPeerList`**  
  Verarbeitet eine empfangene Liste bekannter Module.

### Steuerung & Status

- **`sendControlCommand`**  
  Sendet einen Steuerbefehl (`ControlCommand`) an ein bestimmtes Modul.

- **`handleControlCommand`**  
  Verarbeitet empfangene Steuerbefehle (z. B. Moduswechsel, Statusanfrage).

### Hilfsfunktionen

- **`printMacAddress`**  
  Gibt die eigene MAC-Adresse formatiert aus.

- **`printKnownPeers`**  
  Gibt alle bekannten Module/Peers mit Typ und MAC-Adresse aus.

- **`parseMac`**  
  Wandelt einen MAC-String (z. B. `"AA:BB:CC:DD:EE:FF"`) in ein Byte-Array um.

---

Jede Funktion ist im jeweiligen Quellcode mit Kommentaren dokumentiert.  
Weitere Details findest du in den jeweiligen Dateien im `src`-Verzeichnis.