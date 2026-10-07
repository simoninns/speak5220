/*
 * test_tms5220.c - self-test of the reference emulator against statements in
 * the reference datasheet (not against captures).
 */
#include "tms5220.h"
#include "tms6100.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { \
    if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

/* ---------------- bit stream builder (§8.1: fields MSB first, bytes LSB first) */
typedef struct { uint8_t b[256]; int nbits; } bits;
static void put(bits *s, unsigned v, int n)
{
    int i;
    for (i = n - 1; i >= 0; i--) {
        if ((v >> i) & 1)
            s->b[s->nbits / 8] |= (uint8_t)(1u << (s->nbits % 8));
        s->nbits++;
    }
}
static void f_voiced(bits *s, int e, int p, const int *k)
{
    static const int kb[10] = { 5, 5, 4, 4, 4, 4, 4, 3, 3, 3 };
    int i;
    put(s, (unsigned)e, 4); put(s, 0, 1); put(s, (unsigned)p, 6);
    for (i = 0; i < 10; i++) put(s, (unsigned)k[i], kb[i]);
}
static void f_unvoiced(bits *s, int e, const int *k)
{
    static const int kb[4] = { 5, 5, 4, 4 };
    int i;
    put(s, (unsigned)e, 4); put(s, 0, 1); put(s, 0, 6);
    for (i = 0; i < 4; i++) put(s, (unsigned)k[i], kb[i]);
}
static void f_repeat(bits *s, int e, int p) { put(s, (unsigned)e, 4); put(s, 1, 1); put(s, (unsigned)p, 6); }
static void f_silence(bits *s) { put(s, 0, 4); }
static void f_stop(bits *s) { put(s, 15, 4); }

/* ---------------- harness */
typedef struct {
    tms5220 c;
    tms6100 v;
    int64_t reads[4096]; int nreads;    /* slots of VSM READ functions */
    int ndummy, nload;
    int64_t ts_clear_slot, ts_set_slot;
    int min_code, max_code, nonidle;
    int64_t first_nonidle;
    int64_t nsamples;
} H;

static void ev(void *ctx, const tms5220_event *e)
{
    H *h = ctx;
    if (e->type == TMS5220_EV_VSM) {
        if (e->func == TMS5220_VSM_READ && h->nreads < 4096) h->reads[h->nreads++] = e->slot;
        if (e->func == TMS5220_VSM_DUMMY) h->ndummy++;
        if (e->func == TMS5220_VSM_LOAD) h->nload++;
    }
    if (e->type == TMS5220_EV_FLAG && e->flag == TMS5220_FLAG_TS) {
        if (e->state) h->ts_set_slot = e->slot; else h->ts_clear_slot = e->slot;
    }
}
static void smp(void *ctx, int64_t idx, int64_t slot, int code)
{
    H *h = ctx;
    (void)slot;
    h->nsamples++;
    if (code < h->min_code) h->min_code = code;
    if (code > h->max_code) h->max_code = code;
    if (code != -1) {
        if (!h->nonidle) h->first_nonidle = idx;
        h->nonidle++;
    }
}
static void hinit(H *h)
{
    memset(h, 0, sizeof *h);
    if (tms6100_init(&h->v) < 0) exit(2);
    tms5220_init(&h->c, NULL);
    tms5220_set_vsm(&h->c, tms6100_func, &h->v);
    tms5220_set_event_cb(&h->c, ev, h);
    tms5220_set_sample_cb(&h->c, smp, h);
    h->min_code = 1 << 20; h->max_code = -(1 << 20);
    h->ts_clear_slot = h->ts_set_slot = -1;
}
static void run(H *h, int64_t n) { while (n-- > 0) tms5220_step(&h->c); }
static void wr(H *h, int b)
{
    if (tms5220_write(&h->c, b) < 0) { printf("busy\n"); exit(2); }
    while (tms5220_host_busy(&h->c)) tms5220_step(&h->c);
}
static int rd(H *h)
{
    tms5220_read(&h->c);
    while (tms5220_host_busy(&h->c)) tms5220_step(&h->c);
    return tms5220_host_value(&h->c);
}
static void load_addr(H *h, uint32_t a)
{
    int i;
    for (i = 0; i < 5; i++) wr(h, 0x40 | (int)((a >> (4 * i)) & 0xF));
}
static void reset_counts(H *h)
{
    h->nreads = 0; h->ndummy = 0; h->nload = 0;
    h->nonidle = 0; h->min_code = 1 << 20; h->max_code = -(1 << 20);
}

/* ---------------- tests */

