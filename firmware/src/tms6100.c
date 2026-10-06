/************************************************************************
    tms6100.c

    TMS6100 Voice Synthesis Memory (VSM) emulation and VSM trace

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
//
// Each emulated TMS6100 answers to the bank (address bits 14-17) its image
// was loaded into.  Address bits 18-19 are ignored, as on the real chip.
//
// The same loop records the VSM trace: every function above with the bit
// returned, and every change on the host interface lines (WS, RS, READY,
// INT and SCOPE_TRIG, with the data bus), timestamped with core 1's cycle
// counter.  An event is timestamped when the loop sees the edge, which can
// be up to ~1.5us late if the loop was busy with the previous event.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/m33.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "pins.h"
#include "romdata_acorn.h"
#include "tms6100.h"

// Time to let the M0/M1 inputs settle after an edge before deciding which
// function was requested (M0 and M1 rise together for Read and Branch)
#define EDGE_SETTLE_CYCLES  30   // ~210ns at 144MHz

// Time to let ADD8 settle after releasing our ADD8 output.  The address is
// set up 2.5us before M1 and held for 5us after, so ~1us is safe.
#define ADD8_RELEASE_CYCLES 150  // ~1us at 144MHz

// A TIME event is recorded after this many cycles without another event,
// so that the host can unwrap the 32-bit timestamps
#define KEEPALIVE_CYCLES    (1u << 30)

#define M0_MASK (1u << PIN_M0)
#define M1_MASK (1u << PIN_M1)

// WS, RS, READY and INT must be adjacent for pinsPayload()
static_assert(PIN_RS_N == PIN_WS_N + 1 && PIN_READY_N == PIN_WS_N + 2 &&
              PIN_INT_N == PIN_WS_N + 3, "host interface pins are not adjacent");
#define HOST_MASK   ((0xFu << PIN_WS_N) | (1u << PIN_SCOPE_TRIG))
#define WATCH_MASK  (M0_MASK | M1_MASK | HOST_MASK)

#define EVENT(type, payload)    (((type) << 28) | (payload))

static volatile tms6100Stats_t stats;

// Bank images (written by core 0, read by core 1)
static uint8_t slots[TMS6100_SLOTS][TMS6100_BANK_SIZE];
static int8_t slotBank[TMS6100_SLOTS];          // Bank held by each slot, or -1
static const uint8_t *volatile bankData[TMS6100_BANKS];

// VSM trace (written by core 1; core 0 reads the events below the count)
static tms6100Event_t trace[TMS6100_TRACE_EVENTS];
static volatile uint32_t traceCount;
static volatile uint32_t traceDropped;
static volatile bool traceClearRequest;
static uint32_t lastEventTime;

// Emulation state (only touched by core 1)
static uint32_t currentAddress;
static bool m0ReadyReceived;
static uint8_t outputBuffer;
static uint8_t outputBufferPointer;
static bool outputEnabled;

static inline void __not_in_flash_func(traceRecord)(uint32_t time, uint32_t event)
{
    uint32_t i = traceCount;
    if (i < TMS6100_TRACE_EVENTS) {
        trace[i].time = time;
        trace[i].event = event;
        __dmb();
        traceCount = i + 1;
    } else {
        traceDropped++;
    }
    lastEventTime = time;
}

static inline uint32_t __not_in_flash_func(pinsPayload)(uint32_t gpio)
{
    return ((gpio >> PIN_DATA_BASE) & 0xFF) | (((gpio >> PIN_WS_N) & 0xF) << 8) |
           (((gpio >> PIN_SCOPE_TRIG) & 1) << 12);
}

static inline const uint8_t *__not_in_flash_func(currentBank)(void)
{
    return bankData[(currentAddress >> 14) & 0xF];
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
// ADD8 only if a bank image is loaded for the address
static void __not_in_flash_func(loadOutputBuffer)(void)
{
    const uint8_t *data = currentBank();
    if (data) {
        outputBuffer = data[currentAddress & 0x3FFF];
        setOutputEnabled(true);
    } else {
        outputBuffer = 0x00;
        setOutputEnabled(false);
    }
    outputBufferPointer = 0;
}

static void __not_in_flash_func(loadAddress)(uint32_t time)
{
    // Stop driving ADD8 so that the TMS5220 can drive the address nibble
    // onto it, then give the line time to settle (ADD8 out has 1K in series)
    setOutputEnabled(false);
    busy_wait_at_least_cycles(ADD8_RELEASE_CYCLES);

    uint32_t nibble = (sio_hw->gpio_in >> PIN_ADD1) & 0x0F;

    // The 20-bit address is loaded as 5 nibbles, least significant first
    currentAddress = (currentAddress >> 4) | (nibble << 16);
    m0ReadyReceived = false;

    traceRecord(time, EVENT(TMS6100_EVENT_LOAD_ADDRESS, currentAddress | (nibble << 20)));
    stats.loadAddressCount++;
    stats.address = currentAddress;
}

static void __not_in_flash_func(readData)(uint32_t time)
{
    if (!m0ReadyReceived) {
        // Dummy read following a Load Address
        m0ReadyReceived = true;
        loadOutputBuffer();
        traceRecord(time, EVENT(TMS6100_EVENT_DUMMY_READ,
                                currentAddress | ((uint32_t)outputEnabled << 20)));
    } else {
        // Present the next bit (LSB first); the TMS5220 samples it before
        // the end of the M0 pulse
        uint32_t bit = (outputBuffer >> outputBufferPointer) & 1;
        if (outputEnabled) {
            if (bit) {
                sio_hw->gpio_set = 1u << PIN_ADD8_OUT;
            } else {
                sio_hw->gpio_clr = 1u << PIN_ADD8_OUT;
            }
        }
        traceRecord(time, EVENT(TMS6100_EVENT_READ,
                                currentAddress | ((uint32_t)outputBufferPointer << 20) |
                                (bit << 23) | ((uint32_t)outputEnabled << 24)));
        outputBufferPointer++;

        if (outputBufferPointer == 8) {
            currentAddress = (currentAddress + 1) & 0xFFFFF;
            loadOutputBuffer();
        }
    }

    stats.readCount++;
}

static void __not_in_flash_func(readAndBranch)(uint32_t time)
{
    // Two bytes at the current address form the new 14-bit address.  With
    // no image loaded for the bank, nothing drives the data, so read zero.
    const uint8_t *data = currentBank();
    uint32_t base = currentAddress & 0x3FFF;
    uint32_t target = data ? data[base] | (data[(base + 1) & 0x3FFF] << 8) : 0;
    currentAddress = (currentAddress & ~0x3FFFu) | (target & 0x3FFF);

    // No dummy read is required after a Read and Branch
    m0ReadyReceived = true;
    loadOutputBuffer();

    traceRecord(time, EVENT(TMS6100_EVENT_READ_BRANCH,
                            currentAddress | ((uint32_t)(data != NULL) << 20)));
    stats.readAndBranchCount++;
    stats.address = currentAddress;
}

static void __not_in_flash_func(tms6100Loop)(void)
{
    // Enable this core's cycle counter for trace timestamps
    m33_hw->demcr |= M33_DEMCR_TRCENA_BITS;
    m33_hw->dwt_ctrl |= M33_DWT_CTRL_CYCCNTENA_BITS;

    uint32_t previous = sio_hw->gpio_in;
    lastEventTime = m33_hw->dwt_cyccnt;

    while (true) {
        uint32_t now = sio_hw->gpio_in;
        uint32_t time = m33_hw->dwt_cyccnt;
        uint32_t changed = now ^ previous;

        if (!(changed & WATCH_MASK)) {
            if (traceClearRequest) {
                traceCount = 0;
                traceDropped = 0;
                traceRecord(time, EVENT(TMS6100_EVENT_START, pinsPayload(now)));
                traceClearRequest = false;
            } else if (time - lastEventTime >= KEEPALIVE_CYCLES) {
                traceRecord(time, EVENT(TMS6100_EVENT_TIME, 0));
            }
            continue;
        }

        if (changed & HOST_MASK) traceRecord(time, EVENT(TMS6100_EVENT_PINS, pinsPayload(now)));

        uint32_t rising = now & ~previous;
        previous = now;
        if (!(rising & (M0_MASK | M1_MASK))) continue;

        // Allow both control lines to settle, then decode the function
        busy_wait_at_least_cycles(EDGE_SETTLE_CYCLES);
        uint32_t settled = sio_hw->gpio_in;

        bool m0 = settled & M0_MASK;
        bool m1 = settled & M1_MASK;

        if (m0 && m1) readAndBranch(time);
        else if (m1) loadAddress(time);
        else if (m0) readData(time);

        // Take M0 and M1 from after the settling time, so that the second of
        // two edges for Read and Branch is not seen again.  Host lines that
        // changed meanwhile are still seen as changed on the next pass.
        previous = (settled & (M0_MASK | M1_MASK)) | (previous & ~(M0_MASK | M1_MASK));
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

    // The built-in Acorn PHROM answers to its usual bank until replaced
    memset(slotBank, -1, sizeof(slotBank));
    memcpy(tms6100BankWrite(PHROM_BANK), phromData, TMS6100_BANK_SIZE);
    tms6100BankMap(PHROM_BANK);

    traceClearRequest = true;
    multicore_launch_core1(tms6100Loop);
}

void tms6100GetStats(tms6100Stats_t *s)
{
    s->loadAddressCount = stats.loadAddressCount;
    s->readCount = stats.readCount;
    s->readAndBranchCount = stats.readAndBranchCount;
    s->address = stats.address;
}

uint8_t *tms6100BankWrite(uint bank)
{
    int free = -1;
    for (int i = 0; i < TMS6100_SLOTS; i++) {
        if (slotBank[i] == (int8_t)bank) {
            free = i;
            break;
        }
        if (slotBank[i] < 0 && free < 0) free = i;
    }
    if (free < 0) return NULL;

    bankData[bank] = NULL;
    slotBank[free] = (int8_t)bank;
    return slots[free];
}

void tms6100BankMap(uint bank)
{
    for (int i = 0; i < TMS6100_SLOTS; i++) {
        if (slotBank[i] == (int8_t)bank) bankData[bank] = slots[i];
    }
}

void tms6100BankUnload(uint bank)
{
    bankData[bank] = NULL;
    for (int i = 0; i < TMS6100_SLOTS; i++) {
        if (slotBank[i] == (int8_t)bank) slotBank[i] = -1;
    }
}

const uint8_t *tms6100Bank(uint bank)
{
    return bankData[bank];
}

uint tms6100BuiltinBank(void)
{
    return PHROM_BANK;
}

void tms6100TraceClear(void)
{
    traceClearRequest = true;
    while (traceClearRequest) tight_loop_contents();
}

uint32_t tms6100TraceCount(uint32_t *dropped)
{
    uint32_t count = traceCount;
    __dmb();
    if (dropped) *dropped = traceDropped;
    return count;
}

const tms6100Event_t *tms6100TraceEvents(void)
{
    return trace;
}
