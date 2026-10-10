# TMS5220 reference datasheet

This datasheet describes the TMS5220 as measured on real hardware. It follows the section order of TI's *TMS 5220 Voice Synthesis Processor Data Manual* ([transcribed here](../sources/tms5220-data-manual/index.md), referred to below as "the original"), so that readers of the original can find things. The text is written fresh, and it cites the original rather than copying it.

The aim is that someone working from this document alone can author new phrases, author new speech PROMs, and emulate the chip exactly. [Capture work](../capture/index.md) describes how the measurements are made.

## Contents

| Section | Page |
|---|---|
| Conventions, 1. Introduction | This page |
| 2. System clock, and Appendix A | [System clock](clock.md) |
| 3. CPU interface | [CPU interface](cpu-interface.md) |
| 4. Voice synthesis memory (VSM) interface | [VSM interface](vsm-interface.md) |
| 5. I/O structure | [I/O structure](io-structure.md) |
| 6. Description of commands | [Commands](commands.md) |
| 7. Power-up clear, and the state between utterances | [Power-up and the state between utterances](power-up.md) |
| 8. Speech synthesis | |
| 8.1 Coded speech parameters | [Coded speech parameters](coded-parameters.md) |
| 8.2 D/A conversion, 8.3 Audio output | [DAC and audio output](dac-and-audio.md) |
| 8.4 Serial sample output: `T11` and `I/O` | [Serial sample output](serial-output.md) |
| 8.5 Timing of a frame | [Frame timing](frame-timing.md) |
| 8.6 Excitation, 8.7 Interpolation | [Excitation and interpolation](excitation-interpolation.md) |
| 8.8 Lattice filter | [Lattice filter](lattice-filter.md) |
| 8.9 Reference algorithm and the model | [Reference algorithm](reference-algorithm.md) |
| 8.10 Parameter ROM output: `PROM OUT` | [PROM OUT](prom-out.md) |
| 9. Electrical characteristics | [Electrical characteristics](electrical.md) |
| 11. Pin assignments and functions | [Pin functions](pins.md) |
| 12. Authoring speech data | [Authoring speech data](authoring.md) |
| Appendix B: Known gaps | [Known gaps](gaps.md) |
| Evidence | [Evidence index](evidence.md) |

Section 10 of the original has no counterpart here.

## Conventions

- **Evidence.** Every measured statement cites one or more capture IDs in brackets, for example [p1-03]. The [evidence index](evidence.md) lists each capture with its settings and the SHA-256 hash of its raw data. A citation of the form [corpus `acorn_phrom_a`] refers to a PROM's whole reference set ([data layout](../capture/data-layout.md)). The set itself is kept outside the repository until it is published, but its index is committed in `investigation/captures/corpus/<prom>/`: `index.md`, `takes.csv` and `sweep.csv`. These give every take's checks and the SHA-256 hashes of its output and raw files.
- **Vector sets.** A citation of the form [vectors `energy`] refers to one of the synthetic vector sets ([controlled stimulus](../capture/stimulus.md)): its definition is in `investigation/captures/phase8/vectors/<set>.json` and `.csv`, its captures are kept local like the corpus, and its index (`investigation/captures/vectors/vectors_<set>/index.md`, `takes.csv`) is committed. `investigation/captures/phase8/analysis/` holds every take's comparison with the model of [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model) (`vectors.csv`) and the table measurements (`tables.csv`, `pitch_periods.csv`).
- **Chips.** Four TMS5220NLs, all marked `DEP 8423`, are labelled 1–4 on the package. **Chip 1** made every measurement in this datasheet unless another chip is named. Chip 2 gave no `M0` pulses. Chip 3 loaded a wrong address and then stopped responding to writes, after the board's Pico restarted while the chip was fitted; that symptom matches the wedge described in the [firmware protocol](../hardware/firmware-protocol.md), so chip 3 may work after a power cycle. **Chip 4** is the one compared with chip 1 in [§8.8](lattice-filter.md#88-lattice-filter). `investigation/tools/bench.json` names the chip fitted; captures of chip 4 made before it was numbered record it as `replacement3`.
- **The model.** `investigation/tools/model.py` implements the behaviour this datasheet describes for the synthesis path ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)). Statements of the form "the model matches" mean its I/O codes equal the chip's, sample for sample.
- **New and corrected content.** Content the original does not have is marked **New**. Content that disagrees with the original is marked **Correction** and says what the original states.
- **Voltages** are measured at the probe tip, referred to board ground (`VREF`, 0 V), with `VSS` = +5 V and `VDD` = −5 V. Probes are 10 MΩ passive ×10 probes unless stated. Edge times are 10–90 % unless stated, and edge positions are taken at the midpoint between the low and high levels.
- **Slots.** A *slot* is one ROMCLK period, 6.25 µs at the nominal clock. Each 125 µs sample period has 20 slots, numbered 0–19. Slot 0 is the one in which `T11` is high ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)).
- **Codes.** A *code* is the signed 10-bit sample value the chip sends on `I/O` ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)).

## 1. Introduction

The original's introduction (scope, key features, device operation) still applies. This datasheet adds the measured behaviour that the original leaves out or gets wrong.

What is not known is listed in [Appendix B: Known gaps](gaps.md). The main gap is in [§8.8](lattice-filter.md#88-lattice-filter): the filter's lowest-order arithmetic, which leaves the model of [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model) one code off at a few samples per pitch period. Overflow is modelled as a 14-bit wrap, following [US 4,335,277](../sources/patent-us4335277/index.md).
