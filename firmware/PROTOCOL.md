# speak5220 firmware: USB command protocol

The firmware is driven entirely by the host over the Pico 2's USB serial port (CDC, VID:PID `2e8a:0009`, "Pico - Board CDC"). It never sends anything unprompted. `tools/pico.py` implements the host side, and `tools/README.md` shows how to use it.

At boot the firmware:

1. Sets the system clock to 144 MHz.
2. Copies the built-in Acorn PHROM A into bank `F`.
3. Starts the VSM emulation and trace on core 1.
4. Starts the TMS5220's 640 kHz clock.

It then waits for commands. The TMS5220 is not reset and says nothing until it is told to. Earlier firmware ran a demo loop; that loop is gone, and `pico.py demo` says the same phrase once.

## Framing

- **Commands.** The host sends one command per line, terminated by `\n` (a `\r` is ignored). Command names are not case-sensitive. Numbers may be decimal or `0x` hex.
- **Replies.** Every reply ends with exactly one line that starts with `OK` or `ERR`. Results are given as `key=value` fields on the `OK` line. Some commands send other lines before it.
- **Data.** Binary data is sent as hex digits in both directions, and whitespace and line breaks in it are ignored.
  - Host to Pico (`UPLOAD`, `SPEAKEXT`): the Pico answers the command with `GO`, then reads exactly 2 × `length` hex digits. A gap of more than 2 s between digits aborts the command.
  - Pico to host (`TRACE`): lines of up to 64 hex digits.
- **Checks.** Data in either direction is checked with CRC-32, the IEEE polynomial as used by Python's `zlib.crc32`, written as 8 hex digits.
- **Concurrency.** Commands run one at a time. `SPEAK`, `SPEAKEXT` and `WAIT` return only when the TMS5220 has finished, or on their timeout.

## Commands

| Command | Reply | What it does |
|---|---|---|
| `HELP` | text, `OK` | Lists the commands. |
| `INFO` | `OK proto=1 firmware=<git describe> sys_hz= osc_hz= trace_events= external_max= banks=<bank>:<crc32>,… builtin_bank=15` | Identity and state. `banks` lists each loaded bank with the CRC-32 of its 16 KB image, or `-` if none is loaded. `firmware` is `git describe --always --dirty` of the source at build time. |
| `STATS` | `OK load_address= read= read_branch= address=` | VSM activity counters since boot, and the current 20-bit VSM address register. |
| `CLOCK <hz>` | `OK osc_hz=<actual> at_us=` | Sets the TMS5220 `OSC` frequency. The PWM period is a whole number of 144 MHz cycles, so the result is the nearest achievable frequency (2.2 kHz to 72 MHz). `CLOCK 0` stops the clock with `OSC` held at +5 V. `at_us` is when the clock stopped or started, on the `TIME` timer. See the warning below. |
| `TIME` | `OK us=` | The Pico's 64-bit microsecond timer, counted from boot. It runs from the same crystal as `OSC` and the trace's cycle counter. |
| `POLL [ms]` | `OK poll_ms=` | Sets how often the status register is read during speech as a fallback to `INT`, or with no argument reports it. `0`, the default, reads only on `INT` (below). |
| `VSMDELAY <cycles>` | `OK cycles=` | For timing tests only: delays each VSM data bit on `ADD8` by this many 144 MHz cycles after `M0` rises. `0`, the default, answers at once. |
| `RESET` | `OK status=0x..` | Writes nine `0xFF` bytes, then the Reset command (`0x70`), then reads the status. This is the original datasheet's initialisation sequence (§7). Each `0xFF` is itself a Reset command (`X111XXXX`), so the trace shows a Load Address and a Dummy Read for each one. |
| `UPLOAD <bank> <length> <crc32>` | `GO`, data, `OK bank= length= crc=` | Loads a VSM image (1–16384 bytes) into bank 0–15. Any shorter image is padded with zeros to 16 KB, and the reply's `crc` covers the padded bank. Up to 4 banks can be loaded at once. An upload to a bank that is already loaded replaces it. If the transfer fails, the bank is left empty. |
| `UNLOAD <bank>` or `UNLOAD ALL` | `OK` | Empties a bank. An empty bank does not drive `ADD8`, and Read and Branch in an empty bank reads zeros. |
| `SPEAK <address> [timeout_ms [at_us]]` | `OK status= talked= ints= reads= us= trig_us=` | Raises `SCOPE_TRIG`, sends five Load Address commands (the 20-bit address, least significant nibble first), then Speak (`0x50`). It waits until Talk Status clears, then lowers `SCOPE_TRIG`. The default timeout is 10000 ms. With `at_us`, it first waits until the `TIME` timer reaches `at_us` (a timed start, below). |
| `SPEAKEXT <length> <crc32> [timeout_ms [at_us]]` | `GO`, data, `OK status= talked= ints= reads= us= written= trig_us=` | Speak External with a byte stream of 1–32768 bytes. The stream is uploaded first. The firmware then raises `SCOPE_TRIG`, sends `0x60` and writes the first 16 bytes. Each time Buffer Low is set, it writes 8 more. It finishes as `SPEAK` does. `at_us` works as for `SPEAK`. |
| `WAIT [timeout_ms]` | as `SPEAK` | Waits for Talk Status to clear, for sequences built from `CMD`. |
| `CMD <byte> [<byte> …]` | `OK` | Writes up to 20 bytes to the TMS5220, as commands or FIFO data. |
| `READ [count]` | `OK 0x.. …` | Reads the status register (or the data register after Read Byte), 1–64 times. |
| `TRIG 0` or `TRIG 1` | `OK` | Sets `SCOPE_TRIG` and the LED by hand, for sequences built from `CMD`. |
| `TRACE CLEAR` | `OK` | Empties the VSM trace. Its first new event is `START`. |
| `TRACE` | `TRACE count= dropped= clock_hz=`, hex lines, `OK crc=` | Sends the trace events recorded since the last clear (see below). |

