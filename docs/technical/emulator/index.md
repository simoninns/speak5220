# Emulator

A slot-accurate emulator of the TI TMS5220 voice synthesis processor, with a
minimal TMS6100 VSM model. It is the application's emulator, in
`app/src/core/emulator/`: every phrase Speak5220 plays, measures or exports
as audio is spoken by it (`app/src/core/synth/`), and it is the reference
the encoder's own synthesis and the research tools are checked against.

## Written from the datasheet only

The emulator is written from the [reference datasheet](../datasheet/index.md)
only, and TI's original manual where the reference says the original still
applies. No other emulator, no capture and no project tool
was consulted. Its purpose is to test whether the datasheet is complete and
exact.

It was first written in C99, by an agent that saw only those texts, and
checked against the whole data set of captures
(the validation is in the repository's history: [The research](../index.md#the-research)).
The C++ emulator re-implements that code as classes. The structure changes,
not the arithmetic or the order of evaluation: on every reference run in
`app/golden/emulator/` (the C emulator's self-test scenarios, synthetic
vectors and encoder output, through both paths, at several clock phases)
its codes are the same sample for sample and its events slot for slot
(`EmulatorGoldenTest`). The C emulator has been retired; it is in the
repository's history, last changed in commit `6abe9ed`.

The synthesis path follows the [§8.9 "Reference algorithm (exact)"](../datasheet/reference-algorithm.md)
step for step, with the same state names (`idx`, `cur`, `seen`, `Z`, `U`,
`oldS`, `oldU`, `hold`, `count`, ...). Wherever the text leaves a choice open,
the code has a `DATASHEET GAP:` comment (or `UNMEASURED (§3)` for the host
interface items the datasheet lists as not measured), and
[Implementation choices](choices.md) lists the choice made. Section numbers (§N) on these
pages and in the code refer to the reference datasheet.

C++20. Integer arithmetic throughout the synthesis path, with explicit
helpers for arithmetic right shifts and 14-bit two's-complement wrapping.
A step does not allocate, and the chip gives the same codes on every
platform.

## Pages

- [API](api.md): the classes, the VSM interface, the configuration and how
  the application speaks a phrase.
- [Command line](command-line.md): `tms5220emu`, the emulator's command line
  for the reference-data and research scripts: scenario scripts, bulk mode,
  output formats.
- [Implementation choices](choices.md): what the datasheet leaves open, and the choice
  made for each point.

## Files

In `app/src/core/emulator/`:

| File | Contents |
|---|---|
| `tms5220.h`, `tms5220.cpp` | The chip, stepped one slot at a time |
| `tms5220_config.h`, `tms5220_config.cpp` | The timing choices the datasheet leaves open |
| `tms5220_event.h`, `sample_sink.h`, `vsm_bus.h` | The chip's events, its output and the VSM functions it issues |
| `tms6100.h`, `tms6100.cpp` | Minimal VSM: 20-bit address loaded by nibbles LSB first, data read LSB first, Read and Branch, 1 MB flat memory (unloaded bytes read 0x00) |
| `chip_arithmetic.h`, `chip_arithmetic.cpp` | The multiplier, shifts, 14-bit wrap, noise register and DAC current |
| `chip_driver.h`, `chip_driver.cpp` | A host: host cycles run to completion, the speak procedures |

Its unit tests are in `app/tests/unit/emulator/`, and `tms5220emu` is
`app/tests/support/tms5220emu_command.cpp`.

## Timing model

- A **slot** is one ROMCLK period (6.25 µs). A sample period is 20 slots;
  absolute slot 0 is power-up and is slot 0 of a sample period.
- The **frame clock** runs all the time, 200 samples per frame. Its position
  is 0 in the sample period in which a VSM frame is read (PC 12 of IP 7); the
  first bit is read in slot 17, and each parameter's bits follow two slots
  apart from its offset in [§4](../datasheet/vsm-interface.md) (0, 34, 78, …,
  442 slots), so a voiced frame takes 446 slots. `phase` sets the position at
  power-up.
- A Read Byte reads its eight bits two slots apart from slot 13 of the sample
  period after its latch, or after a Load Address from the first slot 13 at
  least 22 slots after the Dummy Read (§4).
- The code of each sample period is computed at its slot 0 and is the code
  sent on `I/O` in slots 2–11 of that period (and to the DAC from slot 1).
- A host cycle starts at a slot (the strobe) and completes at the slot where
  READY goes active again.
