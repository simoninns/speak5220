# Encoder test speech and quality

These tools provide the speech inputs used to test the encoder and the new speech data built from them, predict the chip's output with the reference emulator, and score the encoding objectively. The encoder itself is described in [Encoder](../encoder/index.md).

## Encoder test speech

```
python investigation/tools/ttsgen.py fetch     # download the Piper voices into investigation/local/piper/ and check their SHA-256
python investigation/tools/ttsgen.py render    # render every prompt not yet rendered; --force re-renders
python investigation/tools/ttsgen.py check     # check every file against the index
python investigation/tools/recimport.py        # split the real recordings in investigation/local/recordings/ into their prompts
python investigation/tools/recimport.py check  # check those files against their index
```

- **`prompts.csv`** lists the encoder's test phrases: single words found in no PROM's word table, numbers, the first three Harvard sentence lists, phrases that stress one class of sound (sibilants, fricatives, plosives, nasals, liquids, vowels, voiced and unvoiced pairs, pitch, sustained sounds), computer phrases and longer passages. A prompt's `voices` column is `all`, a list of voice keys, or empty, for one voice chosen in turn within its category.
- **`ttsgen.py`** renders them with Piper (in `nix develop`) from nine voices, UK and US, from about 90 Hz to 250 Hz median pitch. The voices' datasets are public domain, CC0, CC BY or CC BY-SA. Each render is resampled to 8 kHz, 16-bit mono, with its peak at −3 dBFS, and written to `investigation/captures/phase11/inputs/tts/` as FLAC (lossless; `soundfile` reads it). Each file's row in `index.csv` gives its text, voice, active level, median and spread of pitch, voiced share, and the SHA-256 of its decoded samples (16-bit little-endian), which does not depend on the FLAC encoder. `README.md` there credits the voices. Piper's render is random, so the same text never gives the same file twice: the committed files are the frozen inputs, and `render` never overwrites one without `--force`. The renders at Piper's own rate are kept in `investigation/local/piper/renders/`.
- **`recimport.py`** imports real recordings. The originals stay in the gitignored `investigation/local/recordings/`, listed in its `sources.csv`: the file, a text file of its prompts (`1. text` per line, in the order spoken), an id prefix, a voice key, and the speaker, source and licence to credit. A recording may hold many prompts with a pause between each. The split takes, of the pauses of 150 ms or more, the set that best fits each segment's duration to its text length while preferring long pauses, then pads each segment with up to 150 ms of its pauses. The segments are written as the TTS files are, to `investigation/captures/phase11/inputs/recordings/<id>_<voice>.flac`, with an `index.csv` that also gives each one's place in the original, and a `README.md`. The split depends only on the original, so it is repeatable.

## New speech data and its predictions

```
python investigation/tools/promstats.py          # how the original PROMs were encoded (datasheet section 12.10)
python investigation/tools/phase11.py predict    # the emulator's predictions, to investigation/captures/phase11/predictions/
python investigation/tools/phase11.py check      # the sets and predictions against their hashes
investigation/captures/phase11/bench.sh          # capture every set: 3 takes per path, standard procedure
python investigation/tools/phase11.py compare    # the chip against the predictions (investigation/captures/phase11/results/)
python investigation/tools/asrcheck.py           # word error rates of input, prediction and chip (whisper.cpp)
```

- **The encoder**, in its first version (commit 01b7fff), encoded the new speech data and packed it. It was written from the reference datasheet alone, by a separate agent, as the emulator was. [Encoder](../encoder/index.md) gives the current encoder's method, and [Encoder choices](../encoder/choices.md) lists what the datasheet left open.
- **`promstats.py`** parses every word of the eight PROMs. It reports the frame kinds, the energy, pitch and K indices used, how words start and end, and the bit rate (`investigation/captures/phase11/promstats/`; see [datasheet §12](../datasheet/authoring.md)).
- **The data sets** in `investigation/captures/phase11/data/` are five synthetic PROMs, which `run.py` captures like any other. They were built with that encoder and are committed, with each word's input in `<set>_items.csv`:
  - **`phrom`:** an Acorn-format PHROM of the single words, numbers and computer phrases;
  - **`tts1`, `tts2`:** every TTS input;
  - **`rec`:** the recordings;
  - **`hand`:** frame sequences built by hand to use every frame kind and transition.

  Inputs longer than 8 s were cut at a pause into several utterances, because the Pico's trace holds about 16 s of VSM speech.
