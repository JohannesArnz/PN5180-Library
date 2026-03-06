# PN5180-Library (Fast Polling Fork)

Arduino / ESP32 library for PN5180-NFC Module from NXP Semiconductors — optimized for **fast, reliable card polling**.

![PN5180-NFC module](./doc/PN5180-NFC.png)

Fork of [tueddy/PN5180-Library](https://github.com/tueddy/PN5180-Library) with a `pollCard()` state machine for ~100+ polls/sec, automatic REQA/WUPA switching, glitch filtering, RF recovery, and register overrides that survive `loadRFConfig()` resets.

Originally developed for a toy box (similar to Toniebox) where word cards need to be placed and removed with minimal latency.

## What's Different?

| Feature | Original | This Fork |
|---|---|---|
| `readCardSerial()` duration | ~613 ms | **~9 ms** |
| Empty-field scan | ~752 ms | **~3 ms** |
| Card-ON detection | ~613 ms | **~9 ms** |
| Card-OFF detection | ~1500 ms | **~15 ms** |
| Polls per second | ~1.3 | **~100-300** |
| Card state machine | — | `pollCard()` |
| 8-digit number extraction | — | `readCardNumber()` |
| WUPA for halted cards | — | automatic |
| Glitch tolerance | — | configurable |
| RF recovery | — | automatic |

### Optimizations in Detail

1. **`activateTypeA_fast()`** — Skips `loadRFConfig()` + `setRF_on()` per poll (RF field stays on)
2. **IRQ-based waiting** — Polls `RX_IRQ_STAT` instead of fixed `delay()`, with configurable timeout
3. **SPI timing** — `delay(1)` in `transceiveCommand()` replaced by `delayMicroseconds(2)`
4. **WUPA instead of REQA** — For cards in HALT state after failed anti-collision
5. **Retry logic** — Automatic second attempt when ATQA received but anti-collision fails

## Quick Start

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

Or clone locally into `lib/`:
```bash
cd lib/
git clone https://github.com/JohannesArnz/PN5180-Library.git
```

### Minimal Example

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>

// Adjust pins for your board!
#define SPI_MISO  19
#define SPI_MOSI  23
#define SPI_SCK   18
#define NFC_NSS   16
#define NFC_BUSY   5
#define NFC_RST   17

SPIClass hspi(FSPI);
PN5180ISO14443 nfc(NFC_NSS, NFC_BUSY, NFC_RST, hspi);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    hspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, NFC_NSS);
    nfc.begin();
    nfc.reset();
    
    // IMPORTANT: reduce commandTimeout (default = 500ms!)
    nfc.commandTimeout = 20;
    
    // Configure polling behavior
    nfc.setRemovalThreshold(5);     // 5 fails = card removed
    nfc.setRfRecoveryInterval(30);  // RF reset after 30 fails
    
    // Start RF field
    nfc.clearIRQStatus(0xFFFFFFFF);
    nfc.setupRF();
    
    Serial.println("Ready! Place a card...");
}

