/*
 * tms6100.c - minimal TMS6100 VSM model (reference datasheet §4).
 */
#include "tms6100.h"
#include "tms5220.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tms6100_init(tms6100 *v)
{
    memset(v, 0, sizeof *v);
    v->mem = calloc(TMS6100_MEM_SIZE, 1);
    return v->mem ? 0 : -1;
}

void tms6100_free(tms6100 *v)
{
    free(v->mem);
    v->mem = NULL;
}

void tms6100_load_mem(tms6100 *v, const uint8_t *data, uint32_t len, uint32_t addr)
{
    uint32_t i;
    for (i = 0; i < len; i++)
        v->mem[(addr + i) & (TMS6100_MEM_SIZE - 1)] = data[i];
}

long tms6100_load_file(tms6100 *v, const char *path, uint32_t addr)
{
    FILE *f = fopen(path, "rb");
    long n = 0;
    int ch;
    if (!f)
        return -1;
    while ((ch = fgetc(f)) != EOF) {
        v->mem[(addr + (uint32_t)n) & (TMS6100_MEM_SIZE - 1)] = (uint8_t)ch;
        n++;
    }
    fclose(f);
    return n;
}

int tms6100_func(void *ctx, int func, int nibble)
{
    tms6100 *v = ctx;
    int r = 0;
    switch (func) {
    case TMS5220_VSM_LOAD:
        /* §4: nibbles shift in least significant first; five give a full
         * 20-bit address, fewer leave the earlier address shifted, a sixth
         * shifts it on. */
        v->addr = ((v->addr >> 4) | ((uint32_t)(nibble & 0xF) << 16)) & (TMS6100_MEM_SIZE - 1);
        v->bit = 0;
        break;
    case TMS5220_VSM_DUMMY:
        /* Original §4: the Dummy Read resets the load pointer and starts the
         * fetch; no data is transferred. */
        v->bit = 0;
        break;
    case TMS5220_VSM_READ:
        /* §6: the chip reads each byte LSB first. */
        r = (v->mem[v->addr] >> v->bit) & 1;
        if (++v->bit == 8) {
            v->bit = 0;
            v->addr = (v->addr + 1) & (TMS6100_MEM_SIZE - 1);
        }
        break;
    case TMS5220_VSM_BRANCH: {
        /* §4: the VSM replaces its address with the two bytes at its current
         * address: the first byte read is the low byte, and the low 14 bits
         * of the word replace the low 14 bits of the address. */
        uint32_t lo = v->mem[v->addr];
        uint32_t hi = v->mem[(v->addr + 1) & (TMS6100_MEM_SIZE - 1)];
        uint32_t w = (hi << 8) | lo;
        v->addr = (v->addr & ~0x3FFFu) | (w & 0x3FFFu);
        v->bit = 0;
        break;
    }
    default:
        break;
    }
    return r;
}
