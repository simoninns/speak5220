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

## Capturing PROM words

`run.py` is the capture runner (plan §7). One command takes a PROM, words and a number of takes, and writes complete capture directories (plan §6):

```
python tools/run.py acorn_phrom_a ACORN                    # one take, VSM Speak
python tools/run.py acorn_phrom_a 160 179 --takes 3 --path both
python tools/run.py acorn_phrom_a all --takes 5            # the whole PROM
python tools/run.py acorn_phrom_a --address 0x3CBB7        # a sweep point outside the word table
python tools/prom.py list acorn_phrom_a                    # the word table

# A reference set (plan §12, Phase 7): every word, then the address sweep
python tools/run.py acorn_phrom_a all --takes 5 --path both --sweep --resume --zip-only
python tools/run.py acorn_phrom_a --sweep --sweep-step 64 --sweep-takes 5 --resume --zip-only
```

Words are given by number or by name. `--path` is `vsm` (Speak from the emulated VSM, the default), `external` (Speak External of the same bytes) or `both`. The output goes under `captures/corpus/` (kept local) unless `--out` says otherwise. `--zip` also archives each `raw/scope_raw.npz` as `.npz.zip`, for evidence captures. `--zip-only` keeps only the zip, compressed in the background, which makes corpus takes about 7× smaller. `--board` is as in the table above; `--load-ohms` defaults to 996.

**Long runs.**
- **`--sweep`** adds the address sweep (plan §8), one take through the VSM from each sweep address (`--sweep-takes`). The sweep covers every byte address of the image that is not a word start, except addresses whose data reaches the end of the image without a stop code. Those run into the next bank, which the Pico serves as empty, so what the chip reads there depends on the board, not on the PROM. `--sweep START:END` limits the sweep to a range. `--sweep-step N` takes every Nth address, to give a sample of the sweep extra takes.
- **`--resume`** counts the takes already made that passed every check, and makes only the missing ones, so an interrupted run can be restarted with the same command.
- **Retakes.** A take that fails only checks of the capture procedure (`start_slot`, `start_frame`, `trace_complete`, `scope_overflow`, `io_format`, `fosc`, `external_stream`, `vsm_image`) is kept and retaken, up to `--retries` times (2). If `start_slot` was missed by less than a frame, the path's start may have drifted, so it is recalibrated first. Failures that come from the speech data, such as a sweep point without a stop code, are not retaken.
- **Speed.** The scope is opened once per run, so a take costs its talk time plus about 1.5 s. The runner prints the time left after each entry. If a take raises an error, the runner keeps its directory, which has no `capture.json`, so the tools skip it. Five errors in a row stop the run.

**The standard procedure** (the default, plan §12 Phase 5, datasheet §7). The TMS5220 carries two things from one utterance to the next:
- **A frame clock.** It runs all the time, and a Speak waits for its next boundary.
- **A noise generator.** It runs freely until a Reset holds it, and the next Speak restarts it.

So that every take starts from the same state, the runner works in two stages.

At the start of a run, it:
1. Uploads the PROM image to its bank, if the Pico does not hold it already, and empties every other bank. Reads that run past the end of the image then find an empty bank in every run.
2. Locates the frame clock on the Pico's timer from one Speak's trace (`frameclock.py`).
3. For each path, calibrates the start, so that the chip accepts the Speak command 12256.1 µs before a frame boundary, in the same 6.25 µs slot every time. This takes about 40 s per path.

For each take, it:
1. Sends the reset sequence.
2. Arms the scope for the word's length, worked out from its frames.
3. Clears the trace and has the Pico speak, timed to start half a frame before a frame boundary.
4. Saves `raw/scope_raw.npz`, `raw/vsm_trace.bin` and `capture.json`.
5. Runs `process.py` on the take.

`--start free` speaks as soon as the scope is armed, as in Phase 4; `--reset` then adds the reset sequence before each take, and `--idle` sets the wait between takes.

After each word it runs `compare.py`. At the end, and every 500 entries, it rewrites the PROM's index with `promindex.py`:
- **`index.md`:** the word table and a summary of the sweep.
- **`takes.csv`:** one row per take, with its checks, the SHA-256 of its output and of its raw files. The output hash, `talk_sha256`, covers the `I/O` codes from sample 0 to the end of talk, as 16-bit little-endian integers.
- **`sweep.csv`:** one row per sweep address, with its parse, its checks, its output hash and the number of distinct outputs among its takes.

