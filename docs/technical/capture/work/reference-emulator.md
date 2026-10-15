# Reference emulator validation

The aim is an emulator written **only from the datasheet text**, run against every capture sample for sample, with the VSM trace timing checked too. Any mismatch is fixed in the datasheet first, then in the emulator. Other emulators may be used as a source of hypotheses, never as a reference. This is acceptance test 1 ([acceptance tests](../index.md#acceptance-tests)).

## The emulator and the harness

- **The emulator** ([emulator](../../emulator/index.md)), written in C99 for this work (`investigation/emulator/`, since retired: it is in the repository's history, last changed in commit `6abe9ed`, and the application's emulator re-implements it with the same codes and events). It was written by a separate agent that read only the reference datasheet and, where that defers, the original manual; it did not see the tools, the captures or any other emulator. It is stepped one slot at a time and covers the host interface, the FIFO, the VSM interface and the synthesis path. It has a scenario language, a bulk mode and a self-test. Its [list of choices](../../emulator/choices.md) gives every choice the text left open.
- **The regression harness** (`investigation/tools/regress.py`). It rebuilds every standard take from its `capture.json`, with its predecessors, runs the emulator, and compares the `I/O` codes (layer 2), the VSM timing (layer 1) and, optionally, the model. Results are in `investigation/captures/phase9/regress/`.
- **The variance measurement,** `investigation/tools/variance.py`, results in `investigation/captures/phase9/variance/`. It measures how far the emulator is from the chip, beside how far the chip is from itself and from a second chip.
- **A vector set for free ringing,** `ring` (48 vectors, 192 takes): the longest pitch, for 140-sample free ringing, including a strong resonance at two energies. Evidence p9-01 and p9-02.
- **The localisation set** `loc` (31 vectors: unvoiced frames with two large coefficients, energies around K1 = −488, and free decays; `investigation/tools/vectors.py`, `investigation/captures/vectors/vectors_loc/`).

### What the emulator found unclear in the text

Writing the emulator exposed 36 unclear or missing points in the text. All that affect the output are closed. They concerned:

- the parameter tables, which must be given in full;
- the specification of the Booth multiplier;
- the noise state, which was one sample out;
- energy 0 and the idle value;
- what carries between utterances: the coded parameters, not the interpolator's registers;
- the inhibit and zeroing rules;
- Speak External's timing;
- an exact reference algorithm ([§8.9](../../datasheet/reference-algorithm.md)).

The host-interface details that no capture has measured are listed as such ([datasheet §3](../../datasheet/cpu-interface.md)).

## Result

The emulator is judged against a measured tolerance, not bit for bit. The lowest-order arithmetic could not be found from the data ([lowest-order arithmetic](../../die-analysis/lowest-order-arithmetic.md)), and part of it is marginal (below). So the validation measures how far the emulator is from the chip, beside how far the chip is from itself and from a second chip ([datasheet §8.8](../../datasheet/lattice-filter.md), "Expected agreement"):

- **Layer 1 (VSM timing):** exact on every VSM take.
- **Layer 2 (I/O codes), chip 1 as the reference:** the emulator's typical difference is one code. On speech-like vectors 95–99.8 % of samples are equal (median per set), and on the Acorn words 78.6 %, with a signal-to-difference ratio of 32.5 dB. Chip 1 agrees with its own takes of the Acorn words at 93.4 % and 37.5 dB, and chip 4 agrees with chip 1 on the pitch vectors at 98.8 % and 37.1 dB. So the emulator is about 5–6 dB further from a chip than a chip is from itself or from another chip of the same date code. That margin is the unknown arithmetic. On strongly resonant data it is no further than the chips are from each other.
- **Acceptance test 1 is stated as a tolerance** ([the tolerance](../index.md#the-tolerance)), observed and unproven. It holds on this data: the VSM timing exactly, and layer 2 within the tolerance on the Acorn words and the energy, pitch, K, interpolation, noise and ring vectors.
- **Outside the tolerance, as known gaps:**
    - the loud and random vectors and the decays of `loc`, where a one-code difference grows without limit in an overloaded or ringing filter (the median gap is 12–22 dB);
    - the special frames (15 dB). The failing vectors are built around silence frames, where the first silence frame is approximate in the model ([datasheet §7](../../datasheet/power-up.md)). Those vectors carry little signal, so a few codes weigh heavily in the SDR.

    No second chip has been captured on these sets, so how far two chips differ there is not known ([known gaps](../../datasheet/gaps.md#other-chips-and-the-tms5220c)).
- **The datasheet is complete for what the model knows.** The emulator gives the model's codes at every sample of every take: the 2,104 vector takes (every standard take that passed its checks, including the 192 `ring` takes), the 1,660 corpus word takes and the 17,184 address-sweep takes. The VSM timing matched in every VSM take.
- **It is not bit-exact against the chip,** because the model is not:
    - **voiced data** is one code off at a few samples per pitch period (95–97 % of samples equal on speech-like vectors, 79.6 % on the Acorn words);
    - **the chip does not repeat itself exactly** (upsets);
    - **data far outside the range of speech** turns one-code differences into large ones.

## Findings

- **The voiced error starts early.** With a strong resonance it appears 6 samples into the first chirp (p9-01). In steady state the chip's output is then not periodic with the pitch period, deterministically, while the model's is. Large coefficients hide it, and positive K8–K10 bring it out.
- **Ruled out** ([datasheet §8.8](../../datasheet/lattice-filter.md)):
    - a once-per-interpolation-period disturbance of each K;
    - a half-written K on arrival;
    - *b*₁ through a 10-bit Y latch;
    - toward-zero rounding in one chain;
    - per-stage inversion masks, rescored on the ring set;
    - free corrections to the energy product;
    - the product's lowest bit forced to 0 (the Mega99 core's reading of the patent), in any role;
    - a lowest-bit change on alternate samples;
    - the multiplier of US 4,700,323.
- **A survey of outside sources** found nothing that settles the error:
    - no public netlist of the filter;
    - the published decoders and VHDL cores do not claim a bit-exact multiplier;
    - nobody has published a bit-exact comparison with a real chip.

    The one independent gate-level reading of US 4,335,277, the Mega99 Verilog core, has exactly the model's lattice, apart from the lowest bit, where the chip agrees with the model. So the missing detail is probably one the patents do not describe. The remaining route is a trace of the adder and the delay stack's low-order bits on the TMS5220NL die images (siliconpr0n, through the Wayback Machine; [die analysis](../../die-analysis/index.md)).
- **A marginal part of the error.** The unvoiced departures in `loc` follow the coefficient's second Booth digit (*d*₁ < 0). Chip 1, refitted and captured again two days later, gave a different ±1 only in those vectors ([datasheet §8.8](../../datasheet/lattice-filter.md)). The first-departure search ([lowest-order arithmetic](../../die-analysis/lowest-order-arithmetic.md)) can place each departure only to "*u*₁ off by ±2 in one of a few samples", and no rule in the product's operands fits.

### A second chip

Ring, pitch and energy vectors were captured on chip 4 (`investigation/captures/vectors_chip4/`, kept local). Chips 2 and 3 could not be used: chip 2 gave no `M0` pulses, and chip 3 loaded a wrong address and then stopped responding to writes, perhaps the wedge that a power cycle clears. Chip 4 was compared with chip 1, both against the model ([datasheet §8.8](../../datasheet/lattice-filter.md)):

- **The voiced error is in the design.** On 121 of 126 pitch-vector pairs, both chips leave the model at the same sample with the same ±1.
- **There is also a chip-specific ±1.** In about a third of the ring vectors, one chip departs where the other matches the model. It repeats on each chip, and it does not change with the clock (480, 560 and 720 kHz on chip 4, `investigation/captures/vectors_chip4/clk*/`). So it is not a timing margin. Chips 1 and 4 are both marked TMS5220NL DEP 8423, so it is probably not a die revision either. A marginal level, such as charge sharing on a precharged node, is the likely cause.
- **Chip 4's upsets depend on the clock:** about 30 % of pairs of takes differ at 480–640 kHz, and 2 % at 720 kHz.
- **Chip 4 has more upsets than chip 1,** and its DAC gain differs, so it fails `layer2_vs_layer3` against chip 1's DAC table.

### The host tests replayed

All 88 steps of p8-19 to p8-25 ([synthetic vectors](synthetic-vectors.md)) were run through the emulator from their recorded host cycles, with each step's frame position fitted (`investigation/tools/replay.py`, `investigation/captures/phase9/replay/`).

- **Datasheet corrections:**
    - a frame's parameters are read over 446 slots, not in one burst ([§4](../../datasheet/vsm-interface.md));
    - Read Byte reads its bits from slot 13 of a sample period, at least 22 slots after a Dummy Read ([§4](../../datasheet/vsm-interface.md));
    - reads during a held Read Byte return 0xFF ([§3](../../datasheet/cpu-interface.md));
    - READY after Read and Branch returns at slot 15, 60–63 slots after the Branch ([§3](../../datasheet/cpu-interface.md)).
- **The emulator follows the revised text,** updated by a separate agent from the text alone. It matches every VSM function and every bit read of every VSM step to within 3 µs, every READY latency to within a slot, and every status and data value, apart from two kinds of difference:
    - one read from an unloaded bank, where the Pico's VSM returns 1s and the emulator's returns 0s;
    - Speak External streams that run out of data, where BE and the final Talk Status change by up to 0.5 ms (64 of 15,262 reads).

## Open

What bit-exactness would still need is listed in the [known gaps](../../datasheet/gaps.md): the lowest-order arithmetic, upsets, a first frame of silence, and Speak External's flags within a frame.
