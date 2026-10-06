/************************************************************************
    main.c

    speak5220 test firmware for the Raspberry Pi Pico 2

    Emulates the Acorn speech PHROM (TMS6100) on core 1 and repeatedly
    asks the TMS5220 to say "Acorn".  Progress is reported over USB serial.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#include <stdio.h>

#include "pico/stdlib.h"

#include "pins.h"
#include "tms5220.h"
#include "tms6100.h"

// Word 160 "ACORN" is at 0x0BB6 in the Acorn PHROM, which responds as
// bank 0xF (address bits 14-17), giving a 20-bit VSM address of 0x3CBB6
#define WORD_ACORN_ADDRESS  0x3CBB6

#define SPEECH_TIMEOUT_MS   3000
#define REPEAT_DELAY_MS     750

static void printStats(void)
{
    tms6100Stats_t stats;
    tms6100GetStats(&stats);
    printf("  VSM: load-address=%lu read=%lu read-and-branch=%lu address=0x%05lx\n",
           (unsigned long)stats.loadAddressCount, (unsigned long)stats.readCount,
           (unsigned long)stats.readAndBranchCount, (unsigned long)stats.address);
}

int main(void)
{
    stdio_init_all();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    tms6100Start();
    tms5220Init();

    // Allow the TMS5220 power-up clear (15ms) to complete
    sleep_ms(100);

    uint32_t count = 0;
    while (true) {
        count++;
        printf("\n[%lu] Saying \"Acorn\"\n", (unsigned long)count);

        if (!tms5220Reset()) printf("  Reset: READY timeout\n");

        gpio_put(PIN_LED, 1);
        if (!tms5220Speak(WORD_ACORN_ADDRESS)) printf("  Speak: READY timeout\n");

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
        gpio_put(PIN_LED, 0);

        int64_t elapsedMs = absolute_time_diff_us(start, get_absolute_time()) / 1000;
        printf("  Done in %lld ms (status=0x%02x, INT=%d)\n", elapsedMs, status, gpio_get(PIN_INT_N) ? 1 : 0);
        printStats();

        sleep_ms(REPEAT_DELAY_MS);
    }
}
