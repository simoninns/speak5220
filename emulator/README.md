# TMS5220 reference emulator

A slot-accurate emulator of the TI TMS5220 voice synthesis processor, with a
minimal TMS6100 VSM model and a scenario-driven command-line program.

**It is written from the datasheet only:**
`docs-tech/tms5220_reference_datasheet/tms5220_reference_datasheet.md`, and
TI's original manual where the reference says the original still applies. No
other emulator (MAME or any other), no capture and no project tool was
consulted. Its purpose is to test whether the datasheet is complete and exact.
The synthesis path follows the §8.9 "Reference algorithm (exact)" step for
step, with the same state names (`idx`, `cur`, `seen`, `Z`, `U`, `oldS`,
`oldU`, `hold`, `count`, ...). Wherever the text leaves a choice open, the
code has a `DATASHEET GAP:` comment (or `UNMEASURED (§3)` for the host
interface items the datasheet lists as not measured), and
[`GAPS.md`](GAPS.md) lists the choice made.

C99, libc only. Integer arithmetic throughout the synthesis path, with explicit
helpers for arithmetic right shifts and 14-bit two's-complement wrapping (no
implementation-defined signed shifts).

## Build

    make            # builds tms5220emu and libtms5220.a
    make test       # builds and runs the self-test

From the repository root: `nix develop -c make -C emulator test`.

## Files

| File | Contents |
|---|---|
| `tms5220.h`, `tms5220.c` | The chip, stepped one slot at a time |
| `tms6100.h`, `tms6100.c` | Minimal VSM: 20-bit address loaded by nibbles LSB first, data read LSB first, Read and Branch, 1 MB flat memory (unloaded bytes read 0x00) |
| `tms5220emu.c` | Command-line program: scenario scripts and a bulk mode |
| `test_tms5220.c` | Self-test (`make test`) |
| `GAPS.md` | Every datasheet gap found, and the choice made |

## Timing model

- A **slot** is one ROMCLK period (6.25 µs). A sample period is 20 slots;
  absolute slot 0 is power-up and is slot 0 of a sample period.
- The **frame clock** runs all the time, 200 samples per frame. Its position
  is 0 in the sample period in which a VSM frame is read (PC 12 of IP 7); the
  first bit is read in slot 17, and each parameter's bits follow two slots
  apart from its offset in §4 (0, 34, 78, …, 442 slots), so a voiced frame
  takes 446 slots. `phase` sets the position at power-up.
- A Read Byte reads its eight bits two slots apart from slot 13 of the sample
  period after its latch, or after a Load Address from the first slot 13 at
  least 22 slots after the Dummy Read (§4).
- The code of each sample period is computed at its slot 0 and is the code
  sent on `I/O` in slots 2–11 of that period (and to the DAC from slot 1).
- A host cycle starts at a slot (the strobe) and completes at the slot where
  READY goes active again.

## API (tms5220.h)

    tms5220 chip;  tms6100 vsm;
    tms6100_init(&vsm);  tms6100_load_file(&vsm, "phrom.bin", 0x00000);
    tms5220_init(&chip, NULL);                 /* NULL: default gap choices */
    tms5220_set_vsm(&chip, tms6100_func, &vsm);
    tms5220_set_sample_cb(&chip, my_sample_fn, ctx);  /* (ctx, index, slot, code) */
    tms5220_set_event_cb(&chip, my_event_fn, ctx);    /* VSM, host, flag events */
    tms5220_set_phase(&chip, 0);               /* before the first step */

    tms5220_step(&chip);                       /* advance one slot */

    tms5220_write(&chip, 0x50);                /* start a write at the current slot */
    while (tms5220_host_busy(&chip)) tms5220_step(&chip);   /* wait for READY */
    tms5220_read(&chip);                       /* status or Read Byte data */
    while (tms5220_host_busy(&chip)) tms5220_step(&chip);
    int v = tms5220_host_value(&chip);

Other calls: `tms5220_status()`, `tms5220_talking()`, `tms5220_int_active()`,
`tms5220_code()`, `tms5220_next_boundary()` (slot of the next VSM frame-read
point). The arithmetic is exposed for testing: `tms5220_booth()`,
`tms5220_asr()`, `tms5220_wrap14()`, `tms5220_noise_step()`.
`tms5220_dac_current_ua(code, load)` gives the approximate `SPEAKER` current
from the §8.2 bit weights (load 0 = 996 Ω, 1 = 482 Ω).

The VSM is a callback `int fn(void *ctx, int func, int nibble)` with `func`
one of `TMS5220_VSM_LOAD` (nibble), `TMS5220_VSM_READ` (returns the bit),
`TMS5220_VSM_DUMMY`, `TMS5220_VSM_BRANCH`; any VSM model can be plugged in.

All timing choices the datasheet leaves open are fields of `tms5220_config`
(`tms5220_default_config()` gives the defaults described in GAPS.md).

## Command line

    tms5220emu [-s SAMPLES] [-e EVENTS] [-q] SCRIPT
    tms5220emu [-s SAMPLES] [-e EVENTS] [-q] --image FILE --addr HEX
               [--load-at HEX] [--speak-at SLOTS] [--external] [--reset]
               [--prior SCRIPT] [--phase SAMPLE] [--margin SLOTS]

- `-s FILE` samples output, `-e FILE` event log. Each defaults to stdout;
  `-` is stdout, `none` turns it off.
- `-q` leaves the VSM `READ` functions out of the event log (they are one
  line per frame bit).

### Scenario scripts

One operation per line; `#` starts a comment. Numbers are decimal except
bytes and addresses, which are hex.