**Fields of the `SPEAK`, `SPEAKEXT` and `WAIT` replies:**

- `status`: the last status register value read. Its bits are TS `0x80`, BL `0x40` and BE `0x20`; the low five bits read as 1.
- `talked`: whether Talk Status was ever seen set.
- `ints`: the number of `INT` assertions handled.
- `reads`: the number of status reads.
- `us`: the time from `SCOPE_TRIG` rising to the read that found Talk Status clear.
- `written` (`SPEAKEXT` only): the number of stream bytes written to the FIFO.
- `trig_us`: when `SCOPE_TRIG` rose, on the `TIME` timer.
- `late_us` (timed starts only): how long after `at_us` `SCOPE_TRIG` rose. It is 0 or 1 unless `at_us` had already passed, in which case the command starts at once.

### Timed starts

The TMS5220 reads its frames on a grid that runs freely between words, and its noise generator restarts from the Speak that follows a Reset, counted in slots (datasheet §7). The `TIME` timer, the trace's cycle counter and `OSC` all run from the Pico's crystal, so the grid stays fixed on the timer for as long as the clock runs. `tools/frameclock.py` finds it from one Speak's trace: the `READ` events' cycle counts, relative to the `SCOPE_TRIG` rise, added to `trig_us`. A Speak timed half a frame before a grid point then starts on that frame. The write sequence takes a few hundred microseconds and waits on READY, so the slot in which the chip accepts the Speak can jitter by one; `frameclock.py` calibrates the start time to avoid that. `tools/run.py` uses this for every take (`--start standard`).

On a timeout or a READY failure the line starts with `ERR`. It carries the same fields, followed by the reason.

### How the end of speech is detected

The firmware reads the status register as little as possible, to keep the bus quiet during speech:

- **Start.** Talk Status is not set at once. A read 22 µs after a Speak (preceded by Load Address) still sees it clear, and the original datasheet asks for up to 287 µs after such a Speak before the next command (Appendix C). So the firmware first polls the status every 1 ms until Talk Status is seen set, for up to 20 ms. After those 20 ms, a clear Talk Status ends the wait.
- **During speech.** The status is read only when `INT` asserts. A status read during speech is host bus activity at the chip, so by default there is no other read. A missed interrupt then ends the command at its timeout. `POLL <ms>` adds a fallback read at that interval, as earlier firmware did every 200 ms.
- **One word.** A VSM Speak of one word normally needs 2 status reads: one at the start, which finds Talk Status set, and one at the `INT` at its end.
- **Speak External.** Every Buffer Low raises `INT`, roughly once per 8 bytes, and there is one more `INT` at the end. The 144-byte ACORN stream gave 19.

### Status bits to ignore

- **BE outside Speak External.** BE is only meaningful during Speak External. After a VSM Speak it still holds whatever the last Reset or Speak External left: a Reset purges the FIFO and sets BE.
- **BE after a complete stream.** A Speak External stream whose stop code ends exactly on a byte boundary also finishes with BE set. The FIFO empties as the last bit is read. The speech is still complete: it takes as long as the VSM Speak of the same word.

