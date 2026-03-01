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
//#define DEBUG 1

#include <Arduino.h>
#include "PN5180ISO14443.h"
#include <PN5180.h>
#include "Debug.h"

PN5180ISO14443::PN5180ISO14443(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin, SPIClass& spi) 
              : PN5180(SSpin, BUSYpin, RSTpin, spi) {
  _cardState = 0;          // ABSENT
  _lastUIDLen = 0;
  _consecutiveFails = 0;
  _removalThreshold = 5;
  _rfRecoveryInterval = 30;
  memset(_lastUID, 0, sizeof(_lastUID));
}

bool PN5180ISO14443::setupRF() {
  PN5180DEBUG_PRINTLN(F("Loading RF-Configuration..."));
  PN5180DEBUG_ENTER;
  
  if (loadRFConfig(0x00, 0x80)) {  // ISO14443 parameters
    PN5180DEBUG_PRINTLN(F("done."));
  }
  else {
	PN5180DEBUG_EXIT;
	return false;
  }
  PN5180DEBUG_PRINTLN(F("Turning ON RF field..."));
  if (setRF_on()) {
    PN5180DEBUG_PRINTLN(F("done."));
  }
  else {
    PN5180DEBUG_EXIT;
    return false;
  }
  
  PN5180DEBUG_EXIT;
  return true;
}


uint16_t PN5180ISO14443::rxBytesReceived() {
	PN5180DEBUG_PRINTLN(F("PN5180ISO14443::rxBytesReceived()"));	
	PN5180DEBUG_ENTER;
	
	uint32_t rxStatus;
	uint16_t len = 0;

	readRegister(RX_STATUS, &rxStatus);
	// Lower 9 bits has length
	len = (uint16_t)(rxStatus & 0x000001ff);

	PN5180DEBUG_EXIT;
	return len;
}



/*
* buffer : must be 10 byte array
* buffer[0-1] is ATQA
* buffer[2] is sak
* buffer[3..6] is 4 byte UID
* buffer[7..9] is remaining 3 bytes of UID for 7 Byte UID tags
* kind : 0  we send REQA, 1 we send WUPA
*
* return value: the uid length:
* -	zero if no tag was recognized
* - -1 general error
* - -2 card in field but with error
* -	single Size UID (4 byte)
* -	double Size UID (7 byte)
* -	triple Size UID (10 byte) - not yet supported
*/
int8_t PN5180ISO14443::	activateTypeA(uint8_t *buffer, uint8_t kind) {
	uint8_t cmd[7];
	uint8_t uidLength = 0;
	
	PN5180DEBUG_PRINTF(F("PN5180ISO14443::activateTypeA(*buffer, kind=%d)"), kind);
	PN5180DEBUG_PRINTLN();
	PN5180DEBUG_ENTER;

	// Load standard TypeA protocol already done in reset()
	if (!loadRFConfig(0x0, 0x80)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Load standard TypeA protocol failed!"));
		PN5180DEBUG_EXIT;
		return -1;
	}

	// activate RF field
	setRF_on();
	// wait RF-field to ramp-up
	delay(2);
	
	// OFF Crypto
	if (!writeRegisterWithAndMask(SYSTEM_CONFIG, 0xFFFFFFBF)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: OFF Crypto failed!"));
		PN5180DEBUG_EXIT;
		return -1;
	}
	// clear RX CRC
	if (!writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Clear RX CRC failed!"));
		PN5180DEBUG_EXIT;
		return -1;
	}
	// clear TX CRC
	if (!writeRegisterWithAndMask(CRC_TX_CONFIG, 0xFFFFFFFE)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Clear TX CRC failed!"));
		PN5180DEBUG_EXIT;
		return -1;
	}

	// set the PN5180 into IDLE state  
	if (!writeRegisterWithAndMask(SYSTEM_CONFIG, 0xFFFFFFF8)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: set IDLE state failed!"));
		PN5180DEBUG_EXIT;
		return -1;
	}
		
	// activate TRANSCEIVE routine  
	if (!writeRegisterWithOrMask(SYSTEM_CONFIG, 0x00000003)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Activates TRANSCEIVE routine failed!"));
		PN5180DEBUG_EXIT;
		return -1;
	}
	
	// wait for wait-transmit state
	PN5180TransceiveStat transceiveState = getTransceiveState();
	if (PN5180_TS_WaitTransmit != transceiveState) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Transceiver not in state WaitTransmit!?"));
		PN5180DEBUG_EXIT;
		return -1;
	}
	
