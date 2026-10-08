/*
 * tms5220emu.c - command-line driver for the TMS5220 reference emulator.
 * Written from the reference datasheet only; see README.md and GAPS.md.
 *
 *   tms5220emu [options] SCRIPT          run a scenario script
 *   tms5220emu [options] --image F --addr A [--speak-at N] [--external] ...
 *
 * See usage() and README.md.
 */
#include "tms5220.h"
#include "tms6100.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WAIT_SLOTS (160000LL * 60)   /* 60 s */

typedef struct {
    tms5220 chip;
    tms6100 vsm;
    FILE *fs;              /* samples */
    FILE *fe;              /* events */
    int quiet_reads;       /* do not log VSM READ functions of frames */
    int idle_run;          /* consecutive samples with code -1 */
    int last_read;         /* value of the last host read */
    const char *where;     /* current script file, for messages */
    int line;
} emu;

/* ------------------------------------------------------------------ */
/* Output                                                               */
/* ------------------------------------------------------------------ */

static void on_sample(void *ctx, int64_t index, int64_t slot, int code)
{
    emu *e = ctx;
    if (e->fs)
        fprintf(e->fs, "%lld %lld %d\n", (long long)index, (long long)slot, code);
    e->idle_run = code == -1 ? e->idle_run + 1 : 0;
}

static const char *flag_name(int f)
{
    return f == TMS5220_FLAG_TS ? "TS" : f == TMS5220_FLAG_BL ? "BL" : f == TMS5220_FLAG_BE ? "BE" : "INT";
}

