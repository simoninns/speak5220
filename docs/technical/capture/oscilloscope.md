# Using the PicoScope 3405D

The PicoScope 3405D is the single capture instrument for the output side. It is fully automated: the 3000D series uses Pico Technology's `ps3000a` driver, so a Python host script can arm, trigger and download captures with no GUI.

## Driver access

PicoScope 7, installed system-wide, ships `libps3000a.so`. `investigation/tools/ps3000a.py` loads it through `ctypes`, and the dev shell provides its libusb and libstdc++ dependencies, so the driver needs no packaging in `flake.nix`. `investigation/tools/run.py` drives the scope and the Pico together ([automation](automation.md), [tools](../tools/index.md)).

## Wiring

The top sheet has a marked "PicoScope 3405D probe points" block, with one test point per channel and four ground test points (TP6–TP9) for the probe ground clips:

| Channel | Test point | Signal |
|---|---|---|
| A | TP2 `PROBE_A` | `SPEAKER` (pin 8) across R2 1K. The raw DAC staircase, 0 to about 1.16 V and idling at about 0.6 V ([§8.2](../datasheet/dac-and-audio.md#82-da-conversion)). **DC-coupled**, ±1 V or ±2 V range |
| B | TP3 `PROBE_B` | `T11` (pin 7), sample sync |
| C | TP4 `PROBE_C` | `I/O` (pin 9), serial data out |
| D | TP5 `PROBE_D` | `SCOPE_TRIG` (Pico GPIO26). The firmware raises it just before it issues Speak and lowers it when Talk Status clears. It is the trigger source, so every capture has a deterministic start and length |
| Logic D0 | TP10 `LOGIC_D0` | `PROM OUT` (pin 10), the parameter ROM's open-drain output. Threshold +3 V. Read by `investigation/tools/promout.py` only; the standard captures do not record it |

`ROMCLK` (`OSC` ÷ 4) stays on TP1, for checking the clock with a frequency counter or by temporarily swapping it onto a channel.

## Resolution

The 3405D has an 8-bit ADC. On the ±1 V range, one ADC count is about 7.8 mV. One DAC step across 1 K is about 5.9 mV, so a single scope sample cannot resolve individual DAC codes. However, each DAC value is held flat for 125 µs. At 1–2 MS/s that gives 125–250 scope samples per DAC sample. Averaging the settled middle of each step (aligned to `T11` on Ch B) lets the noise act as dither, and the average resolves well below one ADC count. That is enough to recover the DAC code and to measure DAC linearity. PicoScope's hardware "resolution enhancement" does a similar job. The averaging is done in post-processing instead, so that it stays explicit and repeatable.

## Mode

The runner uses **block mode**. It captures one take per block, triggered by `SCOPE_TRIG`, so every capture starts at the same point relative to the Speak. The block length comes from the frame count (frames + 2, × 25 ms, plus 150 ms).

The 3405D has 64 MS of memory, shared between the enabled channels. With all four channels at 2 MS/s, that is enough for about 8 s. The longest Acorn word fits many times over in that limit, and so does an unvoiced run long enough for the noise generator's period. 2 MS/s gives 12 scope samples per `I/O` bit slot, and decoding at that rate is reliable.

Streaming mode, at about 1–2 MS/s per channel, is far inside the USB 3 bandwidth and has no memory-depth limit. It would only be needed for a single stimulus longer than about 8 s, or to drop the scope's re-arm time between takes. Neither is needed so far.

## The PicoScope captures all of the output

The PicoScope records all four output-side signals on one timebase: the DAC voltage, `T11`, `I/O` and the trigger. That makes it the single capture instrument for layers 2 and 3 ([capture layers](observables.md#capture-strategy-three-layers-per-utterance)). The Pico handles the input side: it drives the chip, serves the VSM data, logs the VSM trace (layer 1) and raises `SCOPE_TRIG`.

- **No level shifting is needed.** `T11` and `I/O` swing from −5 V to +5 V. That is fine for the scope's inputs (use the ±10 V or ±20 V range on Ch B and Ch C), but it would need clamping before it could reach a Pico pin.
- **The rates are easy.** `I/O` changes at most once per 6.25 µs bit time (160 kHz). Sampling all four channels at 1–2 MS/s each gives 6–12 scope samples per bit, which is plenty for software decoding.
- **Digital and analogue line up.** Every decoded sample can be checked directly against the DAC step on Ch A for the same 125 µs period.
- **ROMCLK is not recorded.** The clock is crystal-derived and exact, so the 20 bit times in each sample can be located from the `T11` edge alone. ROMCLK (TP1) was captured once, during [bench bring-up](work/bench-bring-up.md), to establish the clock-edge alignment ([observables](observables.md#clock-edge-alignment)). `investigation/tools/capture.py --d-signal romclk` repeats that capture if it is ever needed.
