# Firmware instrumentation

The aim is that the host can drive every input path without supervision: upload an image, speak any address or any Speak External stream, and fetch the VSM trace, all repeatedly, with no manual steps. The command protocol is documented in [firmware protocol](../../hardware/firmware-protocol.md).

## What the firmware provides

- **The `SCOPE_TRIG` output** ([using the PicoScope](../oscilloscope.md#wiring)).
- **Commands over USB.** `INFO`, `STATS`, `CLOCK`, `RESET`, `UPLOAD`/`UNLOAD`, `SPEAK`, `SPEAKEXT`, `WAIT`, `CMD`, `READ`, `TRIG` and `TRACE`. There is no demo loop; the firmware identifies itself with its git description.
- **End of speech comes from `INT`.** The status register is read only when `INT` asserts, plus a few reads at the start, so there is little bus activity during speech. Speak External tops up the FIFO on each Buffer Low.
- **Up to four 16 KB banks of SRAM**, any of the 16 bank numbers. The Acorn PHROM is built in, in bank `F`, and the host can replace it. Uploads are checked with CRC-32.
- **The VSM trace.** Core 1 logs every Load Address, Dummy Read, bit Read and Read and Branch, with the bit returned. It also logs every change on `WS`, `RS`, `READY`, `INT` and `SCOPE_TRIG`, with the data bus. Each event is 8 bytes and timestamped to about 1.5 µs. The trace holds 32,768 events, about 27 words.
- **Power-up sequencing.** `RESET` (the datasheet's nine `0xFF` and Reset) and `CLOCK` (set or stop `OSC`). Real power cycles need hardware ([bench hardware](../bench-hardware.md#possible-additions)).

On the host side, `investigation/tools/pico.py` provides the host class, the trace decoder, a command line and `selftest`, and `investigation/tools/lpc.py` finds where a word ends. `investigation/tools/capture.py --speak` and `--speakext` arm the scope, speak and save `raw/vsm_trace.bin` and `vsm_trace.csv`, with no manual steps ([tools](../../tools/index.md)).

## The self-test

`pico.py selftest --repeat 3` passes: 54 utterances with no manual steps.

- **Images.** The Acorn image was uploaded to banks `F` and 3.
- **Words.** Six words were each spoken from both banks and through Speak External, three times over.
- **Checks.** In every VSM run, the bits the chip read matched the image. In every Speak External run, the bytes on the bus matched the stream, and the duration matched the VSM's.

Captures p3-01 to p3-03 are in the [evidence index](../../datasheet/evidence.md). They were taken with the C1 link connected, so their `board` entries say so ([host tooling](host-tooling.md#findings)); their traces and `I/O` samples are unaffected.

## Findings

These are written up in the datasheet ([§3](../../datasheet/cpu-interface.md), [§4](../../datasheet/vsm-interface.md), [§6](../../datasheet/commands.md)).

- **No VSM read-ahead.** For every word, the chip read exactly the bits up to the end of its stop code. For example, ACORN reads 1152 bits, 27 frames.
- **Read Byte (`0x1x`) returns the VSM byte bit-reversed:** 0x23 reads back as 0xC4. The first bit read lands on D0, which is TI's MSB. The FIFO, by contrast, takes a written byte LSB first: the same bytes speak correctly through Speak External.
- **Each `0xFF` of the reset sequence is itself a Reset command** (`X111XXXX`). Each one makes the chip issue a Load Address, then a Dummy Read, to the VSM, about 250 µs apart. The trace reads the dummy nibble as 7 for each `0xFF`, and as 0 for the final `0x70`.
- **Talk Status takes time to set.** A status read 22 µs after a Speak (preceded by Load Address) still shows Talk Status clear. The original datasheet asks for up to 287 µs after such a Speak before the next command.
- **BE is not cleared by a VSM Speak.** It is left from the last Reset or Speak External. A Speak External stream whose stop code ends on a byte boundary finishes with BE set, yet its speech is complete.
- **Stopping the clock can wedge the chip.** Reflashing the Pico while it was speaking left the chip unable to complete any write until it was power-cycled. A reflash while it was idle did no harm. Which clock stops are safe is answered in [repeatability](repeatability.md#findings).
- **Repeats differ.** Repeats of the same word differ by about 3 codes rms, whether they come through the same path or not (p3-01 to p3-03). Between the two VSM runs, the delay from `SCOPE_TRIG` to the first sample of speech also differed, by 4.3 ms. The start state is explained in [repeatability](repeatability.md), and the equivalence of the two paths is tested in [synthetic vectors](synthetic-vectors.md).
- The trace's view of host interface timing (READY, `INT`, Speak to first fetch) is used for the timing tests ([controlled stimulus](../stimulus.md#timing-and-handshake-tests), [synthetic vectors](synthetic-vectors.md)).
