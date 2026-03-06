// ============================================================
// NFC Konfiguration - Zentrale Einstellungen
// ============================================================
// Alle konfigurierbaren Werte für den PN5180 NFC-Reader
// und die Spielbox-Anwendung an einem Ort.
//
// Diese Datei wird mit der Library ausgeliefert.
// Um Werte projektspezifisch zu überschreiben, lege eine
// eigene nfc_config.h in deinem Projekt-include/ Ordner an.
// ============================================================

#ifndef NFC_CONFIG_H
#define NFC_CONFIG_H

// ============================================================
// Pin-Konfiguration (ESP32-S3)
// ============================================================
#define SPI_MISO_PIN      37
#define SPI_MOSI_PIN      35
#define SPI_SCK_PIN       36

#define PN5180_NSS_PIN    34
#define PN5180_BUSY_PIN   38
#define PN5180_RST_PIN    33
#define PN5180_IRQ_PIN    39

// ============================================================
// Polling-Parameter
// ============================================================

// Delay zwischen Polls in ms (0 = maximale Geschwindigkeit)
#define POLL_DELAY_MS         0

// Fehlversuche bis "Karte entfernt" erkannt wird
// 1 = sofort, aber empfindlich für Wackler
// 2-3 = guter Kompromiss, 5 = robust
// ntag213 fake funktioniert mit 5 gut. 
#define REMOVAL_THRESHOLD     15


// BUSY-Wait-Timeout der PN5180 Library in ms (Default: 500)
// REQA->ATQA dauert <5ms. Niedrigere Werte = schnellere Leer-Scans.
// Empfohlen: 20-50ms
#define COMMAND_TIMEOUT_MS    20

// Anzahl der ersten Polls mit Debug-Output
#define VERBOSE_FIRST_N       5

// ============================================================
// RF-Einstellungen
// ============================================================

// RF Recovery: Nach wie vielen Fehlversuchen wird
// reset() + setupRF() aufgerufen (0 = deaktiviert)
#define RF_RECOVERY_INTERVAL  5

// RX Gain (RF_CONTROL_RX Register 0x22, Bits 1:0)
// Steuert die Empfangsempfindlichkeit des PN5180.
//
// Mögliche Werte:
//   0 = 33 dB (niedrigste Empfindlichkeit)
//   1 = 40 dB
//   2 = 50 dB
//   3 = 57 dB (höchste Empfindlichkeit)
//
// Hinweis: loadRFConfig() setzt diesen Wert zurück.
// Der Override wird nach jedem setupRF() / RF-Recovery
// automatisch erneut angewendet.
//
// true = Gain-Override aktiv, false = PN5180-Default beibehalten
#define NFC_RX_GAIN_ENABLED   true
#define NFC_RX_GAIN           2 

// TX Clock Config (RF_CONTROL_TX_CLK Register 0x21)
// Steuert Modulation, Ausgangsinvertierung, ALM und DPLL.
//
// Protokoll-Standardwerte (von loadRFConfig gesetzt):
//   0x74 = ISO 14443 A-106 (Default für NTAG/MIFARE)
//   0x82 = ISO 14443 A-212 bis A-848
//   0x8E = ISO 14443 B-106/212, FeliCa-212/424
//
// Hinweis: loadRFConfig() setzt diesen Wert zurück.
// Der Override wird nach jedem setupRF() / RF-Recovery
// automatisch erneut angewendet.

// 0x74 = Standard Vollgas (Push/Pull)
// 0x7A = Halbgas (RF low side pull) - Hack für 35mm Tags
// 0x7C = Halbgas (RF high side push) - Alternativer Hack für 35mm Tags
//
// true = TX-CLK-Override aktiv, false = PN5180-Default beibehalten
#define NFC_TX_CLK_ENABLED    true
#define NFC_TX_CLK            0x74

// AGC Reference Config (AGC_REF_CONFIG Register 0x26)
// Überschreibt den automatischen AGC mit einem festen Wert.
// Verhindert AGC-Drift bei großen/nahen Tags.
//
// Den richtigen Startwert aus einem Debug-Dump ablesen:
// AGC_REF_CONFIG direkt nach Karten-Erkennung (typisch 0x90..0xAA).
//
// Hinweis: loadRFConfig() setzt diesen Wert zurück.
// Der Override wird nach jedem setupRF() / RF-Recovery
// automatisch erneut angewendet.
//
// true = AGC-Override aktiv (Fixed Mode), false = automatischer AGC
#define NFC_AGC_REF_ENABLED   false
#define NFC_AGC_REF           0xA0

// Soft RF Refresh: Periodisch loadRFConfig() wiederholen
// solange eine Karte aufliegt, um AGC-Drift zu verhindern.
// Interval in ms (0 = deaktiviert). Empfehlung: 3000-5000ms
#define NFC_RF_REFRESH_MS         500

// Recovery Cooldown: Mindestzeit zwischen RF-Recoveries
// bei leerem Feld (verhindert Recovery-Sturm). In ms.
// 0 = kein Cooldown (altes Verhalten)
#define NFC_RECOVERY_COOLDOWN_MS  300

// ============================================================
// Debug-Einstellungen
// ============================================================

// Periodischer Debug-Dump mit Countern und Live-Registern
// true = aktiviert, false = deaktiviert
// 'd' im Serial Monitor erzwingt immer einen manuellen Dump.
#define NFC_DEBUG_DUMP_ENABLED    false

// Intervall des automatischen Debug-Dumps in Sekunden
#define NFC_DEBUG_DUMP_INTERVAL_S 8

#endif // NFC_CONFIG_H
