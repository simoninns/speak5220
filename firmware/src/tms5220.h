/************************************************************************
    tms5220.h

    TMS5220 Voice Synthesis Processor (VSP) host interface

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#ifndef TMS5220_H_
#define TMS5220_H_

#include <stdbool.h>
#include <stdint.h>

// Commands (D0-D7 with D0 as MSB, so these are normal byte values)
#define TMS5220_CMD_RESET           0x70
#define TMS5220_CMD_SPEAK           0x50
#define TMS5220_CMD_LOAD_ADDRESS    0x40  // Low nibble is the address nibble
#define TMS5220_CMD_READ_BYTE       0x10

// Status register bits
#define TMS5220_STATUS_TS           0x80  // Talk Status
#define TMS5220_STATUS_BL           0x40  // Buffer Low
#define TMS5220_STATUS_BE           0x20  // Buffer Empty

void tms5220Init(void);

// Write a byte to the VSP; returns false if READY never asserted
bool tms5220Write(uint8_t data);

// Read the status register (or data register after Read Byte);
// returns false if READY never asserted
bool tms5220Read(uint8_t *data);

// Put the VSP into a known state (as recommended by the datasheet)
bool tms5220Reset(void);

// Load a 20-bit VSM address (as 5 nibbles) and start speaking from it
bool tms5220Speak(uint32_t vsmAddress);

#endif /* TMS5220_H_ */
