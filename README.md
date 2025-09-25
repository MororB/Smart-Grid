# Modularer Smart-Grid-Demonstrator

Ein modularer, realitätsnaher Demonstrator für intelligente Stromnetze (Smart Grids), der Erzeugung, Verbrauch und Speicherung abbildet und über einfache, nachvollziehbare Steuerungslogiken koordiniert wird. Fokus: Didaktik, Anschaulichkeit und Erweiterbarkeit.

---

## Inhaltsverzeichnis
- [Motivation & Ziele](#motivation--ziele)
- [Systemüberblick](#systemüberblick)
- [Hardwarearchitektur](#hardwarearchitektur)
- [Kommunikation (ESP-NOW)](#kommunikation-esp-now)
- [Softwarearchitektur & Betriebsmodi](#softwarearchitektur--betriebsmodi)
- [Visualisierung & Bedienung](#visualisierung--bedienung)
- [3D-Druck: Grundkörper & Module](#3d-druck-grundkörper--module)
- [Schnellstart](#schnellstart)
- [Roadmap](#roadmap)
- [Lizenz & Autoren](#lizenz--autoren)

---

## Motivation & Ziele

- Lehr- und Demo-Charakter: Zentrale Smart-Grid-Prinzipien interaktiv vermitteln (Erzeugung, Verbrauch, Speicherung, Lastmanagement/DSM).  
- Modularität: Ein einheitlicher Grundkörper + aufsetzbare Modell-Module (Haus, PV, Wind, Fabrik, Speicher) ermöglichen flexible Szenarien.  
- Bewusste Vereinfachung: Energieströme werden simuliert (keine reale Netzsimulation, keine physikalischen Messungen), um Robustheit und Verständlichkeit zu erhöhen.  

---

## Systemüberblick

Jedes Modul besitzt:
- eine eigene Energieversorgung (18650 Li-Ion, USB-C-Lader/Booster 5 V),
- eine Recheneinheit (ESP32),
- Funkkommunikation über ESP-NOW (dezentral),
- LED-Ring (≥ 20 LEDs) für Energiefluss/SoC und OLED-Display für Status/Fehler.  

Die Module koppeln mechanisch via Magnete, elektrisch/energetisch sind sie autark.  
Ein Raspberry Pi mit Node-RED dient (optional) als zentrale Visualisierung/Bedienung.

---

## Hardwarearchitektur

- Mikrocontroller: ESP32 (ESP-NOW, gute Performance/Energieeffizienz/Kosten).  
- Energie: 18650-Zelle, USB-C Ladeelektronik + 5 V Step-Up; Akkuwechsel über bodenseitigen Deckel.  
- Anzeige/Bedienung: LED-Ring (Netto-Energiefluss bzw. SoC bei Speichern), OLED-Display (Leistungswerte, SoC, Fehlercodes).  
- Mechanik: Hexagonaler Grundkörper (3D-Druck), M2/M4-Gewindeeinsätze, Magnettaschen, separater Ein/Aus-Schalter, JST-Steckverbinder für modulare Aufsätze.  

---

## Kommunikation (ESP-NOW)

- Dezentrales Mesh-ähnliches Konzept ohne Access-Point; Broadcast/Unicast fähig.  
- Eigenschaften: bis 255 Byte Nutzlast pro Nachricht; sehr geringe Latenz, energieeffizient.  
- Netzbeitritt:  
  - Join-Message (Broadcast) mit kompaktem `SmartGridData` (~27 B).  
  - Registry-Abgleich: dezentrale Synchronisation, ID-Vergabe ab 1, Wiederverwendung freier IDs, Mehrfachantwort zur Robustheit.  

---

## Softwarearchitektur & Betriebsmodi

- Struktur: schlanke Main (`begin()`, `update()`), Logik in Bibliothek/Klasse `SmartGrid::SmartGrid(ModuleType myType)`.  
- Zustandsmaschinen: `enum`-basierte Rollen/Events/States.  
- Zeit/Sync: Master sendet Tick-Nachrichten, alle Module laufen synchron durch Lastprofile.  
- Betriebsmodi (Auszug):  
  - `MODE_AUTOMATIK` (regel-/prioritätsbasiert)  
  - `MODE_TAGESZYKLUS` / `MODE_NACHTZYKLUS` / `MODE_TAGNACHTZYKLUS`  
  - `MODE_INTERAKTIV`  
  - `MODE_PAUSE`, `MODE_LEAVE_NETWORK`, `MODE_SHUTDOWN`  
- Steuerkommandos (Master): `SET_MODE`, `REQUEST_STATUS/SET_STATUS`, `START_ALL_MODULES`, `SHUTDOWN_ALL_MODULES`, `LEAVE_NETWORK`, `SHUTDOWN_SINGLE_MODULE`.  

---

## Visualisierung & Bedienung

- LED-Ring:  
  - Netto-Fluss: Rot = Verbraucher > Erzeugung; Grün = Erzeugung > Verbrauch (Helligkeit ∝ Intensität).  
  - Speicher: Anzahl LEDs = SoC, Farbe = Lade-/Entladerichtung.  
- OLED-Display: Leistungswerte, SoC, Fehlercodes.  
- Zentrale Anzeige: Node-RED Dashboard auf Raspberry Pi (Systemzustand beobachten, Befehle senden).  

---

## 3D-Druck: Grundkörper & Module

- Grundkörper: hexagonales Gehäuse, Magnettaschen, M2/M4-Einsätze, USB-C-Lademodul, AN/AUS-Schalter, Service-Deckel.  
- Deckel: einheitlich, LEDs montiert, Magnete unten, zentrale Durchführung (Kabel), optionale Diffusoren für gleichmäßiges Licht.  
- Modell-Module: magnetisch aufsetzbar, elektrische Verbindung via JST; CAD in Fusion 360/Blender.  

**STL-Dateien** (werden ergänzt):  
- `stl/grundkoerper/` – Grundkörper, Deckel, Diffusoren  
- `stl/module/haus/` – Haus-Aufsatz  
- `stl/module/fabrik/` – Fabrik  
- `stl/module/pv/`, `stl/module/wind/`, `stl/module/speicher/`  

---

## Schnellstart

### Voraussetzungen
- ESP32-Toolchain (Arduino IDE oder PlatformIO)  
- Raspberry Pi (optional) mit Node-RED  
- 3D-gedruckte Teile + Elektronik (ESP32, 18650, USB-C Lade/Booster, LED-Ring, OLED, Magnete, JST usw.)  

### Build & Flash
1. Board ESP32 einrichten, Projekt öffnen.  
2. Modultyp im Code konfigurieren (`ModuleType`), WLAN deaktiviert lassen (ESP-NOW).  
3. Kompilieren & flashen; Seriell-Monitor zur Inbetriebnahme prüfen.  

### Inbetriebnahme
1. Module mit Akku bestücken, einschalten.  
2. Master starten (vergibt IDs, sendet Ticks/Modi).  
3. Optional: Node-RED-Flow importieren und Dashboard öffnen.  


---

## Roadmap

- [ ] STL-Dateien & Bilder verlinken  
- [ ] Node-RED Dashboard dokumentieren  
- [ ] Beispiel-Lastprofile als JSON/CSV beilegen  
- [ ] Unit-Tests für Nachrichten-Encoding/Decoding  
- [ ] Erweiterte Modi (Preis-/Tarif-Signale, Aggregation)  

---

## Lizenz & Autoren

- **Autoren:** Moritz Brüggemann, Nico Beyer  
- **Lizenz:** NOCH ANPASSEN

Projektstand: 25.09.2025
