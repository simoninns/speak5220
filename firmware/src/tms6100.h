/************************************************************************
    tms6100.h

    TMS6100 Voice Synthesis Memory (VSM) emulation

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#ifndef TMS6100_H_
#define TMS6100_H_

#include <stdint.h>

// Activity counters (updated by the emulation running on core 1)
typedef struct {
    uint32_t loadAddressCount;   // M1 pulses
    uint32_t readCount;          // M0 pulses
    uint32_t readAndBranchCount; // M0 + M1 pulses
    uint32_t address;            // Current 20-bit address register
} tms6100Stats_t;

// Configure the GPIOs and start the emulation on core 1
void tms6100Start(void);

// Take a snapshot of the emulation activity counters
void tms6100GetStats(tms6100Stats_t *stats);

#endif /* TMS6100_H_ */