/*	uint8_t irqConfig = 0b0000000; // Set IRQ active low + clear IRQ-register
    writeEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
    // enable only RX_IRQ_STAT, TX_IRQ_STAT and general error IRQ
    writeRegister(IRQ_ENABLE, RX_IRQ_STAT | TX_IRQ_STAT | GENERAL_ERROR_IRQ_STAT);  
*/

	// clear all IRQs
	clearIRQStatus(0xffffffff); 

	//Send REQA/WUPA, 7 bits in last byte
	cmd[0] = (kind == 0) ? 0x26 : 0x52;
	if (!sendData(cmd, 1, 0x07)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Send REQA/WUPA failed!"));
		PN5180DEBUG_EXIT;
		return 0;
	}
	
	// wait for end of RF reception
	delay(1);

	// READ 2 bytes ATQA into  buffer
	if (!readData(2, buffer)) {
		PN5180DEBUG(F("*** ERROR: READ 2 bytes ATQA failed!\n"));
		PN5180DEBUG_EXIT;
		return 0;
	}
	
	// 
	unsigned long startedWaiting = millis();
    PN5180DEBUG_PRINTLN(F("wait for PN5180_TS_WaitTransmit (max 200ms)"));
    PN5180DEBUG_OFF;
	while (PN5180_TS_WaitTransmit != getTransceiveState()) {
		delay(1);
		if (millis() - startedWaiting > 200) {
			PN5180DEBUG_ON;
			PN5180DEBUG_PRINTLN(F("*** ERROR: timeout in PN5180_TS_WaitTransmit!"));
			PN5180DEBUG_EXIT;
			return -1; 
		}	
	}
    PN5180DEBUG_ON;
	
	// clear all IRQs
	clearIRQStatus(0xffffffff); 
	
	// send Anti collision 1, 8 bits in last byte
	cmd[0] = 0x93;
	cmd[1] = 0x20;
	if (!sendData(cmd, 2, 0x00)) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Send Anti collision 1 failed!"));
		PN5180DEBUG_EXIT;
		return -2;
	}
	
	// wait for end of RF reception
	delay(1);

	uint8_t numBytes = rxBytesReceived();
	if (numBytes != 5) {
		PN5180DEBUG_PRINTLN(F("*** ERROR: Read 5 bytes sak failed!"));
		PN5180DEBUG_EXIT;
		return -2;
	};
	// read 5 bytes sak, we will store at offset 2 for later usage
	if (!readData(5, cmd+2)) {
		Serial.println("Read 5 bytes failed!");
		PN5180DEBUG_EXIT;
		return -2;
	}
	// We do have a card now! enable CRC and send anticollision
	// save the first 4 bytes of UID
	for (int i = 0; i < 4; i++) buffer[i] = cmd[2 + i];
	
	//Enable RX CRC calculation
	if (!writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) {
		PN5180DEBUG_EXIT;
		return -2;
	}
	//Enable TX CRC calculation
	if (!writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) {
		PN5180DEBUG_EXIT;
		return -2;
	}

	//Send Select anti collision 1, the remaining bytes are already in offset 2 onwards
	cmd[0] = 0x93;
	cmd[1] = 0x70;
	if (!sendData(cmd, 7, 0x00)) {
		// no remaining bytes, we have a 4 byte UID
		PN5180DEBUG_EXIT;
		return 4;
	}
	//Read 1 byte SAK into buffer[2]
	if (!readData(1, buffer+2)) {
		PN5180DEBUG_EXIT;
		return -2;
	}
	// Check if the tag is 4 Byte UID or 7 byte UID and requires anti collision 2
	// If Bit 3 is 0 it is 4 Byte UID
	if ((buffer[2] & 0x04) == 0) {
		// Take first 4 bytes of anti collision as UID store at offset 3 onwards. job done
		for (int i = 0; i < 4; i++) buffer[3+i] = cmd[2 + i];
		uidLength = 4;
	}
	else {
		// Take First 3 bytes of UID, Ignore first byte 88(CT)
		if (cmd[2] != 0x88) {
			PN5180DEBUG_EXIT;
			return 0;
		}
		for (int i = 0; i < 3; i++) buffer[3+i] = cmd[3 + i];
		// Clear RX CRC
		if (!writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		// Clear TX CRC
		if (!writeRegisterWithAndMask(CRC_TX_CONFIG, 0xFFFFFFFE)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		// Do anti collision 2
		cmd[0] = 0x95;
		cmd[1] = 0x20;
		if (!sendData(cmd, 2, 0x00)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		//Read 5 bytes. we will store at offset 2 for later use
		if (!readData(5, cmd+2)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		// first 4 bytes belongs to last 4 UID bytes, we keep it.
		for (int i = 0; i < 4; i++) {
		  buffer[6 + i] = cmd[2+i];
		}
		//Enable RX CRC calculation
		if (!writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		//Enable TX CRC calculation
		if (!writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		//Send Select anti collision 2 
		cmd[0] = 0x95;
		cmd[1] = 0x70;
		if (!sendData(cmd, 7, 0x00)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		//Read 1 byte SAK into buffer[2]
		if (!readData(1, buffer + 2)) {
			PN5180DEBUG_EXIT;
			return -2;
		}
		uidLength = 7;
	}
	PN5180DEBUG_EXIT;
    return uidLength;
}

/*
 * Fast version of activateTypeA() for continuous polling.
 * Skips loadRFConfig() and setRF_on() since they are already done once in setupRF().
 * This saves ~12ms+ per poll cycle.
 * 
 * IMPORTANT: Call setupRF() once before using this function!
 * If RF state becomes inconsistent (e.g. after error recovery), 
 * call setupRF() again before resuming fast polling.
 */
int8_t PN5180ISO14443::activateTypeA_fast(uint8_t *buffer, uint8_t kind) {
	// Retry wrapper: if we got ATQA (card present!) but a later step fails,
	// retry once. This handles transient RF glitches during fast polling.
	for (int attempt = 0; attempt < 2; attempt++) {
		int8_t result = activateTypeA_fast_inner(buffer, kind);
		if (result != -2) return result;  // success, no-card, or fatal error
		// -2 = got ATQA but later step failed -> retry
	}
	return -2; // both attempts failed
}

int8_t PN5180ISO14443::activateTypeA_fast_inner(uint8_t *buffer, uint8_t kind) {
	uint8_t cmd[7];
	uint8_t uidLength = 0;

	// OFF Crypto
	if (!writeRegisterWithAndMask(SYSTEM_CONFIG, 0xFFFFFFBF))
		return -1;
	// clear RX CRC
	if (!writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE))
		return -1;
	// clear TX CRC
	if (!writeRegisterWithAndMask(CRC_TX_CONFIG, 0xFFFFFFFE))
		return -1;

	// NOTE: sendData() internally does IDLE->TRANSCEIVE->WaitTransmit,
	// so we do NOT manually set transceiver state here.

	// clear all IRQs
	clearIRQStatus(0xffffffff); 

	// Send REQA/WUPA, 7 bits in last byte
	cmd[0] = (kind == 0) ? 0x26 : 0x52;
	if (!sendData(cmd, 1, 0x07)) {
		// sendData failed - recover RF
		loadRFConfig(0x0, 0x80);
		setRF_on();
		delay(2);
		return 0;
	}
	
	// Wait for RF reception complete (RX_IRQ) - REQA response ~300us
	{
		unsigned long t = micros();
		while (!(getIRQStatus() & RX_IRQ_STAT)) {
			if (micros() - t > 5000) break; // 5ms timeout = no card
		}
	}

	// Check if we actually received ATQA bytes before reading
	uint16_t atqaLen = rxBytesReceived();
	if (atqaLen < 2) {
		// No card responded
		return 0;
	}

	// READ 2 bytes ATQA into buffer
	if (!readData(2, buffer))
		return 0;
	
	// Validate ATQA (FF FF = garbage/no real response)
	if (buffer[0] == 0xFF && buffer[1] == 0xFF)
		return 0;
	
	// === Card is present from here (got valid ATQA) ===
	// Any failure below returns -2 to trigger retry

	// clear all IRQs before Anti-Col 1
	clearIRQStatus(0xffffffff); 
	
	// send Anti collision 1, 8 bits in last byte
	cmd[0] = 0x93;
	cmd[1] = 0x20;
	if (!sendData(cmd, 2, 0x00))
		return -2;
	
	// Wait for RF reception complete (RX_IRQ)
	{
		unsigned long t = micros();
		while (!(getIRQStatus() & RX_IRQ_STAT)) {
			if (micros() - t > 5000) break;
		}
	}

	uint8_t numBytes = rxBytesReceived();
	if (numBytes != 5)
		return -2;

	// read 5 bytes, store at offset 2 for later usage
	if (!readData(5, cmd+2))
		return -2;
	
	// save the first 4 bytes of UID
	for (int i = 0; i < 4; i++) buffer[i] = cmd[2 + i];
	
	//Enable RX CRC calculation
	if (!writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) 
	  return -2;
	//Enable TX CRC calculation
	if (!writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) 
	  return -2;

	clearIRQStatus(0xffffffff);

	//Send Select anti collision 1
	cmd[0] = 0x93;
	cmd[1] = 0x70;
	if (!sendData(cmd, 7, 0x00))
		return -2;

	// Wait for RF reception complete (RX_IRQ) - Select response
	{
		unsigned long t = micros();
		while (!(getIRQStatus() & RX_IRQ_STAT)) {
			if (micros() - t > 5000) break;
		}
	}
	if (rxBytesReceived() < 1)
		return -2;

	//Read 1 byte SAK into buffer[2]
	if (!readData(1, buffer+2))
		return -2;

	// Check if the tag is 4 Byte UID or 7 byte UID and requires anti collision 2
	// If Bit 3 is 0 it is 4 Byte UID
	if ((buffer[2] & 0x04) == 0) {
		// Take first 4 bytes of anti collision as UID store at offset 3 onwards
		for (int i = 0; i < 4; i++) buffer[3+i] = cmd[2 + i];
		uidLength = 4;
	}
	else {
		// Take First 3 bytes of UID, Ignore first byte 88(CT)
		if (cmd[2] != 0x88)
			return 0;

		for (int i = 0; i < 3; i++) buffer[3+i] = cmd[3 + i];
		// Clear RX CRC
		if (!writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE)) 
			return -2;
		// Clear TX CRC
		if (!writeRegisterWithAndMask(CRC_TX_CONFIG, 0xFFFFFFFE)) 
			return -2;

		clearIRQStatus(0xffffffff);

		// Do anti collision 2
		cmd[0] = 0x95;
		cmd[1] = 0x20;
		if (!sendData(cmd, 2, 0x00))
			return -2;

		// Wait for RF reception complete (RX_IRQ)
		{
			unsigned long t = micros();
			while (!(getIRQStatus() & RX_IRQ_STAT)) {
				if (micros() - t > 5000) break;
			}
		}
		if (rxBytesReceived() < 5)
			return -2;

		//Read 5 bytes. store at offset 2 for later use
		if (!readData(5, cmd+2)) 
			return -2;

		// first 4 bytes belongs to last 4 UID bytes, we keep it.
		for (int i = 0; i < 4; i++) {
		  buffer[6 + i] = cmd[2+i];
		}
		//Enable RX CRC calculation
		if (!writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) 
			return -2;
		//Enable TX CRC calculation
		if (!writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) 
			return -2;

		clearIRQStatus(0xffffffff);

		//Send Select anti collision 2 
		cmd[0] = 0x95;
		cmd[1] = 0x70;
		if (!sendData(cmd, 7, 0x00))
			return -2;

		// Wait for RF reception complete (RX_IRQ)
		{
			unsigned long t = micros();
			while (!(getIRQStatus() & RX_IRQ_STAT)) {
				if (micros() - t > 5000) break;
			}
		}
		if (rxBytesReceived() < 1)
			return -2;

		//Read 1 byte SAK into buffer[2]
		if (!readData(1, buffer + 2)) 
			return -2;

		uidLength = 7;
	}
    return uidLength;
}

bool PN5180ISO14443::mifareBlockRead(uint8_t blockno, uint8_t *buffer) {
	bool success = false;
	uint16_t len;
	uint8_t cmd[2];
	// Send mifare command 30,blockno
	cmd[0] = 0x30;
	cmd[1] = blockno;
	if (!sendData(cmd, 2, 0x00))
	  return false;
	//Check if we have received any data from the tag
	delay(5);
	len = rxBytesReceived();
	if (len == 16) {
		// READ 16 bytes into  buffer
		if (readData(16, buffer))
		  success = true;
	}
	return success;
}


uint8_t PN5180ISO14443::mifareBlockWrite16(uint8_t blockno, const uint8_t *buffer) {
	uint8_t cmd[2];
	// Clear RX CRC
	writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE);

	// Mifare write part 1
	cmd[0] = 0xA0;
	cmd[1] = blockno;
	sendData(cmd, 2, 0x00);
	readData(1, cmd);

	// Mifare write part 2
	sendData(buffer,16, 0x00);
	delay(10);

	// Read ACK/NAK
	readData(1, cmd);

	//Enable RX CRC calculation
	writeRegisterWithOrMask(CRC_RX_CONFIG, 0x1);
	return cmd[0];
}

bool PN5180ISO14443::mifareHalt() {
	uint8_t cmd[2];
	//mifare Halt
	cmd[0] = 0x50;
	cmd[1] = 0x00;
	sendData(cmd, 2, 0x00);	
	return true;
}

int8_t PN5180ISO14443::readCardSerial(uint8_t *buffer) {
	PN5180DEBUG_PRINTLN(F("PN5180ISO14443::readCardSerial(*buffer)"));
	PN5180DEBUG_ENTER;
  
	// Always return 10 bytes
    // Offset 0..1 is ATQA
    // Offset 2 is SAK.
    // UID 4 bytes : offset 3 to 6 is UID, offset 7 to 9 to Zero
    // UID 7 bytes : offset 3 to 9 is UID
	// try to activate Type A until response or timeout
    uint8_t response[10] = { 0 };
	int8_t uidLength = activateTypeA_fast(response, 0);
	
	if (uidLength <= 0){
	  PN5180DEBUG_EXIT;
	  return uidLength;
	}
	// UID length must be at least 4 bytes
	if (uidLength < 4){
	  PN5180DEBUG_EXIT;
	  return 0;
	}
	if ((response[0] == 0xFF) && (response[1] == 0xFF))
	  uidLength = 0;
	
	// first UID byte should not be 0x00
	// explicitly allow unknown manufacturer ID 0xFF
	if (response[3] == 0x00)  
		uidLength = 0;
		
	// check for valid uid, skip first byte (0x04)
	// 0x04 0x00 0xFF 0x00 => invalid uid
	bool validUID = false;
	for (int i = 1; i < uidLength; i++) {
		if ((response[i+3] != 0x00) && (response[i+3] != 0xFF)) {
			validUID = true;
			break;
		}
	}
	if (uidLength == 4) {
		if ((response[3] == 0x88)) {
			// must not be the CT-flag (0x88)!
			validUID = false;
		};
	}
	if (uidLength == 7) {
		if ((response[6] == 0x88)) {
			// must not be the CT-flag (0x88)!
			validUID = false;
		};
		if ((response[6] == 0x00) && (response[7] == 0x00) && (response[8] == 0x00) && (response[9] == 0x00)) {
			validUID = false;
		};
		if ((response[6] == 0xFF) && (response[7] == 0xFF) && (response[8] == 0xFF) && (response[9] == 0xFF)) {
			validUID = false;
		};
	};
//	mifareHalt();
	if (validUID) {
		for (int i = 0; i < uidLength; i++) buffer[i] = response[i+3];
		PN5180DEBUG_EXIT;
		return uidLength;
	} else {
		PN5180DEBUG_EXIT;
		return 0;
	}
}

bool PN5180ISO14443::isCardPresent() {
	PN5180DEBUG_PRINTLN("PN5180ISO14443::isCardPresent()");
	PN5180DEBUG_ENTER;

    uint8_t buffer[10];
	
	bool ret = (readCardSerial(buffer) >=4);

	PN5180DEBUG_EXIT;
	return ret;
}

/*
 * readCardSerial_wupa() - Same as readCardSerial() but uses WUPA (0x52) instead of REQA (0x26).
 * WUPA wakes up cards in HALT state (after failed anti-collision).
 * Use this when card is known to be present for more robust re-detection.
 */
int8_t PN5180ISO14443::readCardSerial_wupa(uint8_t *buffer) {
	uint8_t response[10] = { 0 };
	int8_t uidLength = activateTypeA_fast(response, 1); // 1 = WUPA
	
	if (uidLength <= 0)
		return uidLength;
	if (uidLength < 4)
		return 0;
	if ((response[0] == 0xFF) && (response[1] == 0xFF))
		uidLength = 0;
	if (response[3] == 0x00)
		uidLength = 0;
	
	bool validUID = false;
	for (int i = 1; i < uidLength; i++) {
		if ((response[i+3] != 0x00) && (response[i+3] != 0xFF)) {
			validUID = true;
			break;
		}
	}
	if (uidLength == 4 && response[3] == 0x88)
		validUID = false;
	if (uidLength == 7) {
		if (response[6] == 0x88)
			validUID = false;
		if ((response[6] == 0x00) && (response[7] == 0x00) && (response[8] == 0x00) && (response[9] == 0x00))
			validUID = false;
		if ((response[6] == 0xFF) && (response[7] == 0xFF) && (response[8] == 0xFF) && (response[9] == 0xFF))
			validUID = false;
	}
	if (validUID) {
		for (int i = 0; i < uidLength; i++) buffer[i] = response[i+3];
		return uidLength;
	}
	return 0;
}

// ============================================================
// resetCardState() - Reset internal card tracking state
// ============================================================
void PN5180ISO14443::resetCardState() {
	_cardState = 0;
	_consecutiveFails = 0;
	_lastUIDLen = 0;
	memset(_lastUID, 0, sizeof(_lastUID));
}

// ============================================================
// pollCard() - High-level single-call polling with state machine
//
// Handles internally:
//   - REQA (card absent) vs WUPA (card present) switching
//   - Glitch filtering (uidLen==-2 means ATQA received but anti-col failed)
//   - Removal detection after _removalThreshold consecutive fails
//   - RF recovery after _rfRecoveryInterval consecutive fails
//
// Parameters:
//   uid    - buffer for UID (min 8 bytes)
//   uidLen - filled with UID length (4 or 7) on valid read
//
// Returns CardEvent:
//   CARD_EVENT_NONE    - no card, state unchanged (was absent)
//   CARD_EVENT_NEW     - new card detected (transition absent->present)
//   CARD_EVENT_CHANGED - different card (UID changed while present)
//   CARD_EVENT_PRESENT - same card still there
//   CARD_EVENT_REMOVED - card removed (transition present->absent)
// ============================================================
CardEvent PN5180ISO14443::pollCard(uint8_t *uid, uint8_t *uidLen) {
	// Choose REQA or WUPA based on current state
	int8_t result;
	if (_cardState == 1) {
		result = readCardSerial_wupa(uid);
	} else {
		result = readCardSerial(uid);
	}

	bool validRead = (result == 4 || result == 7);
	bool cardPresentButGlitch = (result == -2);

	if (validRead) {
		// ---- CARD DETECTED ----
		_consecutiveFails = 0;
		uint8_t len = (result <= 7) ? result : 7;
		*uidLen = len;

		if (_cardState == 0) {
			// Transition: ABSENT -> PRESENT (new card)
			_cardState = 1;
			memcpy(_lastUID, uid, len);
			_lastUIDLen = len;
			return CARD_EVENT_NEW;
		} else {
			// Already present - check if same card
			bool same = (len == _lastUIDLen);
			if (same) {
				for (uint8_t i = 0; i < len; i++) {
					if (uid[i] != _lastUID[i]) { same = false; break; }
				}
			}
			if (!same) {
				// Different card
				memcpy(_lastUID, uid, len);
				_lastUIDLen = len;
				return CARD_EVENT_CHANGED;
			}
			return CARD_EVENT_PRESENT;
		}

	} else if (cardPresentButGlitch) {
		// ---- ATQA received but anti-collision failed ----
		// Card is physically present! Don't count as fail.
		if (_consecutiveFails > 0) _consecutiveFails = 0;

		if (_cardState == 1) {
			// Card still there, copy last known UID
			*uidLen = _lastUIDLen;
			memcpy(uid, _lastUID, _lastUIDLen);
			return CARD_EVENT_PRESENT;
		} else {
			// Edge case: glitch but we thought absent
			*uidLen = 0;
			return CARD_EVENT_NONE;
		}

	} else {
		// ---- NO CARD (result == 0 or -1) ----
		_consecutiveFails++;
		*uidLen = 0;

		if (_cardState == 1) {
			if (_consecutiveFails >= _removalThreshold) {
				// Enough consecutive fails -> card removed
				_cardState = 0;
				return CARD_EVENT_REMOVED;
			}
			// Not yet sure - might be a flicker, report as still present
			*uidLen = _lastUIDLen;
			memcpy(uid, _lastUID, _lastUIDLen);
			return CARD_EVENT_PRESENT;
		}

		// RF recovery after many consecutive fails
		if (_rfRecoveryInterval > 0 && _consecutiveFails > 0 
		    && (_consecutiveFails % _rfRecoveryInterval) == 0) {
			reset();
			setupRF();
		}

		return CARD_EVENT_NONE;
	}
}

// ============================================================
// readCardNumber() - Read 8-digit ASCII number from card memory
//
// Supports: NTAG213, NTAG215, NTAG216, MIFARE Ultralight
// Expected layout:
//   NTAG:       number in Pages 6-7 (bytes 24-31 from start)
//   Ultralight: number in Pages 7-8 (bytes 28-35 from start)
//
// Strategy 1: Check expected position for 8 ASCII digits
// Strategy 2: Scan all read pages for any 8 consecutive ASCII digits
//
// Parameters:
//   number - buffer for result (min 9 bytes), null-terminated
//
// Returns true if 8-digit number found
// ============================================================
bool PN5180ISO14443::readCardNumber(char *number) {
	uint8_t pageData[16];
	number[0] = '\0';

	// Read Pages 0-3: UID + Capability Container
	if (!mifareBlockRead(0, pageData)) {
		return false;
	}

	// Page 3, Byte 2 (= pageData[14]) = CC Memory Size byte
	uint8_t ccMemSize = pageData[14];
	bool isUltralight = (ccMemSize == 0x06);

	// Read Pages 4-7 (NTAG number expected in Page 6-7)
	uint8_t block1[16];
	if (!mifareBlockRead(4, block1)) {
		return false;
	}

	// For Ultralight: also read Pages 8-11 (number spans Page 7-8)
	uint8_t block2[16];
	bool hasBlock2 = false;
	if (isUltralight) {
		hasBlock2 = mifareBlockRead(8, block2);
	}

	// Strategy 1: Check expected position
	bool found = false;

	if (!isUltralight) {
		// NTAG: Page 6-7 = block1 bytes [8..15]
		bool allDigits = true;
		for (int i = 8; i < 16; i++) {
			if (block1[i] < '0' || block1[i] > '9') { allDigits = false; break; }
		}
		if (allDigits) {
			memcpy(number, &block1[8], 8);
			number[8] = '\0';
			found = true;
		}
	} else if (hasBlock2) {
		// Ultralight: Page 7 = block1[12..15], Page 8 = block2[0..3]
		uint8_t candidate[8];
		memcpy(&candidate[0], &block1[12], 4);
		memcpy(&candidate[4], &block2[0], 4);
		bool allDigits = true;
		for (int i = 0; i < 8; i++) {
			if (candidate[i] < '0' || candidate[i] > '9') { allDigits = false; break; }
		}
		if (allDigits) {
			memcpy(number, candidate, 8);
			number[8] = '\0';
			found = true;
		}
	}

	// Strategy 2: Fallback scan for 8 consecutive ASCII digits
	if (!found) {
		for (int i = 0; i <= 8; i++) {
			bool allDigits = true;
			for (int j = 0; j < 8; j++) {
				if (block1[i+j] < '0' || block1[i+j] > '9') { allDigits = false; break; }
			}
			if (allDigits) {
				memcpy(number, &block1[i], 8);
				number[8] = '\0';
				found = true;
				break;
			}
		}
	}
	if (!found && hasBlock2) {
		// Search across block boundary
		uint8_t combined[32];
		memcpy(combined, block1, 16);
		memcpy(combined + 16, block2, 16);
		for (int i = 0; i <= 24; i++) {
			bool allDigits = true;
			for (int j = 0; j < 8; j++) {
				if (combined[i+j] < '0' || combined[i+j] > '9') { allDigits = false; break; }
			}
			if (allDigits) {
				memcpy(number, &combined[i], 8);
				number[8] = '\0';
				found = true;
				break;
			}
		}
	}

	return found;
}