- **The round trip** (acceptance test 3) parsed each PROM word with the encoder's frame parser and packed it again. The bits had to be identical up to the end of the stop code. Its results are committed in `investigation/captures/phase11/roundtrip/`.
- **`phase11.py`**:
  - **`predict`** runs the emulator on every word and path, spoken after the word `run.py` speaks before it. Where the predecessor changes the output, it also predicts the word after itself. The predictions are committed before the chip runs.
  - **`compare`** judges each set against the tolerance set in the [capture plan](../capture/index.md) and checks the VSM timing. It also compares the chip's VSM output with its Speak External output.
- **`asrcheck.py`** transcribes each item's input, prediction and chip output with whisper.cpp (`whisper-cli`; the model is kept in `investigation/local/whisper/`), and reports the word error rate against the prompt. It is a check that words survive, not a substitute for listening.

## Encoding quality

```
python investigation/tools/speechqual.py IN.wav --bin IN.bin                 # speak the frames on the emulator, score against the input
python investigation/tools/speechqual.py IN.wav IN2.flac --bin DIR           # DIR/IN.bin for IN.wav, DIR/IN2.bin for IN2.flac
python investigation/tools/speechqual.py IN.wav --bin IN.bin --wav DIR --anchor  # also write input, chip and anchor audio, aligned
python investigation/tools/speechqual.py --set all --bin DIR --anchor --csv q.csv  # the encoder test inputs (243 files)
python investigation/tools/speechqual.py --set all --bin DIR --ladder        # where the quality goes, step by step
```

`speechqual.py` gives an encoding an objective score. It does not encode: it takes each input's packed frames from `--bin`, which is a file for one input, or a directory holding `NAME.bin` for each input `NAME`. `--set` takes the test inputs (`tts`, `recordings` or `all`). It speaks the frames with `investigation/emulator/tms5220emu` (VSM Speak after a Reset), not with the encoder's own synthesis. It then lines the output up with the input, matches their levels, and compares them:

| Column | Measure |
|---|---|
| `MCD` | Mel-cepstral distortion (dB), coefficients 1–24 of 40 mel bands from 100 to 3800 Hz. It measures the spectral shape, not level or phase. It is computed from a filter bank, not with SPTK's `mcep`, so compare it only with itself |
| `lo`, `mid`, `hi` | The level-normalised spectrum, output minus input (dB), below 800 Hz, from 800 to 2000 Hz, and above 2000 Hz |
| `lvl`, `lvlr` | Per 10 ms frame: the mean absolute level error (dB, after removing the file's median offset), and the correlation of the two level envelopes |
| `drop%` | Speech frames where the output is more than 20 dB below the input |
| `pause` | In the input's pauses: the output's level minus the input's, each relative to its own loudest frame (median, dB). Positive means the output's pauses are noisier |
| `vuv%`, `gpe%`, `f0c` | One pitch tracker, run on both signals: the voicing disagreement, the gross pitch errors (more than 20 %), and the RMS f0 error of the remaining frames in cents |

Speech frames are those within 35 dB of the input's loudest 10 ms frame. The alignment is the shift that best correlates the two level envelopes, refined to within ±25 ms by the closest spectra; `lagms` reports it.

### The anchor

`--anchor` also scores a plain floating-point LPC vocoder of the same input. It uses order 10, 25 ms frames, the K interpolated in 8 steps as on the chip, pulse or noise excitation, and no quantisation. It is independent of the encoder. The gap between the anchor and the chip is what the chip's tables and excitation and the encoder's choices cost together.

### The ladder

`--ladder` scores the steps from the anchor to the chip, one change at a time, and prints each step's median and its change from the step before. It shows where the quality is lost.

| Step | What changes |
|---|---|
| `anchor` | The vocoder above |
| `q` | The chip's K, pitch and energy tables (nearest K in log-area ratio, K1–K4 if unvoiced, the period as value + 1) |
| `qc` | The chip's excitation: the chirp, pre-emphasis for voiced frames only and not undone, and energy set from the input's RMS |
| `qci` | Those frames through `investigation/tools/model.py`'s timing, interpolation and excitation, with a floating-point lattice |
| `enc` | The encoder's frames (from `--bin`) through that floating-point model |
| `chip` | The encoder's frames on the emulator (the main result) |

`qk`, `qp` and `qe` break `q` down: each is the anchor with only the K, pitch or energy table applied.

Steps `q` to `qci` use the anchor's simple analysis, not the encoder's. So the change from `qci` to `enc` is what the encoder's own analysis is worth against it, and the change from `enc` to `chip` is the chip's integer arithmetic. The reference model runs in Python, so a ladder run takes several times as long as a plain run.

### Per-frame measures and audio

`--frames-csv` writes the per-frame measures of one input, to find where an encoding goes wrong. `--wav DIR` writes audio aligned and at the same level, for listening:

- `<name>_input.wav` and `<name>_chip.wav`;
- `<name>_lpc.wav` with `--anchor`;
- `<name>_<step>.wav` for each step with `--ladder`.
