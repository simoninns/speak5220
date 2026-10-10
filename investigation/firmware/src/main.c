/************************************************************************
    main.c

    speak5220 capture firmware for the Raspberry Pi Pico 2

    Clocks the TMS5220, emulates its speech PHROMs (TMS6100) on core 1 and
    records the VSM trace there, and runs the host's commands over USB
    serial on core 0 (command.c, protocol in docs/technical/hardware/firmware-protocol.md).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "command.h"
#include "pins.h"
#include "tms5220.h"
#include "tms6100.h"

// 144MHz is an exact multiple of the 640kHz TMS5220 clock (x225) and can
// be generated exactly from the 12MHz crystal (VCO 1440MHz / 5 / 2)
#define SYS_CLOCK_KHZ       144000

int main(void)
{
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);
    stdio_init_all();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    // PicoScope trigger: the host's commands raise it around each utterance
    gpio_init(PIN_SCOPE_TRIG);
    gpio_set_dir(PIN_SCOPE_TRIG, GPIO_OUT);

    tms6100Start();
    tms5220Init();
    commandSetClock(tms5220ClockStart(TMS5220_CLOCK_HZ));

    // Allow the TMS5220 power-up clear (15ms) to complete
    sleep_ms(100);

    commandLoop();
}