The corpus is kept local (plan §6). These index files and `prom.json` are the only parts of it in the repo. The runner exits non-zero if any check failed.

```
<out>/<prom>/
  prom.json, index.md, takes.csv, sweep.csv
  sweep/addr_<address>/     one per address sweep point, laid out like a word
  <number>_<name>/          one per word
    compare.json
    vsm01/, vsm02/, ...     one per take: VSM Speak
    ext01/, ...             Speak External
      capture.json          metadata, the raw files' SHA-256 (raw_sha256),
                            plus "processed": summary and checks
      raw/scope_raw.npz     raw scope data (never modified; with --zip-only, only
                            raw/scope_raw.npz.zip, which holds it unchanged)
      raw/vsm_trace.bin     raw VSM trace (never modified)
      vsm_trace.csv, frames.csv, samples.wav, samples.csv, dac.wav, listen.wav
```

PROMs are described by manifests, `proms/<name>.json`: the image's source and hashes, its bank, provenance, target chip and word table. `proms/README.md` is the inventory. `promimport.py` writes both, and rebuilds the images from their pinned public sources into `proms/images/`, which is git-ignored. Run it once before capturing any PROM except the Acorn PHROM A, whose image is in `firmware/src/romdata_acorn.h`:

```bash
python tools/promimport.py              # fetch, rebuild and check every image; write the manifests
python tools/prom.py list ti99_speech   # a PROM's word table
```

