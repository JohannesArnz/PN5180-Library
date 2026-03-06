# PN5180-Library

Arduino Uno / Arduino ESP-32 library for PN5180-NFC Module from NXP Semiconductors

![PN5180-NFC module](./doc/PN5180-NFC.png)
![PN5180 Schematics](./doc/FritzingLayout.jpg)

Release Notes:

Version 2.3.7 - 01.09.2025
	* ISO14443: Explicitly allow unknown manufacturer ID 0xFF, thanks to tom !

Version 2.3.6 - 03.06.2025
	*  Allow to change SPI frequency #16, thanks to @saidmoya12 !
	
Version 2.3.5 - 15.05.2025

	*  Less blocking delays when using other tasks #15, thanks to @joe91 !

Version 2.3.4 - 31.10.2024

	* Better debug #13, thanks to @mjmeans !
	* Fix warning "will be initialized after [-Wreorder]"
	* suppress [-Wunused-variable]
	
Version 2.3.3 - 27.10.2024

	* Bugfix start with default SPI
	
Version 2.3.2 - 27.10.2024

	* Allow to use custom spi pins #12, thanks to @mjmeans !
	* Create .gitattributes #10, thanks to @mjmeans 
	* replace errno, which is often a macro, thanks to @egnor

Version 2.3.1 - 01.10.2024

	* create a release with new version numbering

Version 2.3 - 29.05.2024

	* cppcheck: make some params const
	* transceiveCommand: restore state of SS in case of an error 
	
Version 2.2 - 13.01.2024

	* Add code to allow authentication with Mifare Classic cards, thanks to @golyalpha !
	* Unify and simplify command creation
	* Smaller memory footprint if reading UID only (save ~500 Bytes)
	* Refactor SPI transaction into transceiveCommand()

Version 2.1 - 01.04.2023

	* readMultipleBlock(), getInventoryMultiple() and getInventoryPoll() has been implemented, thanks to @laplacier !
	* multiple ISO-15693 tags can be read at once, thanks to @laplacier ! 
	* fixed some bugs and warnings with c++17
	
Version 2.0 - 17.10.2022

	* allow to instantiate with custom SPI class, thanks to  @mwick83!
	
Version 1.9 - 05.10.2021

	* avoid endless loop in reset()
	
Version 1.8 - 05.04.2021

	* Revert previous changes, SPI class was copied and caused problems
	* Speedup reading (shorter delays) in reset/transceive commands
	* better initialization for ISO-14443 cards, see https://www.nxp.com.cn/docs/en/application-note/AN12650.pdf
	
Version 1.7 - 27.03.2021

	* allow to setup with other SPIClass than default SPI, thanks to @tyllmoritz !
	
Version 1.6 - 31.01.2021

	* fix compiler warnings for platform.io
	* add LPCD (low power card detection) example for ESP-32 (with deep sleep tp save battery power)

	Version 1.5 - 07.12.2020

	* ISO-14443 protocol, basic support for Mifaire cards
	* Low power card detection
	* handle transceiveCommand timeout

Version 1.4 - 13.11.2019

	* ICODE SLIX2 specific commands, see https://www.nxp.com/docs/en/data-sheet/SL2S2602.pdf
	* Example usage, currently outcommented

Version 1.3 - 21.05.2019

	* Initialized Reset pin with HIGH
	* Made readBuffer static
	* Typo fixes
	* Data type corrections for length parameters

Version 1.2 - 28.01.2019

	* Cleared Option bit in PN5180ISO15693::readSingleBlock and ::writeSingleBlock

Version 1.1 - 26.10.2018

	* Cleanup, bug fixing, refactoring
	* Automatic check for Arduino vs. ESP-32 platform via compiler switches
	* Added open pull requests
	* Working on documentation

Version 1.0.x - 21.09.2018

	* Initial versions


# PN5180-Library (Fast Polling Fork)

