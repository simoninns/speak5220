# Capture tools

Python tools for driving the TMS5220 through the Pico 2 and capturing its output with a PicoScope 3405D. `docs-tech/speech_capture_plan.md` describes the plan they implement.

## Requirements

- **PicoScope 7.** It provides `libps3000a.so`, which is found next to the `picoscope` executable. To use a different copy, set `PS3000A_LIB`.
- **`nix develop`.** It provides Python with numpy, and sets `PICO_DRIVER_DEPS` to the driver's libusb and libstdc++ dependencies.
- **The capture firmware** (`firmware/`, protocol in `firmware/PROTOCOL.md`). It raises `SCOPE_TRIG` (GPIO26, TP5) from just before each Speak or Speak External until Talk Status clears.

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

## Driving the Pico

`pico.py` talks to the firmware. It finds the Pico by its USB ID; `--port` overrides that.

```
python tools/pico.py info                            # firmware version, clock, loaded banks
python tools/pico.py reset                           # nine 0xFF, then Reset
python tools/pico.py upload 0xF image.bin            # load a VSM image into a bank ('builtin' = Acorn PHROM A)
python tools/pico.py speak 0x3CBB6 --trace out       # speak a word, save the VSM trace in out/
python tools/pico.py speakext builtin --word 0x3CBB6 # the same word through Speak External
python tools/pico.py cmd 0x10 && python tools/pico.py read  # Read Byte: a raw command, then a read
python tools/pico.py demo                            # "Acorn BBC Computer"
python tools/pico.py selftest --repeat 3             # the Phase 3 acceptance test
```

`selftest` needs no scope. It does the following, and exits non-zero if any check fails:

1. Uploads the Acorn image to banks `F` and 3, and checks their CRCs.
2. Resets the chip.
3. Reads a byte with Read Byte (command `0x1x`), and checks it against the image.
4. Speaks six words from each bank and through Speak External, as many times as `--repeat` asks.

For every word it checks:

- the bits the chip read from the VSM against the image;
- that the chip read the whole word, up to its stop code;
- the final status;
- that the Speak External bytes on the bus match the stream;
- that the Speak External duration matches the VSM duration.

The trace is saved as `raw/vsm_trace.bin` (the firmware's 8-byte events, unchanged) and `vsm_trace.csv` (decoded: one row per event, times in µs). `pico.py` can also be used as a library: the `Pico` class, `decodeTrace()`, `hostCycles()` and `bitsRead()`.

## Usage

```
python tools/capture.py captures/test --speak 0x3CBB6 --reset  # arm, speak through the Pico, capture
python tools/capture.py captures/test --speakext builtin --word 0x3CBB6
python tools/capture.py captures/test --seconds 3   # just wait for SCOPE_TRIG, capture
python tools/decode.py captures/test                # decode each Speak window
python tools/edge_timing.py captures/test --plot     # edge timing (use --timebase 3 captures)

# DAC transfer function: pool captures, subtract the scope zero, compare two loads
python tools/capture.py captures/zero --seconds 2 --note "probe A tip on GND"
python tools/dac_transfer.py captures/a captures/b --zero captures/zero --out captures/dac --plot
python tools/dac_compare.py captures/dac captures/dac_half --out captures/compare --plot
```

With `--speak` or `--speakext`, `capture.py` does the following:

1. Arms the scope.
2. Waits 0.2 s plus the pre-trigger time. The scope accepts a trigger only after it has filled its pre-trigger buffer.
3. Clears the trace.
4. Has the Pico speak.
5. Saves the VSM trace beside the scope data.

`capture.json` gets a `pico` section with the firmware version, the loaded banks, the command, its result and the trace's metadata. Without these options, something else has to raise `SCOPE_TRIG`.

`capture.py` writes `raw/scope_raw.npz`, which holds the raw ADC data for channels A–D plus the range, offset and sample interval. It also writes `capture.json`. Raw files are never modified afterwards. To commit an evidence capture, zip `raw/scope_raw.npz` unchanged as `raw/scope_raw.npz.zip` (the `.npz` itself is git-ignored). The tools read the zip directly when the `.npz` is absent. PROM corpus captures go under `captures/corpus/`, which is kept local (plan §6).

`decode.py` writes two files for each word (one SCOPE_TRIG window):

- `word<N>_samples.wav`: the decoded `I/O` samples as 16-bit PCM at 8 kHz. Each 10-bit code (−512 to 511) is left-justified (× 64), so the chip's full scale is the WAV's full scale. It is lossless: the code is the sample shifted right by 6.
- `word<N>_samples.csv`: the sample index, the code, the mean DAC voltage and the `T11` time.

## Files

- `pico.py`: the host side of the firmware protocol, with the trace decoder, the self-test and a command line.
- `lpc.py`: parses LPC frames as far as their boundaries, to find where a word ends. The full frame decoder is Phase 4.
- `ps3000a.py`: ctypes wrapper for the ps3000a driver.
- `tms5220_decode.py`: decodes `I/O` (a 10-bit two's-complement sample, LSB first, in bit slots 2–11 after the `T11` rising edge) and averages the DAC voltage over each settled step.
- `capture.py`, `decode.py`: the command-line tools.
- `dac_transfer.py`: the static DAC transfer function from one or more captures. It writes a per-code table of TP2 voltage and current (`dac_table.csv`), fits one weight per DAC input bit, and reports linearity, the step from −1 to 0, and whether the output depends on earlier samples (`dac_transfer.json`, `dac_transfer.png`). `--zero` subtracts the scope's zero offset, measured from a capture taken with probe A's tip grounded.
- `dac_compare.py`: compares the tables from two loads, giving the current ratio per code and the equivalent output resistance. The reference datasheet's §8.2 and §8.3 come from these two tools.
- `edge_timing.py`: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's §8.4 and §9.5 come from its output.