An image may span several banks (TI-99's is 32 KB, in banks 0 and 1); `run.py` uploads all of them. `bench.json` records the chip's marking, the measured supplies and the temperature; every `capture.json` copies it, so update it whenever the chip or the board changes.

### Readable files and checks

`process.py` rebuilds a take's readable files from `raw/` and can be rerun at any time: `python tools/process.py <take>...`.

- `frames.csv`: one row per frame the chip parsed, up to the stop code. The bits come from the VSM trace (VSM path) or from the bytes written to the FIFO (Speak External). Columns: bit offset, length, kind, energy, repeat, pitch and the K1–K10 indices. For the VSM path, `read_us` is when the frame's first bit was read, after `SCOPE_TRIG` rose, and `read_sample` is the `samples.wav` sample period it fell in. The parameters are the coded indices; each is taken most significant bit first, as MAME assumes (to be confirmed in Phase 8).
- `samples.wav`, `samples.csv`: layer 2, from the first `T11` period after `SCOPE_TRIG` rose to the end of the capture. The WAV convention is as for `decode.py` below. The CSV adds the DAC voltage and the DAC code recovered from it.
- `dac.wav`: layer 3, the recovered DAC codes (code × 64). The codes come from the Phase 2 bit-weight model fitted to this capture. They are measured, not exact: about 2/3 match `I/O` exactly, and almost all are within ±2.
- `listen.wav`: for listening only. It holds the DAC codes for each sample period at 48 kHz and passes them through a model of the BBC Micro's audio filter (`--listen-filter none` leaves the filter out).

The checks go into `capture.json` under `processed.checks`. Each is true, false, or null when it does not apply:

| Check | Passes when |
|---|---|
| `scope_overflow` | No channel went over range |
| `io_format` | No `I/O` activity outside bit slots 2–11 |
| `fosc` | The `T11` rate gives `OSC` within 50 ppm of the clock set |
| `trace_complete` | No trace events were dropped |
| `talk_status` | Talk Status was seen set, and was clear at the end |
| `stop_frame` | The parsed data ends with a stop frame |
| `vsm_image` | VSM: the bits read are the image's, from the word address |
| `bits_vs_frames` | VSM: the bits read are exactly the frames up to the end of the stop code |
| `frame_period` | VSM: frame reads are 25 ms apart, within 0.1 ms |
| `duration` | VSM: Talk Status cleared within 2 ms of frames × 25 ms after the first frame read. Speak External: the talk time exceeds frames × 200 samples by 0–400 samples |
| `external_stream` | Speak External: the bytes on the bus are the stream sent |
| `start_frame` | VSM: the first frame read is on the frame clock's grid (within 5 µs), and on the frame it was timed for. Speak External: the timed start was not late |
| `start_slot` | Standard procedure: the chip accepted the Speak within 2 µs of the target time before the frame |
| `layer2_vs_layer3` | The Phase 2 DAC model of the `I/O` codes, fitted with a gain and an offset, leaves 3 mV rms or less; the gain is within 15 %; and 99 % of the recovered DAC codes are within ±2. All three are judged on codes inside ±119: at the rails (127, and −128, which clipped codes take) the chip sits 6–10 mV below the model, so the rail samples are reported separately (`rail_samples`, `rail_residual_mean_mv`). The gain is judged only when the codes span at least 32 (`gain_judged`): over a few codes around 0 the fit is dominated by the 6-LSB step between −1 and 0. Null for `--board audio` |

The gain limit is wide on purpose. It catches a wrong board configuration (half load gives about 0.5, and the audio stage leaves a large residual), not drift: the DAC scale was 7 % above the Phase 2 table in Phase 4. The fitted gain is recorded in every capture.

### Comparing repeats

`compare.py` compares the takes of a word with the first one taken: `python tools/compare.py <word dir>` writes `compare.json` there. Each take is shifted by up to ±400 samples to match the first one best, because without the standard procedure the start of speech varies. Then the comparison reports the differing samples, the first difference (counted from the start of speech), the largest and rms difference, and whether the frames are identical.

Takes made with the standard procedure should match exactly, apart from upsets (datasheet §7). These are small departures, of a few codes, that start at a few points in a word. Only the standard takes that passed every check count. A take that missed its start slot has different noise, so it is listed in `standard_failed_takes` instead. For each path, `compare.json` (`standard_variants`) gives:
- the per-sample majority of those takes, as the reference output;
- for each take, how far it departs from that majority, and the sample where each upset starts;
- the number of distinct outputs;
- the samples where no majority exists.

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

The trace is saved as `raw/vsm_trace.bin` (the firmware's 8-byte events, unchanged) and `vsm_trace.csv` (decoded: one row per event, times in µs). `pico.py` can also be used as a library: the `Pico` class, `decodeTrace()`, `hostCycles()` and `bitsRead()`. `Pico.time()` reads the Pico's microsecond timer. `speak()` and `speakExternal()` take an optional `atUs`, a time on that timer at which the command starts.

## Single captures and analysis

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
- `run.py`: the capture runner (see "Capturing PROM words").
- `process.py`: builds a capture's readable files from `raw/` and runs the checks.
- `compare.py`: the repeat comparator. `promindex.py`: writes a PROM's `index.md`, `takes.csv` and `sweep.csv`.
- `prom.py`: PROM manifests (`proms/`). `promimport.py`: the PROM inventory; it rebuilds the images and writes the manifests. `bench.json`: the bench details recorded in every capture.
- `frameclock.py`: the chip's frame clock on the Pico's timer, and the calibration of timed starts. `python tools/frameclock.py` locates the clock and calibrates both paths.
- `lpc.py`: the LPC frame parser: frame boundaries and the coded parameters of each frame.
- `ps3000a.py`: ctypes wrapper for the ps3000a driver.
- `tms5220_decode.py`: decodes `I/O` (a 10-bit two's-complement sample, LSB first, in bit slots 2–11 after the `T11` rising edge) and averages the DAC voltage over each settled step.
- `capture.py`, `decode.py`: single captures outside a PROM, and decoding of multi-word captures (Phases 1–3). `run.py` uses `capture.py`'s scope functions.
- `dac_transfer.py`: the static DAC transfer function from one or more captures. It writes a per-code table of TP2 voltage and current (`dac_table.csv`), fits one weight per DAC input bit, and reports linearity, the step from −1 to 0, and whether the output depends on earlier samples (`dac_transfer.json`, `dac_transfer.png`). `--zero` subtracts the scope's zero offset, measured from a capture taken with probe A's tip grounded.
- `dac_compare.py`: compares the tables from two loads, giving the current ratio per code and the equivalent output resistance. The reference datasheet's §8.2 and §8.3 come from these two tools.
- `edge_timing.py`: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's §8.4 and §9.5 come from its output.
