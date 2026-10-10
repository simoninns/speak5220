# 8.10 Parameter ROM output: `PROM OUT`

**New.** Measured on chip 1 with the PicoScope's logic input D0 on pin 10 (test point TP10) [`investigation/captures/promout/chip1/`].

The original lists pin 10 only as "Testing use only". It is the serial output of the parameter output register. One parameter at a time, it carries the parameter ROM's value for the chip's stored coded index: the target that the interpolator works towards ([§8.7](excitation-interpolation.md#87-interpolation), target(i) in [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)). [US 4,335,277](../sources/patent-us4335277/index.md) shows this in Fig. 8f, though its text never mentions the pin. In that figure, the 2720-bit parameter ROM (202, Fig. 8e) loads the parameter output register (201). The register's serial output goes both to the interpolator ("PROM OUT to Fig. 9a") and, through an output transistor, to the `PROM OUT` pad. [§8.1](coded-parameters.md#81-coded-speech-parameters)'s tables were read through it.

**Electrical.** `PROM OUT` is an open-drain output. It pulls up towards `VSS` and otherwise does not drive. On the logic input (about 200 kΩ to 0 V), the released level settles between +0.5 V and +1.5 V, the same as the inputs left open beside it. The driven level is above +3 V, and nearly always below +4.5 V. A +3 V threshold reads it cleanly. The edges fall 0.6–3 µs into the slot, after the `T11` rising edge (falling edges are slower, with nothing pulling the pin down), so read each bit late in its slot, at 75 %. The patent draws a load to `VDD` on the pad; a pull-down resistor to `VDD` or to 0 V would sharpen the falling edges, but none was needed.

**Format.** One bit per slot, high = 1, running continuously whether the chip is talking or idle. The stream repeats every 25 sample periods (500 slots): one cycle of the parameter counter, PC 0–11 with an A and a B cycle each, and PC 12 with an A cycle only ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)). Each of the 12 parameters is a 10-bit value, LSB first, followed by copies of its top bit until the next parameter begins. Each field starts one slot earlier in its sample period than the one before it:

| Parameter | Sample period (from energy's) | First slot | Value |
|---|---|---|---|
| Energy | 0 | 0 | Table 8-4, two's complement |
| Pitch | 2 | 0 | Table 8-5 × 4, unsigned (the value in bits 2–9). The ROM itself stores the value unshifted, in bits 0–7 (die, [the ROMs](../die-analysis/roms.md)) |
| K1 | 3 | 19 | Table 8-6, two's complement |
| K2 | 5 | 18 | 〃 |
| K3 | 7 | 17 | 〃 |
| K4 | 9 | 16 | 〃 |
| K5 | 11 | 15 | 〃 |
| K6 | 13 | 14 | 〃 |
| K7 | 15 | 13 | 〃 |
| K8 | 17 | 12 | 〃 |
| K9 | 19 | 11 | 〃 |
| K10 | 21 | 10 | 〃 |

*Table 8-8. Where each parameter appears on `PROM OUT`, within one 25-sample-period cycle. Sample periods 22–24 carry only K10's sign.*

The order is the parameter counter's, energy first (PC 0), as in the patent. Which sample period is PC 0 A was not measured against the frame read ([§8.5](frame-timing.md#85-timing-of-a-frame), [Appendix B](gaps.md#prom-outs-phase)), so the cycle is identified from its contents: a capture's alignment is the one at which every field holds a table value.

**What it shows.** The table value at the stored index, not the value in use:

- **Not interpolated.** The patent's register 201 holds the target. The runs here held each setting for 25 frames, so they show the target but do not test how the field changes from one frame to the next.
- **No zeroing.** Silence and the K5–K10 zeroing of unvoiced frames ([§8.7](excitation-interpolation.md#87-interpolation)) do not appear. In an unvoiced frame, K5–K10 show the values at their stored indices, which are the last voiced frame's, even from an earlier utterance. This is the stored state that [§12.2](authoring.md#122-when-a-frame-is-heard) warns about. Both kinds of zeroing are applied after the ROM, as target(i) in [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model) describes.
- **Between utterances**, the last frame's indices, with energy 0 for the stop code (index 15).

**Reading the tables.** `investigation/tools/promout.py dump` speaks 64 runs through Speak External, each of 25 identical frames. Run *i* uses energy index *i* mod 14 + 1, pitch index *i*, and every K at index *i* modulo its table's size; run 0 is unvoiced. It reads the fields in mid-speech, then reads energy index 0 from a silence frame and index 15 while idle after the stop code. Every one of the 248 entries (energy 16, pitch 64, K 168) equals Tables 8-4 to 8-6, in two complete dumps. The chirp ROM (Table 8-7, ROM 409 in the patent) is a separate ROM and does not appear on `PROM OUT`. `investigation/tools/promout.py idle` decodes the stream at any moment, which shows the chip's stored indices directly.

**Relation to TI's bit-time names (inference).** With [§8.4](serial-output.md#84-serial-sample-output-t11-and-io)'s mapping (slot 0 is T11), energy and pitch start in T11, and K1 to K10 start in T10 down to T1. The patent's load signal LDP is also staggered from one parameter to the next, by a different pattern. That pattern is given in its own bit-time numbering, which does not match the pin name's ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)), so neither mapping is confirmed.
