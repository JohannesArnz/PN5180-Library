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

// Pin and polling defaults come from nfc_config.h (included by the library).
// To override, create your own nfc_config.h in your project's include/ folder
// and define only the values you want to change. For example:
//
//   #ifndef NFC_CONFIG_H
//   #define NFC_CONFIG_H
//   #define PN5180_NSS_PIN  5
//   #define PN5180_BUSY_PIN 16
//   #define PN5180_RST_PIN  17
//   #endif

// ============================================================
// Setup
// ============================================================

#if defined(ARDUINO_ARCH_ESP32)
  SPIClass hspi(FSPI);
  PN5180ISO14443 nfc(PN5180_NSS_PIN, PN5180_BUSY_PIN, PN5180_RST_PIN, hspi);
#else
  PN5180ISO14443 nfc(PN5180_NSS_PIN, PN5180_BUSY_PIN, PN5180_RST_PIN);
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("=================================="));
  Serial.println(F("PN5180 Fast-Polling Example"));
  Serial.println(F("=================================="));

  // Initialize SPI bus (must happen before nfc.init())
  #if defined(ARDUINO_ARCH_ESP32)
    hspi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, PN5180_NSS_PIN);
  #endif

  // One-call init: reset, configure all parameters from nfc_config.h,
  // apply register overrides, clear IRQ, activate RF field.
  // Optional: pass a callback that fires on every RF recovery.
  if (!nfc.init([]() {
    Serial.println(F("  [RF Recovery triggered]"));
  })) {
    Serial.println(F("ERROR: NFC init failed!"));
    while (1) delay(1000);
  }

  // Print PN5180 info
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version: "));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

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
