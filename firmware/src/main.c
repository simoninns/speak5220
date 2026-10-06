/************************************************************************
    main.c

    speak5220 test firmware for the Raspberry Pi Pico 2

    Emulates the Acorn speech PHROM (TMS6100) on core 1 and repeatedly
    asks the TMS5220 to say "Acorn BBC Computer".  Progress is reported
    over USB serial.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#include <stdio.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "pins.h"
#include "tms5220.h"
#include "tms6100.h"

// The Acorn PHROM responds as bank 0xF (address bits 14-17), so a word at
// PHROM offset 0x0BB6 has a 20-bit VSM address of 0x3CBB6
#define PHROM_ADDRESS(offset)   (0x3C000 | (offset))

typedef struct {
    const char *name;
    uint32_t address;
} word_t;

// "Acorn BBC Computer" (word numbers from the PHROM word list)
static const word_t phrase[] = {
    { "ACORN",    PHROM_ADDRESS(0x0BB6) },  // 160
    { "B",        PHROM_ADDRESS(0x1065) },  // 170
    { "B",        PHROM_ADDRESS(0x1065) },  // 170
    { "C",        PHROM_ADDRESS(0x124D) },  // 175
    { "COMPUTER", PHROM_ADDRESS(0x140B) },  // 179
};

// 144MHz is an exact multiple of the 640kHz TMS5220 clock (x225) and can
// be generated exactly from the 12MHz crystal (VCO 1440MHz / 5 / 2)
#define SYS_CLOCK_KHZ       144000

#define SPEECH_TIMEOUT_MS   3000
#define REPEAT_DELAY_MS     1000

static void printStats(void)
{
    tms6100Stats_t stats;
    tms6100GetStats(&stats);
    printf("  VSM: load-address=%lu read=%lu read-and-branch=%lu address=0x%05lx\n",
           (unsigned long)stats.loadAddressCount, (unsigned long)stats.readCount,
           (unsigned long)stats.readAndBranchCount, (unsigned long)stats.address);
}

// Speak one word and wait for it to finish
static void sayWord(const word_t *word)
{
    gpio_put(PIN_LED, 1);
    gpio_put(PIN_SCOPE_TRIG, 1);
    if (!tms5220Speak(word->address)) printf("  Speak: READY timeout\n");

    // Poll the status register until Talk Status clears
    absolute_time_t start = get_absolute_time();
    uint8_t status = 0;
    bool talking = true;
    while (talking) {
        sleep_ms(5);
        if (!tms5220Read(&status)) {
            printf("  Status read: READY timeout\n");
            break;
        }
        talking = status & TMS5220_STATUS_TS;

        if (absolute_time_diff_us(start, get_absolute_time()) > SPEECH_TIMEOUT_MS * 1000) {
            printf("  Speech timeout (status=0x%02x)\n", status);
            break;
        }
    }
    gpio_put(PIN_SCOPE_TRIG, 0);
    gpio_put(PIN_LED, 0);

    int64_t elapsedMs = absolute_time_diff_us(start, get_absolute_time()) / 1000;
    printf("  %-8s 0x%05lx done in %lld ms (status=0x%02x, INT=%d)\n", word->name,
           (unsigned long)word->address, elapsedMs, status, gpio_get(PIN_INT_N) ? 1 : 0);
}

int main(void)
{
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);
    stdio_init_all();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    // PicoScope trigger: high from just before Speak until Talk Status clears
    gpio_init(PIN_SCOPE_TRIG);
    gpio_set_dir(PIN_SCOPE_TRIG, GPIO_OUT);

    tms6100Start();
    tms5220Init();
    uint32_t clockHz = tms5220ClockStart();

    // Allow the TMS5220 power-up clear (15ms) to complete
    sleep_ms(100);

    uint32_t count = 0;
    while (true) {
        count++;
        printf("\n[%lu] Saying \"Acorn BBC Computer\" (TMS5220 clock %lu Hz)\n",
               (unsigned long)count, (unsigned long)clockHz);

        if (!tms5220Reset()) printf("  Reset: READY timeout\n");

        for (uint i = 0; i < count_of(phrase); i++) {
            sayWord(&phrase[i]);
        }
        printStats();

        sleep_ms(REPEAT_DELAY_MS);
    }
}
