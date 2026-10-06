/************************************************************************
    command.c

    Host command interpreter (USB serial)

    One command per line.  Every command ends its reply with a line that
    starts "OK" or "ERR"; firmware/PROTOCOL.md describes each command.
    Nothing is ever sent unprompted.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include "command.h"
#include "pins.h"
#include "tms5220.h"
#include "tms6100.h"
#include "version.h"

#define PROTOCOL_VERSION    1

#define LINE_MAX            256
#define MAX_TOKENS          20
#define HEX_TIMEOUT_US      2000000     // Between characters of a hex upload

// Speak External streams are uploaded into this buffer, then fed to the FIFO
#define EXTERNAL_MAX        32768

// Talk Status is checked when INT asserts.  It can also be read at an
// interval, as a fallback, so that a missed INT cannot hang a command (the
// command's timeout ends it anyway).  POLL sets the interval; 0, the
// default, reads only on INT, because a status read during speech can
// disturb the output (datasheet section 7).
#define DEFAULT_POLL_MS     0

// Talk Status is not set at once: a read 22us after a Speak still finds it
// clear (the datasheet allows a Speak after Load Address up to 287us before
// the next command), and Speak External sets it once nine bytes are in the FIFO.
// Until it is seen set, the status is polled at START_POLL_MS, and Talk
// Status clear only ends the wait after START_MS.
#define START_POLL_MS       1
#define START_MS            20
#define DEFAULT_TIMEOUT_MS  10000

// The FIFO holds 16 bytes.  Buffer Low means fewer than 8 remain, so 8
// more always fit.
#define FIFO_SIZE           16
#define FIFO_TOP_UP         8

static uint8_t external[EXTERNAL_MAX];
static uint32_t clockHz;
static uint32_t statusPollMs = DEFAULT_POLL_MS;

void commandSetClock(uint32_t hz)
{
    clockHz = hz;
}

// ---------------------------------------------------------------------
// Utilities

static uint32_t crc32(const uint8_t *data, uint32_t length)
{
    // CRC-32 (IEEE 802.3, as zlib.crc32)
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}

static bool parseNumber(const char *text, uint32_t *value)
{
    char *end;
    unsigned long v = strtoul(text, &end, 0);
    if (*text == '\0' || *end != '\0') return false;
    *value = (uint32_t)v;
    return true;
}

static bool parseNumber64(const char *text, uint64_t *value)
{
    char *end;
    unsigned long long v = strtoull(text, &end, 0);
    if (*text == '\0' || *end != '\0') return false;
    *value = (uint64_t)v;
    return true;
}

// Read one line (without its terminator) into line; returns its length
static int readLine(char *line, int size)
{
    int length = 0;
    bool overflow = false;
    while (true) {
        int c = getchar_timeout_us(1000000);
        if (c == PICO_ERROR_TIMEOUT) continue;
        if (c == '\r') continue;
        if (c == '\n') break;
        if (length < size - 1) line[length++] = (char)c;
        else overflow = true;
    }
    line[length] = '\0';
    return overflow ? -1 : length;
}

static int hexValue(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Receive length bytes as hex digits (whitespace is ignored) and check
// their CRC-32.  Returns NULL on success, else an error message.
static const char *receiveHex(uint8_t *buffer, uint32_t length, uint32_t crc)
{
    printf("GO\n");
    for (uint32_t i = 0; i < 2 * length; ) {
        int c = getchar_timeout_us(HEX_TIMEOUT_US);
        if (c == PICO_ERROR_TIMEOUT) return "timeout receiving data";
        if (isspace(c)) continue;
        int v = hexValue(c);
        if (v < 0) return "bad hex digit";
        if (i & 1) buffer[i / 2] |= (uint8_t)v;
        else buffer[i / 2] = (uint8_t)(v << 4);
        i++;
    }
    return crc32(buffer, length) == crc ? NULL : "CRC mismatch";
}

static void sendHex(const uint8_t *data, uint32_t length)
{
    static const char digits[] = "0123456789abcdef";
    char line[2 * 32 + 1];
    for (uint32_t i = 0; i < length; i += 32) {
        uint32_t n = length - i < 32 ? length - i : 32;
        for (uint32_t j = 0; j < n; j++) {
            line[2 * j] = digits[data[i + j] >> 4];
            line[2 * j + 1] = digits[data[i + j] & 0xF];
        }
        line[2 * n] = '\0';
        puts(line);
    }
}

static void setTrigger(bool on)
{
    gpio_put(PIN_SCOPE_TRIG, on);
    gpio_put(PIN_LED, on);
}

// ---------------------------------------------------------------------
// Speaking

typedef struct {
    const uint8_t *data;    // Speak External stream (NULL for Speak)
    uint32_t length;
    uint32_t written;
} feed_t;

typedef struct {
    uint8_t status;         // Last status register value
    bool talked;            // Talk Status was seen set
    uint32_t interrupts;    // INT assertions seen
    uint32_t statusReads;
    int64_t elapsedUs;      // From the start of the command to Talk Status clear
    uint64_t trigUs;        // The microsecond timer when SCOPE_TRIG rose
    int64_t lateUs;         // How late SCOPE_TRIG rose after the requested start time
    bool timed;             // A start time was requested
} talk_t;

static bool feed(feed_t *f, uint32_t count)
{
    while (count-- && f->written < f->length) {
        if (!tms5220Write(f->data[f->written])) return false;
        f->written++;
    }
    return true;
}

// Wait for Talk Status to clear, topping up the FIFO for Speak External.
// The status register is only read when INT asserts (or as a slow
// fallback), to keep bus activity during speech to a minimum.
static const char *waitTalk(feed_t *f, uint32_t timeoutMs, absolute_time_t start, talk_t *talk)
{
    absolute_time_t begun = get_absolute_time();
    absolute_time_t timeout = delayed_by_ms(start, timeoutMs);
    absolute_time_t poll = make_timeout_time_ms(START_POLL_MS);
    const char *error = NULL;

    while (true) {
        bool interrupt = !gpio_get(PIN_INT_N);
        if (interrupt || time_reached(poll)) {
            if (interrupt) talk->interrupts++;
            talk->statusReads++;
            if (!tms5220Read(&talk->status)) {
                error = "READY timeout reading status";
                break;
            }
            if (talk->status & TMS5220_STATUS_TS) talk->talked = true;
            if (f && (talk->status & TMS5220_STATUS_BL) && !feed(f, FIFO_TOP_UP)) {
                error = "READY timeout writing FIFO";
                break;
            }
            if (!(talk->status & TMS5220_STATUS_TS) &&
                (talk->talked || absolute_time_diff_us(begun, get_absolute_time()) > START_MS * 1000)) {
                break;
            }
            if (!talk->talked) poll = make_timeout_time_ms(START_POLL_MS);
            else if (statusPollMs) poll = make_timeout_time_ms(statusPollMs);
            else poll = at_the_end_of_time;
        }
        if (time_reached(timeout)) {
            error = "timeout waiting for Talk Status to clear";
            break;
        }
    }
    talk->elapsedUs = absolute_time_diff_us(start, get_absolute_time());
    return error;
}

static void replyTalk(const char *error, const talk_t *talk, const feed_t *f)
{
    printf("%s status=0x%02x talked=%d ints=%lu reads=%lu us=%lld", error ? "ERR" : "OK",
           talk->status, talk->talked, (unsigned long)talk->interrupts,
           (unsigned long)talk->statusReads, talk->elapsedUs);
    if (f) printf(" written=%lu", (unsigned long)f->written);
    printf(" trig_us=%llu", (unsigned long long)talk->trigUs);
    if (talk->timed) printf(" late_us=%lld", (long long)talk->lateUs);
    if (error) printf(" %s", error);
    printf("\n");
}

// Wait for the requested start time (if any), then raise SCOPE_TRIG and
// note when.  The microsecond timer and the trace's cycle counter both
// run from the crystal that clocks the TMS5220, so a start time can be
// placed exactly on the chip's frame grid (PROTOCOL.md).
static absolute_time_t begin(talk_t *talk, uint64_t atUs)
{
    if (talk->timed) {
        while (time_us_64() < atUs) tight_loop_contents();
    }
    uint32_t interrupts = save_and_disable_interrupts();
    talk->trigUs = time_us_64();
    setTrigger(true);
    restore_interrupts(interrupts);
    if (talk->timed) talk->lateUs = (int64_t)(talk->trigUs - atUs);
    return from_us_since_boot(talk->trigUs);
}

// ---------------------------------------------------------------------
// Commands

static void cmdInfo(void)
{
    printf("OK proto=%d firmware=%s sys_hz=%lu osc_hz=%lu trace_events=%d external_max=%d "
           "banks=", PROTOCOL_VERSION, FIRMWARE_VERSION, (unsigned long)clock_get_hz(clk_sys),
           (unsigned long)clockHz, TMS6100_TRACE_EVENTS, EXTERNAL_MAX);
    bool first = true;
    for (uint bank = 0; bank < TMS6100_BANKS; bank++) {
        const uint8_t *data = tms6100Bank(bank);
        if (!data) continue;
        printf("%s%u:%08lx", first ? "" : ",", bank,
               (unsigned long)crc32(data, TMS6100_BANK_SIZE));
        first = false;
    }
    printf("%s builtin_bank=%u\n", first ? "-" : "", tms6100BuiltinBank());
}

static void cmdStats(void)
{
    tms6100Stats_t stats;
    tms6100GetStats(&stats);
    printf("OK load_address=%lu read=%lu read_branch=%lu address=0x%05lx\n",
           (unsigned long)stats.loadAddressCount, (unsigned long)stats.readCount,
           (unsigned long)stats.readAndBranchCount, (unsigned long)stats.address);
}

static void cmdClock(uint32_t hz)
{
    // at_us: when the clock stopped or started, on the microsecond timer
    uint64_t atUs;
    if (hz == 0) {
        atUs = time_us_64();
        tms5220ClockStop();
        clockHz = 0;
    } else {
        atUs = time_us_64();
        uint32_t actual = tms5220ClockStart(hz);
        if (!actual) {
            printf("ERR frequency out of range\n");
            return;
        }
        clockHz = actual;
    }
    printf("OK osc_hz=%lu at_us=%llu\n", (unsigned long)clockHz, (unsigned long long)atUs);
}

static void cmdReset(void)
{
    // tms5220Reset() ends with a status read, which clears INT
    bool ok = tms5220Reset();
    uint8_t status = 0;
    ok &= tms5220Read(&status);
    printf("%s status=0x%02x\n", ok ? "OK" : "ERR READY timeout", status);
}

static void cmdUpload(uint32_t bank, uint32_t length, uint32_t crc)
{
    if (bank >= TMS6100_BANKS || length == 0 || length > TMS6100_BANK_SIZE) {
        printf("ERR bank must be 0-15 and length 1-%d\n", TMS6100_BANK_SIZE);
        return;
    }
    uint8_t *data = tms6100BankWrite(bank);
    if (!data) {
        printf("ERR all %d bank slots are in use (UNLOAD one)\n", TMS6100_SLOTS);
        return;
    }
    const char *error = receiveHex(data, length, crc);
    if (error) {
        tms6100BankUnload(bank);
        printf("ERR %s (bank %lu unloaded)\n", error, (unsigned long)bank);
        return;
    }
    memset(data + length, 0, TMS6100_BANK_SIZE - length);
    tms6100BankMap(bank);
    printf("OK bank=%lu length=%lu crc=%08lx\n", (unsigned long)bank, (unsigned long)length,
           (unsigned long)crc32(data, TMS6100_BANK_SIZE));
}

static void cmdUnload(const char *which)
{
    uint32_t bank;
    if (!strcasecmp(which, "ALL")) {
        for (bank = 0; bank < TMS6100_BANKS; bank++) tms6100BankUnload(bank);
    } else if (parseNumber(which, &bank) && bank < TMS6100_BANKS) {
        tms6100BankUnload(bank);
    } else {
        printf("ERR bank must be 0-15 or ALL\n");
        return;
    }
    printf("OK\n");
}

static void cmdTime(void)
{
    printf("OK us=%llu\n", (unsigned long long)time_us_64());
}

static void cmdSpeak(uint32_t address, uint32_t timeoutMs, bool timed, uint64_t atUs)
{
    talk_t talk = { .timed = timed };
    absolute_time_t start = begin(&talk, atUs);
    const char *error = tms5220Speak(address & 0xFFFFF) ? NULL : "READY timeout sending Speak";
    if (!error) error = waitTalk(NULL, timeoutMs, start, &talk);
    setTrigger(false);
    replyTalk(error, &talk, NULL);
}

static void cmdSpeakExternal(uint32_t length, uint32_t crc, uint32_t timeoutMs, bool timed,
                             uint64_t atUs)
{
    if (length == 0 || length > EXTERNAL_MAX) {
        printf("ERR length must be 1-%d\n", EXTERNAL_MAX);
        return;
    }
    const char *error = receiveHex(external, length, crc);
    if (error) {
        printf("ERR %s\n", error);
        return;
    }

    feed_t f = { external, length, 0 };
    talk_t talk = { .timed = timed };
    absolute_time_t start = begin(&talk, atUs);
    if (!tms5220Write(TMS5220_CMD_SPEAK_EXTERNAL)) error = "READY timeout sending Speak External";
    else if (!feed(&f, FIFO_SIZE)) error = "READY timeout writing FIFO";
    if (!error) error = waitTalk(&f, timeoutMs, start, &talk);
    setTrigger(false);
    replyTalk(error, &talk, &f);
}

static void cmdWait(uint32_t timeoutMs)
{
    talk_t talk = { 0 };
    const char *error = waitTalk(NULL, timeoutMs, get_absolute_time(), &talk);
    replyTalk(error, &talk, NULL);
}

static void cmdWrite(char **tokens, int count)
{
    uint32_t value[MAX_TOKENS];
    for (int i = 0; i < count; i++) {
        if (!parseNumber(tokens[i], &value[i]) || value[i] > 0xFF) {
            printf("ERR bad byte '%s'\n", tokens[i]);
            return;
        }
    }
    for (int i = 0; i < count; i++) {
        if (!tms5220Write((uint8_t)value[i])) {
            printf("ERR READY timeout on byte %d\n", i);
            return;
        }
    }
    printf("OK\n");
}

static void cmdRead(uint32_t count)
{
    if (count == 0 || count > 64) {
        printf("ERR count must be 1-64\n");
        return;
    }
    char reply[6 * 64];
    int length = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t value;
        if (!tms5220Read(&value)) {
            printf("ERR READY timeout on read %lu\n", (unsigned long)i);
            return;
        }
        length += sprintf(reply + length, " 0x%02x", value);
    }
    printf("OK%s\n", reply);
}

static void cmdTrace(void)
{
    uint32_t dropped;
    uint32_t count = tms6100TraceCount(&dropped);
    const uint8_t *data = (const uint8_t *)tms6100TraceEvents();
    uint32_t length = count * sizeof(tms6100Event_t);
    printf("TRACE count=%lu dropped=%lu clock_hz=%lu\n", (unsigned long)count,
           (unsigned long)dropped, (unsigned long)clock_get_hz(clk_sys));
    sendHex(data, length);
    printf("OK crc=%08lx\n", (unsigned long)crc32(data, length));
}

static void cmdHelp(void)
{
    puts("INFO | STATS | CLOCK <hz> | RESET | UPLOAD <bank> <length> <crc32> | "
         "UNLOAD <bank>|ALL\n"
         "SPEAK <address> [timeout_ms [at_us]] | SPEAKEXT <length> <crc32> [timeout_ms [at_us]] | "
         "WAIT [timeout_ms] | TIME | POLL [ms] | VSMDELAY <cycles>\n"
         "CMD <byte>... | READ [count] | TRIG 0|1 | TRACE [CLEAR]");
    printf("OK\n");
}

static void execute(char *line)
{
    char *tokens[MAX_TOKENS + 1];
    int count = 0;
    for (char *token = strtok(line, " \t"); token; token = strtok(NULL, " \t")) {
        if (count == MAX_TOKENS + 1) {
            printf("ERR too many arguments\n");
            return;
        }
        tokens[count++] = token;
    }
    if (count == 0) return;

    const char *name = tokens[0];
#define IS_NAME(command) (!strcasecmp(name, command))
    uint32_t n[3] = { 0 };
    int args = count - 1;
    bool numeric = true;
    for (int i = 0; i < args && i < 3; i++) numeric &= parseNumber(tokens[i + 1], &n[i]);
    // SPEAK and SPEAKEXT take a 64-bit start time as their last argument
    uint64_t atUs = 0;
    bool timedSpeak = (IS_NAME("SPEAK") && args == 3) || (IS_NAME("SPEAKEXT") && args == 4);
    if (timedSpeak) numeric &= parseNumber64(tokens[args], &atUs);

#define IS(command)     IS_NAME(command)
#define NEED(lo, hi)    if (args < (lo) || args > (hi)) { printf("ERR usage: see HELP\n"); return; }
#define NUMERIC         if (!numeric) { printf("ERR bad number\n"); return; }

    if (IS("HELP")) {
        cmdHelp();
    } else if (IS("INFO")) {
        cmdInfo();
    } else if (IS("STATS")) {
        cmdStats();
    } else if (IS("CLOCK")) {
        NEED(1, 1); NUMERIC;
        cmdClock(n[0]);
    } else if (IS("RESET")) {
        NEED(0, 0);
        cmdReset();
    } else if (IS("UPLOAD")) {
        NEED(3, 3); NUMERIC;
        cmdUpload(n[0], n[1], n[2]);
    } else if (IS("UNLOAD")) {
        NEED(1, 1);
        cmdUnload(tokens[1]);
    } else if (IS("SPEAK")) {
        NEED(1, 3); NUMERIC;
        cmdSpeak(n[0], args > 1 ? n[1] : DEFAULT_TIMEOUT_MS, timedSpeak, atUs);
    } else if (IS("SPEAKEXT")) {
        NEED(2, 4); NUMERIC;
        cmdSpeakExternal(n[0], n[1], args > 2 ? n[2] : DEFAULT_TIMEOUT_MS, timedSpeak, atUs);
    } else if (IS("VSMDELAY")) {
        NEED(1, 1); NUMERIC;
        tms6100SetReadDelay(n[0]);
        printf("OK cycles=%lu\n", (unsigned long)n[0]);
    } else if (IS("POLL")) {
        NEED(0, 1); NUMERIC;
        if (args) statusPollMs = n[0];
        printf("OK poll_ms=%lu\n", (unsigned long)statusPollMs);
    } else if (IS("TIME")) {
        NEED(0, 0);
        cmdTime();
    } else if (IS("WAIT")) {
        NEED(0, 1); NUMERIC;
        cmdWait(args ? n[0] : DEFAULT_TIMEOUT_MS);
    } else if (IS("CMD")) {
        NEED(1, MAX_TOKENS);
        cmdWrite(tokens + 1, args);
    } else if (IS("READ")) {
        NEED(0, 1); NUMERIC;
        cmdRead(args ? n[0] : 1);
    } else if (IS("TRIG")) {
        NEED(1, 1); NUMERIC;
        setTrigger(n[0] != 0);
        printf("OK\n");
    } else if (IS("TRACE")) {
        NEED(0, 1);
        if (args == 0) {
            cmdTrace();
        } else if (!strcasecmp(tokens[1], "CLEAR")) {
            tms6100TraceClear();
            printf("OK\n");
        } else {
            printf("ERR usage: TRACE [CLEAR]\n");
        }
    } else {
        printf("ERR unknown command '%s'\n", name);
    }
}

void commandLoop(void)
{
    char line[LINE_MAX];
    while (true) {
        if (readLine(line, sizeof(line)) < 0) {
            printf("ERR line too long\n");
            continue;
        }
        execute(line);
        fflush(stdout);
    }
}
