# Host tooling

The aim is that one command turns a request ("speak word N of PROM X") into a complete capture directory ([data layout](../data-layout.md)), with all checks passing. The tools are described in [tools](../../tools/index.md), and how they fit together in [automation](../automation.md).

## The tools

- **Capture and decode.** `investigation/tools/capture.py` and `investigation/tools/decode.py` (triggered block capture, `I/O` decode, DAC averaging), and the Pico client and trace decoder (`investigation/tools/pico.py`).
- **PROM manifests.** `investigation/proms/acorn_phrom_a.json` holds the Acorn PHROM A's hash, provenance and 165-entry word table, built from `romdata_acorn.h`. The other PROMs' manifests are built by `investigation/tools/promimport.py` ([PROM corpus](../prom-corpus.md#the-inventory)).
- **The runner, `investigation/tools/run.py`.** It takes a PROM and words (numbers, names, `all`, or `--address` for sweep points), a path (VSM, Speak External or both) and a number of takes. It uploads the image if the Pico's bank does not hold it, captures each take, and writes the directory layout. It takes `--reset` and `--idle`, and records them in `capture.json`.
- **`investigation/tools/process.py`** rebuilds every readable file from `raw/`: `vsm_trace.csv`, `frames.csv`, `samples.wav` and `.csv`, `dac.wav` and `listen.wav` (48 kHz, through a model of the BBC audio stage). It runs twelve checks and records them in `capture.json` (the [tools](../../tools/index.md) pages list them).
- **The frame decoder** (`investigation/tools/lpc.py`): every frame's bit offset, length, kind, energy, repeat flag, pitch and K1–K10 indices. For the VSM path, `frames.csv` also gives when each frame's first bit was read, and in which output sample period.
- **Layer 2 against layer 3.** The bit-weight model of each `I/O` code ([measurement hardware](measurement-hardware.md)) is fitted to the DAC voltage with a gain and an offset. The check needs a residual of 3 mV rms or less, a gain within 15 % of the table's, and 99 % of the DAC codes recovered within ±2 of `I/O`.
- **`investigation/tools/compare.py`** aligns each take with the first and compares them sample for sample, and frame for frame. **`investigation/tools/promindex.py`** writes `index.md`.
- **`investigation/tools/bench.json`** records the chip (TMS5220NL, `DEP 8423`, and its number) and the measured supplies. Four chips are labelled 1–4 ([datasheet conventions](../../datasheet/index.md)). Chip 1 made every capture until the second-chip comparison ([reference emulator validation](reference-emulator.md#a-second-chip)).
- **Block mode** is chosen for the scope ([using the PicoScope](../oscilloscope.md#mode)).

## The demonstration

One command, `run.py acorn_phrom_a ACORN --takes 2 --path both --out investigation/captures/phase4 --zip`, wrote `investigation/captures/phase4/acorn_phrom_a/` with four takes. Every check passed on every take, and the runner exited 0. The set is p4-01 to p4-04 in the [evidence index](../../datasheet/evidence.md).

## Findings

- **The C1 link matters to the DAC check.** The firmware instrumentation captures (p3-01 to p3-03) were taken with the C1 link connected. The layer 3 check fails on them: TP2 carries the audio stage's high-pass history (6.5 mV rms of residual, which a first-order term brings down to 1.8 mV). With the link removed, the residual is 2.2–2.3 mV. The p3 `board` entries record the link, and the evidence index says so.
- **The DAC scale differs between sessions.** It is 7–8.5 % above the [measurement hardware](measurement-hardware.md) table (code −1 at 0.635 V, against 0.592 V), with the supplies within 6 mV of nominal. The table's shape still fits ([datasheet §8.2](../../datasheet/dac-and-audio.md)). A meter reading confirmed it ([repeatability](repeatability.md#findings)).
- **Frames are read exactly 25 ms apart, but the start varies by most of a frame.** The first frame read came 12.5–25.5 ms after `SCOPE_TRIG` in three VSM Speaks, while the rest of the word took exactly 674.87 ms each time ([datasheet §4](../../datasheet/vsm-interface.md)). The start delay is a wait for the next boundary of a free-running frame clock ([repeatability](repeatability.md)).
- **Repeats differ by 3.1–3.5 codes rms** after alignment, as in the firmware instrumentation captures. The frames parsed are identical in every take.
