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

Words are given by number or by name. `--path` is `vsm` (Speak from the emulated VSM, the default), `external` (Speak External of the same bytes) or `both`. Words shorter than 9 bytes are spoken through the VSM only: Speak External does not start speech until the FIFO holds 9 bytes (datasheet §6). The output goes under `captures/corpus/` (kept local) unless `--out` says otherwise. `--zip` also archives each `raw/scope_raw.npz` as `.npz.zip`, for evidence captures. `--zip-only` keeps only the zip, compressed in the background, which makes corpus takes about 7× smaller. `--board` is as in the table above; `--load-ohms` defaults to 996.

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

- `frames.csv`: one row per frame the chip parsed, up to the stop code. The bits come from the VSM trace (VSM path) or from the bytes written to the FIFO (Speak External). Columns: bit offset, length, kind, energy, repeat, pitch and the K1–K10 indices. For the VSM path, `read_us` is when the frame's first bit was read, after `SCOPE_TRIG` rose, and `read_sample` is the `samples.wav` sample period it fell in. The parameters are the coded indices, each taken most significant bit first (confirmed in Phase 8; the values are in datasheet §8.1).
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

## Synthetic vectors, the model and the protocol tests (Phase 8)

```
python tools/vectors.py build all                  # write the vector sets to captures/phase8/vectors/
python tools/run.py captures/phase8/vectors/energy.json all --takes 2 --path both \
    --out captures/vectors --zip-only --resume     # capture a set like a PROM
python tools/model.py captures/vectors/vectors_energy/*/vsm01   # compare takes with the model
python tools/vecfit.py captures/vectors            # every take vs the model; table measurements
python tools/protocol.py all                       # host interface and VSM command tests (no scope)
python tools/protocol.py fifo_start --analyse      # rebuild a test's result.json from its traces
```

- **`vectors.py`** builds sets of frame sequences (energy, pitch, K, noise, interpolation, special frames, random, loud, Phase 9's ring and loc sets, and Phase 10's onset set). Each set is a synthetic PROM: a manifest (`<set>.json`) whose word table lists the vectors, the image (`<set>.bin`), and every vector's frames (`<set>.csv`). `run.py` accepts a manifest path in place of a PROM name. Speak External streams are the vectors' bytes up to their stop codes. `lpc.encodeFrames` does the encoding: each field most significant bit first, the bytes read least significant bit first.
- **`model.py`** is the candidate model of the synthesis path that the datasheet's §8.5–§8.9 describe. Its defaults are the measured behaviour; its options select the alternatives that were tested.
  - **`compareTake`** models a standard-procedure take and compares it with the chip's `I/O` codes. The noise state and alignment for each path come from `STANDARD_START`. Its `before` argument speaks earlier utterances first: the coded parameters carry over (datasheet §7).
  - **It is not the Phase 9 emulator.** That is written from the datasheet alone.
- **`vecfit.py`** writes `captures/phase8/analysis/`:
  - `vectors.csv`: every take compared with the model;
  - `tables.csv`: each table entry perturbed by 1, 2 and 4 either way, confirmed when its value fits best;
  - `pitch_periods.csv`: each pitch period, measured from the output alone;
  - `summary.json`.
- **`protocol.py`** drives the chip with raw commands and bytes (`CMD`, `READ`, `WAIT`) and keeps each step's raw trace. The trace times every host cycle with READY and `INT`, and every VSM function, on the chip's crystal. Results go to `captures/phase8/protocol/<test>/`.
  - **Between steps** it ends any Speak External first. The reset sequence cannot: during Speak External its bytes are FIFO data.
  - **The tests:** FIFO start and starvation, the start of Speak External against the frame clock, commands written during speech, a held command, Reset bytes during Speak External, and the VSM commands.

The vector captures, like the corpus, are kept local; each set's `index.md` and `takes.csv` are committed.

## The reference emulator and the regression harness (Phase 9)

