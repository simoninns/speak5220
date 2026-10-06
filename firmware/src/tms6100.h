/************************************************************************
    tms6100.h

    TMS6100 Voice Synthesis Memory (VSM) emulation and VSM trace

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#ifndef TMS6100_H_
#define TMS6100_H_

#include <stdbool.h>
#include <stdint.h>

#include "pico/types.h"

// Each emulated TMS6100 holds 16K bytes and answers to one of 16 banks
// (address bits 14-17).  Up to TMS6100_SLOTS banks can be loaded at once.
#define TMS6100_BANK_SIZE   16384
#define TMS6100_BANKS       16
#define TMS6100_SLOTS       4

// The VSM trace holds this many events (8 bytes each)
#define TMS6100_TRACE_EVENTS    32768

// Trace event: a timestamp in core 1 clock cycles (it wraps after 2^32
// cycles; a TIME event is recorded whenever 2^30 cycles pass without
// another event, so the host can unwrap it) and an event word whose top
// four bits give the type.  firmware/PROTOCOL.md describes the fields.
typedef struct {
    uint32_t time;
    uint32_t event;
} tms6100Event_t;

#define TMS6100_EVENT_TIME          0x0u  // No payload
#define TMS6100_EVENT_LOAD_ADDRESS  0x1u  // 0-19 address after the load, 20-23 nibble
#define TMS6100_EVENT_DUMMY_READ    0x2u  // 0-19 address, 20 driven
#define TMS6100_EVENT_READ          0x3u  // 0-19 byte address, 20-22 bit number,
                                          // 23 bit value, 24 driven
#define TMS6100_EVENT_READ_BRANCH   0x4u  // 0-19 address after the branch, 20 driven
#define TMS6100_EVENT_PINS          0x5u  // A host interface line changed: 0-7 data
                                          // bus, 8 WS, 9 RS, 10 READY, 11 INT,
                                          // 12 SCOPE_TRIG (as on the pins)
#define TMS6100_EVENT_START         0x6u  // First event after a clear: pins as PINS

// Activity counters (updated by the emulation running on core 1)
typedef struct {
    uint32_t loadAddressCount;   // M1 pulses
    uint32_t readCount;          // M0 pulses
    uint32_t readAndBranchCount; // M0 + M1 pulses
    uint32_t address;            // Current 20-bit address register
} tms6100Stats_t;

// Configure the GPIOs, load the built-in Acorn PHROM into its bank and
// start the emulation on core 1
void tms6100Start(void);

// Take a snapshot of the emulation activity counters
void tms6100GetStats(tms6100Stats_t *stats);

// Bank images.  tms6100BankWrite unmaps the bank and returns a free (or
// its existing) slot to fill, or NULL if every slot is in use;
// tms6100BankMap then makes it visible to the TMS5220.  Only change banks
// while the TMS5220 is not reading from the VSM.
uint8_t *tms6100BankWrite(uint bank);
void tms6100BankMap(uint bank);
void tms6100BankUnload(uint bank);
const uint8_t *tms6100Bank(uint bank);  // NULL if the bank is empty
uint tms6100BuiltinBank(void);

// VSM trace
void tms6100TraceClear(void);
uint32_t tms6100TraceCount(uint32_t *dropped);
const tms6100Event_t *tms6100TraceEvents(void);

#endif /* TMS6100_H_ */
