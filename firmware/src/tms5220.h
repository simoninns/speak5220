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
#define TMS5220_CMD_SPEAK_EXTERNAL  0x60

// Status register bits
#define TMS5220_STATUS_TS           0x80  // Talk Status
#define TMS5220_STATUS_BL           0x40  // Buffer Low
#define TMS5220_STATUS_BE           0x20  // Buffer Empty

// OSC frequency: 640kHz gives ROMCLK = 160kHz and an 8kHz sample rate.
// OSC must swing between the TMS5220's supply rails (-5V to +5V), so the
// PWM output needs a level shifter (as on the Atari Star Wars sound PCB).
#define TMS5220_CLOCK_HZ            640000

// Start the OSC clock on PIN_OSC_CLK at the nearest frequency the system
// clock allows; returns the actual frequency in Hz (0 if out of range).
// The TMS5220 does nothing (including its power-up clear) until this runs.
uint32_t tms5220ClockStart(uint32_t hz);

// Stop the OSC clock, holding PIN_OSC_CLK low (so OSC sits at +5V)
void tms5220ClockStop(void);

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