| Operation | Meaning |
|---|---|
| `load <file> <hexaddr>` | Load a binary image into VSM memory |
| `phase <sample>` | Frame-clock position (0–199) of the sample period at power-up; 0 = the period in which a frame would be read. Only before anything runs |
| `set <param> <value>` | Change a gap choice (a `tms5220_config` field, e.g. `se_shift 0`). Only before anything runs |
| `slot <n>` | Advance to absolute slot n |
| `at <n>` | As `slot`, but a slot already passed is not an error (for replays of recorded host cycles) |
| `wait <slots>` | Advance n slots |
| `write <hexbyte>` | Host write at the current slot; advances until READY goes active |
| `writes <hexbyte>...` | Several writes, each starting at the slot after the previous one's READY |
| `status`, `read` | A host read (RS) at the current slot; the chip returns status, or the Read Byte data if one is pending. Both are logged with what was returned |
| `until_idle [margin]` | Advance until Talk Status is clear, no speech is running or pending, and the last 25 codes were −1; then `margin` more slots (default 4000, one frame) |
| `boundary <n>` | Advance so that a command written at the current slot is latched exactly n slots before the next VSM frame-read slot (the first one far enough ahead) |
| `mark <name>` | Write the current slot to the event log |

Example (address 0x01000, Speak accepted 1961 slots before a frame boundary,
after the reset sequence, then a Load Address written during speech):

    phase 50
    load phrase.bin 1000
    wait 1000
    writes FF FF FF FF FF FF FF FF FF 70
    writes 40 40 40 41 40        # Load Address 0x01000, low nibble first
    boundary 1961
    mark speak
    write 50
    wait 400
    write 43                     # held until Talk Status clears
    until_idle

### Bulk mode

`--image FILE --addr HEX` loads `FILE` at `--load-at` (default `--addr`),
sets `--phase` (default 0), runs the `--prior` script if given (for example
a predecessor utterance; it runs from slot 0), waits until the chip is idle,
then:

- with `--reset`, writes the reset sequence (nine `FF`, then `70`);
- **Speak** (default): writes Load Address for `--addr` (five nibbles, low
  first), then writes Speak so that it is latched `--speak-at` slots (default
  1961, the standard procedure of §7) before a VSM frame-read slot;
- **`--external`**: writes Speak External the same way, then the phrase bytes
  from `--addr` up to the byte holding the end of the first stop code (parsed
  by the §8.1 frame format): 16 bytes at once, then 8 more whenever a status
  read, made every 1 ms (160 slots), shows Buffer Low;
- runs `until_idle` with `--margin` (default 4000 slots).

The event log has a `MARK speak` or `MARK speak_external` line at the slot of
the command's write strobe.

### Output formats

**Samples** — one line per sample period, from power-up:

    <index> <slot> <code>

`index` is the sample period number (0 at power-up), `slot` the absolute slot
at which it began (`20 × index`), `code` the signed 10-bit `I/O` value.

**Events** — one line per event, `<slot> <TYPE> ...`:

| Line | Meaning |
|---|---|
| `<slot> VSM LOAD <nibble>` | Load Address function, nibble 0–15 (ADD8..ADD1) |
| `<slot> VSM READ <bit>` | Read function and the data bit returned |
| `<slot> VSM DUMMY` | Dummy Read |
| `<slot> VSM BRANCH` | Read and Branch function |
| `<strobe> HOST WRITE_CMD <hex> ready <slot>` | Write latched into the command register |
| `<strobe> HOST WRITE_FIFO <hex> ready <slot>` | Write routed to the FIFO (Speak External) |
| `<strobe> HOST READ_STATUS <hex> ready <slot>` | Read returning the status register |
| `<strobe> HOST READ_DATA <hex> ready <slot>` | Read returning the Read Byte data |
| `<slot> FLAG TS\|BL\|BE\|INT <0\|1>` | Flag change (INT 1 = active) |
| `<slot> MARK <name>` | A `mark` operation |
| `<slot> INFO <what> <a> <b>` | Informative internal milestones: `start_speak`, `start_external`, `frame` (bits, energy index), `frame_truncated`, `load` (energy index, inhibit), at PC 12 of IP 0, `end`, `held`, `held_exec`, `ignored`, `reset_halt`, `fifo_overflow` |

Lines are written in the order events happen. A `HOST` line is written when
the cycle completes, so its first field (the strobe slot) is earlier than the
lines just before it; sort on the first field if strict slot order is needed.

## Self-test

`make test` checks, against statements in the datasheet (not captures):
the §8.8 multiplier worked examples, and the noise register's period; that the idle output is code −1 (u₁ = −9)
and the status after power-up and Reset is 0x7F; that frames parse to 50, 11,
29, 4 and 4 bits (voiced, repeat, unvoiced, silence, stop) at several clock
phases, read from slot 17 with each parameter's bits at the §4 offsets, 4000
slots apart, with nothing read past the stop code and Talk Status clearing
24.87 ms after the stop frame; that a stop code
as the first frame reads 4 bits, gives no output change, clears Talk Status
0.09 ms later and raises INT; the Speak External FIFO thresholds (0x5F at 8
bytes and no speech, 0x9F at the 9th); Read Byte bit reversal
(0x23 → 0xC4 and the other §6 examples), its bits from slot 13 of the sample
period after the latch, or the first slot 13 at least 22 slots after the Dummy
Read, and the data read's READY 4 slots
after the eighth bit; a Read Byte held during speech (reads return 0xFF at
once, then the byte, read from slot 13 after Talk Status clears); and READY
after Read and Branch at slot 15, 60–79 slots after the Branch (79 and 63 for
a Branch in slots 16 and 12), for each `branch_ready` from 60 to 63.
