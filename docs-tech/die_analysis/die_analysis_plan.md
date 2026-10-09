# TMS5220NL die analysis plan

**Status: Phase 0 done (`phase0.md`); Phase 1 in progress (`phase1.md`), 2026-10-09.** A working pin map is in place, and A and B are confirmed as 14 bits wide. The block identifications in §2 are still hypotheses unless `phase1.md` §2 says otherwise.

The capture work has taken the reference datasheet as far as measurement can take it. Most of what is still unknown sits in the arithmetic, and the vectors constrain it without naming it. The lowest-order arithmetic in particular (datasheet §8.8, `lowest_order_arithmetic_ideas.md`) has survived every model built from the data and from the patents. A die image shows the circuit itself. This plan sets out how to read the image in phases, ordered so that each one either answers one of the important unknowns or makes the next phase reliable.

## 1. The image

| | |
|---|---|
| File | `local/die_image/ti_tms5220nl_mcmaster_mz_mit20x.jpg` (git-ignored, not committed) |
| Source | John McMaster (siliconpr0n, `mcmaster:ti:tms5220nl`), decapped April 2013, Mitutoyo 20× objective, stitched. **CC BY 4.0**: figures must credit him (`phase0.md` §1) |
| SHA-256 | `42e5707a0a24d8fa62444abe8efa11e8e1834070b08e822359dd9a3e665d6858` |
| Size | 18,074 × 14,916 px, RGB JPEG, 85 MB. Scale 0.341 µm/px, so the die is 6.16 × 5.09 mm (`phase0.md` §3) |
| Layer | Top metal intact (not delayered). Polysilicon and diffusion show only where the metal does not cover them |
| Markings | `TO285E` and `5220` in the centre (TMC0285, TI's internal number for the 5220); `© 11-78` beside block D; mask-level marks `1A 3A 4A 6A 7A` (left edge), `1-1`, `2-1`, `D.30`, `DL`, `TK`, `A.4` near the pads |

At full resolution the metal lines, contacts, and most of the poly and diffusion between them can be read (`phase0.md`). The image has three limits:
- **Metal hides what is beneath it.** This is a P-channel MOS die (the datasheet's supplies: `VSS` = +5 V substrate, `VDD` = −5 V). In a process of this age there is one metal layer, used mostly for power, clocks and long buses. Gates under a wide metal line cannot be seen. There is no delayered image of this die. The TMS5200NL image (the same T0285E design) may be partly without metal (`phase0.md` §1).
- **JPEG compression** blurs the edges of thin features. A contact, or a buried contact under poly, needs care, and a transistor's W/L can only be measured to about ±1 px.
- **One part.** The imaged part is DEP 8319; chips 1 and 4 are DEP 8423. The die carries a 1978 copyright, and the mask set probably did not change across NL production, but Phase 6 should keep this in mind.

## 2. First look: floorplan

![Floorplan hypotheses](fig_floorplan.jpg)

*Full-resolution pixel = figure pixel × 9.04. Pads P1–P29 are numbered counterclockwise from the top-left corner (`phase0.md` §4). Pin names are the working map of `phase1.md` §1 ("?" = not yet confirmed). Die image: John McMaster, siliconpr0n, CC BY 4.0.*

| Block | What is seen | Working hypothesis |
|---|---|---|
| **A** | The largest regular array. A header row, then **14 bit rows of 452 px**, and **five column groups of 1,260 px**: three identical between two specialised end groups (`phase1.md` §3) | The **array multiplier**: five Booth stages (stage 0 to stage 4) of 14 bit slices. US 4,335,277 FIG. 10 describes five stages of 14 cells |
| **B** | Narrow columns of small, repeated cells at the **same 452 px vertical pitch**, bit-aligned with A | The **summer and its multiplexer, the delay stack, the Y latch and the multiplier multiplexer** (patent 402, 403, 404, 406, 415) |
| **C** | A dense, uniform grid of bit cells with decoders on two sides | A mask ROM, probably the **parameter tables** (energy, pitch, K1–K10) |
| **D** | Two regular arrays with about 16 rows, column decoders below | The **FIFO** (16 × 8), and/or the parameter registers and interpolation (K-stack 302) |
| **E** | Five groups of comb-like columns under a row of drivers | A second ROM or a PLA: the **chirp ROM**, or a timing decoder. Not the DAC (`phase1.md` §2) |
| **F** | Random logic and routing across the top | **Control**: the PC and timing PLA, the frame parser, VSM and host interface state machines |
| **G** | Very wide, interdigitated transistors along the bottom-left, next to pads | Probably the **clock drivers**: symmetric pairs, not a binary-weighted DAC (`phase1.md` §2) |
| **H** | Nine identical large drivers in the right-hand column, between P20 and P16 | The **D0–D7 bus** drivers, or the VSM outputs |

Two features make A and B a strong lead. They share the 452 px bit pitch, so they are one bit-sliced datapath. A's five column groups match the multiplier's five stages. If this holds, the cells that matter most (bit slices 0–2 of stages 0 and 1, and the adder's bit 0) are at one end of the A/B rows and can be found in an evening.

## 3. Targets, in order of value

Each target is an unknown named in the reference datasheet. The order is by the value of the answer to the datasheet's acceptance tests, discounted by how hard the target is to read from a metal-layer image.

| # | Unknown | Datasheet | What to read on the die | Difficulty |
|---|---|---|---|---|
| T1 | **The lowest-order arithmetic** (the voiced one-code error, the period doubling, the *d*₁ < 0 sensitivity) | §8.8 "Not known yet"; ideas §1, §2.1 | Multiplier bit slices 0–2 in stages 0 and 1: the forced 1, what drives a partial-product line when no digit is selected, where the completing carry of a negative digit enters. Patent cells A-1, A-2, B-1…B-C (FIG. 10c) | Medium. Regular, and the patent is a map |
| T2 | **The adder's low end and the summer multiplexer** | §8.8 | Adder bit 0 and 1, the carry-in, the precharged-zero input (patent gate 420), the truncation logic (425), the Y latch's width | Medium |
| T3 | **Word lengths** | §8.8 "Word length and overflow" | Bit rows in A and B; the delay stack's depth and width. Confirms the 14-bit wrap and shows whether any register is wider or narrower | Easy |
| T4 | **The chip-specific departures and upsets** | §8.8 "A second error"; §7 upsets | The dynamic and precharged nodes on the T1/T2 paths, and any ratioed gate there: transistor W/L, node size, which clock phase precharges it | Hard. Needs sizes, not just connections |
| T5 | **The parameter and chirp tables, read optically** | §8.1, §8.6 | The ROM bits in C (and E) | Easy but tedious. Done mainly to validate the reading method (Phase 2) |
| T6 | **The DAC** | §8.2: the 26 µA step between codes −1 and 0, the 7 % scale step after a power cycle | Whether the DAC is sign-magnitude, offset binary or two's complement, its weighting, and its bias source | Medium |
| T7 | **`PROM OUT` and `TEST`** | §11.2 | The nets behind pins 10 and 20 | Easy to find; meaning depends on F |
| T8 | **The Speak External two-sample offset** | §8.5 "Speak External" | Where the Speak External path enters the frame timing in F | Hard. Control logic |
| T9 | **Host interface timing details** | §3 "Not measured yet" | The READY, INT and FIFO-full logic in F and D | Hard, and also measurable on the bench, which is easier |

T1 and T2 are the reason for doing this. T3 and T5 are cheap, and they check the method. T4 and T6 explain effects that the model has to tolerate rather than reproduce. T7 to T9 can wait.

## 4. Phases

### Phase 0: preparation

1. **Viewer.** Cut the image into a tiled pyramid (Deep Zoom or plain 1024 px tiles), so that any region can be opened without decoding the 85 MB file. Pillow and numpy in the dev shell are enough. A `libvips` entry in `flake.nix` would make it faster. Write the tool as `tools/die.py`, with subcommands for `tiles`, `crop X Y W H` and `annotate`.
2. **Coordinates.** All positions are in full-resolution pixels of this image, origin top left. Every traced cell gets a record in `docs-tech/die_analysis/cells.json` (name, block, bounding box, notes), and crops are regenerated from it.
3. **Scale.** Find µm per pixel from a known feature: the pad pitch, or the design-rule width of a minimum poly line (about 5–6 µm for a TI process of 1978). Needed for T4 and T6.
4. **Pads to pins.** Map the 28 bond pads to package pins: `SPEAKER`, `OSC`, `VDD`, `VSS`, D0–D7, `PROM OUT` and `TEST`. Wide power buses mark VDD and VSS, and the bus drivers (H) mark D0–D7. This anchors every later trace.
5. **Other images.** Look for a delayered image, or another layer, of a TMS5220NL or TMS5220C on siliconpr0n (directly, or through the Wayback Machine). Look for a TMS5100 or TMC0280 die too. Its multiplier is the patent's, so a cell that differs between the two dies is the first suspect for T1.

**Exit:** a tiled viewer, a scale, and a labelled pad ring.

**Result (`phase0.md`):** the viewer, coordinates and scale (0.341 µm/px) are done, and the source and licence are confirmed. 29 bonded pads are numbered P1–P29, so one pin is double-bonded. Two pin orders fit the pads, and the choice moves to Phase 1 step 0.

### Phase 1: floorplan and bit slices

**Status: in progress (`phase1.md`).** Step 0 has a working map with two open conflicts. Step 2 is done: 14 bit rows, LSB at the bottom (tentative). Steps 3 and 4 are not started.

0. **The pin map** (`phase0.md` §4.3). Trace the datapath's two widest rails to their pads (`VDD`, `VSS`), trace block G's output to its pad, and find the doubled pin. Then fill in `pin` in `cells.json`.
1. Confirm or replace each hypothesis in §2. Follow the A/B datapath's bus into C (the coefficient path) and from B to G (the output path, `YL13`–`YL4` per §8.4).
2. Count the bit rows in A and B exactly (T3). Identify the LSB end: the multiplier's partial sums shift right by two per stage (patent FIG. 10), so the stage boundary cells at the LSB end look different from the rest.
3. Count the delay stack's cells per bit (T3). The patent implies 9 or 10 stages plus the Y latch.
4. Find the two-phase clock lines and the precharge phase that reach A and B.

**Exit:** an annotated floorplan that replaces `fig_floorplan.jpg`, with the bit order of A and B and the datapath's word lengths. If the rows are not 14, update datasheet §8.8 straight away.

### Phase 2: calibration (reading what is already known)

Before trusting a reading of the unknown cells, read some known ones.

MAME has already checked the 5220's LPC and chirp tables against this decap, so this phase tests our method rather than the tables.

1. **The parameter ROM (C).** Decode its bits and compare them with the tables of datasheet §8.1, which are the values *as the chip uses them*, from the sweeps. Each bit cell is a transistor that is present or absent, which shows as a contact or a gate over diffusion. Work out the row and column decoding from the address lines in F.
2. **The chirp ROM** (E, if that is what it is), against §8.6.
3. **One complete multiplier cell** in the middle of the array (bit 7 of stage 2, say). Draw its transistors, reduce them to logic, and check the logic against the patent's cell (FIG. 10c) and against the multiplier form in §8.8, which is right for ordinary operands (ideas §1, constraint 1).

**Exit:** the ROM agrees bit for bit with the measured tables, and the middle cell agrees with the patent. A disagreement is a finding in itself, but until it is explained, the method is not trusted for T1.

### Phase 3: the multiplier's low end (T1)

This is the main phase.

1. For each of the five stages, trace bit slices 0, 1 and 2, and the stage's Booth recoder inputs (+1, −1, +2, −2 from recoding logic 301/313), down to transistors. Stage 0 gets the most care: the patent says it receives only −2, −1 and +1 and sees only MR2–MR13.
2. For each cell, write down:
   - what drives each partial-product line when no digit is selected (a precharge, a pull-down, or a floating node);
   - how a negative digit is formed: inversion, and where the +1 completion enters (a carry-in at the cell, at the next stage, at the adder, or never);
   - where the "forced 1" of datasheet §8.8 comes from. The model says the product's lowest bit is 1, and the Mega99 reading of the patent says 0. The die will show which, and why;
   - any input that is tied off, gated by a clock phase, or comes from outside the slice.
3. Turn the trace into a bit-level model of the multiplier. Run it in place of the current multiplier in `model.py`, scored with `tools/regress.py` over the ring, pitch, energy, K and unvoiced K sets. Constraints 1–8 of ideas §1 are the test: the first pitch period stays exact, the *d*₁ < 0 coefficients depart, and *d*₀ < 0 energies show more differences.

**Exit:** either the voiced first departures in the pitch set disappear (the answer), or the traced circuit matches the current model exactly, which moves the search to Phase 4.

### Phase 4: the adder, the multiplexers and the delay stack (T2, T3)

1. Trace adder bit 0 and bit 1 and their carry-in. Find what drives the carry-in at each slot of the sample period: patent FIG. 11 gives the summer's second input as the adder's own output (T10–T18), the delay stack (T20–T7, T9), the Y latch (T8), or a precharged zero (T19).
2. Trace the precharge gate 420 and check that "zero" is a clean zero at bit 0, not a weak node.
3. Trace the truncation logic 425 and the Y latch. The Phase 9 survey ruled out several low-bit truncations of *b*₁. The die shows which one, if any, exists.
4. Trace the overflow path. The model wraps at 14 bits and the vectors agree; confirm that nothing saturates.

**Exit:** a bit-level model of the whole lattice step from the die, scored as in Phase 3. If Phases 3 and 4 together reproduce the chip, datasheet §8.8 and §8.9 are rewritten from the circuit, the emulator follows, and acceptance test 1 can be judged (plan §1.5).

### Phase 5: marginal nodes (T4)

Only if Phases 3 and 4 leave the *d*₁ < 0 sensitivity or the chip-specific departures unexplained.

1. List every dynamic node on the stage 0 and stage 1 negative-digit paths and in the adder's low bits. For each one, note the clock phase that precharges it, what can discharge it, and how far it runs (its capacitance).
2. Measure W/L for each ratioed pair on those paths. A pull-up/pull-down ratio near the switching threshold is a candidate for a chip-specific outcome.
3. Rank the candidates by how well each explains the data: departures that repeat on a chip, do not depend on the clock, and differ between chips (§8.8). Then design one bench test per candidate, such as a `VDD` or temperature change (ideas §4.5) that should move it.

**Exit:** a short list of physical mechanisms, each with a bench test. These belong in the datasheet's description of the tolerance, not in the model.

### Phase 6: the DAC and output (T6)

1. Trace G from `YL13`–`YL4` to the `SPEAKER` pad. Count the current-source transistors and measure their W/L.
2. Decide the coding: a separate sign branch would explain the 26 µA step between −1 and 0 (§8.2).
3. Find the bias network. It decides whether the 7 % scale step after a power cycle could be a bias state rather than the load.

**Exit:** a DAC transfer model to set beside the measured table of §8.2.

### Phase 7: control (T7, T8, T9)

This phase is open-ended, and much of it can also be measured on the bench, so it is done last and only as far as it is useful.

1. Trace pads 10 (`PROM OUT`) and 20 (`TEST`) to their sources (T7).
2. Find the PC and timing PLA in F, and read its terms. With that, find where Speak External enters the frame timing (T8).
3. Trace the FIFO control and the READY/INT logic only where a bench test cannot answer an item on the §3 "Not measured yet" list (T9).

## 5. Ways of working

- **Record the trace, not only the conclusion.** Each traced cell goes into `cells.json` with its crop and a transistor-level drawing (KiCad schematic, or a netlist that a short script can simulate). A later reader should be able to check every claim against the pixels.
- **Never trust the patent without the die.** The patent is the map for finding cells. Where the die and the patent differ, the die wins, and the difference is written down: it may be the answer to T1.
- **Score every model change with the regression.** No change goes into `model.py` because it looks right on the die. It goes in because `tools/regress.py` shows it fits the vectors better.
- **Write up each phase** in this directory as `phaseN.md`, as the capture phases are, and carry the results into the reference datasheet with citations to the cell records.

## 6. Effort and order

| Phase | Effort | Depends on |
|---|---|---|
| 0 Preparation | done (`phase0.md`); pin map moved to Phase 1 | — |
| 1 Floorplan | 1–2 evenings; in progress (`phase1.md`) | 0 |
| 2 Calibration | 2–3 evenings (the ROM is most of it) | 1 |
| 3 Multiplier low end | 3–5 evenings | 2 |
| 4 Adder and stack | 2–3 evenings | 1; scored together with 3 |
| 5 Marginal nodes | 2–4 evenings, if needed | 3, 4 |
| 6 DAC | 2 evenings | 0 |
| 7 Control | open-ended | 1 |

If time is short, Phase 2 can be cut down to step 3 (the one middle multiplier cell) so that Phases 3 and 4 start sooner. The ROM readout is good evidence for the method, but the measured tables already give the values. Phase 6 does not depend on the datapath and can be done at any time.
