/*
 * tms5220.h - reference emulator of the TI TMS5220 voice synthesis processor.
 *
 * Written ONLY from docs-tech/tms5220_reference_datasheet/tms5220_reference_datasheet.md
 * (and, where that document says the original still applies, TI's original
 * data manual).  No other emulator was consulted.  Every place where the
 * datasheet leaves a choice open is marked "DATASHEET GAP:" in tms5220.c and
 * listed in GAPS.md.
 *
 * The chip is stepped one slot at a time.  A slot is one ROMCLK period
 * (6.25 us at 640 kHz); a sample period is 20 slots (datasheet §2, §8.4).
 * Absolute slot 0 is power-up and is slot 0 of a sample period.
 */
#ifndef TMS5220_H
#define TMS5220_H

#include <stdint.h>

#define TMS5220_SLOTS_PER_SAMPLE  20
#define TMS5220_SAMPLES_PER_FRAME 200
#define TMS5220_SLOTS_PER_FRAME   (TMS5220_SLOTS_PER_SAMPLE * TMS5220_SAMPLES_PER_FRAME)

/* VSM functions issued on M0/M1 (§4).  The callback returns the data bit for
 * TMS5220_VSM_READ and is ignored otherwise.  'nibble' is the Load Address
 * nibble (ADD8..ADD1, bit 0 = ADD1) for TMS5220_VSM_LOAD, else 0. */
enum tms5220_vsm_func {
    TMS5220_VSM_LOAD = 1,
    TMS5220_VSM_READ = 2,
    TMS5220_VSM_DUMMY = 3,
    TMS5220_VSM_BRANCH = 4
};
typedef int (*tms5220_vsm_fn)(void *ctx, int func, int nibble);

/* Events reported through the event callback. */
enum tms5220_event_type {
    TMS5220_EV_VSM = 1,     /* a VSM function: func, arg (nibble, or bit read) */
    TMS5220_EV_HOST,        /* a completed host cycle: kind, value, start, ready */
    TMS5220_EV_FLAG,        /* a change of TS, BL, BE or INT: flag, state */
    TMS5220_EV_SPEECH       /* internal milestones (informative): what, a, b */
};
enum tms5220_host_kind {
    TMS5220_HOST_CMD = 1,   /* write latched into the command register */
    TMS5220_HOST_FIFO,      /* write routed to the FIFO (Speak External) */
    TMS5220_HOST_STATUS,    /* read returning the status register */
    TMS5220_HOST_DATA       /* read returning the Read Byte data register */
};
enum tms5220_flag { TMS5220_FLAG_TS = 1, TMS5220_FLAG_BL, TMS5220_FLAG_BE, TMS5220_FLAG_INT };

typedef struct {
    int type;
    int64_t slot;           /* slot at which the event happened (host: strobe slot) */
    int func, arg;          /* VSM */
    int kind, value;        /* host */
    int64_t start, ready;   /* host: strobe slot, slot READY went active */
    int flag, state;        /* flag: which, new state (1 = set / INT active) */
    const char *what;       /* speech milestone name */
    int a, b;               /* speech milestone details */
} tms5220_event;
typedef void (*tms5220_event_fn)(void *ctx, const tms5220_event *ev);

/* Called once per sample period, at its slot 0, with the 10-bit I/O code sent
 * in slots 2..11 of that period (§8.4).  'index' is the sample period number
 * since power-up; 'slot' is the absolute slot at which the period began. */
typedef void (*tms5220_sample_fn)(void *ctx, int64_t index, int64_t slot, int code);

/* Timing choices that the datasheet does not pin down to the slot.  Each one
 * is a DATASHEET GAP (see GAPS.md); the defaults are the choices made there. */
typedef struct {
    int cmd_latch;          /* slots from write strobe to command latched / READY active */
    int fifo_latch;         /* slots from write strobe to FIFO byte latched / READY active */
    int read_ready;         /* slots from read strobe to READY active */
    int vsm_delay;          /* slots from command latch to its first VSM function */
    int vsm_spacing;        /* slots between consecutive VSM functions (12.5 us = 2) */
    int branch_timer;       /* slots the chip stays busy after a Read and Branch function */
    int speak_lead;         /* min slots from command accepted to the first frame read */
    int ts_set_delay;       /* slots from Speak latched to Talk Status set */
    int ts_clear_stop;      /* slots from a stop frame's first bit to TS clear (24.87 ms) */
    int ts_clear_first_stop;/* same when the first frame is a stop code (0.09 ms) */
    int noise_rel_speak;    /* slots from Speak accepted to the noise register stepping */
    int noise_rel_se;       /* same for Speak External */
    int noise_hold_steps;   /* held state = 0x1FFF advanced this many steps */
    int se_shift;           /* Speak External runs this many samples ahead of Speak */
    int frame_read_slot;    /* slot of the sample period in which a frame read starts */
    int reset_nibble;       /* Load Address nibble issued by Reset */
} tms5220_config;

