// NAME: PN5180ISO14443.h
//
// DESC: ISO14443 protocol on NXP Semiconductors PN5180 module for Arduino.
//
// Copyright (c) 2019 by Dirk Carstensen. All rights reserved.
//
// This file is part of the PN5180 library for the Arduino environment.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
#ifndef PN5180ISO14443_H
#define PN5180ISO14443_H

#include "PN5180.h"

// Card events returned by pollCard()
enum CardEvent : uint8_t {
  CARD_EVENT_NONE     = 0,  // No card in field, no change
  CARD_EVENT_NEW      = 1,  // New card detected (was absent)
  CARD_EVENT_CHANGED  = 2,  // Different card detected (UID changed)
  CARD_EVENT_PRESENT  = 3,  // Same card still present
  CARD_EVENT_REMOVED  = 4   // Card removed (after threshold)
};

class PN5180ISO14443 : public PN5180 {

public:
  PN5180ISO14443(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin, SPIClass& spi=SPI);
  
private:
  uint16_t rxBytesReceived();
  uint32_t GetNumberOfBytesReceivedAndValidBits();

  // Internal card tracking state
  uint8_t _cardState;          // 0=ABSENT, 1=PRESENT
  uint8_t _lastUID[8];
  uint8_t _lastUIDLen;
  uint8_t _consecutiveFails;
  uint8_t _removalThreshold;   // consecutive fails before REMOVED (default 5)
  uint8_t _rfRecoveryInterval; // RF reset every N consecutive fails (default 30)
  void (*_onRfRecovery)(void); // optional callback on RF recovery
  unsigned long _lastRefreshMs;        // millis() of last RF refresh
  unsigned long _rfRefreshIntervalMs;  // soft RF refresh period while card present (0=off)
  unsigned long _lastRecoveryMs;       // millis() of last RF recovery
  unsigned long _rfRecoveryCooldownMs; // min ms between recoveries (0=off)

public:
  // Mifare TypeA - low level
  int8_t activateTypeA(uint8_t *buffer, uint8_t kind);
  int8_t activateTypeA_fast(uint8_t *buffer, uint8_t kind);
  int8_t activateTypeA_fast_inner(uint8_t *buffer, uint8_t kind);
  bool mifareBlockRead(uint8_t blockno, uint8_t *buffer);
  uint8_t mifareBlockWrite16(uint8_t blockno, const uint8_t *buffer);
  bool mifareHalt();

  /*
   * High-level polling API
   */
public:
  // Single-call polling with internal state machine.
  // Handles REQA/WUPA switching, glitch filtering, removal detection, RF recovery.
  // uid: buffer for UID (min 8 bytes), uidLen: filled with UID length on valid read.
  // Returns CardEvent indicating what happened.
  CardEvent pollCard(uint8_t *uid, uint8_t *uidLen);

  // Read 8-digit ASCII number from NTAG/Ultralight card memory.
  // Call after pollCard() returns CARD_EVENT_NEW or CARD_EVENT_CHANGED.
  // number: buffer for result (min 9 bytes, null-terminated).
  // Returns true if 8-digit number found, false otherwise.
  bool readCardNumber(char *number);

  // Configuration
  void setRemovalThreshold(uint8_t n) { _removalThreshold = n; }
  void setRfRecoveryInterval(uint8_t n) { _rfRecoveryInterval = n; }
  void onRfRecovery(void (*cb)(void)) { _onRfRecovery = cb; }
  void setRfRefreshInterval(unsigned long ms) { _rfRefreshIntervalMs = ms; }
  void setRfRecoveryCooldown(unsigned long ms) { _rfRecoveryCooldownMs = ms; }
  void resetCardState();

  /*
   * Helper functions
   */
public:   
  bool setupRF();
  int8_t readCardSerial(uint8_t *buffer);    
  int8_t readCardSerial_wupa(uint8_t *buffer);
  bool isCardPresent();    
};

#endif /* PN5180ISO14443_H */
