# Repeatability

The aim is a capture procedure whose results can be reproduced, and a documented account of any state the chip carries between utterances. The acceptance criterion is that repeats under the standard procedure are either bit-identical, or differ only in the way the documented state model predicts.

The earlier captures showed a 6- or 25-sample variation in the start ([observables](../observables.md#first-observations-of-the-outputs)), unvoiced output that differs between repeats, and a first VSM frame read that moves by up to 13 ms between takes while the rest of the word keeps exactly the same timing ([host tooling](host-tooling.md#findings)). The same word was therefore repeated under different conditions: plain Speak, a Reset command first, a full power cycle, and different idle times.

## Method and tools

- **Firmware.** Timed starts (`TIME`, and an optional start time on `SPEAK` and `SPEAKEXT`), and `trig_us` and `late_us` in their replies. `POLL` controls status reads during speech; they are off by default, so the end of speech is found from `INT` alone. `VSMDELAY` delays the VSM data, for timing tests. `CLOCK` reports when it acted ([firmware protocol](../../hardware/firmware-protocol.md)).
- **`investigation/tools/frameclock.py`.** It locates the chip's frame clock on the Pico's timer from one Speak's trace. For each path, it calibrates the start so that the chip accepts the command in the target slot.
- **The standard procedure, the runner's default (`run.py --start standard`).** Before every take, the reset sequence; then a Speak timed to the frame clock, accepted 12256.1 µs before a frame boundary. `capture.json` records the frame and the acceptance time. `process.py` checks them (`start_frame`, `start_slot`), and `compare.py` compares the standard takes of a word with their per-sample majority. `--start free` gives an untimed Speak ([automation](../automation.md#the-standard-procedure)).
- **Evidence:** p5-01 to p5-18 in the [evidence index](../../datasheet/evidence.md). p5-01 to p5-08 are the standard procedure, through VSM and Speak External; the rest are the experiments.

The results are written up in [datasheet §7](../../datasheet/power-up.md), which covers power-up, the frame clock, the noise generator, clock stops and upsets, and says what an emulator needs, with additions to [§4](../../datasheet/vsm-interface.md), [§6](../../datasheet/commands.md), [§8.2](../../datasheet/dac-and-audio.md) and [§8.4](../../datasheet/serial-output.md).

## Result

One command, `run.py acorn_phrom_a ACORN --takes 4 --path both --out investigation/captures/phase5 --zip`, gave eight takes that all passed every check. Within each path, every take started on the same sample, with the same noise, and differed from the others by at most 3 codes. They were not bit-identical: each carries small upsets (below). Elsewhere in the experiments, groups of standard takes were bit-identical, including across clock sessions [p5-14, p5-15]. So the procedure fixes all the state that was found; upsets are the documented remainder.

## Findings

- **The start delay is a free-running frame clock.** A Speak waits for the first frame boundary at least about 320 µs after it is written. Neither Speak, Reset nor idle time moves the clock; a power cycle does.
- **The noise generator runs freely with a period of 8191 frames.** It keeps running between words, so takes whose start frames differed by a multiple of 8191 matched. Reset holds it until the next Speak, which restarts it from a fixed state. That is why a Reset before a timed Speak gives the same noise on any frame.
- **The restart is counted in slots.** The firmware's write sequence sometimes finished one slot (6.25 µs) later, depending on how its timing fell against the chip's READY. That alone changed the noise. The runner therefore calibrates the start time each run, and checks every take.
- **Upsets.** Even with all of this fixed, most runs of takes include ±1–3 code departures. They start at a few recurring points in each word and then persist for the rest of it. They did not change with the clock rate (600, 640, 800 kHz), the status polls, the VSM data timing, a preceding word, a power cycle or the spacing between takes ([known gaps](../../datasheet/gaps.md#upsets-and-chip-specific-departures)).
- **Clock stops while idle are safe.** Up to 1 s they freeze the chip's state. After 10 s the chip still works, but its frame clock has moved. `CLOCK 0` is not a reset.
- **The VSM data timing has a wide margin.** The chip samples `ADD8` about 10 µs after `M0` rises.
- **Speak External, accepted at the same time as a VSM Speak, starts 2 samples earlier, with different noise.** Each path repeats as well as the other.
- **The DAC scale step is real.** A meter read 0.642 V on TP2 at idle, against 0.592 V in the [measurement hardware](measurement-hardware.md) table, and the step is the one seen in [host tooling](host-tooling.md#findings).
- **The code resting at 0 between words** ([bench bring-up](bench-bring-up.md)) is explained in [datasheet §8.4](../../datasheet/serial-output.md).

## Consequences for later captures

- Every reference set uses the standard procedure, with at least 5 takes per word and path, and all of them are kept. While upsets are unexplained, the reference output for a word is the per-sample majority of its takes, and `compare.json` says where there is no majority.
- The noise generator's polynomial, steps per slot and state after Reset, and the 2-sample difference between the paths, are measured with [synthetic vectors](synthetic-vectors.md).
- Whether the DAC scale step is in the chip or in the load, and the extra DAC noise during speech, are [known gaps](../../datasheet/gaps.md#dac-and-audio-output). Upset tests that need many power cycles would want Pico-switched power ([bench hardware](../bench-hardware.md#possible-additions)); the one here was done by hand.