static void test_arith(void)
{
    int32_t m, k, worst = 0;
    unsigned n = 1, i;
    CHECK(tms5220_asr(-1, 4) == -1, "asr(-1,4)");
    CHECK(tms5220_asr(-17, 4) == -2, "asr(-17,4)");
    CHECK(tms5220_asr(16, 4) == 1, "asr(16,4)");
    CHECK(tms5220_wrap14(8192) == -8192, "wrap14(8192)");
    CHECK(tms5220_wrap14(-8193) == 8191, "wrap14(-8193)");
    {
        /* §8.8 worked examples: K, M, product. */
        static const int ex[][3] = {
            { 0, 0, 1 }, { 0, 5000, 1 }, { 33, 192, 13 }, { 114, 4096, 911 },
            { -18, 3, -1 }, { -18, -3, 1 }, { 506, 1000, 989 }, { -501, -777, 761 },
            { 409, -8192, -6543 }, { -256, 8191, -4095 },
        };
        size_t j;
        for (j = 0; j < sizeof ex / sizeof ex[0]; j++)
            CHECK(tms5220_booth(ex[j][1], ex[j][0]) == ex[j][2], "booth K=%d M=%d: %d, expected %d",
                  ex[j][0], ex[j][1], (int)tms5220_booth(ex[j][1], ex[j][0]), ex[j][2]);
    }
    CHECK(tms5220_booth(0, 0) == 1, "booth(0,0) = 1 (forced P0)");
    CHECK(tms5220_booth(1234, 0) == 1, "booth(M,0) = 1");
    /* The multiplier approximates M*K/512 to within a couple of LSB. */
    for (m = -8192; m < 8192; m += 37)
        for (k = -512; k < 512; k += 7) {
            int32_t d = tms5220_booth(m, k) - (int32_t)(((int64_t)m * k) / 512);
            if (d < 0) d = -d;
            if (d > worst) worst = d;
        }
    CHECK(worst <= 3, "booth vs exact product: worst %d", (int)worst);
    /* §8.6: the noise register has period 8191. */
    for (i = 1; i <= 8191; i++) {
        n = tms5220_noise_step(n);
        if (n == 1) break;
    }
    CHECK(i == 8191, "noise period %u", i);
    /* §8.2: idle code -1 gives about 600 uA into 996 ohm. */
    CHECK(tms5220_dac_current_ua(-1, 0) > 590 && tms5220_dac_current_ua(-1, 0) < 610, "dac idle");
    CHECK(tms5220_dac_current_ua(127, 0) == 0.0, "dac +127");
}

static void test_idle(void)
{
    H h;
    hinit(&h);
    run(&h, 20 * 4000);   /* 4000 samples, 20 frames */
    CHECK(h.nsamples == 4000, "samples %lld", (long long)h.nsamples);
    CHECK(h.nonidle == 0 && h.min_code == -1 && h.max_code == -1,
          "idle output is code -1 (min %d max %d)", h.min_code, h.max_code);
    CHECK(h.c.u[1] == -9, "idle u1 = -9 (got %d)", (int)h.c.u[1]);
    CHECK(rd(&h) == 0x7F, "power-up status 0x7F");
    wr(&h, 0x70);
    CHECK(rd(&h) == 0x7F, "status after Reset 0x7F");
    CHECK(h.nload == 1 && h.ndummy == 1, "Reset issues Load Address and Dummy Read");
    tms6100_free(&h.v);
}

static void test_frame_lengths(void)
{
    static const int kv[10] = { 20, 10, 7, 8, 8, 8, 8, 4, 4, 4 };
    static const int ku[4] = { 20, 10, 7, 8 };
    static const int expect[5] = { 50, 11, 29, 4, 4 };
    bits s;
    H h;
    int i, j, burst[16], nb = 0, phase;
    int64_t stop_read;

    for (phase = 0; phase < 200; phase += 37) {
        memset(&s, 0, sizeof s);
        f_voiced(&s, 10, 30, kv);
        f_repeat(&s, 10, 30);
        f_unvoiced(&s, 8, ku);
        f_silence(&s);
        f_stop(&s);
        put(&s, 0xFF, 8);   /* data after the stop code: must not be read */
        hinit(&h);
        tms5220_set_phase(&h.c, phase);
        tms6100_load_mem(&h.v, s.b, (uint32_t)((s.nbits + 7) / 8), 0x1234);
        run(&h, 1000);
        load_addr(&h, 0x1234);
        reset_counts(&h);
        wr(&h, 0x50);
        while (tms5220_talking(&h.c)) tms5220_step(&h.c);
        run(&h, 4000);

        CHECK(h.ndummy == 1, "one Dummy Read before the first frame (got %d)", h.ndummy);
        /* group reads into bursts */
        nb = 0;
        for (i = 0; i < h.nreads; i++) {
            if (i == 0 || h.reads[i] - h.reads[i - 1] > 2) burst[nb++] = 0;
            burst[nb - 1]++;
        }
        CHECK(nb == 5, "5 frame reads (got %d)", nb);
        for (j = 0; j < 5 && j < nb; j++)
            CHECK(burst[j] == expect[j], "frame %d length %d, expected %d", j, burst[j], expect[j]);
        /* frames 25 ms (4000 slots) apart, first bit in slot 17 */
        for (i = 1, j = 0; i < h.nreads; i++)
            if (h.reads[i] - h.reads[i - 1] > 2) {
                CHECK((h.reads[i] - h.reads[0]) % 4000 == 0, "frame start spacing");
                j++;
            }
        CHECK(h.reads[0] % 20 == 17, "first bit in slot 17 (slot %lld)", (long long)h.reads[0]);
        stop_read = h.reads[h.nreads - 4];
        CHECK(h.ts_clear_slot - stop_read == 3979, "TS clears 24.87 ms after the stop frame (%lld)",
              (long long)(h.ts_clear_slot - stop_read));
        CHECK(h.nonidle > 0, "voiced speech changes the output");
        /* §7: first non-idle sample 206-207 after the first frame read (approx.) */
        printf("  phase %3d: first non-idle sample %lld after the first frame read\n", phase,
               (long long)(h.first_nonidle - h.reads[0] / 20));
        tms6100_free(&h.v);
    }
}

