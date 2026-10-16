# Technical documentation

This part of the site documents how the TMS5220 works, as measured, and the software built from the results.

## Where to start

- **To emulate the chip or write speech data for it**, read the [reference datasheet](datasheet/index.md). It is self-contained: the emulator and the encoder were both written from it alone.
- **To run the emulator**, see the [emulator](emulator/index.md) pages: the application's emulator, its API and its command line.
- **To encode new speech**, use the application's [encoder](../application/user-manual/encode.md). The [speech encoder](encoder/index.md) pages describe how it works.
- **To check a statement in the datasheet**, follow its capture IDs to the evidence in the repository's history ([The research](#the-research), below).

## Contents

| Section | What it covers |
|---|---|
| [Reference datasheet](datasheet/index.md) | The TMS5220 as measured: interfaces, commands, timing, the synthesis path, an exact reference algorithm, and a guide to authoring speech data |
| [Emulator](emulator/index.md) | The application's emulator, written from the datasheet: its API, command line and choices |
| [Speech encoder](encoder/index.md) | The application's encoder: how it works, its quality, and what other encoders do |
| [Original documents](sources/index.md) | Transcriptions of TI's TMS5220 and TMS6100 data manuals and US patent 4,335,277 |

## The research

The research that produced the datasheet was removed from the repository after commit `c537c1d`: the capture board and its firmware, the capture and analysis tools, the captures, the PROM inventory, the die analysis, and the pages that documented them (the capture method, the hardware, the tools, the die analysis and the datasheet's evidence pages). It can still be read at that commit: [github.com/simoninns/speak5220/tree/c537c1d](https://github.com/simoninns/speak5220/tree/c537c1d). The datasheet's capture IDs, and the paths it names under `investigation/`, refer to that tree.

This documentation is built with MkDocs (`mkdocs.yml` at the repository root); [`BUILD.md`](https://github.com/simoninns/speak5220/blob/main/BUILD.md) describes building it and everything else.