typedef struct {
    int e, rep, pitch, k[10];   /* coded indices */
    int nbits, field, fbits, acc, done;
} tms5220_frame;

typedef struct {
    int64_t slot;
    int op, arg;
} tms5220_qent;

typedef struct tms5220 {
    tms5220_config cfg;

    /* callbacks */
    tms5220_vsm_fn vsm;       void *vsm_ctx;
    tms5220_event_fn on_event; void *event_ctx;
    tms5220_sample_fn on_sample; void *sample_ctx;

    /* counters (§7, §8.5) */
    int64_t slot;             /* the next slot to be executed */
    int64_t period;           /* sample period index of the current period */
    int frame_pos;            /* frame clock: 0..199, 0 = PC 12 of IP 7 (VSM read) */
    int synth_off;            /* synthesis position = frame_pos + synth_off */
    int synth_pos;

    /* host interface (§3, §5, §6) */
    int h_active, h_write, h_kind, h_value;
    int64_t h_start;
    int64_t busy_until;       /* chip cannot latch a new command before this */
    int held_cmd;             /* command held during speech, or -1 */
    int need_dummy;           /* a Load Address has been issued since the last read */
    int data_mode;            /* next read returns the Read Byte data */
    int rb_done, rb_bits, rb_value;

    tms5220_qent q[64];
    int nq;

    /* status flags */
    int ts, bl, be, intr;
    int64_t ts_clear_at;

    /* speech control (host side) */
    int pend_speak;  int64_t pend_L;
    int se_active;   int64_t se_L;
    int se_ready;    int64_t se_ready_at;
    int speak_se, utt_frames;

    /* frame input */
    int fr_active; int64_t fr_next, fr_start;
    tms5220_frame fr;         /* frame being read */
    tms5220_frame newf;       /* frame read, waiting to be loaded at PC 12 of IP 0 */
    int newf_valid;

    /* FIFO (§5): 16 bytes, read LSB first */
    uint8_t fifo[16];
    int fifo_n, fifo_bit;

    /* noise generator (§7, §8.6) */
    unsigned noise;
    int noise_held;

    /* synthesis state, named as in §8.9 "Reference algorithm (exact)" */
    int idx[12];              /* coded indices: energy, pitch, K1..K10 */
    int cur[12];              /* interpolator registers */
    int seen[12];             /* values used by the filter / pitch counter */
    struct { int64_t due; int i, val; } wq[64];
    int nwq;
    int talking, requested;
    int Z, U, loads, lastU, inhibit, oldS, oldU, oldU_prev, hold, count;

    /* lattice (§8.8) */
    int32_t u[12], b[11];
    int code;
} tms5220;

/* Construction and configuration. */
void tms5220_default_config(tms5220_config *cfg);
void tms5220_init(tms5220 *c, const tms5220_config *cfg /* NULL = defaults */);
void tms5220_set_vsm(tms5220 *c, tms5220_vsm_fn fn, void *ctx);
void tms5220_set_event_cb(tms5220 *c, tms5220_event_fn fn, void *ctx);
void tms5220_set_sample_cb(tms5220 *c, tms5220_sample_fn fn, void *ctx);
/* Set the frame-clock phase of power-up: 'sample' is the position (0..199) in
 * the 200-sample frame cycle of the sample period that starts at slot 0;
 * 0 = the period in which a frame would be read (PC 12 of IP 7).  Call before
 * the first step. */
void tms5220_set_phase(tms5220 *c, int sample);

/* Advance one slot. */
void tms5220_step(tms5220 *c);

/* Host cycles.  A cycle starts at the current slot (before it is stepped).
 * Only one cycle may be in progress; these return -1 if one is.  Step the chip
 * until tms5220_host_done() is true: READY has then gone active. */
int tms5220_write(tms5220 *c, int byte);
int tms5220_read(tms5220 *c);
int tms5220_host_busy(const tms5220 *c);       /* 1 while READY is inactive */
int tms5220_host_value(const tms5220 *c);      /* value of the last completed read */

/* State inspection. */
int tms5220_status(const tms5220 *c);          /* status byte as a read would see it */
int tms5220_talking(const tms5220 *c);         /* TS set, or speech or a Speak pending */
int tms5220_int_active(const tms5220 *c);
int tms5220_code(const tms5220 *c);            /* I/O code of the current period */
int64_t tms5220_next_boundary(const tms5220 *c);/* slot of the next VSM frame read point */

/* Arithmetic exposed for testing (§8.8). */
int32_t tms5220_booth(int32_t m, int k);
int32_t tms5220_asr(int32_t x, int n);
int32_t tms5220_wrap14(int32_t x);
unsigned tms5220_noise_step(unsigned n);

/* Optional analogue output (§8.2): approximate SPEAKER current in uA for a
 * code, from the Table 8-2 bit weights.  load = 0: 996 ohm, 1: 482 ohm. */
double tms5220_dac_current_ua(int code, int load);

#endif
