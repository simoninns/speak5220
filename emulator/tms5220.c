/*
 * tms5220.c - reference emulator of the TI TMS5220, written only from the
 * reference datasheet (docs-tech/tms5220_reference_datasheet/).  Section
 * numbers (§n) refer to that datasheet.  "DATASHEET GAP:" comments mark every
 * choice the datasheet does not make; GAPS.md lists them.
 */
#include "tms5220.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tables (§8.1, Tables 8-4 to 8-7)                                     */
/* ------------------------------------------------------------------ */

/* Table 8-4.  Index 15 (stop) is never multiplied; it sets the target to 0. */
static const int energy_tab[16] = {
    0, 1, 2, 3, 4, 6, 8, 11, 16, 23, 33, 47, 63, 85, 114, 0
};

/* Table 8-5: the value held in the pitch register (period = value + 1). */
static const int pitch_tab[64] = {
    0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 44, 46, 48,
    50, 52, 53, 56, 58, 60, 62, 65, 68, 70, 72, 76, 78, 80, 84, 86,
    91, 94, 98, 101, 105, 109, 114, 118, 122, 127, 132, 137, 142, 148, 153, 159
};

/* Table 8-6. */
static const int k1_tab[32] = {
    -501, -498, -497, -495, -493, -491, -488, -482, -478, -474, -469, -464, -459, -452, -445, -437,
    -412, -380, -339, -288, -227, -158, -81, -1, 80, 157, 226, 287, 337, 379, 411, 436
};
static const int k2_tab[32] = {
    -328, -303, -274, -244, -211, -175, -138, -99, -59, -18, 24, 64, 105, 143, 180, 215,
    248, 278, 306, 331, 354, 374, 392, 408, 422, 435, 445, 455, 463, 470, 476, 506
};
static const int k3_tab[16] = { -441, -387, -333, -279, -225, -171, -117, -63, -9, 45, 98, 152, 206, 260, 314, 368 };
static const int k4_tab[16] = { -328, -273, -217, -161, -106, -50, 5, 61, 116, 172, 228, 283, 339, 394, 450, 506 };
static const int k5_tab[16] = { -328, -282, -235, -189, -142, -96, -50, -3, 43, 90, 136, 182, 229, 275, 322, 368 };
static const int k6_tab[16] = { -256, -212, -168, -123, -79, -35, 10, 54, 98, 143, 187, 232, 276, 320, 365, 409 };
static const int k7_tab[16] = { -308, -260, -212, -164, -117, -69, -21, 27, 75, 122, 170, 218, 266, 314, 361, 409 };
static const int k8_tab[8] = { -256, -161, -66, 29, 124, 219, 314, 409 };
static const int k9_tab[8] = { -256, -176, -96, -15, 65, 146, 226, 307 };
static const int k10_tab[8] = { -205, -132, -59, 14, 87, 160, 234, 307 };
static const int *const k_tab[10] = { k1_tab, k2_tab, k3_tab, k4_tab, k5_tab, k6_tab, k7_tab, k8_tab, k9_tab, k10_tab };
static const int k_bits[10] = { 5, 5, 4, 4, 4, 4, 4, 3, 3, 3 };

