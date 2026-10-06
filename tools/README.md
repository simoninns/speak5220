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

Every capture records the board configuration (`--board`, plan §5) and the TP2 load as measured with a meter (`--load-ohms`):

| `--board` | TP2 load |
|---|---|
| `measure` (default) | R2 alone. The C1 link is removed, so the audio stage is disconnected |
| `measure-half` | R2 with a second 1K in parallel |
| `audio` | R2 plus the audio stage (C1 connected). Not for DAC measurements |

## Usage

```
python tools/capture.py captures/test --seconds 3   # wait for SCOPE_TRIG, capture
python tools/decode.py captures/test                # decode each Speak window
python tools/edge_timing.py captures/test --plot     # edge timing (use --timebase 3 captures)

# DAC transfer function: pool captures, subtract the scope zero, compare two loads
python tools/capture.py captures/zero --seconds 2 --note "probe A tip on GND"
python tools/dac_transfer.py captures/a captures/b --zero captures/zero --out captures/dac --plot
python tools/dac_compare.py captures/dac captures/dac_half --out captures/compare --plot
```

`capture.py` writes `raw/scope_raw.npz`, which holds the raw ADC data for channels A–D plus the range, offset and sample interval. It also writes `capture.json`. Raw files are never modified afterwards. To commit an evidence capture, zip `raw/scope_raw.npz` unchanged as `raw/scope_raw.npz.zip` (the `.npz` itself is git-ignored). The tools read the zip directly when the `.npz` is absent. PROM corpus captures go under `captures/corpus/`, which is kept local (plan §6).

`decode.py` writes two files for each word (one SCOPE_TRIG window):

- `word<N>_samples.wav`: the decoded `I/O` samples as 16-bit PCM at 8 kHz. Each 10-bit code (−512 to 511) is left-justified (× 64), so the chip's full scale is the WAV's full scale. It is lossless: the code is the sample shifted right by 6.
- `word<N>_samples.csv`: the sample index, the code, the mean DAC voltage and the `T11` time.

## Files

- `ps3000a.py`: ctypes wrapper for the ps3000a driver.
- `tms5220_decode.py`: decodes `I/O` (a 10-bit two's-complement sample, LSB first, in bit slots 2–11 after the `T11` rising edge) and averages the DAC voltage over each settled step.
- `capture.py`, `decode.py`: the command-line tools.
- `dac_transfer.py`: the static DAC transfer function from one or more captures. It writes a per-code table of TP2 voltage and current (`dac_table.csv`), fits one weight per DAC input bit, and reports linearity, the step from −1 to 0, and whether the output depends on earlier samples (`dac_transfer.json`, `dac_transfer.png`). `--zero` subtracts the scope's zero offset, measured from a capture taken with probe A's tip grounded.
- `dac_compare.py`: compares the tables from two loads, giving the current ratio per code and the equivalent output resistance. The reference datasheet's §8.2 and §8.3 come from these two tools.
- `edge_timing.py`: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's §8.4 and §9.5 come from its output.