void loop() {
    uint8_t uid[10];
    uint8_t uidLen = 0;
    
    CardEvent event = nfc.pollCard(uid, &uidLen);
    
    switch (event) {
        case CARD_EVENT_NEW: {
            Serial.print("New card: ");
            for (int i = 0; i < uidLen; i++) {
                if (uid[i] < 0x10) Serial.print("0");
                Serial.print(uid[i], HEX);
                if (i < uidLen - 1) Serial.print(":");
            }
            Serial.println();
            
            // Optional: read 8-digit number from card memory
            char number[9];
            if (nfc.readCardNumber(number)) {
                Serial.print("Number: ");
                Serial.println(number);
            }
            break;
        }
        
        case CARD_EVENT_CHANGED:
            Serial.println("Different card placed!");
            break;
            
        case CARD_EVENT_PRESENT:
            // Card still there — do nothing
            break;
            
        case CARD_EVENT_REMOVED:
            Serial.println("Card removed!");
            break;
            
        case CARD_EVENT_NONE:
            // No card in field
            break;
    }
}
```

See also [`examples/PN5180-FastPolling/`](examples/PN5180-FastPolling/) for a complete example sketch.

## API Reference

### `CardEvent pollCard(uint8_t *uid, uint8_t *uidLen)`

**The central function.** One call per loop iteration. Internally manages:
- REQA/WUPA switching (REQA when no card, WUPA when card known)
- Anti-collision with automatic retry
- UID comparison (same card vs. new card)
- Glitch tolerance (ATQA received ≠ immediately "removed")
- Removal threshold (only after N consecutive real empty scans)
- RF recovery (automatic reset after configurable number of fails)

**Return values:**

| Event | Meaning | `uid` filled? | Typical trigger |
|---|---|---|---|
| `CARD_EVENT_NONE` | No card in field | No | No tag in range |
| `CARD_EVENT_NEW` | New card detected | Yes | Card placed |
| `CARD_EVENT_CHANGED` | Different card (UID changed) | Yes | Card swapped |
| `CARD_EVENT_PRESENT` | Same card still there | Yes | Card still on reader |
| `CARD_EVENT_REMOVED` | Card removed | No | Card taken away |

### `bool readCardNumber(char *number)`

Reads an 8-digit ASCII number from NTAG/Ultralight card memory.

Supported cards:
- **NTAG213/215/216** — Number in Pages 6-7
- **MIFARE Ultralight** — Number in Pages 7-8

**Parameters:**
- `number` — Buffer of at least 9 bytes (8 digits + null terminator)

**Returns:** `true` if 8-digit number found, `false` otherwise.

**Fallback scan:** If the number is not at the expected position, all read bytes are searched for 8 consecutive ASCII digits (`'0'-'9'`).

> **Important:** Only call `readCardNumber()` after `CARD_EVENT_NEW` or `CARD_EVENT_CHANGED` — not on every `CARD_EVENT_PRESENT`! Reading takes ~2-5ms extra.

### Configuration

```cpp
// SPI BUSY-wait timeout (default: 500ms)
// 20ms is optimal for fast polling
nfc.commandTimeout = 20;

// Consecutive empty scans (no ATQA) before CARD_EVENT_REMOVED
// Higher = more stable with weak RF contact, but slower removal
nfc.setRemovalThreshold(5);   // Default: 5

// RF reset (loadRFConfig + setRF_on) after N consecutive fails
// Lower = more frequent recovery, Higher = less overhead
nfc.setRfRecoveryInterval(30);  // Default: 30

// Minimum time between RF recoveries (prevents recovery storms)
nfc.setRfRecoveryCooldown(300);  // ms, Default: 0

// Periodic RF config reload while card present (0 = disabled)
nfc.setRfRefreshInterval(0);  // ms, Default: 0

// Reset state machine (e.g. after manual nfc.reset())
nfc.resetCardState();

// Optional callback on RF recovery
nfc.onRfRecovery([]() { Serial.println("RF recovered!"); });
```

### Register Overrides

`loadRFConfig()` resets all TX/RX registers to protocol defaults. These overrides are automatically re-applied after every `setupRF()` and RF recovery:

```cpp
// RX Gain (0=33dB, 1=40dB, 2=50dB, 3=57dB)
nfc.setRxGain(2);        // 50dB — good for large 35mm+ tags

// TX Clock (0x74=full field, 0x7C=half field for large tags)
nfc.setTxClk(0x7C);      // Half-field prevents overcoupling