/* Table 8-7: 52 locations; 21..51 hold 0; a counter above 51 reads 51. */
static const int chirp_tab[52] = {
    0, 3, 15, 40, 76, 108, 113, 80, 37, 38, 76, 68, 26, 50, 59, 19, 55, 26, 37, 31, 29,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* Bit-exact helpers                                                    */
/* ------------------------------------------------------------------ */

/* Arithmetic shift right, rounding down, without implementation-defined
 * signed shifts. */
int32_t tms5220_asr(int32_t x, int n)
{
    if (x >= 0)
        return x >> n;
    return -(((-(x + 1)) >> n)) - 1;
}

/* Two's-complement wrap to 14 bits, -8192..8191 (§8.8). */
int32_t tms5220_wrap14(int32_t x)
{
    uint32_t u = ((uint32_t)x + 8192u) & 0x3FFFu;
    return (int32_t)u - 8192;
}

/* The array multiplier of §8.8: booth(K, M) of §8.9, with the operands in
 * the order (M, K).  m: 14-bit operand, k: 10-bit two's-complement
 * coefficient.  d_i = -2*k(2i+1) + k(2i) + k(2i-1); p_0 inverted without the
 * +1 for a negative d0; p_1..p_4 exact; no wrap inside; (acc >> 1) OR 1. */
int32_t tms5220_booth(int32_t m, int k)
{
    unsigned kb = (unsigned)k & 0x3FFu;
    int d[5], prev = 0, i;
    int32_t acc, r;

    for (i = 0; i < 5; i++) {
        int b0 = (int)((kb >> (2 * i)) & 1u);
        int b1 = (int)((kb >> (2 * i + 1)) & 1u);
        d[i] = -2 * b1 + b0 + prev;
        prev = b1;
    }
    if (d[0] < 0)
        acc = -((-d[0]) * m) - 1;      /* inverted |d0| x M, no +1 */
    else
        acc = d[0] * m;
    acc = tms5220_asr(acc, 2);
    acc = acc + d[1] * m;
    for (i = 2; i < 5; i++)
        acc = tms5220_asr(acc, 2) + d[i] * m;
    r = tms5220_asr(acc, 1);
    if (r % 2 == 0)                     /* "P0 is always made a logical one" */
        r += 1;
    return r;
}

/* §8.6: shift left, new bit 0 = b12 ^ b3 ^ b2 ^ b0. */
unsigned tms5220_noise_step(unsigned n)
{
    unsigned fb = ((n >> 12) ^ (n >> 3) ^ (n >> 2) ^ n) & 1u;
    return ((n << 1) | fb) & 0x1FFFu;
}

/* §8.2, Table 8-2 bit weights; LSB from Table 8-1. */
double tms5220_dac_current_ua(int code, int load)
{
    static const double w996[8] = { 0.86, 1.82, 3.99, 7.85, 15.79, 32.33, 63.97, 132.64 };
    static const double w482[8] = { 0.87, 1.82, 3.94, 7.78, 15.74, 32.09, 62.89, 130.96 };
    const double *w = load ? w482 : w996;
    double lsb = load ? 4.80 : 4.49, sum = 0.0;
    int byte, i;
    if (code > 127) code = 127;
    if (code < -128) code = -128;
    byte = code + 128;
    for (i = 0; i < 8; i++)
        if (!((byte >> i) & 1))
            sum += w[i];
    return sum * lsb;
}

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

void tms5220_default_config(tms5220_config *cfg)
{
    /* UNMEASURED (§3 "Not measured yet"): READY latencies as whole slots.
     * Chosen from Table 3-1 (status 11.5-17.4 us, command 18.4-23.9 us,
     * FIFO 14.7-20.7 us). */
    cfg->cmd_latch = 4;          /* 25.0 us */
    cfg->fifo_latch = 3;         /* 18.75 us */
    cfg->read_ready = 3;         /* 18.75 us */
    /* §4 gives the Load Address function "about 35-45 us after the write":
     * latch (4) + 2 = 37.5 us.  §4 gives the Read Byte's Dummy Read 2 slots
     * after the latch too.  UNMEASURED (§3): the same delay is used for every
     * other command's first VSM function, the Speak's Dummy Read included. */
    cfg->vsm_delay = 2;
    cfg->vsm_spacing = 2;        /* §4: bits two slots (12.5 us) apart */
    /* §3 "When READY returns": READY for the next write goes active at the
     * start of the first slot 15 that comes more than 59 and at most 63 slots
     * after the Branch function.  DATASHEET GAP: the exact limit is not
     * settled (two cases); 60, the "more than 59 slots" of the text. */
    cfg->branch_ready = 60;
    /* §8.5, §8.9: the first frame-read sample whose slot 17 is at least 52
     * slots after the latch. */
    cfg->speak_lead = 52;
    /* UNMEASURED (§3): when Talk Status sets after Speak (later than 22 us). */
    cfg->ts_set_delay = 2;
    /* §4, §8.5: TS clears 24.87 ms after the stop frame is read (slot 16 of
     * its sample 199 = 3979 slots after the first bit in slot 17), or
     * 0.09 ms after a stop code that is the first frame. */
    cfg->ts_clear_stop = 3979;
    cfg->ts_clear_first_stop = 14;
    /* §7: first step 6 slots (Speak) or 1 slot (Speak External) after the
     * latch; Reset holds the register at 0x1FFF. */
    cfg->noise_rel_speak = 6;
    cfg->noise_rel_se = 1;
    cfg->noise_hold_steps = 0;
    /* §8.5 "Speak External": sample 0 two sample periods before the VSM read
     * point; the frame is taken in sample 198 of the frame clock. */
    cfg->se_shift = 2;
    cfg->frame_read_slot = 17;   /* §4 */
    /* §4 observed nibble 8 for a Reset during speech; used for every Reset. */
    cfg->reset_nibble = 8;
}

/* ------------------------------------------------------------------ */
/* Events                                                               */
/* ------------------------------------------------------------------ */

static void emit(tms5220 *c, tms5220_event *e)
{
    if (c->on_event)
        c->on_event(c->event_ctx, e);
}

static void ev_speech(tms5220 *c, const char *what, int a, int b)
{
    tms5220_event e;
    memset(&e, 0, sizeof e);
    e.type = TMS5220_EV_SPEECH;
    e.slot = c->slot;
    e.what = what;
    e.a = a;
    e.b = b;
    emit(c, &e);
}

static int vsm_call(tms5220 *c, int func, int nibble)
{
    tms5220_event e;
    int r = 0;
    if (c->vsm)
        r = c->vsm(c->vsm_ctx, func, nibble) & 1;
    memset(&e, 0, sizeof e);
    e.type = TMS5220_EV_VSM;
    e.slot = c->slot;
    e.func = func;
    e.arg = func == TMS5220_VSM_LOAD ? nibble : func == TMS5220_VSM_READ ? r : -1;
    emit(c, &e);
    return r;
}

static void set_int(tms5220 *c, int v);

static void set_flag(tms5220 *c, int which, int v)
{
    int *p = which == TMS5220_FLAG_TS ? &c->ts : which == TMS5220_FLAG_BL ? &c->bl :
             which == TMS5220_FLAG_BE ? &c->be : &c->intr;
    int old = *p;
    tms5220_event e;
    if (old == v)
        return;
    *p = v;
    memset(&e, 0, sizeof e);
    e.type = TMS5220_EV_FLAG;
    e.slot = c->slot;
    e.flag = which;
    e.state = v;
    emit(c, &e);
    /* §3 (Correction): INT goes active when TS goes clear and when BL goes
     * set, not when BE goes set. */
    if (which == TMS5220_FLAG_TS && old && !v)
        set_int(c, 1);
    if (which == TMS5220_FLAG_BL && !old && v)
        set_int(c, 1);
}

static void set_int(tms5220 *c, int v) { set_flag(c, TMS5220_FLAG_INT, v); }

/* ------------------------------------------------------------------ */
/* Scheduled host-side actions                                          */
/* ------------------------------------------------------------------ */

enum { Q_LOAD = 1, Q_DUMMY, Q_BRANCH, Q_RBBIT, Q_TS_SET, Q_NOISE_REL };

static void qadd(tms5220 *c, int64_t slot, int op, int arg)
{
    if (c->nq < (int)(sizeof c->q / sizeof c->q[0])) {
        c->q[c->nq].slot = slot;
        c->q[c->nq].op = op;
        c->q[c->nq].arg = arg;
        c->nq++;
    }
}

static unsigned noise_hold_state(const tms5220 *c)
{
    unsigned n = 0x1FFFu;
    int i;
    for (i = 0; i < c->cfg.noise_hold_steps; i++)
        n = tms5220_noise_step(n);
    return n;
}

/* ------------------------------------------------------------------ */
/* Init                                                                 */
/* ------------------------------------------------------------------ */

void tms5220_init(tms5220 *c, const tms5220_config *cfg)
{
    int i;
    memset(c, 0, sizeof *c);
    if (cfg)
        c->cfg = *cfg;
    else
        tms5220_default_config(&c->cfg);
    c->held_cmd = -1;
    c->ts_clear_at = -1;
    c->busy_until = -1000;
    c->ready_min = -1000;
    /* §8.9 "Not known for this path": the power-up state is not measured;
     * every index, register and flag at 0 and the filter at its idle state
     * (u11 = +1, each stage one less, u1 = -9; b_i as left by that state).
     * UNMEASURED (§3): what the power-up clear issues to the VSM and its
     * effect on INT: nothing is issued, INT inactive.  The FIFO is empty
     * (original §7: BL and BE set).  The noise register is held at 0x1FFF as
     * after Reset.  DATASHEET GAP: the power-up noise state is not given. */
    for (i = 1; i <= 11; i++)
        c->u[i] = i - 10;
    for (i = 1; i <= 10; i++)
        c->b[i] = i - 10;
    c->bl = 1;
    c->be = 1;
    c->noise = noise_hold_state(c);
    c->noise_held = 1;
    c->code = -1;
}

void tms5220_set_vsm(tms5220 *c, tms5220_vsm_fn fn, void *ctx) { c->vsm = fn; c->vsm_ctx = ctx; }
void tms5220_set_event_cb(tms5220 *c, tms5220_event_fn fn, void *ctx) { c->on_event = fn; c->event_ctx = ctx; }
void tms5220_set_sample_cb(tms5220 *c, tms5220_sample_fn fn, void *ctx) { c->on_sample = fn; c->sample_ctx = ctx; }

void tms5220_set_phase(tms5220 *c, int sample)
{
    sample %= TMS5220_SAMPLES_PER_FRAME;
    if (sample < 0)
        sample += TMS5220_SAMPLES_PER_FRAME;
    c->frame_pos = sample;
}

/* ------------------------------------------------------------------ */
/* FIFO (§5)                                                            */
/* ------------------------------------------------------------------ */

static void fifo_purge(tms5220 *c)
{
    c->fifo_n = 0;
    c->fifo_bit = 0;
    set_flag(c, TMS5220_FLAG_BL, 1);
    set_flag(c, TMS5220_FLAG_BE, 1);
}

static int fifo_pop_bit(tms5220 *c, int *bit)
{
    if (c->fifo_n == 0)
        return 0;
    *bit = (c->fifo[0] >> c->fifo_bit) & 1;   /* LSB first (original §5.2) */
    if (++c->fifo_bit == 8) {
        memmove(c->fifo, c->fifo + 1, (size_t)(c->fifo_n - 1));
        c->fifo_n--;
        c->fifo_bit = 0;
    }
    return 1;
}

static void fifo_write(tms5220 *c, int byte)
{
    if (c->fifo_n >= 16) {
        /* UNMEASURED (§3): a 17th byte written to a full FIFO: discarded. */
        ev_speech(c, "fifo_overflow", byte, c->fifo_n);
        return;
    }
    c->fifo[c->fifo_n++] = (uint8_t)byte;
    set_flag(c, TMS5220_FLAG_BE, 0);          /* §5: BE clears with the first byte */
    if (c->fifo_n >= 9) {
        set_flag(c, TMS5220_FLAG_BL, 0);      /* §5: BL clears with the 9th byte */
        if (c->se_active && !c->se_ready) {
            c->se_ready = 1;                  /* §5: TS sets at once with the 9th */
            c->se_ready_at = c->slot;
            set_flag(c, TMS5220_FLAG_TS, 1);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Frame parsing (§8.1)                                                 */
/* ------------------------------------------------------------------ */

static int field_width(int field)
{
    if (field == 0) return 4;
    if (field == 1) return 1;
    if (field == 2) return 6;
    return k_bits[field - 3];
}

/* Feed one bit, most significant bit of each field first. */
static void frame_feed(tms5220_frame *f, int bit)
{
    f->acc = (f->acc << 1) | (bit & 1);
    f->fbits++;
    f->nbits++;
    if (f->fbits < field_width(f->field))
        return;
    switch (f->field) {
    case 0:
        f->e = f->acc;
        if (f->e == 0 || f->e == 15)
            f->done = 1;
        break;
    case 1:
        f->rep = f->acc;
        break;
    case 2:
        f->pitch = f->acc;
        if (f->rep)
            f->done = 1;
        break;
    default:
        f->k[f->field - 3] = f->acc;
        if (f->field == 6 && f->pitch == 0)
            f->done = 1;
        if (f->field == 12)
            f->done = 1;
        break;
    }
    f->field++;
    f->fbits = 0;
    f->acc = 0;
}

/* A frame has been read in full, or (Speak External) ran out of data. */
static void frame_finished(tms5220 *c, int truncated)
{
    c->fr_active = 0;
    ev_speech(c, truncated ? "frame_truncated" : "frame", c->fr.nbits, c->fr.e);
    if (truncated) {
        /* §8.9: "a stop code if a Speak External ran out of data". */
        memset(&c->fr, 0, sizeof c->fr);
        c->fr.e = 15;
    }
    if (c->fr.e == 15) {
        /* §4: TS clears 0.09 ms after a stop code that is the first frame,
         * else 24.87 ms after the stop frame is read.  DATASHEET GAP: for a
         * first-frame stop, §8.9 still loads the frame and keeps talking to
         * sample 200 (clearing Z and U); implemented literally. */
        if (c->utt_frames == 0 && !truncated)
            c->ts_clear_at = c->fr_start + c->cfg.ts_clear_first_stop;
        else
            c->ts_clear_at = c->fr_start + c->cfg.ts_clear_stop;
    }
    c->newf = c->fr;
    c->newf_valid = 1;
    c->utt_frames++;
}

/* §4 "The rest of the frame": slots from the frame's first bit to the first
 * bit of each parameter (energy, repeat and pitch, K1..K10).  Within a
 * parameter the bits are two slots apart.  The table gives repeat and pitch
 * as one parameter of 1 + 6 bits, so the pitch follows the repeat bit.
 * K10's last bit (slot 446) still comes before the frame is loaded, in PC 12
 * of IP 0, 25 samples after the read sample (483 slots after the first bit). */
static const int param_slot[12] = { 0, 34, 78, 118, 160, 200, 240, 280, 320, 362, 402, 442 };

static void frame_read_start(tms5220 *c)
{
    memset(&c->fr, 0, sizeof c->fr);
    c->fr_active = 1;
    c->fr_start = c->period * TMS5220_SLOTS_PER_SAMPLE + c->cfg.frame_read_slot;
    c->fr_next = c->fr_start;
}

static void frame_read_slot(tms5220 *c)
{
    int bit;
    if (!c->speak_se) {
        /* §4: one bit per Read function, two slots apart within a
         * parameter; each parameter starts at its offset from the frame's
         * first bit.  A frame that ends early stops after its last field. */
        bit = vsm_call(c, TMS5220_VSM_READ, 0);
        frame_feed(&c->fr, bit);
        if (c->fr.done)
            frame_finished(c, 0);
        else if (c->fr.fbits > 0 || c->fr.field == 2)
            c->fr_next += c->cfg.vsm_spacing;
        else
            c->fr_next = c->fr_start + param_slot[c->fr.field <= 1 ? c->fr.field : c->fr.field - 1];
        return;
    }
    /* §5: bits leave the FIFO as the chip reads each frame.  DATASHEET GAP:
     * §5 says the flags fit the VSM path's 446-slot schedule (§4) better than
     * taking all the bits at once, but leaves the rule (and so the times of
     * BL, BE and TS within a frame) "not yet settled".  Kept: all of a
     * frame's bits leave at once, at slot 17 of the (Speak External)
     * frame-read sample, as for the VSM's first bit. */
    while (!c->fr.done) {
        if (!fifo_pop_bit(c, &bit))
            break;
        frame_feed(&c->fr, bit);
    }
    if (!c->fr.done) {
        /* §5: BE sets when a frame read needs more bits than the FIFO
         * holds.  DATASHEET GAP: what is left in the FIFO is not said; it is
         * emptied. */
        c->fifo_n = 0;
        c->fifo_bit = 0;
        set_flag(c, TMS5220_FLAG_BE, 1);
        set_flag(c, TMS5220_FLAG_BL, 1);
        frame_finished(c, 1);
        return;
    }
    /* DATASHEET GAP: §6 says a stream whose stop code ends exactly on a byte
     * boundary finishes with BE set, but §5 defines BE only for a frame that
     * cannot be read in full.  BE also sets here when a frame read leaves the
     * FIFO empty. */
    if (c->fifo_n == 0)
        set_flag(c, TMS5220_FLAG_BE, 1);
    /* §5: BL sets at the frame read that leaves 8 bytes or fewer. */
    if (c->fifo_n <= 8)
        set_flag(c, TMS5220_FLAG_BL, 1);
    frame_finished(c, 0);
}

/* ------------------------------------------------------------------ */
/* §8.9 reference algorithm                                              */
/* ------------------------------------------------------------------ */

static int silence_idx(const int *idx) { return idx[0] == 0 || idx[0] == 15; }
static int unvoiced_idx(const int *idx) { return idx[1] == 0; }

static int target(const tms5220 *c, int i)
{
    if (c->Z || (c->U && i >= 6))
        return 0;
    if (i == 0)
        return energy_tab[c->idx[0]];
    if (i == 1)
        return pitch_tab[c->idx[1]];
    return k_tab[i - 2][c->idx[i]];
}

static void wq_add(tms5220 *c, int64_t due, int i, int val)
{
    if (due <= c->period) {
        c->seen[i] = val;                     /* due now: applied at once */
        return;
    }
    if (c->nwq < (int)(sizeof c->wq / sizeof c->wq[0])) {
        c->wq[c->nwq].due = due;
        c->wq[c->nwq].i = i;
        c->wq[c->nwq].val = val;
        c->nwq++;
    }
}

static void wq_apply(tms5220 *c)
{
    int j, k = 0;
    for (j = 0; j < c->nwq; j++) {
        if (c->wq[j].due <= c->period)
            c->seen[c->wq[j].i] = c->wq[j].val;
        else
            c->wq[k++] = c->wq[j];
    }
    c->nwq = k;
}

/* Loading a frame (§8.9), in PC 12 of IP 0 while talking. */
static void load_frame(tms5220 *c)
{
    const tms5220_frame *f = &c->newf;
    int j, sil = (f->e == 0 || f->e == 15);

    c->loads++;
    if (!(f->e == 0 && c->loads == 1)) {
        c->Z = 0;                            /* §8.9: clear Z and U */
        c->U = 0;
    }
    c->idx[0] = f->e;
    if (f->e == 15)
        c->requested = 0;
    if (!sil) {
        c->idx[1] = f->pitch;
        if (f->rep) {
            c->U = (f->pitch == 0) || c->lastU;
        } else {
            c->U = (f->pitch == 0);
            c->lastU = c->U;
            for (j = 0; j < (f->pitch ? 10 : 4); j++)
                c->idx[2 + j] = f->k[j];
        }
    }
    c->inhibit = (c->oldU != unvoiced_idx(c->idx)) ||
                 (c->oldS && !silence_idx(c->idx)) ||
                 (c->oldU && silence_idx(c->idx));
    c->newf_valid = 0;
    ev_speech(c, "load", f->e, c->inhibit);
}

/* A Speak (or Speak External) takes effect at the start of its frame-read
 * sample (§8.9). */
static void check_start(tms5220 *c)
{
    int64_t rd = c->period * TMS5220_SLOTS_PER_SAMPLE + c->cfg.frame_read_slot;
    int se_pos = ((TMS5220_SAMPLES_PER_FRAME - c->cfg.se_shift) % TMS5220_SAMPLES_PER_FRAME
                  + TMS5220_SAMPLES_PER_FRAME) % TMS5220_SAMPLES_PER_FRAME;
    if (c->requested)
        return;
    if (c->pend_speak && c->frame_pos == 0 && rd - c->pend_L >= c->cfg.speak_lead) {
        c->pend_speak = 0;
        c->speak_se = 0;
        c->synth_off = 0;
    } else if (c->se_active && c->se_ready && c->frame_pos == se_pos &&
               rd - c->se_L >= c->cfg.speak_lead && rd - c->se_ready_at >= c->cfg.speak_lead) {
        /* UNMEASURED: whether the 52-slot lead is counted from the command or
         * from the 9th byte; both are required here. */
        c->speak_se = 1;
        c->synth_off = c->cfg.se_shift;
    } else {
        return;
    }
    /* DATASHEET GAP: §8.9 says both "at the start of that sample" and (step
     * 6) "a Speak latched in time starts here"; Z, U, loads and requested are
     * set at the start, talking at the end (step 6) (confirmed as intended).
     * UNMEASURED: the Speak External counter offset is kept until the next
     * Speak starts (§8.5 says only "for that utterance"). */
    c->Z = 1;
    c->U = 1;
    c->loads = 0;
    c->requested = 1;
    c->utt_frames = 0;
    c->newf_valid = 0;
    ev_speech(c, c->speak_se ? "start_external" : "start_speak", c->frame_pos, 0);
}

static const int filt_delay[12] = { 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const int shift_tab[8] = { 0, 3, 3, 3, 2, 2, 1, 1 };

static void period_start(tms5220 *c)
{
    int p, ip, local, i, pc_a, exc_U, was;
    int32_t x, *u = c->u, *b = c->b;

    if (c->slot > 0)
        c->frame_pos = (c->frame_pos + 1) % TMS5220_SAMPLES_PER_FRAME;
    c->period = c->slot / TMS5220_SLOTS_PER_SAMPLE;

    check_start(c);
    c->synth_pos = (c->frame_pos + c->synth_off) % TMS5220_SAMPLES_PER_FRAME;
    p = c->synth_pos;
    if (p == 0) {
        ip = 7;
        local = 24;
    } else {
        ip = (p - 1) / 25;
        local = (p - 1) % 25;
    }
    pc_a = (local < 24 && (local & 1) == 0) ? local / 2 : -1;

    /* oldU as it was in sample n-1 (step 2.4); remember it as it is now. */
    exc_U = c->oldU_prev;
    c->oldU_prev = c->oldU;

    /* A frame is read in every frame-read sample while requested (§8.9). */
    if (p == 0 && c->requested && !c->fr_active)
        frame_read_start(c);

    /* 1. Queued writes due now. */
    wq_apply(c);

    if (c->talking) {
        /* 2.1 */
        if (ip == 0 && local == 24 && c->newf_valid)
            load_frame(c);
        /* 2.2 */
        if (pc_a >= 0) {
            int now = c->inhibit && ip != 0;
            int step = now ? 0 : (int)tms5220_asr(target(c, pc_a) - c->cur[pc_a], shift_tab[ip]);
            int zero = pc_a <= 5 ? c->Z : c->U;
            int value;
            if (pc_a >= 6 && now)
                zero = 0;
            if (zero && c->Z) {
                c->cur[pc_a] += step;
                value = 0;
            } else if (zero) {
                c->cur[pc_a] = 0;
                value = 0;
            } else {
                c->cur[pc_a] += step;
                value = c->cur[pc_a];
            }
            wq_add(c, c->period + filt_delay[pc_a], pc_a, value);
        }
        /* 2.3 In the A sample of PC 1 of IP 0 (local 2), clear hold: the
         * counter is held through PC 0 A and B and PC 1 A (§8.6). */
        if (ip == 0 && local == 2)
            c->hold = 0;
    } else {
        /* 3. */
        if (pc_a >= 0)
            wq_add(c, c->period + filt_delay[pc_a] - 1, pc_a, 0);
        exc_U = c->oldU;
    }

    /* 2.4 Excitation (§8.6).  Bit 0 of the noise register at slot 0. */
    if (exc_U)
        x = (c->noise & 1u) ? -64 : 64;
    else
        x = chirp_tab[c->count > 51 ? 51 : c->count];

    /* 4. Filter (§8.8). */
    u[11] = tms5220_wrap14(tms5220_booth(64 * x, c->seen[0]));
    for (i = 10; i >= 1; i--)
        u[i] = tms5220_wrap14(u[i + 1] - tms5220_booth(b[i], c->seen[1 + i]));
    for (i = 10; i >= 2; i--)
        b[i] = tms5220_wrap14(b[i - 1] + tms5220_booth(u[i - 1], c->seen[i]));
    b[1] = u[1];

    /* 5. Output. */
    c->code = (int)tms5220_asr(u[1], 4);
    if (c->on_sample)
        c->on_sample(c->sample_ctx, c->period, c->slot, c->code);

    /* 6. End of the period. */
    if (p == 0) {
        if (c->inhibit && c->talking)
            c->hold = 1;
        c->oldS = silence_idx(c->idx);
        c->oldU = unvoiced_idx(c->idx);
        was = c->talking;
        c->talking = c->requested;
        if (was && !c->talking)
            ev_speech(c, "end", 0, 0);
    }
    if (c->talking) {
        c->count = c->count + 1;
        if (c->count >= c->seen[1] + 1 || c->hold)
            c->count = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Commands (§6)                                                        */
/* ------------------------------------------------------------------ */

/* §4 "When Read Byte reads": the slot of a sample period at which the eight
 * bits start (13, 15, ..., 19, 1, ..., 7). */
#define RB_FIRST_SLOT 13
/* §4: after a Dummy Read, the first slot 13 at least this many slots on. */
#define RB_AFTER_DUMMY 22

/* A Read Byte latched (or, held, released) at slot ref, whose first VSM
 * function may occur at slot s0. */
static void read_byte_start(tms5220 *c, int64_t ref, int64_t s0)
{
    int64_t first;
    int i;
    if (c->need_dummy) {
        /* §4: after a Load Address the Dummy Read comes first, 2 slots after
         * the latch (s0); reading starts at the first slot 13 that is at
         * least 22 slots after it. */
        int64_t m = s0 + RB_AFTER_DUMMY;
        qadd(c, s0, Q_DUMMY, 0);
        c->need_dummy = 0;
        first = m + (RB_FIRST_SLOT - m % TMS5220_SLOTS_PER_SAMPLE + TMS5220_SLOTS_PER_SAMPLE)
                    % TMS5220_SLOTS_PER_SAMPLE;
    } else {
        /* §4: without a Dummy Read, slot 13 of the sample period after the
         * one in which the command was latched; held during speech, after
         * the one in which Talk Status cleared. */
        first = (ref / TMS5220_SLOTS_PER_SAMPLE + 1) * TMS5220_SLOTS_PER_SAMPLE + RB_FIRST_SLOT;
    }
    c->rb_done = 0;
    c->rb_bits = 0;
    c->rb_value = 0;
    for (i = 0; i < 8; i++)
        qadd(c, first + (int64_t)i * c->cfg.vsm_spacing, Q_RBBIT, i);
    /* UNMEASURED (§3): a command written after Read Byte waits for the
     * eighth bit, then the normal latency. */
    c->busy_until = first + 7 * (int64_t)c->cfg.vsm_spacing;
}

/* Execute a command latched (or released from hold) at slot ref, whose first
 * VSM function may occur at slot s0. */
static void exec_cmd(tms5220 *c, int byte, int64_t ref, int64_t s0)
{
    int cmd = (byte >> 4) & 7;
    int64_t r;
    switch (cmd) {
    case 1:                                  /* Read Byte */
        read_byte_start(c, ref, s0);
        break;
    case 3:                                  /* Read and Branch */
        qadd(c, s0, Q_BRANCH, 0);
        c->need_dummy = 0;                   /* §4: no Dummy Read needed after it */
        c->busy_until = s0;
        /* §3 "When READY returns": READY for the next write goes active at
         * the start of the first slot 15 at least branch_ready slots after
         * the Branch function, whenever the write is made. */
        r = s0 + c->cfg.branch_ready;
        r += (15 - r % TMS5220_SLOTS_PER_SAMPLE + TMS5220_SLOTS_PER_SAMPLE) % TMS5220_SLOTS_PER_SAMPLE;
        c->ready_min = r;
        break;
    case 4:                                  /* Load Address */
        qadd(c, s0, Q_LOAD, byte & 0xF);
        c->need_dummy = 1;
        c->busy_until = s0;
        break;
    default:
        break;
    }
}

static void do_reset(tms5220 *c, int64_t L)
{
    int64_t s0 = L + c->cfg.vsm_delay;
    /* §6: Reset halts speech at once and resets the noise generator, not the
     * frame clock or the coded parameters.  §8.9 "Not known for this path":
     * the output after a Reset during speech is treated as the end of speech
     * (not talking, step 3 from then on).  DATASHEET GAP: what happens to
     * Z, U and the queued writes is not said; they are left as they are. */
    if (c->talking || c->requested || c->ts)
        ev_speech(c, "reset_halt", 0, 0);
    c->nq = 0;                               /* cancel pending host-side actions */
    c->talking = 0;
    c->requested = 0;
    c->fr_active = 0;
    c->newf_valid = 0;
    c->pend_speak = 0;
    c->ts_clear_at = -1;
    c->se_active = 0;
    c->se_ready = 0;
    c->data_mode = 0;
    /* UNMEASURED (§3): what a held command does when a Reset arrives:
     * discarded. */
    c->held_cmd = -1;
    set_flag(c, TMS5220_FLAG_TS, 0);
    fifo_purge(c);
    /* §7: Reset holds the noise register at 0x1FFF. */
    c->noise = noise_hold_state(c);
    c->noise_held = 1;
    /* §4: Load Address, then a Dummy Read. */
    qadd(c, s0, Q_LOAD, c->cfg.reset_nibble);
    qadd(c, s0 + c->cfg.vsm_spacing, Q_DUMMY, 0);
    c->need_dummy = 0;
    c->busy_until = s0 + c->cfg.vsm_spacing;
    /* UNMEASURED (§3): whether Reset releases INT (the original says so):
     * it does, after its own TS/BL changes. */
    set_int(c, 0);
}

/* A write latched into the command register at slot L (READY active). */
static void latch_cmd(tms5220 *c, int byte, int64_t L)
{
    int cmd = (byte >> 4) & 7;
    int64_t s0 = L + c->cfg.vsm_delay;

    c->data_mode = 0;
    switch (cmd) {
    case 0:
    case 2:                                  /* NOP */
        break;
    case 1:                                  /* Read Byte */
    case 3:                                  /* Read and Branch */
    case 4:                                  /* Load Address */
        if (cmd == 1)
            c->data_mode = 1;
        if (c->ts) {
            /* §6: held until speech ends (TS clears). */
            c->held_cmd = byte;
            ev_speech(c, "held", byte, 0);
        } else {
            exec_cmd(c, byte, L, s0);
        }
        break;
    case 5:                                  /* Speak */
        if (c->ts || c->pend_speak) {
            /* §6: ignored during speech.  UNMEASURED (§3): a Speak while
             * another waits for its frame boundary: also ignored. */
            ev_speech(c, "ignored", byte, 0);
            break;
        }
        if (c->need_dummy) {
            /* UNMEASURED (§3): when the Speak's Dummy Read occurs. */
            qadd(c, s0, Q_DUMMY, 0);
            c->need_dummy = 0;
        }
        c->busy_until = s0;
        c->pend_speak = 1;
        c->pend_L = L;
        qadd(c, L + c->cfg.ts_set_delay, Q_TS_SET, 0);
        if (c->noise_held)
            qadd(c, L + c->cfg.noise_rel_speak, Q_NOISE_REL, 0);
        break;
    case 6:                                  /* Speak External */
        if (c->ts || c->pend_speak) {
            /* UNMEASURED (§3): Speak External during a VSM Speak: ignored. */
            ev_speech(c, "ignored", byte, 0);
            break;
        }
        /* Original §6.5: purge the FIFO (BL, BE set) and route writes to it.
         * UNMEASURED (§3): whether this purge raises INT: it does when it
         * sets BL (the §3 rule). */
        fifo_purge(c);
        c->se_active = 1;
        c->se_ready = 0;
        c->se_L = L;
        if (c->noise_held)
            qadd(c, L + c->cfg.noise_rel_se, Q_NOISE_REL, 0);
        break;
    case 7:                                  /* Reset */
        do_reset(c, L);
        break;
    }
}

static void ts_cleared(tms5220 *c)
{
    c->ts_clear_at = -1;
    set_flag(c, TMS5220_FLAG_TS, 0);
    /* UNMEASURED (§3): when writes stop going to the FIFO after a Speak
     * External ends: when TS clears. */
    c->se_active = 0;
    c->se_ready = 0;
    if (c->held_cmd >= 0) {
        /* §6: a held command reaches the VSM at the moment TS clears; a held
         * Read Byte reads from slot 13 of the next sample period (§4). */
        int b = c->held_cmd;
        c->held_cmd = -1;
        ev_speech(c, "held_exec", b, 0);
        exec_cmd(c, b, c->slot, c->slot);
    }
}

/* ------------------------------------------------------------------ */
/* Host cycles                                                          */
/* ------------------------------------------------------------------ */

int tms5220_status(const tms5220 *c)
{
    /* §5: bit 7 TS, bit 6 BL, bit 5 BE, low five bits read as 1. */
    return (c->ts << 7) | (c->bl << 6) | (c->be << 5) | 0x1F;
}

static void host_done(tms5220 *c, int value)
{
    tms5220_event e;
    c->h_active = 0;
    c->h_value = value;
    memset(&e, 0, sizeof e);
    e.type = TMS5220_EV_HOST;
    e.slot = c->h_start;
    e.kind = c->h_kind;
    e.value = value;
    e.start = c->h_start;
    e.ready = c->slot;
    emit(c, &e);
}

static void host_progress(tms5220 *c)
{
    int64_t s = c->slot;
    if (!c->h_active)
        return;
    if (c->h_write) {
        if (c->h_kind == TMS5220_HOST_FIFO) {
            if (s >= c->h_start + c->cfg.fifo_latch) {
                fifo_write(c, c->h_value);
                host_done(c, c->h_value);
            }
            return;
        }
        if (c->held_cmd >= 0 && c->ts) {
            /* §3: a second command written while one is held keeps READY
             * inactive.  UNMEASURED (§3): what releases it: here, the held
             * command running at the end of speech. */
            return;
        }
        {
            /* UNMEASURED (§3): what a command waits for after Read Byte or a
             * Load Address: the latency counts from the end of the busy
             * period.  After Read and Branch READY returns at ready_min (§3),
             * or later with the normal latency. */
            int64_t from = c->h_start > c->busy_until ? c->h_start : c->busy_until;
            int64_t ready = from + c->cfg.cmd_latch;
            if (ready < c->ready_min)
                ready = c->ready_min;
            if (s >= ready && c->held_cmd < 0) {
                latch_cmd(c, c->h_value, s);
                host_done(c, c->h_value);
            }
        }
        return;
    }
    /* read */
    if (s < c->h_start + c->cfg.read_ready)
        return;
    if (c->data_mode) {
        if (c->held_cmd >= 0) {
            /* §3: while a Read Byte is held during speech, reads do not wait:
             * they return 0xFF at once and do not use it up.  DATASHEET GAP:
             * whether such a read acts as a status read is not said; it is
             * logged as a data read and leaves INT as it is. */
            c->h_kind = TMS5220_HOST_DATA;
            host_done(c, 0xFF);
            return;
        }
        /* §3: the data read's READY goes active 4 slots after the eighth bit
         * is read.  DATASHEET GAP: for a read made later, the normal
         * status-read latency applies (whichever is later). */
        if (!c->rb_done || s < c->rb_ready_at)
            return;
        /* UNMEASURED (§3): which reads return Read Byte data when it is not
         * held: only the first read after the Read Byte. */
        c->data_mode = 0;
        c->h_kind = TMS5220_HOST_DATA;
        host_done(c, c->rb_value);
        return;
    }
    c->h_kind = TMS5220_HOST_STATUS;
    {
        int v = tms5220_status(c);
        set_int(c, 0);                       /* §3: a status read releases INT */
        host_done(c, v);
    }
}

int tms5220_write(tms5220 *c, int byte)
{
    if (c->h_active)
        return -1;
    c->h_active = 1;
    c->h_write = 1;
    c->h_value = byte & 0xFF;
    c->h_start = c->slot;
    /* §5: during Speak External every write is FIFO data. */
    c->h_kind = c->se_active ? TMS5220_HOST_FIFO : TMS5220_HOST_CMD;
    return 0;
}

int tms5220_read(tms5220 *c)
{
    if (c->h_active)
        return -1;
    c->h_active = 1;
    c->h_write = 0;
    c->h_start = c->slot;
    c->h_kind = TMS5220_HOST_STATUS;
    return 0;
}

int tms5220_host_busy(const tms5220 *c) { return c->h_active; }
int tms5220_host_value(const tms5220 *c) { return c->h_value; }
int tms5220_talking(const tms5220 *c)
{
    return c->ts || c->talking || c->requested || c->pend_speak || c->ts_clear_at >= 0;
}
int tms5220_int_active(const tms5220 *c) { return c->intr; }
int tms5220_code(const tms5220 *c) { return c->code; }

int64_t tms5220_next_boundary(const tms5220 *c)
{
    /* The next slot at which a VSM frame read would start (frame_pos 0,
     * slot frame_read_slot), at or after the current slot. */
    int64_t per = c->slot / TMS5220_SLOTS_PER_SAMPLE;
    int k = (int)(c->slot % TMS5220_SLOTS_PER_SAMPLE);
    int fp = c->frame_pos;
    int64_t r;
    if (k == 0 && c->slot != 0)
        fp = (fp + 1) % TMS5220_SAMPLES_PER_FRAME;   /* period 'per' not started yet */
    r = (per + (TMS5220_SAMPLES_PER_FRAME - fp) % TMS5220_SAMPLES_PER_FRAME) * TMS5220_SLOTS_PER_SAMPLE
        + c->cfg.frame_read_slot;
    if (r < c->slot)
        r += TMS5220_SLOTS_PER_FRAME;
    return r;
}

/* ------------------------------------------------------------------ */
/* The slot                                                             */
/* ------------------------------------------------------------------ */

static void run_queue(tms5220 *c)
{
    int i = 0;
    while (i < c->nq) {
        tms5220_qent e = c->q[i];
        if (e.slot > c->slot) {
            i++;
            continue;
        }
        c->q[i] = c->q[--c->nq];
        switch (e.op) {
        case Q_LOAD:
            vsm_call(c, TMS5220_VSM_LOAD, e.arg);
            break;
        case Q_DUMMY:
            vsm_call(c, TMS5220_VSM_DUMMY, 0);
            break;
        case Q_BRANCH:
            vsm_call(c, TMS5220_VSM_BRANCH, 0);
            break;
        case Q_RBBIT: {
            int bit = vsm_call(c, TMS5220_VSM_READ, 0);
            /* §6: the first bit read appears on D0, TI's MSB (host bit 7). */
            c->rb_value |= bit << (7 - c->rb_bits);
            if (++c->rb_bits == 8) {
                c->rb_done = 1;
                c->rb_ready_at = c->slot + 4;    /* §3: READY 4 slots later */
            }
            break;
        }
        case Q_TS_SET:
            if (c->pend_speak)
                set_flag(c, TMS5220_FLAG_TS, 1);
            break;
        case Q_NOISE_REL:
            c->noise_held = 0;
            break;
        }
    }
}

void tms5220_step(tms5220 *c)
{
    if (c->slot % TMS5220_SLOTS_PER_SAMPLE == 0)
        period_start(c);
    if (c->ts_clear_at >= 0 && c->slot >= c->ts_clear_at)
        ts_cleared(c);
    run_queue(c);
    if (c->fr_active && c->slot == c->fr_next)
        frame_read_slot(c);
    host_progress(c);
    /* §7, §8.6: the noise register steps once per slot unless held. */
    if (!c->noise_held)
        c->noise = tms5220_noise_step(c->noise);
    c->slot++;
}