static void test_stop_first(void)
{
    uint8_t img[2] = { 0x0F, 0xFF };   /* energy 1111 first: a stop code */
    H h;
    hinit(&h);
    tms5220_set_phase(&h.c, 77);
    tms6100_load_mem(&h.v, img, 2, 0x200);
    run(&h, 300);
    load_addr(&h, 0x200);
    reset_counts(&h);
    wr(&h, 0x50);
    while (tms5220_talking(&h.c)) tms5220_step(&h.c);
    run(&h, 20 * 1000);
    CHECK(h.nreads == 4, "a stop code as the first frame reads 4 bits (got %d)", h.nreads);
    CHECK(h.nonidle == 0, "a stop code as the first frame gives no output change (%d samples)", h.nonidle);
    CHECK(h.nreads > 0 && h.ts_clear_slot - h.reads[0] == 14, "TS clears 0.09 ms after it is read (%lld)",
          h.nreads ? (long long)(h.ts_clear_slot - h.reads[0]) : -1LL);
    CHECK(tms5220_int_active(&h.c), "INT active after TS clears");
    rd(&h);
    CHECK(!tms5220_int_active(&h.c), "status read releases INT");
    tms6100_free(&h.v);
}

static void test_speak_external(void)
{
    static const int kv[10] = { 20, 10, 7, 8, 8, 8, 8, 4, 4, 4 };
    bits s;
    H h;
    int i;
    memset(&s, 0, sizeof s);
    for (i = 0; i < 3; i++) f_voiced(&s, 10, 30, kv);   /* 150 bits */
    f_stop(&s);                                          /* 154 bits: 20 bytes */
    hinit(&h);
    run(&h, 100);
    wr(&h, 0x60);
    CHECK(rd(&h) == 0x7F, "after Speak External: BL, BE set");
    for (i = 0; i < 8; i++) wr(&h, s.b[i]);
    CHECK(rd(&h) == 0x5F, "8 bytes: status 0x5F");
    run(&h, 20 * 1000);
    CHECK(h.nonidle == 0 && !(rd(&h) & 0x80), "8 bytes or fewer: speech never starts");
    wr(&h, s.b[8]);
    CHECK(rd(&h) == 0x9F, "9th byte: status 0x9F");
    for (i = 9; i < 16; i++) wr(&h, s.b[i]);
    while (!(rd(&h) & 0x40)) run(&h, 160);
    for (i = 16; i < 20; i++) wr(&h, s.b[i]);
    while (rd(&h) & 0x80) run(&h, 160);
    CHECK(h.nonidle > 0, "Speak External speaks");
    tms6100_free(&h.v);
}

static void test_read_byte(void)
{
    uint8_t img[4] = { 0x23, 0x9B, 0x35, 0x85 };
    H h;
    hinit(&h);
    tms6100_load_mem(&h.v, img, 4, 0x3C140);
    load_addr(&h, 0x3C140);
    wr(&h, 0x10); CHECK(rd(&h) == 0xC4, "Read Byte 0x23 -> 0xC4");
    wr(&h, 0x10); CHECK(rd(&h) == 0xD9, "Read Byte 0x9B -> 0xD9");
    wr(&h, 0x10); CHECK(rd(&h) == 0xAC, "Read Byte 0x35 -> 0xAC");
    wr(&h, 0x10); CHECK(rd(&h) == 0xA1, "Read Byte 0x85 -> 0xA1");
    CHECK(h.ndummy == 1, "one Dummy Read for four Read Bytes");
    tms6100_free(&h.v);
}

int main(void)
{
    test_arith();
    test_idle();
    test_frame_lengths();
    test_stop_first();
    test_speak_external();
    test_read_byte();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
