# Measurement hardware

The aim is that TP2 measures the DAC and nothing else, so that a static code-to-voltage table explains it, with no dependence on earlier samples, and the jump at zero can be measured cleanly.

## Changes to the board

- C1 is disconnected for measurements, so the audio stage does not load the DAC ([bench hardware](../bench-hardware.md#measurement-configuration)).
- R27, the GPIO22 pull-down, is fitted on the board and in the schematic ([bench hardware](../bench-hardware.md#pull-down-on-gpio22)).
- R2 was measured (996 Ω) rather than replaced with a 0.1 % part.
- `capture.py` records the board configuration and the load in every capture, so that every capture states which configuration it was taken with.

## Tools and evidence

- `investigation/tools/dac_transfer.py` gives the per-code table, bit weights, linearity and history check. `investigation/tools/dac_compare.py` compares two loads.
- Captures p2-01 to p2-06 are in the [evidence index](../../datasheet/evidence.md). The datasheet's [§8.2 and §8.3](../../datasheet/dac-and-audio.md) are drafted from them.

## Findings

The DAC transfer function is summarised in [observables](../observables.md#dac-transfer-function). In addition:

- **No dependence on earlier samples.** Regressing on the four previous samples removes less than 0.02 mV.
- **The table explains TP2 to within the noise, but the noise on this breadboard is larger than first assumed.** A target of 0.3–0.5 mV rms is too optimistic: idle noise is 0.5–0.8 mV per sample, and during speech the scatter is 1–2 mV. That scatter depends on neither the code nor the history.
- **The jump at zero is clean.** It measures 5.6–5.9 LSB at both loads.
- **The output current depends on the load** ([datasheet §8.3](../../datasheet/dac-and-audio.md)).

## Followed up elsewhere

- **Codes below −111 and the negative clipping** were not reached by the demo phrase. Controlled vectors reach them ([synthetic vectors](synthetic-vectors.md)); loud PROM words show the DAC model failing at the rails ([PROM corpus](../prom-corpus.md#what-the-inventory-showed)).
- **One word's DAC step is 0.3 µs earlier** than the others (p1-01). Split by direction, the steps of all three 8 ns captures agree: a step to more current is about 0.7 µs slower than a step to less current, and that word has more of the fast steps (`investigation/tools/edge_timing.py`, `by_direction`). [Datasheet §8.2](../../datasheet/dac-and-audio.md#82-da-conversion) gives the figures.
- **The extra noise during speech,** which may come from VSM bus activity on the breadboard, and **the absolute current scale,** which rests on the scope's DC gain (accurate to a few per cent): [known gaps](../../datasheet/gaps.md#the-dacs-absolute-scale). A meter check of the idle TP2 voltage was made later ([repeatability](repeatability.md#findings)).
