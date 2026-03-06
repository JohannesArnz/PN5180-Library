// ============================================================
// NFC Configuration Example
// ============================================================
// Copy this file to your project's include/ folder and rename
// to "nfc_config.h", then adjust the values for your hardware.
//
// This file is NOT required by the PN5180 library itself — all
// settings can be configured via setter methods at runtime.
// This is just a convenient way to centralize configuration.
// ============================================================

#ifndef NFC_CONFIG_H
#define NFC_CONFIG_H

// ============================================================
// Pin Configuration
// ============================================================
// Adjust these for your board and wiring!

// --- ESP32 default SPI pins ---
#define SPI_MISO_PIN      19
#define SPI_MOSI_PIN      23
#define SPI_SCK_PIN       18

// --- PN5180 control pins ---
#define PN5180_NSS_PIN    16
#define PN5180_BUSY_PIN    5
#define PN5180_RST_PIN    17
#define PN5180_IRQ_PIN    -1   // -1 = not connected (optional)

// ============================================================
// Polling Parameters
// ============================================================

// Delay between polls in ms (0 = maximum speed, ~100-300 polls/sec)
#define POLL_DELAY_MS         0

// Consecutive empty scans before CARD_EVENT_REMOVED is reported.
// Higher = more robust against brief RF glitches, but slower removal.
//   1 = immediate (sensitive to noise)
//   5 = good default
//  10-15 = very robust (for noisy environments)
#define REMOVAL_THRESHOLD     5

// BUSY-wait timeout of the PN5180 library in ms (default: 500).
// REQA->ATQA takes <5ms, so 20ms is generous.
// Lower values = faster empty-field scans.
#define COMMAND_TIMEOUT_MS    20

// ============================================================
// RF Settings
// ============================================================

// RF Recovery: reset() + setupRF() after N consecutive fails.
// Recovers from PN5180 lock-ups. Set to 0 to disable.
#define RF_RECOVERY_INTERVAL  5

// Recovery Cooldown: minimum ms between RF recoveries.
// Prevents recovery storms when no card is present.
// 0 = no cooldown (not recommended with low RF_RECOVERY_INTERVAL)
#define NFC_RECOVERY_COOLDOWN_MS  300

// RX Gain (RF_CONTROL_RX register 0x22, Bits 1:0)
//   0 = 33 dB (lowest, for cards directly on antenna)
//   1 = 40 dB
//   2 = 50 dB (recommended for large 35mm+ tags)
//   3 = 57 dB (maximum, for distant/small tags)
// Note: loadRFConfig() resets this. The library re-applies it automatically.
#define NFC_RX_GAIN_ENABLED   false
#define NFC_RX_GAIN           2

// TX Clock Config (RF_CONTROL_TX_CLK register 0x21)
//   0x74 = Standard push/pull (full RF field) — default for ISO 14443A
//   0x7C = Half-field (high-side push) — recommended for large tags
// Large tags in direct contact can overcoupled with 0x74.
// Note: loadRFConfig() resets this. The library re-applies it automatically.
#define NFC_TX_CLK_ENABLED    false
#define NFC_TX_CLK            0x74

// AGC Reference Config (AGC_REF_CONFIG register 0x26)
// WARNING: The PN5180 firmware overrides this value immediately.
// Fixed AGC mode does NOT work reliably. Leave disabled.
#define NFC_AGC_REF_ENABLED   false
#define NFC_AGC_REF           0xA0

// Soft RF Refresh: periodically reload RF config while card is present.
// Interval in ms (0 = disabled). Can help with AGC drift on some setups.
// Usually not needed if polling is uninterrupted.
#define NFC_RF_REFRESH_MS     0

// ============================================================
// Debug Settings
// ============================================================

// Periodic debug dump with counters and live register values.
// WARNING: SPI reads during active card polling can cause
// card dropouts! Only enable for debugging, not production.
#define NFC_DEBUG_DUMP_ENABLED    false
#define NFC_DEBUG_DUMP_INTERVAL_S 8

#endif // NFC_CONFIG_H
