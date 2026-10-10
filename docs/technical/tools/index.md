# Capture tools

The Python tools in `investigation/tools/` drive the TMS5220 through the Pico 2 and capture its output with a PicoScope 3405D. They also hold the analysis that turns those captures into the reference datasheet, the regression harness for the reference emulator, and the measures used to judge the encoder. [Speech capture](../capture/index.md) describes the plan they implement.

## Pages

- [Pico control](pico.md): `pico.py`, the host side of the firmware protocol, and the firmware self-test.
- [Capturing PROM words](capture.md): `run.py`, the standard procedure, the capture layout and the PROM manifests.
- [Readable files and checks](checks.md): `process.py` and `compare.py`.
- [Synthetic vectors and the model](vectors-and-model.md): `vectors.py`, `model.py`, `vecfit.py` and `protocol.py`.
- [Emulator regression](emulator-regression.md): `regress.py`, `replay.py`, `variance.py` and the departure searches.
- [Encoder test speech and quality](encoder-speech.md): the test inputs, the new speech data and its predictions, and `speechqual.py`.
- [Single captures and analysis](single-captures.md): `capture.py`, `decode.py` and the DAC and edge timing tools.
- [PROM OUT](prom-out.md): reading the parameter ROM with `promout.py`.
- [Die image](die-image.md): `die.py`.
- [File list](files.md): every tool in one list.

## Requirements

- **PicoScope 7.** It provides `libps3000a.so`, which is found next to the `picoscope` executable. To use a different copy, set `PS3000A_LIB`.
- **`nix develop`.** It provides Python with numpy, and sets `PICO_DRIVER_DEPS` to the driver's libusb and libstdc++ dependencies.
- **The capture firmware** (`investigation/firmware/`, protocol in [Firmware protocol](../hardware/firmware-protocol.md)). It raises `SCOPE_TRIG` (GPIO26, TP5) from just before each Speak or Speak External until Talk Status clears.

## Probes

| Channel | Test point | Signal | Probe |
|---|---|---|---|
| A | TP2 | `SPEAKER` (DAC) across the 1K load | ×1 |
| B | TP3 | `T11` | ×10 |
| C | TP4 | `I/O` | ×10 |
| D | TP5 | `SCOPE_TRIG` | ×10 |
| Logic D0 | TP10 | `PROM OUT` | Direct, threshold +3 V (`promout.py` only) |

For a clock-alignment capture, move probe D to TP1 (ROMCLK) and pass `--d-signal romclk`. The capture then triggers on `T11`.

Every capture records the board configuration (`--board`, see [Bench hardware](../capture/bench-hardware.md)) and the TP2 load as measured with a meter (`--load-ohms`):

| `--board` | TP2 load |
|---|---|
| `measure` (default) | R2 alone. The C1 link (the breadboard wire from the `SPEAKER`/R2 node to C1) is removed, so the audio stage is disconnected |
| `measure-half` | R2 with a second 1K in parallel |
| `audio` | R2 plus the audio stage (C1 connected). Not for DAC measurements |

The board and its test points are described in [Hardware and firmware](../hardware/index.md).
