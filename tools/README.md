# Capture tools

Python tools for capturing the TMS5220's output with a PicoScope 3405D. `docs-tech/speech_capture_plan.md` describes the plan they implement.

## Requirements

- **PicoScope 7.** It provides `libps3000a.so`, which is found next to the `picoscope` executable. To use a different copy, set `PS3000A_LIB`.
- **`nix develop`.** It provides Python with numpy, and sets `PICO_DRIVER_DEPS` to the driver's libusb and libstdc++ dependencies.
- **Firmware that drives `SCOPE_TRIG`** (GPIO26, TP5). The pin is high from just before each Speak until Talk Status clears.

## Probes

| Channel | Test point | Signal | Probe |
|---|---|---|---|
| A | TP2 | `SPEAKER` (DAC) across the 1K load | ×1 |
| B | TP3 | `T11` | ×10 |
| C | TP4 | `I/O` | ×10 |
| D | TP5 | `SCOPE_TRIG` | ×10 |

For a clock-alignment capture, move probe D to TP1 (ROMCLK) and pass `--d-signal romclk`. The capture then triggers on `T11`.

## Usage

```
python tools/capture.py captures/test --seconds 3   # wait for SCOPE_TRIG, capture
python tools/decode.py captures/test                # decode each Speak window
python tools/edge_timing.py captures/test --plot     # edge timing (use --timebase 3 captures)
```

`capture.py` writes `raw/scope_raw.npz`, which holds the raw ADC data for channels A–D plus the range, offset and sample interval. It also writes `capture.json`. Raw files are never modified afterwards. To commit an evidence capture, zip `raw/scope_raw.npz` unchanged as `raw/scope_raw.npz.zip` (the `.npz` itself is git-ignored). The tools read the zip directly when the `.npz` is absent. PROM corpus captures go under `captures/corpus/`, which is kept local (plan §6).

`decode.py` writes two files for each word (one SCOPE_TRIG window):

- `word<N>_samples.wav`: the decoded `I/O` samples as 16-bit PCM at 8 kHz. Each 10-bit code (−512 to 511) is left-justified (× 64), so the chip's full scale is the WAV's full scale. It is lossless: the code is the sample shifted right by 6.
- `word<N>_samples.csv`: the sample index, the code, the mean DAC voltage and the `T11` time.

## Files

- `ps3000a.py`: ctypes wrapper for the ps3000a driver.
- `tms5220_decode.py`: decodes `I/O` (a 10-bit two's-complement sample, LSB first, in bit slots 2–11 after the `T11` rising edge) and averages the DAC voltage over each settled step.
- `capture.py`, `decode.py`: the command-line tools.
- `edge_timing.py`: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's §8.4 and §9.5 come from its output.