static void on_event(void *ctx, const tms5220_event *ev)
{
    emu *e = ctx;
    if (!e->fe)
        return;
    switch (ev->type) {
    case TMS5220_EV_VSM:
        switch (ev->func) {
        case TMS5220_VSM_LOAD:
            fprintf(e->fe, "%lld VSM LOAD %d\n", (long long)ev->slot, ev->arg);
            break;
        case TMS5220_VSM_READ:
            if (!e->quiet_reads)
                fprintf(e->fe, "%lld VSM READ %d\n", (long long)ev->slot, ev->arg);
            break;
        case TMS5220_VSM_DUMMY:
            fprintf(e->fe, "%lld VSM DUMMY\n", (long long)ev->slot);
            break;
        case TMS5220_VSM_BRANCH:
            fprintf(e->fe, "%lld VSM BRANCH\n", (long long)ev->slot);
            break;
        }
        break;
    case TMS5220_EV_HOST:
        fprintf(e->fe, "%lld HOST %s 0x%02X ready %lld\n", (long long)ev->start,
                ev->kind == TMS5220_HOST_CMD ? "WRITE_CMD" :
                ev->kind == TMS5220_HOST_FIFO ? "WRITE_FIFO" :
                ev->kind == TMS5220_HOST_STATUS ? "READ_STATUS" : "READ_DATA",
                ev->value, (long long)ev->ready);
        break;
    case TMS5220_EV_FLAG:
        fprintf(e->fe, "%lld FLAG %s %d\n", (long long)ev->slot, flag_name(ev->flag), ev->state);
        break;
    case TMS5220_EV_SPEECH:
        fprintf(e->fe, "%lld INFO %s %d %d\n", (long long)ev->slot, ev->what, ev->a, ev->b);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Operations                                                           */
/* ------------------------------------------------------------------ */

static void die(emu *e, const char *msg, const char *arg)
{
    if (e->where)
        fprintf(stderr, "tms5220emu: %s:%d: %s%s%s\n", e->where, e->line, msg, arg ? " " : "", arg ? arg : "");
    else
        fprintf(stderr, "tms5220emu: %s%s%s\n", msg, arg ? " " : "", arg ? arg : "");
    exit(2);
}

static void step_to(emu *e, int64_t slot)
{
    while (e->chip.slot < slot)
        tms5220_step(&e->chip);
}

/* Run the host cycle just started until READY goes active. */
static void finish_cycle(emu *e)
{
    int64_t limit = e->chip.slot + MAX_WAIT_SLOTS;
    while (tms5220_host_busy(&e->chip)) {
        if (e->chip.slot >= limit) {
            fprintf(stderr, "tms5220emu: warning: READY did not go active within 60 s (slot %lld); cycle abandoned\n",
                    (long long)e->chip.slot);
            e->chip.h_active = 0;
            return;
        }
        tms5220_step(&e->chip);
    }
    /* READY went active during the last slot executed; the next operation
     * starts at the following slot. */
}

static void op_write(emu *e, int byte)
{
    if (tms5220_write(&e->chip, byte) < 0)
        die(e, "host cycle already in progress", NULL);
    finish_cycle(e);
}

static int op_read(emu *e)
{
    if (tms5220_read(&e->chip) < 0)
        die(e, "host cycle already in progress", NULL);
    finish_cycle(e);
    e->last_read = tms5220_host_value(&e->chip);
    return e->last_read;
}

static void op_until_idle(emu *e, int64_t margin)
{
    int64_t limit = e->chip.slot + MAX_WAIT_SLOTS;
    while (tms5220_talking(&e->chip) || e->idle_run < 25) {
        if (e->chip.slot >= limit) {
            fprintf(stderr, "tms5220emu: warning: until_idle gave up after 60 s at slot %lld\n",
                    (long long)e->chip.slot);
            break;
        }
        tms5220_step(&e->chip);
    }
    step_to(e, e->chip.slot + margin);
}

/* Advance so that a command written now is latched exactly n slots before the
 * next VSM frame-read boundary that is far enough ahead. */
static void op_boundary(emu *e, int64_t n)
{
    int64_t lat = e->chip.cfg.cmd_latch;
    int64_t r = tms5220_next_boundary(&e->chip);
    while (r - n - lat < e->chip.slot)
        r += TMS5220_SLOTS_PER_FRAME;
    step_to(e, r - n - lat);
}

static int set_param(tms5220_config *c, const char *k, int v)
{
    struct { const char *name; int *p; } t[] = {
        { "cmd_latch", &c->cmd_latch }, { "fifo_latch", &c->fifo_latch },
        { "read_ready", &c->read_ready }, { "vsm_delay", &c->vsm_delay },
        { "vsm_spacing", &c->vsm_spacing }, { "branch_ready", &c->branch_ready },
        { "speak_lead", &c->speak_lead }, { "ts_set_delay", &c->ts_set_delay },
        { "ts_clear_stop", &c->ts_clear_stop }, { "ts_clear_first_stop", &c->ts_clear_first_stop },
        { "noise_rel_speak", &c->noise_rel_speak }, { "noise_rel_se", &c->noise_rel_se },
        { "noise_hold_steps", &c->noise_hold_steps }, { "se_shift", &c->se_shift },
        { "frame_read_slot", &c->frame_read_slot }, { "reset_nibble", &c->reset_nibble },
    };
    size_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(t[i].name, k)) {
            *t[i].p = v;
            return 0;
        }
    return -1;
}

static long long parse_num(emu *e, const char *s, int base)
{
    char *end;
    long long v;
    if (!s)
        die(e, "missing argument", NULL);
    v = strtoll(s, &end, base);
    if (*end)
        die(e, "bad number", s);
    return v;
}

static void run_line(emu *e, char *line)
{
    char *argv[64];
    int argc = 0, i;
    char *p = strchr(line, '#');
    if (p)
        *p = 0;
    for (p = strtok(line, " \t\r\n"); p && argc < 64; p = strtok(NULL, " \t\r\n"))
        argv[argc++] = p;
    if (argc == 0)
        return;

    if (!strcmp(argv[0], "load")) {
        long n;
        if (argc < 3)
            die(e, "usage: load <file> <hex address>", NULL);
        n = tms6100_load_file(&e->vsm, argv[1], (uint32_t)parse_num(e, argv[2], 16));
        if (n < 0)
            die(e, "cannot read", argv[1]);
    } else if (!strcmp(argv[0], "phase")) {
        if (e->chip.slot != 0)
            die(e, "phase must be set before anything runs", NULL);
        tms5220_set_phase(&e->chip, (int)parse_num(e, argv[1], 10));
    } else if (!strcmp(argv[0], "set")) {
        if (e->chip.slot != 0)
            die(e, "set must be used before anything runs", NULL);
        if (argc < 3 || set_param(&e->chip.cfg, argv[1], (int)parse_num(e, argv[2], 10)) < 0)
            die(e, "unknown parameter", argc > 1 ? argv[1] : NULL);
        if (!strcmp(argv[1], "noise_hold_steps")) {
            /* re-derive the held noise state */
            unsigned n = 0x1FFF;
            int k;
            for (k = 0; k < e->chip.cfg.noise_hold_steps; k++)
                n = tms5220_noise_step(n);
            e->chip.noise = n;
        }
    } else if (!strcmp(argv[0], "slot")) {
        int64_t s = parse_num(e, argv[1], 10);
        if (s < e->chip.slot)
            die(e, "slot is in the past:", argv[1]);
        step_to(e, s);
    } else if (!strcmp(argv[0], "at")) {
        /* as slot, but a slot already passed is not an error (replays of
         * recorded host cycles, which may arrive while READY is still held) */
        int64_t s = parse_num(e, argv[1], 10);
        if (s > e->chip.slot)
            step_to(e, s);
    } else if (!strcmp(argv[0], "wait")) {
        step_to(e, e->chip.slot + parse_num(e, argv[1], 10));
    } else if (!strcmp(argv[0], "write")) {
        op_write(e, (int)parse_num(e, argv[1], 16));
    } else if (!strcmp(argv[0], "writes")) {
        for (i = 1; i < argc; i++)
            op_write(e, (int)parse_num(e, argv[i], 16));
    } else if (!strcmp(argv[0], "status") || !strcmp(argv[0], "read")) {
        op_read(e);
    } else if (!strcmp(argv[0], "until_idle")) {
        op_until_idle(e, argc > 1 ? parse_num(e, argv[1], 10) : TMS5220_SLOTS_PER_FRAME);
    } else if (!strcmp(argv[0], "boundary")) {
        op_boundary(e, parse_num(e, argv[1], 10));
    } else if (!strcmp(argv[0], "mark")) {
        if (e->fe)
            fprintf(e->fe, "%lld MARK %s\n", (long long)e->chip.slot, argc > 1 ? argv[1] : "");
    } else {
        die(e, "unknown operation", argv[0]);
    }
}

static void run_script(emu *e, const char *path)
{
    char buf[4096];
    FILE *f = fopen(path, "r");
    const char *ow = e->where;
    int ol = e->line;
    if (!f)
        die(e, "cannot open script", path);
    e->where = path;
    e->line = 0;
    while (fgets(buf, sizeof buf, f)) {
        e->line++;
        run_line(e, buf);
    }
    fclose(f);
    e->where = ow;
    e->line = ol;
}

/* ------------------------------------------------------------------ */
/* Bulk mode                                                            */
/* ------------------------------------------------------------------ */

/* Bytes from 'addr' up to and including the last bit of the stop frame
 * (§8.1 frame format), or -1 if no stop code is found. */
static long phrase_bytes(const tms6100 *v, uint32_t addr)
{
    static const int kb[10] = { 5, 5, 4, 4, 4, 4, 4, 3, 3, 3 };
    uint64_t bit = 0, maxbits = (uint64_t)(TMS6100_MEM_SIZE - addr) * 8;
    for (;;) {
        unsigned val = 0, e, rep, pitch;
        int i, j, n;
        int fields[13], nf;
        /* energy */
        for (i = 0; i < 4; i++) {
            if (bit >= maxbits) return -1;
            val = (val << 1) | ((v->mem[addr + bit / 8] >> (bit % 8)) & 1u);
            bit++;
        }
        e = val;
        if (e == 15)
            return (long)((bit + 7) / 8);
        if (e == 0)
            continue;
        nf = 0;
        fields[nf++] = 1;
        fields[nf++] = 6;
        /* repeat and pitch */
        rep = 0;
        pitch = 0;
        for (j = 0; j < nf; j++) {
            val = 0;
            for (i = 0; i < fields[j]; i++) {
                if (bit >= maxbits) return -1;
                val = (val << 1) | ((v->mem[addr + bit / 8] >> (bit % 8)) & 1u);
                bit++;
            }
            if (j == 0) rep = val; else pitch = val;
        }
        if (rep)
            continue;
        n = pitch ? 10 : 4;
        for (j = 0; j < n; j++)
            for (i = 0; i < kb[j]; i++) {
                if (bit >= maxbits) return -1;
                bit++;
            }
    }
}

static void bulk(emu *e, uint32_t addr, int64_t speak_at, int external, int do_reset, int64_t margin)
{
    int i;
    /* Let any prior speech finish. */
    op_until_idle(e, 0);
    if (do_reset) {
        for (i = 0; i < 9; i++)
            op_write(e, 0xFF);
        op_write(e, 0x70);
    }
    if (!external) {
        for (i = 0; i < 5; i++)
            op_write(e, 0x40 | (int)((addr >> (4 * i)) & 0xF));
        op_boundary(e, speak_at);
        if (e->fe)
            fprintf(e->fe, "%lld MARK speak\n", (long long)e->chip.slot);
        op_write(e, 0x50);
    } else {
        long n = phrase_bytes(&e->vsm, addr), pos = 0;
        if (n < 0) {
            fprintf(stderr, "tms5220emu: warning: no stop code after 0x%05X; sending 256 bytes\n", (unsigned)addr);
            n = 256;
        }
        op_boundary(e, speak_at);
        if (e->fe)
            fprintf(e->fe, "%lld MARK speak_external\n", (long long)e->chip.slot);
        op_write(e, 0x60);
        /* Fill the FIFO, then top it up by 8 bytes whenever a status read
         * (every 1 ms) shows Buffer Low. */
        while (pos < n && pos < 16)
            op_write(e, e->vsm.mem[(addr + (uint32_t)pos++) & (TMS6100_MEM_SIZE - 1)]);
        while (pos < n) {
            int k;
            step_to(e, e->chip.slot + 160);
            if (!(op_read(e) & 0x40))
                continue;
            for (k = 0; k < 8 && pos < n; k++)
                op_write(e, e->vsm.mem[(addr + (uint32_t)pos++) & (TMS6100_MEM_SIZE - 1)]);
        }
    }
    op_until_idle(e, margin);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    fprintf(stderr,
        "usage: tms5220emu [-s SAMPLES] [-e EVENTS] [-q] SCRIPT\n"
        "       tms5220emu [-s SAMPLES] [-e EVENTS] [-q] --image FILE --addr HEX\n"
        "                  [--load-at HEX] [--speak-at SLOTS] [--external] [--reset]\n"
        "                  [--prior SCRIPT] [--phase SAMPLE] [--margin SLOTS]\n"
        "  -s FILE   write the I/O code of every sample period (default stdout)\n"
        "  -e FILE   write the event log (default stdout; '-' = stdout, 'none' = off)\n"
        "  -q        omit VSM READ functions from the event log\n"
        "Bulk mode: load FILE at --load-at (default: --addr), run --prior, then speak\n"
        "the phrase at --addr with the Speak (or Speak External) command accepted\n"
        "--speak-at slots (default 1961) before a frame boundary.\n");
    exit(2);
}

static FILE *open_out(const char *path)
{
    FILE *f;
    if (!path || !strcmp(path, "-"))
        return stdout;
    if (!strcmp(path, "none"))
        return NULL;
    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "tms5220emu: cannot write %s\n", path);
        exit(2);
    }
    return f;
}

