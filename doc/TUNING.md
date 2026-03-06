# PN5180 — Tuning & Konfigurationsempfehlungen

Zusammenfassung aller Erkenntnisse aus der Entwicklung und dem Debugging des PN5180 mit ESP32-S3, großen 35mm NTAG-Tags.

> Siehe auch [examples/PN5180-FastPolling/nfc_config_example.h](../examples/PN5180-FastPolling/nfc_config_example.h) für eine kommentierte Beispielkonfiguration.

---

## Architektur: Register-Overrides

`loadRFConfig()` (aufgerufen in `setupRF()` und bei jeder RF-Recovery) setzt **alle** TX/RX-Register auf Protokoll-Defaults zurück. Eigene Werte gehen dabei verloren.

**Lösung:** Die Library speichert Override-Werte intern und wendet sie nach jedem `setupRF()` automatisch erneut an:
- `setRxGain(value)` → schreibt nach jedem `setupRF()` in Register `RF_CONTROL_RX` (0x22)
- `setTxClk(value)` → schreibt nach jedem `setupRF()` in Register `RF_CONTROL_TX_CLK` (0x21)
- `setAgcRef(value)` → schreibt nach jedem `setupRF()` in Register `AGC_REF_CONFIG` (0x26)

Das heißt: **Einmal setzen, nie wieder kümmern** — egal wie oft Recovery oder Reset passiert.

---

## RX_GAIN — Empfangsempfindlichkeit (Register 0x22, Bits 1:0)

| Wert | Gain | Anwendung |
|------|------|-----------|
| 0 | 33 dB | Große Tags direkt auf dem Reader. Verhindert Übersteuerung. |
| 1 | 40 dB | Leicht erhöhte Empfindlichkeit |
| **2** | **50 dB** | **Empfohlen für 35mm NTAG-Tags** |
| 3 | 57 dB | Maximum — nur bei großem Abstand oder sehr kleinen Tags |

**Warum nicht 3 (57 dB)?**
Große Tags in direktem Kontakt mit der Antenne verursachen starke Rückkopplung. Bei 57 dB kann der Empfänger übersteuern → instabile Erkennung, AGC-Drift, sporadische Ausfälle.

**Warum nicht 0 (33 dB)?**
Funktioniert stabil, aber reduziert den nutzbaren Leseabstand erheblich. Tags müssen exakt aufliegen.

**Empfehlung:**
```cpp
nfc.setRxGain(2);  // 50 dB — bester Kompromiss
```

---

## TX_CLK — Sendemodulation (Register 0x21)

Steuert wie das RF-Feld erzeugt wird (Push/Pull-Modus, Invertierung, ALM, DPLL).

| Wert | Beschreibung | Anwendung |
|------|-------------|-----------|
| **0x74** | **Standard Push/Pull (volles RF-Feld)** | Default für ISO 14443A-106 |
| 0x7A | Halbes Feld (Low-Side Pull) | Für Tags die den Reader überkoppeln |
| **0x7C** | **Halbes Feld (High-Side Push)** | **Empfohlen für große 35mm Tags** |
| 0x82 | ISO 14443A-212 bis A-848 | Höhere Baudraten |
| 0x8E | ISO 14443B / FeliCa | Andere Protokolle |

**Problem mit großen Tags:**
Das volle RF-Feld (0x74) kann große Tags überkoppeln — die Antenne wird verstimmt, die Detuning-Frequenz verschiebt sich, und der PN5180 verliert den Lock.

**Lösung:**
Mit 0x7C wird nur die High-Side angesteuert → effektiv halbe Feldstärke → weniger Übersteuerung → stabilere Erkennung.

**Empfehlung:**
```cpp
nfc.setTxClk(0x7C);  // Halbes Feld für große Tags
```
Für Standard-Tags (Kreditkartengröße oder kleiner) bei 0x74 belassen.

---

## AGC — Automatische Verstärkungsregelung (Register 0x26)

Der PN5180 hat einen automatischen AGC (Automatic Gain Control), der die Empfangsempfindlichkeit dynamisch anpasst.

**Erkenntnis: Fixed AGC funktioniert NICHT zuverlässig.**

Die PN5180-Firmware überschreibt den Wert in AGC_REF_CONFIG sofort nach dem Schreiben. Ein Write von 0xA0 ergibt beim nächsten Readback z.B. 0x2B. Der AGC wird intern vom DPC (Dynamic Power Control) gesteuert und lässt sich nicht von außen fixieren.

**Beobachtetes Verhalten:**
- AGC-Wert springt beim Auflegen einer Karte (typisch 0x6F→0x8D→0x5E)
- Stabilisiert sich nach 1-2 Sekunden
- Drift allein verursacht **keine** Kartenausfälle (das war unsere falsche Annahme — siehe "SPI-Blocking" unten)

**Empfehlung:** AGC-Override nicht verwenden — PN5180 ignoriert es.

---

## RF Recovery & Cooldown

**RF Recovery** (`setRfRecoveryInterval(n)`): Nach N aufeinanderfolgenden Leer-Scans wird `reset()` + `setupRF()` aufgerufen. Nützlich wenn der PN5180 sich "aufhängt".