// AGC Reference (fixed mode — WARNING: PN5180 may override this)
// nfc.setAgcRef(0xA0);  // Not recommended, see doc/TUNING.md
```

### Timing Expectations

With `commandTimeout = 20` and `removalThreshold = 5`:

| Scenario | Duration |
|---|---|
| Card placed → `CARD_EVENT_NEW` | ~9-10 ms |
| Card removed → `CARD_EVENT_REMOVED` | ~15-25 ms |
| Card swapped → `CARD_EVENT_CHANGED` | ~9-10 ms |
| `readCardNumber()` | ~2-5 ms |
| Poll without card (1 cycle) | ~3 ms |
| Poll with card (1 cycle) | ~9 ms |
| Polls per second (no card) | ~300 |
| Polls per second (with card) | ~100 |

## Application Pattern

```cpp
void loop() {
    uint8_t uid[10];
    uint8_t uidLen = 0;
    
    CardEvent event = nfc.pollCard(uid, &uidLen);
    
    switch (event) {
        case CARD_EVENT_NEW:
        case CARD_EVENT_CHANGED: {
            char number[9];
            if (nfc.readCardNumber(number)) {
                handleCardPlaced(number);  // Your application logic
            }
            break;
        }
        
        case CARD_EVENT_REMOVED:
            handleCardRemoved();  // Your application logic
            break;
            
        case CARD_EVENT_PRESENT:
            // Card still there — game/app continues
            break;
            
        case CARD_EVENT_NONE:
            // Waiting for card
            break;
    }
    
    // Your other code (audio, display, etc.)
    updateApp();
}
```

## Hardware

Tested with:
- **Board:** ESP32-S3 (N16R8)
- **NFC Reader:** PN5180
- **Cards:** NTAG213, NTAG215, MIFARE Ultralight
- **SPI:** FSPI (custom pins)

![PN5180 Schematics](./doc/FritzingLayout.jpg)

### Pin Wiring (Example: ESP32-S3)

| PN5180 | ESP32-S3 | Function |
|---|---|---|
| NSS | GPIO 16 | SPI Chip-Select |
| BUSY | GPIO 5 | Busy Signal |
| RST | GPIO 17 | Hardware Reset |
| IRQ | (optional) | Interrupt |
| MISO | GPIO 19 | SPI Data Out |
| MOSI | GPIO 23 | SPI Data In |
| SCK | GPIO 18 | SPI Clock |

> **Note:** When using an Arduino Uno/Mega (5V), you need level converters (5V→3.3V) for all PN5180 input pins! ESP32 operates at 3.3V natively.

## RF Tuning Guide

For detailed hardware tuning information (RX Gain, TX Clock, AGC, recovery settings, SPI-blocking pitfalls), see **[doc/TUNING.md](doc/TUNING.md)**.

## Credits

- **Original Library:** [ATrappmann/PN5180-Library](https://github.com/ATrappmann/PN5180-Library)
- **Maintained Fork:** [tueddy/PN5180-Library](https://github.com/tueddy/PN5180-Library)
- **Fast-Polling Fork:** [JohannesArnz/PN5180-Library](https://github.com/JohannesArnz/PN5180-Library)

## License

Original license preserved — see [LICENSE](LICENSE).

---

<details>
<summary><strong>Original Library Release Notes (v1.0 – v2.3.7)</strong></summary>

Version 2.3.7 - 01.09.2025
	* ISO14443: Explicitly allow unknown manufacturer ID 0xFF, thanks to tom !

Version 2.3.6 - 03.06.2025
	* Allow to change SPI frequency #16, thanks to @saidmoya12 !

Version 2.3.5 - 15.05.2025
	* Less blocking delays when using other tasks #15, thanks to @joe91 !

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
	* readMultipleBlock(), getInventoryMultiple() and getInventoryPoll(), thanks to @laplacier !
	* multiple ISO-15693 tags can be read at once, thanks to @laplacier !
	* fixed some bugs and warnings with c++17

Version 2.0 - 17.10.2022
	* allow to instantiate with custom SPI class, thanks to @mwick83!

Version 1.9 - 05.10.2021
	* avoid endless loop in reset()

Version 1.8 - 05.04.2021
	* Revert previous changes, SPI class was copied and caused problems
	* Speedup reading (shorter delays) in reset/transceive commands
	* better initialization for ISO-14443 cards

Version 1.7 - 27.03.2021
	* allow to setup with other SPIClass than default SPI, thanks to @tyllmoritz !

Version 1.6 - 31.01.2021
	* fix compiler warnings for platform.io
	* add LPCD example for ESP-32 (with deep sleep)

Version 1.5 - 07.12.2020
	* ISO-14443 protocol, basic support for Mifare cards
	* Low power card detection
	* handle transceiveCommand timeout

Version 1.4 - 13.11.2019
	* ICODE SLIX2 specific commands

Version 1.3 - 21.05.2019
	* Initialized Reset pin with HIGH
	* Made readBuffer static
	* Typo fixes, data type corrections

Version 1.2 - 28.01.2019
	* Cleared Option bit in readSingleBlock and writeSingleBlock

Version 1.1 - 26.10.2018
	* Cleanup, bug fixing, refactoring
	* Automatic Arduino vs. ESP-32 platform detection

Version 1.0.x - 21.09.2018
	* Initial versions

</details>