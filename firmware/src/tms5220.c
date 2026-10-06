/************************************************************************
    tms5220.c

    TMS5220 Voice Synthesis Processor (VSP) host interface

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include "pins.h"
#include "tms5220.h"

// READY goes inactive (high) once the VSP has seen WS/RS go low, then
// active (low) when the transfer completes.  The VSP samples the strobes
// with its own (slow) clock, so READY can take several microseconds to go
// high; the strobe must be held until that has happened.  Commands may hold
// READY high until the previous command has finished executing, so allow
// plenty of time for the transfer to complete.
//
// While a previous command is still executing, READY can go high and back
// low again within ~15us, so interrupts are disabled during the handshake
// to avoid missing the acknowledgement.
#define READY_ACK_TIMEOUT_US    1000
#define READY_TIMEOUT_US        50000

// The VSP samples WS/RS with its own clock (ROMCLK period ~5-6us), so a
// strobe that is inactive for only a few microseconds between cycles may not
// be seen to end; the next cycle is then acknowledged but its data is lost.
// Keep the strobes inactive for several VSP clock periods between cycles.
#define STROBE_RECOVERY_US      20

static absolute_time_t strobeRecoveryEnd;

static void busRelease(void)
{
    // Disable U4 before changing direction to avoid bus contention
    gpio_put(PIN_BUS_OE_N, 1);
    gpio_set_dir_in_masked(PIN_DATA_MASK);
    gpio_put(PIN_BUS_DIR, 1);
}

static bool waitReady(void)
{
    // Wait for the VSP to acknowledge the strobe (READY inactive)
    absolute_time_t timeout = make_timeout_time_us(READY_ACK_TIMEOUT_US);
    while (!gpio_get(PIN_READY_N)) {
        if (time_reached(timeout)) return false;
    }

    // Wait for the transfer to complete (READY active)
    timeout = make_timeout_time_us(READY_TIMEOUT_US);
    while (gpio_get(PIN_READY_N)) {
        if (time_reached(timeout)) return false;
    }
    return true;
}

void tms5220Init(void)
{
    // Strobes inactive (high)
    gpio_init(PIN_WS_N);
    gpio_put(PIN_WS_N, 1);
    gpio_set_dir(PIN_WS_N, GPIO_OUT);

    gpio_init(PIN_RS_N);
    gpio_put(PIN_RS_N, 1);
    gpio_set_dir(PIN_RS_N, GPIO_OUT);

    // Data bus buffer disabled, direction TMS5220 -> Pico
    gpio_init(PIN_BUS_OE_N);
    gpio_put(PIN_BUS_OE_N, 1);
    gpio_set_dir(PIN_BUS_OE_N, GPIO_OUT);

    gpio_init(PIN_BUS_DIR);
    gpio_put(PIN_BUS_DIR, 1);
    gpio_set_dir(PIN_BUS_DIR, GPIO_OUT);

    gpio_init_mask(PIN_DATA_MASK);
    gpio_set_dir_in_masked(PIN_DATA_MASK);
    for (uint pin = PIN_DATA_BASE; pin < PIN_DATA_BASE + 8; pin++) {
        gpio_disable_pulls(pin);
    }

    // Status inputs (driven by U3)
    gpio_init(PIN_READY_N);
    gpio_set_dir(PIN_READY_N, GPIO_IN);
    gpio_disable_pulls(PIN_READY_N);

    gpio_init(PIN_INT_N);
    gpio_set_dir(PIN_INT_N, GPIO_IN);
    gpio_disable_pulls(PIN_INT_N);
}

bool tms5220Write(uint8_t data)
{
    busy_wait_until(strobeRecoveryEnd);

    // Drive the byte through U4 towards the TMS5220
    gpio_put(PIN_BUS_OE_N, 1);
    gpio_put(PIN_BUS_DIR, 0);
    gpio_put_masked(PIN_DATA_MASK, (uint32_t)data << PIN_DATA_BASE);
    gpio_set_dir_out_masked(PIN_DATA_MASK);
    gpio_put(PIN_BUS_OE_N, 0);
    busy_wait_us_32(1);

    uint32_t interrupts = save_and_disable_interrupts();
    gpio_put(PIN_WS_N, 0);
    bool ready = waitReady();
    gpio_put(PIN_WS_N, 1);
    restore_interrupts(interrupts);
    strobeRecoveryEnd = make_timeout_time_us(STROBE_RECOVERY_US);

    busy_wait_us_32(1);
    busRelease();

    return ready;
}

bool tms5220Read(uint8_t *data)
{
    busy_wait_until(strobeRecoveryEnd);

    busRelease();
    gpio_put(PIN_BUS_OE_N, 0);

    uint32_t interrupts = save_and_disable_interrupts();
    gpio_put(PIN_RS_N, 0);
    bool ready = waitReady();
    busy_wait_us_32(1);
    *data = (uint8_t)(gpio_get_all() >> PIN_DATA_BASE);
    gpio_put(PIN_RS_N, 1);
    restore_interrupts(interrupts);
    strobeRecoveryEnd = make_timeout_time_us(STROBE_RECOVERY_US);

    busy_wait_us_32(1);
    gpio_put(PIN_BUS_OE_N, 1);

    return ready;
}

bool tms5220Reset(void)
{
    // Datasheet section 7: writing nine bytes of all ones followed by a
    // Reset command guarantees the VSP is in a clear state (this
    // terminates a Speak External command if one is in progress)
    bool ok = true;
    for (int i = 0; i < 9; i++) ok &= tms5220Write(0xFF);
    ok &= tms5220Write(TMS5220_CMD_RESET);

    // Reading the status clears any pending interrupt
    uint8_t status;
    ok &= tms5220Read(&status);
    return ok;
}

bool tms5220Speak(uint32_t vsmAddress)
{
    bool ok = true;

    // Five Load Address commands, least significant nibble first
    for (int i = 0; i < 5; i++) {
        ok &= tms5220Write(TMS5220_CMD_LOAD_ADDRESS | ((vsmAddress >> (4 * i)) & 0x0F));
    }
    ok &= tms5220Write(TMS5220_CMD_SPEAK);

    return ok;
}