int main(int argc, char **argv)
{
    static emu e;
    const char *spath = NULL, *epath = NULL, *script = NULL, *image = NULL, *prior = NULL;
    long long addr = -1, load_at = -1, speak_at = 1961, margin = TMS5220_SLOTS_PER_FRAME;
    int external = 0, do_reset = 0, phase = 0, i;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
#define NEXT() (i + 1 < argc ? argv[++i] : (usage(), (char *)0))
        if (!strcmp(a, "-s")) spath = NEXT();
        else if (!strcmp(a, "-e")) epath = NEXT();
        else if (!strcmp(a, "-q")) e.quiet_reads = 1;
        else if (!strcmp(a, "--image")) image = NEXT();
        else if (!strcmp(a, "--addr")) addr = strtoll(NEXT(), NULL, 16);
        else if (!strcmp(a, "--load-at")) load_at = strtoll(NEXT(), NULL, 16);
        else if (!strcmp(a, "--speak-at")) speak_at = strtoll(NEXT(), NULL, 10);
        else if (!strcmp(a, "--external")) external = 1;
        else if (!strcmp(a, "--reset")) do_reset = 1;
        else if (!strcmp(a, "--prior")) prior = NEXT();
        else if (!strcmp(a, "--phase")) phase = atoi(NEXT());
        else if (!strcmp(a, "--margin")) margin = strtoll(NEXT(), NULL, 10);
        else if (a[0] == '-' && a[1]) usage();
        else if (!script) script = a;
        else usage();
#undef NEXT
    }
    if (!script && !image)
        usage();

    if (tms6100_init(&e.vsm) < 0) {
        fprintf(stderr, "tms5220emu: out of memory\n");
        return 2;
    }
    tms5220_init(&e.chip, NULL);
    e.fs = open_out(spath);
    e.fe = open_out(epath);
    tms5220_set_vsm(&e.chip, tms6100_func, &e.vsm);
    tms5220_set_event_cb(&e.chip, on_event, &e);
    tms5220_set_sample_cb(&e.chip, on_sample, &e);

    if (image) {
        if (addr < 0)
            usage();
        if (load_at < 0)
            load_at = addr;
        tms5220_set_phase(&e.chip, phase);
        if (tms6100_load_file(&e.vsm, image, (uint32_t)load_at) < 0) {
            fprintf(stderr, "tms5220emu: cannot read %s\n", image);
            return 2;
        }
        if (prior)
            run_script(&e, prior);
        bulk(&e, (uint32_t)addr, speak_at, external, do_reset, margin);
    } else {
        run_script(&e, script);
    }
    if (e.fs && e.fs != stdout) fclose(e.fs);
    if (e.fe && e.fe != stdout) fclose(e.fe);
    tms6100_free(&e.vsm);
    return 0;
}
