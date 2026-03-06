// NAME: PN5180-FastPolling.ino
//
// DESC: Fast card polling example using the PN5180 Fast-Polling Fork.
//       Uses pollCard() state machine for ~100+ polls/sec with automatic
//       REQA/WUPA switching, glitch filtering, and RF recovery.
//
// This is the recommended way to use this library for applications that
// need fast card detection and removal (e.g. toy boxes, game controllers).
//
// Copyright (c) 2025 by Johannes Arnz. All rights reserved.
//
// This file is part of the PN5180 library for the Arduino environment.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//

#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>

// ============================================================
// Pin Configuration — adjust for your board!
// ============================================================
#if defined(ARDUINO_ARCH_ESP32)
  // ESP32 / ESP32-S3 example pins
  #define SPI_MISO   19
  #define SPI_MOSI   23
  #define SPI_SCK    18
  #define NFC_NSS    16
  #define NFC_BUSY    5
  #define NFC_RST    17
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_NANO)
  // Arduino Uno/Mega/Nano — uses default SPI pins
  #define NFC_NSS    10
  #define NFC_BUSY    9
  #define NFC_RST     7
#else
  #error "Please define your pin configuration here!"
#endif

// ============================================================
// Polling Parameters
// ============================================================

// BUSY-wait timeout in ms (default: 500). Lower = faster empty-field scans.
// 20ms is optimal for fast polling (REQA->ATQA takes <5ms).
#define COMMAND_TIMEOUT_MS   20

// Consecutive empty scans before CARD_EVENT_REMOVED is reported.
// Higher = more robust against brief RF glitches, but slower removal detection.
#define REMOVAL_THRESHOLD     5

// After this many consecutive fails, RF is reset (loadRFConfig + setRF_on).
// Set to 0 to disable. Useful if the PN5180 loses RF lock.
#define RF_RECOVERY_INTERVAL 30

// ============================================================
// Setup
// ============================================================

#if defined(ARDUINO_ARCH_ESP32)
  SPIClass hspi(FSPI);
  PN5180ISO14443 nfc(NFC_NSS, NFC_BUSY, NFC_RST, hspi);
#else
  PN5180ISO14443 nfc(NFC_NSS, NFC_BUSY, NFC_RST);
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("=================================="));
  Serial.println(F("PN5180 Fast-Polling Example"));
  Serial.println(F("=================================="));

  // Initialize SPI
  #if defined(ARDUINO_ARCH_ESP32)
    hspi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, NFC_NSS);
  #endif

  nfc.begin();
  nfc.reset();

  // Print PN5180 info
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version: "));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

  // Configure fast polling parameters
  nfc.commandTimeout = COMMAND_TIMEOUT_MS;
  nfc.setRemovalThreshold(REMOVAL_THRESHOLD);
  nfc.setRfRecoveryInterval(RF_RECOVERY_INTERVAL);

  // Optional: Register overrides for large tags (35mm+)
  // nfc.setRxGain(2);        // 50 dB (0=33dB, 1=40dB, 2=50dB, 3=57dB)
  // nfc.setTxClk(0x7C);      // Half-field for large tags (default: 0x74)

  // Optional: RF recovery callback for logging
  nfc.onRfRecovery([]() {
    Serial.println(F("  [RF Recovery triggered]"));
  });

  // Start RF field
  nfc.clearIRQStatus(0xFFFFFFFF);
  nfc.setupRF();

  Serial.println(F("Ready! Place a card..."));
  Serial.println();
}

// ============================================================
// Main Loop — single pollCard() call per iteration
// ============================================================
void loop() {
  uint8_t uid[10];
  uint8_t uidLen = 0;

  CardEvent event = nfc.pollCard(uid, &uidLen);

  switch (event) {
    case CARD_EVENT_NEW: {
      Serial.print(F("NEW card: UID="));
      for (int i = 0; i < uidLen; i++) {
        if (uid[i] < 0x10) Serial.print("0");
        Serial.print(uid[i], HEX);
        if (i < uidLen - 1) Serial.print(":");
      }
      Serial.println();

      // Optional: Read 8-digit number from NTAG memory
      char number[9];
      if (nfc.readCardNumber(number)) {
        Serial.print(F("  Card number: "));
        Serial.println(number);
      }
      break;
    }

    case CARD_EVENT_CHANGED: {
      Serial.print(F("CHANGED card: UID="));
      for (int i = 0; i < uidLen; i++) {
        if (uid[i] < 0x10) Serial.print("0");
        Serial.print(uid[i], HEX);
        if (i < uidLen - 1) Serial.print(":");
      }
      Serial.println();

      char number[9];
      if (nfc.readCardNumber(number)) {
        Serial.print(F("  Card number: "));
        Serial.println(number);
      }
      break;
    }

    case CARD_EVENT_PRESENT:
      // Same card still on the reader — do nothing
      break;

    case CARD_EVENT_REMOVED:
      Serial.println(F("Card REMOVED"));
      break;

    case CARD_EVENT_NONE:
      // No card in field
      break;
  }
}
