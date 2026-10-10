# Measurement work

These pages describe each piece of measurement work behind the reference datasheet and the data set: what it set out to establish, how, what it found, and the evidence. The first five build the capture system; the rest build the data set and test the datasheet against it. Each datasheet section was drafted from the work that produced its evidence ([from captures to the datasheet](../analysis.md)).

| Page | What it established | Builds on |
|---|---|---|
| [Bench bring-up](bench-bring-up.md) | The chip running from the Pico; the `T11` and `I/O` output format; clock-edge alignment | — |
| [Measurement hardware](measurement-hardware.md) | A clean DAC measurement at TP2; the DAC transfer function | Bench bring-up |
| [Firmware instrumentation](firmware-instrumentation.md) | Command-driven firmware and the VSM trace; first interface findings | Bench bring-up |
| [Host tooling](host-tooling.md) | One command per capture directory; processing and checks | Bench bring-up, firmware instrumentation |
| [Repeatability](repeatability.md) | The standard procedure; the frame clock and the noise generator | Firmware instrumentation, host tooling |
| [PROM inventory](../prom-corpus.md#the-inventory) | Which original speech data exists, checked and identified | — |
| [Acorn PHROM A reference set](acorn-phrom-a.md) | The first complete reference set, and a sweep of every address | Measurement hardware, host tooling, repeatability, PROM inventory |
| [Synthetic vectors](synthetic-vectors.md) | The chip's internal behaviour, measured with controlled stimulus | Host tooling, repeatability |
| [Reference emulator validation](reference-emulator.md) | An emulator written from the text alone, compared with the chip; the tolerance | Synthetic vectors |
| [The remaining PROM corpora](prom-corpora.md) | Every inventoried PROM captured and compared with the emulator | PROM inventory, Acorn PHROM A reference set |
| [New speech data](new-speech-data.md) | Authoring, and confirmation with speech the chip has never seen | Synthetic vectors, reference emulator validation |

The evidence captures cited on these pages (p1-01, p8-19 and so on) are listed with their hashes in the [evidence index](../../datasheet/evidence.md). Directory names under `investigation/captures/` such as `investigation/captures/phase8/` are the repository's own and are kept as they are.