Optimierter Fork der [PN5180-Library](https://github.com/tueddy/PN5180-Library) für **schnelles, zuverlässiges Karten-Polling** mit dem NXP PN5180 NFC-Reader.

Ursprünglich entwickelt für eine Spielbox (ähnlich Toniebox), bei der Wortkarten aufgelegt und entfernt werden müssen — mit minimaler Latenz und maximaler Zuverlässigkeit.

## Was ist anders gegenüber dem Original?

| Feature | Original | Dieser Fork |
|---|---|---|
| `readCardSerial()` Dauer | ~613 ms | **~9 ms** |
| Leer-Scan (keine Karte) | ~752 ms | **~3 ms** |
| Card-ON Erkennung | ~613 ms | **~9.4 ms** |
| Card-OFF Erkennung | ~1500 ms | **~15 ms** |
| Polls pro Sekunde | ~1.3 | **~100-300** |
| Karten-State-Machine | ❌ | ✅ `pollCard()` |
| 8-stellige Nummernextraktion | ❌ | ✅ `readCardNumber()` |
| WUPA für gehaltete Karten | ❌ | ✅ automatisch |
| Glitch-Toleranz | ❌ | ✅ konfigurierbar |
| RF-Recovery | ❌ | ✅ automatisch |

### Optimierungen im Detail

1. **`activateTypeA_fast()`** — Überspringt `loadRFConfig()` + `setRF_on()` bei jedem Poll (RF bleibt dauerhaft an)
2. **IRQ-basiertes Warten** — Statt fester `delay()` wird auf `RX_IRQ_STAT` gepollt mit konfigurierbarem Timeout
3. **SPI-Timing** — `delay(1)` in `transceiveCommand()` durch `delayMicroseconds(2)` ersetzt
4. **WUPA statt REQA** — Für Karten die nach fehlgeschlagener Anti-Collision im HALT-Status sind
5. **Retry-Logik** — Automatischer zweiter Versuch wenn ATQA empfangen aber Anti-Collision fehlschlägt

## Schnelleinstieg

### Installation (PlatformIO)

```ini
; platformio.ini
[env:myproject]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps = 
    https://github.com/JohannesArnz/PN5180-Library.git
```

Oder lokal in `lib/` klonen:
```bash
cd lib/
git clone https://github.com/JohannesArnz/PN5180-Library.git
```

### Minimales Beispiel

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>

// Pins anpassen!
#define SPI_MISO  37
#define SPI_MOSI  35
#define SPI_SCK   36
#define NFC_NSS   34
#define NFC_BUSY  38
#define NFC_RST   33

SPIClass hspi(FSPI);
PN5180ISO14443 nfc(NFC_NSS, NFC_BUSY, NFC_RST, hspi);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    hspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, NFC_NSS);
    nfc.begin();
    nfc.reset();
    
    // WICHTIG: commandTimeout reduzieren (Default = 500ms!)
    nfc.commandTimeout = 20;
    
    // Polling-Verhalten konfigurieren
    nfc.setRemovalThreshold(5);     // 5 Fehlversuche = Karte entfernt
    nfc.setRfRecoveryInterval(30);  // RF-Reset nach 30 Fehlversuchen
    
    // RF starten
    nfc.clearIRQStatus(0xFFFFFFFF);
    nfc.setupRF();
    
    Serial.println("Bereit! Karte auflegen...");
}

void loop() {
    uint8_t uid[10];
    uint8_t uidLen = 0;
    
    CardEvent event = nfc.pollCard(uid, &uidLen);
    
    switch (event) {
        case CARD_EVENT_NEW: {
            Serial.print("Neue Karte: ");
            for (int i = 0; i < uidLen; i++) {
                if (uid[i] < 0x10) Serial.print("0");
                Serial.print(uid[i], HEX);
                if (i < uidLen - 1) Serial.print(":");
            }
            Serial.println();
            
            // Optional: 8-stellige Nummer lesen
            char number[9];
            if (nfc.readCardNumber(number)) {
                Serial.printf("Nummer: %s\n", number);
            }
            break;
        }
        
        case CARD_EVENT_CHANGED:
            Serial.println("Andere Karte aufgelegt!");
            break;
            
        case CARD_EVENT_PRESENT:
            // Karte liegt noch da - nichts tun
            break;
            
        case CARD_EVENT_REMOVED:
            Serial.println("Karte entfernt!");
            break;
            
        case CARD_EVENT_NONE:
            // Keine Karte im Feld
            break;
    }
}
```

## API-Referenz

### `CardEvent pollCard(uint8_t *uid, uint8_t *uidLen)`

**Die zentrale Funktion.** Ein Aufruf pro Loop-Iteration. Verwaltet intern:
- REQA/WUPA-Umschaltung (REQA wenn keine Karte, WUPA wenn Karte bekannt)
- Anti-Collision mit automatischem Retry
- UID-Vergleich (gleiche Karte vs. neue Karte)
- Glitch-Toleranz (ATQA empfangen ≠ sofort "entfernt")
- Removal-Threshold (erst nach N aufeinanderfolgenden echten Leer-Scans)
- RF-Recovery (automatischer Reset nach konfigurierbarer Anzahl Fehlversuche)

**Rückgabewerte:**

| Event | Bedeutung | `uid` gefüllt? | Wann typischerweise |
|---|---|---|---|
| `CARD_EVENT_NONE` | Kein Kartenfeld | ❌ | Kein Tag in Reichweite |
| `CARD_EVENT_NEW` | Neue Karte erkannt | ✅ | Kind legt Karte auf |
| `CARD_EVENT_CHANGED` | Andere Karte (UID anders) | ✅ | Kind tauscht Karte |
| `CARD_EVENT_PRESENT` | Gleiche Karte noch da | ✅ | Karte liegt weiterhin |
| `CARD_EVENT_REMOVED` | Karte entfernt | ❌ | Kind nimmt Karte weg |

### `bool readCardNumber(char *number)`

Liest eine 8-stellige ASCII-Nummer von der Karte. Unterstützt:
- **NTAG213** — Nummer in Pages 6-7
- **NTAG215** — Nummer in Pages 6-7
- **NTAG216** — Nummer in Pages 6-7
- **MIFARE Ultralight** — Nummer in Pages 7-8

**Parameter:**
- `number` — Buffer mit mindestens 9 Bytes (8 Ziffern + Null-Terminator)

**Rückgabe:** `true` wenn 8-stellige Nummer gefunden, `false` sonst.

**Fallback-Suche:** Wenn die Nummer nicht an der erwarteten Position steht, werden alle gelesenen Bytes nach 8 aufeinanderfolgenden ASCII-Ziffern (`'0'-'9'`) durchsucht.

> **Wichtig:** `readCardNumber()` nur nach `CARD_EVENT_NEW` oder `CARD_EVENT_CHANGED` aufrufen — nicht bei jedem `CARD_EVENT_PRESENT`! Das Lesen dauert ~2-5ms extra.

### Konfiguration

```cpp
// BUSY-Wait-Timeout der SPI-Kommunikation (Default: 500ms)
// 20ms ist optimal für schnelles Polling
nfc.commandTimeout = 20;

