# Technical documentation

This part of the site documents how the TMS5220 works, how that was measured, and the software built from the results.

## Where to start

- **To emulate the chip or write speech data for it**, read the [reference datasheet](datasheet/index.md). It is self-contained: the emulator and the encoder were both written from it alone.
- **To run the emulator**, see the [reference emulator](emulator/index.md).
- **To encode new speech**, use the application's [encoder](../application/user-manual/encoder.md). The [speech encoder](encoder/index.md) pages describe the research encoder it re-implements.
- **To check a statement in the datasheet**, follow its capture IDs to the [evidence index](datasheet/evidence.md), and the [capture method](capture/index.md) for how captures are made.

## Contents

| Section | What it covers |
|---|---|
| [Reference datasheet](datasheet/index.md) | The TMS5220 as measured: interfaces, commands, timing, the synthesis path, an exact reference algorithm, and a guide to authoring speech data |
| [Reference emulator](emulator/index.md) | A C99 emulator written from the datasheet, its API and command line |
| [Speech encoder](encoder/index.md) | The research encoder, how it works, its quality, and what other encoders do |
| [Capture method](capture/index.md) | What can be observed on the chip, how it is captured, the data layout, the PROM corpus, and the work that produced the datasheet |
| [Hardware](hardware/index.md) | The capture board, its schematics, the audio output stage and the firmware's USB protocol |
| [Tools](tools/index.md) | The Python host tools: capture, analysis, the model, regression and encoding quality |
| [Die analysis](die-analysis/index.md) | What the die image shows: floorplan, ROMs, the multiplier and the filter's lowest-order arithmetic |
| [Original documents](sources/index.md) | Transcriptions of TI's TMS5220 and TMS6100 data manuals and US patent 4,335,277 |

## Repository layout

Everything made to investigate the chip (the board, the firmware, the tools, the captures, the PROM manifests and the reference emulator) is under `investigation/`. The repository root holds the documentation and is kept clear for the application.

| Path | Contents |
|---|---|
| `docs/` | This documentation (MkDocs; `mkdocs.yml` at the root). Building it, and everything else: [`BUILD.md`](https://github.com/simoninns/speak5220/blob/main/BUILD.md) |
| `investigation/kicad/tms5220_usb/` | The capture board's KiCad project, with the project symbol library for the TMS5220 and TMS6100 |
| `investigation/kicad/libraries/ki-lime-pi-pico/` | The Ki-Lime Pi library submodule (Raspberry Pi Pico symbols, footprints and 3D models) |
| `investigation/firmware/` | The Raspberry Pi Pico 2 capture firmware (C, Pico SDK) |
| `investigation/tools/` | Python host tools: the capture runner, Pico control, PicoScope capture, analysis, the model and the regression harness |
| `investigation/emulator/` | The reference emulator (C99) |
| `investigation/proms/` | The speech PROM inventory and manifests: image hashes, provenance and word tables. `python investigation/tools/promimport.py` rebuilds the images, which are not redistributed |
| `investigation/captures/` | Evidence captures cited by the datasheet. The large PROM and vector capture sets are kept out of the repository apart from their indexes (`index.md`, `takes.csv`, `sweep.csv`) |
