# Single captures and analysis

`capture.py` makes single captures outside a PROM, and `decode.py` decodes captures that hold several words. The DAC transfer and edge timing tools measure the results behind the reference datasheet's [§8.2, §8.3](../datasheet/dac-and-audio.md), [§8.4](../datasheet/serial-output.md) and [§9.5](../datasheet/electrical.md).

```
python investigation/tools/capture.py investigation/captures/test --speak 0x3CBB6 --reset  # arm, speak through the Pico, capture
python investigation/tools/capture.py investigation/captures/test --speakext builtin --word 0x3CBB6
python investigation/tools/capture.py investigation/captures/test --seconds 3   # just wait for SCOPE_TRIG, capture
python investigation/tools/decode.py investigation/captures/test                # decode each Speak window
python investigation/tools/edge_timing.py investigation/captures/test --plot     # edge timing (use --timebase 3 captures)

# DAC transfer function: pool captures, subtract the scope zero, compare two loads
python investigation/tools/capture.py investigation/captures/zero --seconds 2 --note "probe A tip on GND"
python investigation/tools/dac_transfer.py investigation/captures/a investigation/captures/b --zero investigation/captures/zero --out investigation/captures/dac --plot
python investigation/tools/dac_compare.py investigation/captures/dac investigation/captures/dac_half --out investigation/captures/compare --plot
```

## capture.py

With `--speak` or `--speakext`, `capture.py` does the following:

1. Arms the scope.
2. Waits 0.2 s plus the pre-trigger time. The scope accepts a trigger only after it has filled its pre-trigger buffer.
3. Clears the trace.
4. Has the Pico speak.
5. Saves the VSM trace beside the scope data.

`capture.json` gets a `pico` section with the firmware version, the loaded banks, the command, its result and the trace's metadata. Without these options, something else has to raise `SCOPE_TRIG`.

`capture.py` writes `raw/scope_raw.npz`, which holds the raw ADC data for channels A–D plus the range, offset and sample interval. It also writes `capture.json`. Raw files are never modified afterwards. To commit an evidence capture, zip `raw/scope_raw.npz` unchanged as `raw/scope_raw.npz.zip` (the `.npz` itself is git-ignored). The tools read the zip directly when the `.npz` is absent. PROM corpus captures go under `investigation/captures/corpus/`, which is kept local ([Data layout](../capture/data-layout.md)).

## decode.py

`decode.py` writes two files for each word (one SCOPE_TRIG window):

- `word<N>_samples.wav`: the decoded `I/O` samples as 16-bit PCM at 8 kHz. Each 10-bit code (−512 to 511) is left-justified (× 64), so the chip's full scale is the WAV's full scale. It is lossless: the code is the sample shifted right by 6.
- `word<N>_samples.csv`: the sample index, the code, the mean DAC voltage and the `T11` time.

## DAC and edge timing tools

- **`dac_transfer.py`**: the static DAC transfer function from one or more captures. It writes a per-code table of TP2 voltage and current (`dac_table.csv`), fits one weight per DAC input bit, and reports linearity, the step from −1 to 0, and whether the output depends on earlier samples (`dac_transfer.json`, `dac_transfer.png`). `--zero` subtracts the scope's zero offset, measured from a capture taken with probe A's tip grounded.
- **`dac_compare.py`**: compares the tables from two loads, giving the current ratio per code and the equivalent output resistance. The reference datasheet's [§8.2 and §8.3](../datasheet/dac-and-audio.md) come from these two tools.
- **`edge_timing.py`**: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's [§8.4](../datasheet/serial-output.md) and [§9.5](../datasheet/electrical.md) come from its output.