```
nix develop -c make -C emulator test               # build the emulator and run its self-test
python tools/vectors.py build ring                 # the Phase 9 vector set (filter arithmetic)
python tools/regress.py vectors corpus             # every vector set and the Acorn PHROM A words
python tools/regress.py corpus --sweep             # ... and its address sweep
python tools/regress.py vectors --model            # also compare the emulator with model.py
python tools/vectors.py build loc                  # localisation and decay vectors (lowest-order error)
python tools/vectors.py build onset                # Phase 10: the first pitch pulse after silence
python tools/variance.py                           # chip against itself, chip against chip, model against chip
python tools/departures.py corpus repeat            # where the chip departs from the model and from itself
python tools/replay.py                             # the Phase 8 host tests (p8-19 to p8-25) through the emulator
python tools/replay.py --detail vsm_commands read_byte_repeat   # one step, chip and emulator side by side
```

- **`emulator/`** is the reference emulator: C99, written from the reference datasheet alone (its `README.md` and `GAPS.md`). It is stepped one slot at a time and covers the host interface, the VSM interface and the synthesis path.
- **`replay.py`** replays the Phase 8 host-interface and VSM command tests (`protocol.py`, p8-19 to p8-25) through the emulator. Each step's recorded host cycles become a scenario, written at the slots of their recorded strobes. The step's position against the frame clock is not recorded, so it is fitted: one value per step, searched over all 4000 slots of a frame. It compares:
  - the value of every status and data read;
  - READY latencies;
  - INT edges;
  - the times of the Load Address, Dummy Read and Read and Branch functions;
  - every VSM bit read.

  A step is compared only up to the end of its recording, or up to a host cycle that the chip never released. Results are in `captures/phase9/replay/`. `--detail TEST STEP` prints one step's recorded and emulated timelines side by side.
- **`regress.py`** runs the emulator on every standard take that passed its checks, and compares it with the chip.
  - **Rebuilding a take.** Each take is rebuilt from its `capture.json`: the image and address, the path, and the time the command was latched before the frame boundary. The takes captured before it are spoken first, back to the latest that has a voiced frame, because the coded parameters carry over (datasheet §7).
  - **Layer 2:** the `I/O` codes, from 5 samples before the first frame read to the end of the capture.
  - **Layer 1, VSM path:** the number of bits read, the sample period of each frame read, and when Talk Status cleared.
  - **`--model`:** also runs `model.py` on the same take, and counts the samples where the emulator and the model differ. Zero means the datasheet text reproduces the model exactly.
  - **Output:** one CSV per set and `summary.json` in `captures/phase9/regress/`. `TMS5220EMU` selects another emulator binary.
- **`variance.py`** measures how far apart two outputs of the same utterance are, the same way for five kinds of pair:
  - chip 1's own takes, in the same session;
  - chip 1 two days apart (`captures/vectors_chip1_repeat/`);
  - chip 4 against chip 1: the vector sets in `captures/vectors_chip4/`, and the PROM words in `captures/corpus_chip4/` (takes 2-5, which follow the same word on both chips);
  - the model against each chip, rebuilt as `regress.py` does.

  For each pair it gives the share of samples equal, the RMS and largest difference, and the signal-to-difference ratio, over the speech window. Corpus takes are paired only when both follow the same word, because the filter's state carries over. Output: `pairs.csv` and `summary.json` in `captures/phase9/variance/` (datasheet §8.8, "Expected agreement").

