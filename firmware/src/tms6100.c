/************************************************************************
    tms6100.c

    TMS6100 Voice Synthesis Memory (VSM) emulation

    Based on the TMS6100-Emulator by Simon Inns
    (https://github.com/simoninns/TMS6100-Emulator)

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

// The TMS6100 was mask programmed for either 1-bit or 4-bit data transfer.
// The TMS5220 VSP only supports 1-bit mode, so 4-bit data is not emulated.
//
// The emulation runs in a tight polling loop on core 1 from SRAM:
//
//  M1 rising          - Load Address: the nibble on ADD8,4,2,1 is shifted
//                       into the top of the 20-bit address register
//  M0 rising (first)  - Dummy Read following a Load Address: fetch the
//                       addressed byte and start driving ADD8
//  M0 rising          - Read: drive the next bit (LSB first) onto ADD8
//  M0 and M1 rising   - Read and Branch: the two bytes at the current
//                       address replace the low 14 bits of the address

#include <stdbool.h>
#include <stdint.h>

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/m33.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "pins.h"
#include "romdata_acorn.h"
#include "tms6100.h"

// Time to let the M0/M1 inputs settle after an edge before deciding which
// function was requested (M0 and M1 rise together for Read and Branch)
#define EDGE_SETTLE_CYCLES  30   // ~200ns at 150MHz

// Time to let ADD8 settle after releasing our ADD8 output.  The address is
// set up 2.5us before M1 and held for 5us after, so ~1us is safe.
#define ADD8_RELEASE_CYCLES 150  // ~1us at 150MHz

#define M0_MASK (1u << PIN_M0)
#define M1_MASK (1u << PIN_M1)

static volatile tms6100Stats_t stats;

#ifdef TMS6100_TRACE
// Optional bus trace (GPIO8-19 changes with timestamps) for debugging
#define TRACE_MASK  (0xFFFu << 8)
#define TRACE_SIZE  16384
volatile uint32_t tms6100TraceTime[TRACE_SIZE];
volatile uint32_t tms6100TraceValue[TRACE_SIZE];
volatile uint32_t tms6100TraceCount;
volatile uint32_t tms6100Heartbeat;

static inline void __not_in_flash_func(traceRecord)(uint32_t value)
{
    uint32_t i = tms6100TraceCount;
    if (i < TRACE_SIZE) {
        tms6100TraceTime[i] = m33_hw->dwt_cyccnt;
        tms6100TraceValue[i] = value;
        tms6100TraceCount = i + 1;
    }
}
#endif

// Emulation state (only touched by core 1)
static uint32_t currentAddress;
static bool m0ReadyReceived;
static uint8_t outputBuffer;
static uint8_t outputBufferPointer;
static bool outputEnabled;

static inline bool __not_in_flash_func(isOurBank)(uint32_t address)
{
    return ((address & 0x3C000) >> 14) == PHROM_BANK;
}

static inline void __not_in_flash_func(setOutputEnabled)(bool enabled)
{
    if (enabled == outputEnabled) return;

    if (enabled) {
        // The original TMS6100 drives ADD8 high when it starts driving
        sio_hw->gpio_set = 1u << PIN_ADD8_OUT;
        sio_hw->gpio_clr = 1u << PIN_ADD8_OE_N;
    } else {
        sio_hw->gpio_set = 1u << PIN_ADD8_OE_N;
    }
    outputEnabled = enabled;
}

// Load the byte at the current address into the output buffer, driving
// ADD8 only if the address falls within our bank
static void __not_in_flash_func(loadOutputBuffer)(void)
{
    if (isOurBank(currentAddress)) {
        outputBuffer = phromData[currentAddress & 0x3FFF];
        setOutputEnabled(true);
    } else {
        outputBuffer = 0x00;
        setOutputEnabled(false);
    }
    outputBufferPointer = 0;
}

static void __not_in_flash_func(loadAddress)(void)
{
    // Stop driving ADD8 so that the TMS5220 can drive the address nibble
    // onto it, then give the line time to settle (ADD8 out has 1K in series)
    setOutputEnabled(false);
    busy_wait_at_least_cycles(ADD8_RELEASE_CYCLES);

    uint32_t nibble = (sio_hw->gpio_in >> PIN_ADD1) & 0x0F;

    // The 20-bit address is loaded as 5 nibbles, least significant first
    currentAddress = (currentAddress >> 4) | (nibble << 16);
    m0ReadyReceived = false;

    stats.loadAddressCount++;
    stats.address = currentAddress;
}

static void __not_in_flash_func(readData)(void)
{
    if (!m0ReadyReceived) {
        // Dummy read following a Load Address
        m0ReadyReceived = true;
        loadOutputBuffer();
    } else {
        // Present the next bit (LSB first); the TMS5220 samples it before
        // the end of the M0 pulse
        if (isOurBank(currentAddress)) {
            if (outputBuffer & (1u << outputBufferPointer)) {
                sio_hw->gpio_set = 1u << PIN_ADD8_OUT;
            } else {
                sio_hw->gpio_clr = 1u << PIN_ADD8_OUT;
            }
        }
        outputBufferPointer++;

        if (outputBufferPointer == 8) {
            currentAddress = (currentAddress + 1) & 0xFFFFF;
            loadOutputBuffer();
        }
    }

    stats.readCount++;
}

static void __not_in_flash_func(readAndBranch)(void)
{
    // Two bytes at the current address form the new 14-bit address
    uint32_t base = currentAddress & 0x3FFF;
    uint32_t target = phromData[base] | (phromData[(base + 1) & 0x3FFF] << 8);
    currentAddress = (currentAddress & ~0x3FFFu) | (target & 0x3FFF);

    // No dummy read is required after a Read and Branch
    m0ReadyReceived = true;
    loadOutputBuffer();

    stats.readAndBranchCount++;
    stats.address = currentAddress;
}

static void __not_in_flash_func(tms6100Loop)(void)
{
    uint32_t previous = sio_hw->gpio_in;

#ifdef TMS6100_TRACE
    // Enable this core's cycle counter for trace timestamps
    m33_hw->demcr |= M33_DEMCR_TRCENA_BITS;
    m33_hw->dwt_ctrl |= M33_DWT_CTRL_CYCCNTENA_BITS;
#endif

    while (true) {
        uint32_t now = sio_hw->gpio_in;
#ifdef TMS6100_TRACE
        tms6100Heartbeat++;
        if ((now ^ previous) & TRACE_MASK) traceRecord(now);
#endif
        uint32_t rising = now & ~previous;
        previous = now;

        if (!(rising & (M0_MASK | M1_MASK))) continue;

        // Allow both control lines to settle, then decode the function
        busy_wait_at_least_cycles(EDGE_SETTLE_CYCLES);
        now = sio_hw->gpio_in;
#ifdef TMS6100_TRACE
        traceRecord(now | (1u << 31));
#endif

        bool m0 = now & M0_MASK;
        bool m1 = now & M1_MASK;

        if (m0 && m1) readAndBranch();
        else if (m1) loadAddress();
        else if (m0) readData();

        previous = now;
    }
}

void tms6100Start(void)
{
    // Inputs from the TMS5220 (driven by U3, so no pulls are required)
    const uint inputs[] = { PIN_ADD1, PIN_ADD2, PIN_ADD4, PIN_ADD8, PIN_M0, PIN_M1 };
    for (uint i = 0; i < count_of(inputs); i++) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_disable_pulls(inputs[i]);
    }

    // ADD8 output buffer, initially disabled
    gpio_init(PIN_ADD8_OE_N);
    gpio_put(PIN_ADD8_OE_N, 1);
    gpio_set_dir(PIN_ADD8_OE_N, GPIO_OUT);

    gpio_init(PIN_ADD8_OUT);
    gpio_put(PIN_ADD8_OUT, 1);
    gpio_set_dir(PIN_ADD8_OUT, GPIO_OUT);

    currentAddress = 0;
    m0ReadyReceived = false;
    outputBuffer = 0xFF;
    outputBufferPointer = 0;
    outputEnabled = false;

    multicore_launch_core1(tms6100Loop);
}

void tms6100GetStats(tms6100Stats_t *s)
{
    s->loadAddressCount = stats.loadAddressCount;
    s->readCount = stats.readCount;
    s->readAndBranchCount = stats.readAndBranchCount;
    s->address = stats.address;
}
