# Bench bring-up

The aim of the bring-up is that the chip runs from the Pico, and that every output signal is understood well enough to capture it.

## What was established

- **The clock.** The 640 kHz rail-to-rail clock from the Pico ([bench hardware](../bench-hardware.md#pico-driven-oscillator)).
- **The probes.** Probes A–D wired, and automated PicoScope capture working ([using the PicoScope](../oscilloscope.md)).
- **The `I/O` format.** 10 bits, LSB first, in slots 2–11.
- **The DAC update timing.**
- **The audio stage's loading of TP2**, identified and modelled.
- **The ROMCLK edge alignment.** All `T11` and `I/O` edges follow ROMCLK rising edges.

The measurements are given in [observables](../observables.md#first-observations-of-the-outputs). They are the basis of the datasheet's [system clock (§2, Appendix A)](../../datasheet/clock.md), [DAC timing (§8.2)](../../datasheet/dac-and-audio.md), [serial sample output (§8.4)](../../datasheet/serial-output.md), [measured output signals (§9.5)](../../datasheet/electrical.md) and [pin functions (§11.2)](../../datasheet/pins.md).

## Tools and evidence

- `investigation/tools/edge_timing.py` measures every edge relative to `T11` and ROMCLK.
- Captures p1-01 to p1-03 are in the [evidence index](../../datasheet/evidence.md) with their hashes. The captures themselves are committed in `investigation/captures/phase1/`, with the raw data zipped ([data layout](../data-layout.md#where-it-is-stored)).

## Followed up elsewhere

- **The DAC transfer function,** including the jump between codes −1 and 0: [measurement hardware](measurement-hardware.md).
- **The code resting at 0 rather than −1 between back-to-back words:** explained in [datasheet §8.4](../../datasheet/serial-output.md) ([repeatability](repeatability.md)).
- **The mapping from slots to TI's bit times T1–T20,** which is inferred from the pin name only: [known gaps](../../datasheet/gaps.md#slots-and-tis-bit-times).