- **`departures.py`** (Phase 10) finds where the chip departs from the model, and from itself, in groups of takes of one word and path that all follow a take of the same word. Its sources are `corpus` (every PROM's words) and `repeat` (`captures/phase10/repeat/`, 49 takes of six words). It labels deterministic departures (every take agrees, the model differs), upset starts (the model and the takes agree, then some takes leave) and a sample of controls, each with the model's state there: u1 and its position in the output step, the excitation, and each K with its Booth digits and the b value it multiplies. Output in `captures/phase10/departures/<source>/`: `events.csv` (kept local), `words.csv` and `summary.json`, with a logistic fit of each kind against the controls (`docs-tech/lowest_order_arithmetic_ideas.md`).


**Other clocks.** `python tools/pico.py clock <hz>` (while the chip is idle) sets `OSC`, and `run.py` then works as at 640 kHz. The frame period is taken from the exact PWM divider, because `INFO` rounds the frequency to whole hertz. The Speak's target latch time scales with the clock, so the noise generator restarts in the same slot and `model.py` lines up as at 640 kHz. Restore 640 kHz afterwards.

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

## Encoder test speech (Phase 11)

```
python tools/ttsgen.py fetch     # download the Piper voices into local/piper/ and check their SHA-256
python tools/ttsgen.py render    # render every prompt not yet rendered; --force re-renders
python tools/ttsgen.py check     # check every file against the index
python tools/recimport.py        # split the real recordings in local/recordings/ into their prompts
python tools/recimport.py check  # check those files against their index
```

- **`prompts.csv`** lists the encoder's test phrases: single words found in no PROM's word table, numbers, the first three Harvard sentence lists, phrases that stress one class of sound (sibilants, fricatives, plosives, nasals, liquids, vowels, voiced and unvoiced pairs, pitch, sustained sounds), computer phrases and longer passages. A prompt's `voices` column is `all`, a list of voice keys, or empty, for one voice chosen in turn within its category.
- **`ttsgen.py`** renders them with Piper (in `nix develop`) from nine voices, UK and US, from about 90 Hz to 250 Hz median pitch. The voices' datasets are public domain, CC0, CC BY or CC BY-SA. Each render is resampled to 8 kHz, 16-bit mono, with its peak at −3 dBFS, and written to `captures/phase11/inputs/tts/` as FLAC (lossless; `soundfile` reads it). Each file's row in `index.csv` gives its text, voice, active level, median and spread of pitch, voiced share, and the SHA-256 of its decoded samples (16-bit little-endian), which does not depend on the FLAC encoder. `README.md` there credits the voices. Piper's render is random, so the same text never gives the same file twice: the committed files are the frozen inputs, and `render` never overwrites one without `--force`. The renders at Piper's own rate are kept in `local/piper/renders/`.
- **`recimport.py`** imports real recordings. The originals stay in the gitignored `local/recordings/`, listed in its `sources.csv`: the file, a text file of its prompts (`1. text` per line, in the order spoken), an id prefix, a voice key, and the speaker, source and licence to credit. A recording may hold many prompts with a pause between each. The split takes, of the pauses of 150 ms or more, the set that best fits each segment's duration to its text length while preferring long pauses, then pads each segment with up to 150 ms of its pauses. The segments are written as the TTS files are, to `captures/phase11/inputs/recordings/<id>_<voice>.flac`, with an `index.csv` that also gives each one's place in the original, and a `README.md`. The split depends only on the original, so it is repeatable.

## New speech data and its predictions (Phase 11)

```
nix develop -c python3 encoder/test_tms5220enc.py   # the encoder's self-test
python tools/promstats.py          # how the original PROMs were encoded (datasheet section 12.10)
python tools/phase11.py roundtrip  # acceptance test 3: every PROM word parsed and re-packed
python tools/phase11.py build      # encode the inputs, write the data sets to captures/phase11/data/
python tools/phase11.py predict    # the emulator's predictions, to captures/phase11/predictions/
python tools/phase11.py check      # the sets and predictions against their hashes
captures/phase11/bench.sh          # capture every set: 3 takes per path, standard procedure
python tools/phase11.py compare    # the chip against the predictions (captures/phase11/results/)
python tools/asrcheck.py           # word error rates of input, prediction and chip (whisper.cpp)
```

- **`encoder/`** is the project's encoder and packager. It was written from the reference datasheet alone, by a separate agent, as the emulator was. Its `README.md` gives the method, and its `GAPS.md` lists what the datasheet left open.
- **`promstats.py`** parses every word of the eight PROMs. It reports the frame kinds, the energy, pitch and K indices used, how words start and end, and the bit rate (`captures/phase11/promstats/`).
- **`phase11.py`**:
  - **`roundtrip`** parses each PROM word with the encoder's frame parser and packs it again. The bits must be identical up to the end of the stop code.
  - **`build`** writes five synthetic PROMs, which `run.py` captures like any other:
    - **`phrom`:** an Acorn-format PHROM of the single words, numbers and computer phrases;
    - **`tts1`, `tts2`:** every TTS input;
    - **`rec`:** the recordings;
    - **`hand`:** frame sequences built by hand to use every frame kind and transition.

    Inputs longer than 8 s are cut at a pause into several utterances, because the Pico's trace holds about 16 s of VSM speech.
  - **`predict`** runs the emulator on every word and path, spoken after the word `run.py` speaks before it. Where the predecessor changes the output, it also predicts the word after itself. The predictions were committed before the chip ran.
  - **`compare`** judges each set against the tolerance of plan §1.5 and checks the VSM timing. It also compares the chip's VSM output with its Speak External output.
- **`asrcheck.py`** transcribes each item's input, prediction and chip output with whisper.cpp (`whisper-cli`; the model is kept in `local/whisper/`), and reports the word error rate against the prompt. It is a check that words survive, not a substitute for listening.

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

## Die image

`die.py` reads the TMS5220NL die photograph (`docs-tech/die_analysis/`). The image goes in `local/die_image/` (git-ignored), along with everything made from it. All positions are full-resolution pixels, origin top left.

```
python tools/die.py cache                   # decode the 85 MB JPEG once, to a raw memmap (809 MB)
python tools/die.py tiles                   # Deep Zoom pyramid and viewer.html (114 MB)
python -m http.server -d local/die_image/tiles   # then open http://localhost:8000/viewer.html
python tools/die.py crop 16500 4300 1300 1600 --scale 0.75 --out p.png
python tools/die.py crop --cell pad21       # a record of cells.json, with a margin
python tools/die.py annotate                # redraw docs-tech/die_analysis/fig_floorplan.jpg
python tools/die.py tiles --viewer-only     # refresh the viewer's overlay after editing cells.json
```

The viewer draws every record of `docs-tech/die_analysis/cells.json` and shows the image coordinates under the mouse, ready to copy into a new record.

## Files

- `pico.py`: the host side of the firmware protocol, with the trace decoder, the self-test and a command line.
- `run.py`: the capture runner (see "Capturing PROM words").
- `process.py`: builds a capture's readable files from `raw/` and runs the checks.
- `compare.py`: the repeat comparator. `promindex.py`: writes a PROM's `index.md`, `takes.csv` and `sweep.csv`.
- `prom.py`: PROM manifests (`proms/`). `promimport.py`: the PROM inventory; it rebuilds the images and writes the manifests. `bench.json`: the bench details recorded in every capture.
- `frameclock.py`: the chip's frame clock on the Pico's timer, and the calibration of timed starts. `python tools/frameclock.py` locates the clock and calibrates both paths.
- `lpc.py`: the LPC frame parser and encoder: frame boundaries, the coded parameters of each frame, and frames back to bytes.
- `vectors.py`, `model.py`, `vecfit.py`, `protocol.py`: the Phase 8 vectors, the synthesis model, the table measurements and the host interface tests (see above).
- `regress.py`, `variance.py`: the Phase 9 regression harness for the reference emulator in `emulator/`, and the measured agreement between chips and the model (see above).
- `ttsgen.py`, `prompts.csv`, `recimport.py`: the Phase 11 encoder test speech and recordings (see above).
- `promstats.py`, `phase11.py`, `asrcheck.py`: Phase 11's PROM statistics, new speech data, predictions, comparison and intelligibility check (see above). The encoder itself is in `encoder/`.
- `ps3000a.py`: ctypes wrapper for the ps3000a driver.
- `tms5220_decode.py`: decodes `I/O` (a 10-bit two's-complement sample, LSB first, in bit slots 2–11 after the `T11` rising edge) and averages the DAC voltage over each settled step.
- `capture.py`, `decode.py`: single captures outside a PROM, and decoding of multi-word captures (Phases 1–3). `run.py` uses `capture.py`'s scope functions.
- `dac_transfer.py`: the static DAC transfer function from one or more captures. It writes a per-code table of TP2 voltage and current (`dac_table.csv`), fits one weight per DAC input bit, and reports linearity, the step from −1 to 0, and whether the output depends on earlier samples (`dac_transfer.json`, `dac_transfer.png`). `--zero` subtracts the scope's zero offset, measured from a capture taken with probe A's tip grounded.
- `dac_compare.py`: compares the tables from two loads, giving the current ratio per code and the equivalent output resistance. The reference datasheet's §8.2 and §8.3 come from these two tools.
- `edge_timing.py`: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's §8.4 and §9.5 come from its output.
- `die.py`: the die image tools (see "Die image").