// Wie viele echte Leer-Scans (kein ATQA) bis CARD_EVENT_REMOVED
// Höher = stabiler bei schwachem RF-Kontakt, aber langsamere Entfernung
nfc.setRemovalThreshold(5);   // Default: 5

// Nach wie vielen Fehlversuchen wird loadRFConfig()+setRF_on() aufgerufen
// Niedrig = häufigere Recovery, Hoch = weniger Overhead
nfc.setRfRecoveryInterval(30);  // Default: 30

// State-Machine zurücksetzen (z.B. nach manuellem nfc.reset())
nfc.resetCardState();
```

### Timing-Erwartungen

Bei `commandTimeout = 20` und `REMOVAL_THRESHOLD = 5`:

| Szenario | Dauer |
|---|---|
| Karte auflegen → `CARD_EVENT_NEW` | ~9-10 ms |
| Karte entfernen → `CARD_EVENT_REMOVED` | ~15-25 ms |
| Karte wechseln → `CARD_EVENT_CHANGED` | ~9-10 ms |
| `readCardNumber()` | ~2-5 ms |
| Poll ohne Karte (1 Zyklus) | ~3 ms |
| Poll mit Karte (1 Zyklus) | ~9 ms |
| Polls pro Sekunde (ohne Karte) | ~300 |
| Polls pro Sekunde (mit Karte) | ~100 |

## Spielbox-Integration: Typisches Pattern

```cpp
// Dein Spiel-Loop
void loop() {
    uint8_t uid[10];
    uint8_t uidLen = 0;
    
    CardEvent event = nfc.pollCard(uid, &uidLen);
    
    switch (event) {
        case CARD_EVENT_NEW:
        case CARD_EVENT_CHANGED: {
            char number[9];
            if (nfc.readCardNumber(number)) {
                // number = "12345678"
                handleCardPlaced(number);  // Deine Spiel-Logik
            }
            break;
        }
        
        case CARD_EVENT_REMOVED:
            handleCardRemoved();  // Deine Spiel-Logik
            break;
            
        case CARD_EVENT_PRESENT:
            // Karte liegt noch → Spiel läuft weiter
            break;
            
        case CARD_EVENT_NONE:
            // Warte auf Karte → z.B. "Bitte Karte auflegen" anzeigen
            break;
    }
    
    // Dein restlicher Code (Audio, Display, etc.)
    updateGame();
}
```

## Hardware

Getestet mit:
- **Board:** ESP32-S3 (N16R8)
- **NFC-Reader:** PN5180
- **Karten:** NTAG213, NTAG215, MIFARE Ultralight
- **SPI:** FSPI (MISO=37, MOSI=35, SCK=36)

### Pin-Belegung (Beispiel)

| PN5180 | ESP32-S3 | Funktion |
|---|---|---|
| NSS | GPIO 34 | SPI Chip-Select |
| BUSY | GPIO 38 | Busy-Signal |
| RST | GPIO 33 | Hardware-Reset |
| IRQ | GPIO 39 | Interrupt (optional) |
| MISO | GPIO 37 | SPI Data Out |
| MOSI | GPIO 35 | SPI Data In |
| SCK | GPIO 36 | SPI Clock |

## Notizen:

Die blauen Breakout Boards sind bei mir meist PN5180 C2 Chips.

## Credits

- **Original-Library:** [ATrappmann/PN5180-Library](https://github.com/ATrappmann/PN5180-Library)
- **Maintained Fork:** [tueddy/PN5180-Library](https://github.com/tueddy/PN5180-Library)
- **Fast-Polling Fork:** [JohannesArnz/PN5180-Library](https://github.com/JohannesArnz/PN5180-Library)

## Lizenz

Original-Lizenz beibehalten — siehe [LICENSE](LICENSE).