**Problem ohne Cooldown:**
Bei einem leeren Feld mit `rfRecoveryInterval=5` und ~6ms Poll-Zykluszeit → Recovery alle 30ms → massiver Recovery-Sturm (47 Recoveries in 1,9s beobachtet!). Das belastet den PN5180 und verlangsamt die Erkennung.

**Recovery Cooldown** (`setRfRecoveryCooldown(ms)`): Mindestzeit zwischen zwei Recoveries. Verhindert den Sturm.

**Empfehlung:**
```cpp
nfc.setRfRecoveryInterval(5);       // Recovery nach 5 Fehlversuchen
nfc.setRfRecoveryCooldown(500);     // Aber max. alle 500ms
```

---

## Soft RF Refresh (während Karte aufliegt)

Idee: Periodisch `loadRFConfig()` + `setRF_on()` aufrufen während eine Karte aufliegt, um AGC-Drift zu kompensieren.

**Ergebnis: Nicht notwendig.**

Die Kartenausfälle wurden nicht durch AGC-Drift verursacht (siehe nächster Abschnitt). Der Soft RF Refresh existiert als Option, sollte aber deaktiviert bleiben.

**Empfehlung:**
```cpp
nfc.setRfRefreshInterval(0);  // Deaktiviert (nicht benötigt)
```

---

## KRITISCHE Erkenntnis: SPI-Blocking verursacht Kartenausfälle

**Das wichtigste Learning aus dem gesamten Debugging:**

Periodische Debug-Dumps, die PN5180-Register per SPI auslesen (`readRegister()`), blockieren die Poll-Schleife für ~20-30ms. Während dieser Zeit findet kein `pollCard()` statt → der PN5180 sieht keine REQA/WUPA-Kommandos → die ISO 14443A-Verbindung zur Karte geht verloren → nach der Removal-Threshold → `CARD_EVENT_REMOVED`.

**Symptome:**
- Karte wird exakt alle N Sekunden als "entfernt" und sofort wieder als "neu" erkannt (Flicker)
- Das Intervall entspricht dem Debug-Dump-Intervall (z.B. 8s)
- REMOVED → NEW innerhalb von ~24ms (physisch hat sich nichts bewegt)

**Die Fehldiagnose:**
Wir haben wochenlang AGC-Drift, Antenna-Detuning und DPC-Probleme untersucht — aber der Debug-Dump selbst hat die Ausfälle verursacht. **Messung hat das Ergebnis verändert.**

**Lösung:**
```cpp
// Debug-Dump überspringt Register-Reads wenn eine Karte aufliegt
if (!cardPresent) {
    printRegisterSnapshot(nfc);  // SPI-Reads nur ohne Karte
}
```

**Allgemein:** **Keine SPI-Kommunikation zum PN5180 zwischen den `pollCard()`-Aufrufen, solange eine Karte erkannt ist!** Jede SPI-Transaktion die länger als ~5ms dauert kann die Verbindung zur Karte stören.

---

## Polling-Parameter

| Parameter | Empfehlung | Begründung |
|-----------|-----------|------------|
| `commandTimeout` | **20** | REQA→ATQA dauert <5ms. 20ms genügt mit Reserve. Default 500ms ist viel zu hoch. |
| `setRemovalThreshold()` | **5–8** | Robust gegen einzelne Fehlscans. Bei korrektem Code (keine SPI-Blockierung) reichen auch 5. |
| Poll-Delay | **0** | Maximale Polling-Geschwindigkeit (~4-6ms pro Zyklus) |

---

## Optimale Konfiguration

Für ESP32-S3 mit PN5180 und großen 35mm NTAG-Tags:

```cpp
nfc.commandTimeout = 20;
nfc.setRemovalThreshold(8);
nfc.setRfRecoveryInterval(5);
nfc.setRfRecoveryCooldown(500);
nfc.setRfRefreshInterval(0);

nfc.setRxGain(2);        // 50 dB
nfc.setTxClk(0x7C);      // Halbes Feld für große Tags
// nfc.setAgcRef(0xA0);  // Nicht verwenden — PN5180 ignoriert es
```

---

## Zusammenfassung der PN5180-Eigenheiten

1. **`loadRFConfig()` setzt alle Register zurück** → Immer Overrides verwenden (`setRxGain()`/`setTxClk()`).
2. **AGC lässt sich nicht fixieren** → PN5180-Firmware überschreibt AGC_REF_CONFIG sofort.
3. **DPC (Dynamic Power Control) steuert AGC intern** → DPC_XI (EEPROM 0x5C) zeigt den Index, ist aber read-only.
4. **SPI-Reads blockieren den PN5180** → Keine Register-Reads während eine Karte aktiv ist!
5. **Recovery-Sturm bei leerem Feld** → Immer Cooldown verwenden.
6. **Große Tags brauchen reduzierte Feldstärke** → TX_CLK=0x7C und RX_GAIN=2 statt Defaults.
7. **ISO 14443A braucht kontinuierliches Polling** → Jede Unterbrechung >5ms kann die Verbindung kosten.