## The VSM trace

Core 1 runs the VSM emulation in a polling loop. The same loop records every VSM function the TMS5220 performs, and every change on the host interface lines. Each event is 8 bytes, two little-endian 32-bit words:

| Word | Contents |
|---|---|
| 0 | Timestamp: core 1's cycle counter (`clock_hz` = 144 MHz, 6.9 ns). It wraps every 29.8 s. A `TIME` event is recorded whenever 2^30 cycles pass without another event, so the host unwraps the counter by summing the differences modulo 2^32. |
| 1 | Event: the type in bits 28–31, and the payload below |

| Type | Name | Payload |
|---|---|---|
| 0 | `TIME` | None (keeps the timestamps unwrappable) |
| 1 | `LOAD_ADDRESS` | Bits 0–19: the address register after the load. Bits 20–23: the nibble loaded. |
| 2 | `DUMMY_READ` | Bits 0–19: the address. Bit 20: driven (a bank image is loaded for the address). |
| 3 | `READ` | Bits 0–19: the address of the byte. Bits 20–22: bit number (0 = LSB, sent first). Bit 23: the bit value. Bit 24: driven. |
| 4 | `READ_BRANCH` | Bits 0–19: the address after the branch. Bit 20: driven. |
| 5 | `PINS` | A host interface line changed. Bits 0–7: the data bus (GPIO0–7, as a byte value). Bit 8: `WS`. Bit 9: `RS`. Bit 10: `READY`. Bit 11: `INT`. Bit 12: `SCOPE_TRIG`. Each bit is the line's level, so 0 means active for the first four. |
| 6 | `START` | First event after `TRACE CLEAR`. Its payload is the line levels, as in `PINS`. |

**What the trace records:**

- **Write data.** In a write cycle, the `PINS` event at `WS` falling carries the byte written.
- **Read data.** In a read cycle, the event at `READY` falling carries the byte read.
- **Scope alignment.** `SCOPE_TRIG` is in the trace, so its rising edge lines the trace up with a scope capture.
- **Driven bits.** `READ` events record the bit the emulation drove onto `ADD8`. When nothing is driven (driven = 0), the bit the chip actually saw is not known.

**Limits:**

- **Timestamps are late by up to about 1.5 µs.** An event is timestamped when the loop sees the edge. That can be late if the loop is still busy with the previous event: a Load Address takes about 1 µs to settle.
- **Capacity.** The trace holds 32768 events. Any further events are counted in `dropped` and lost. One word of speech is about 1200 events.

`tools/pico.py` saves the trace as `raw/vsm_trace.bin`, which holds the event bytes exactly as received. It also decodes it to `vsm_trace.csv`, and its `hostCycles()` function rebuilds the read and write cycles from the `PINS` events.

## VSM emulation

- **Bank decoding.** Each bank image answers to address bits 14–17. Bits 18–19 are ignored, as on a TMS6100.
- **Read and Branch** replaces the low 14 bits of the address with the two bytes at the current address, low byte first.
- **Upload while idle.** Images can be uploaded or unloaded only while the TMS5220 is not reading the VSM. Commands run one at a time, so this is only a concern after a timeout.

## Warning: stopping the clock can wedge the TMS5220

The TMS5220 has no clock of its own. `OSC` stops whenever the Pico resets or is reflashed, and whenever `CLOCK 0` is sent. On 2026-10-06, reflashing the Pico while the old demo loop was speaking left the chip unable to complete a write: `READY` went high on `WS` and never returned low. Status reads still worked and returned `0x7F`. The chip had not run its power-up clear either, because the trace showed no VSM accesses. Restarting the clock did not help. Only cycling the TMS5220's power did. A later reflash while the chip was idle did no harm.

Phase 5 found that stops while the chip is idle are safe (datasheet §7):
- **Up to 1 s:** the chip's state freezes, and it carries on where it stopped.
- **After 10 s:** the chip still worked, but its frame clock had moved.
- **During speech:** not repeated.

So:
- Reflash only while the chip is idle.
- After a reflash, check that `RESET` replies `OK`.
- If it times out, power-cycle the TMS5220. The Pico can stay on USB.
- `CLOCK 0` is not a reset.

`picotool load -f -x firmware/build/speak5220.uf2` reflashes over USB, with no BOOTSEL button.
