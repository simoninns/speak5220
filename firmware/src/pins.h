/************************************************************************
    pins.h

    Raspberry Pi Pico 2 GPIO assignments for the speak5220 board
    (see kicad/tms5220_usb/pico_interface.kicad_sch)

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#ifndef PINS_H_
#define PINS_H_

// TMS5220 data bus via U4 (74LVC4245A)
//
// TI numbers the TMS5220 data bus with D0 as the MSB, so the bus is
// wired with TMS5220 D7 (LSB) on GPIO0 through to D0 (MSB) on GPIO7.
// A byte value can therefore be written to GPIO0-7 without reordering.
#define PIN_DATA_BASE   0
#define PIN_DATA_MASK   (0xFFu << PIN_DATA_BASE)

// VSM (TMS6100) interface inputs via U3 (74LVC4245A, fixed 5V -> 3V3)
#define PIN_ADD1        8
#define PIN_ADD2        9
#define PIN_ADD4        10
#define PIN_ADD8        11
#define PIN_M0          12
#define PIN_M1          13

// ADD8/DATA output to the TMS5220 via U5 (74AHCT125) and R23
#define PIN_ADD8_OUT    14
#define PIN_ADD8_OE_N   15  // Active low, pulled up (disabled) by R19

// TMS5220 CPU interface
#define PIN_WS_N        16  // Via U5, pulled up by R20
#define PIN_RS_N        17  // Via U5, pulled up by R21
#define PIN_READY_N     18  // Via U3 (input)
#define PIN_INT_N       19  // Via U3 (input)
#define PIN_BUS_DIR     20  // U4 DIR: 1 = TMS5220 -> Pico, 0 = Pico -> TMS5220
#define PIN_BUS_OE_N    21  // U4 ~OE, pulled up (disabled) by R22

// On-board LED of the Pico 2
#define PIN_LED         25

#endif /* PINS_H_ */
