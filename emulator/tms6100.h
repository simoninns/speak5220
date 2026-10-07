/*
 * tms6100.h - minimal TMS6100 voice synthesis memory (VSM) model, sufficient
 * to drive the TMS5220 emulator.  Written from the reference datasheet §4 and
 * the original manual's VSM description.
 */
#ifndef TMS6100_H
#define TMS6100_H

#include <stdint.h>

#define TMS6100_MEM_SIZE (1u << 20)   /* 20-bit address space, 1 MB */

typedef struct {
    uint8_t *mem;        /* TMS6100_MEM_SIZE bytes; unloaded bytes read 0x00 */
    uint32_t addr;       /* 20-bit address register */
    int bit;             /* next bit of the current byte, 0 = LSB */
} tms6100;

int  tms6100_init(tms6100 *v);              /* 0 on success */
void tms6100_free(tms6100 *v);
/* Load a binary image at 'addr'; returns bytes loaded or -1. */
long tms6100_load_file(tms6100 *v, const char *path, uint32_t addr);
void tms6100_load_mem(tms6100 *v, const uint8_t *data, uint32_t len, uint32_t addr);
/* The VSM callback for tms5220_set_vsm(): func is a tms5220_vsm_func. */
int  tms6100_func(void *ctx, int func, int nibble);

#endif
