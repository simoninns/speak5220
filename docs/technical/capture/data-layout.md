# Data layout and file formats

Every capture is written in the same directory layout, whether it is a word of a PROM, a swept address or a synthetic vector. The raw files are the record of what was measured; everything else is regenerated from them.

## Per PROM

```
investigation/captures/<prom>/
  prom.json           # inventory: name, hash, provenance, target chip, word table
  index.md            # browsable page: word list, frame counts, links to audio, sweep summary
  takes.csv           # every take: its checks, the hash of its output and of its raw files
  sweep.csv           # every swept address: its parse, checks and output hash
  sweep/addr_<address>/   # one directory per swept address, laid out like a word
  <word>/             # one directory per word, <number>_<name>
    compare.json      # every take compared with the first (investigation/tools/compare.py)
    vsm01/, vsm02/    # one capture directory per take: VSM Speak ...
    ext01/, ...       # ... and Speak External
```

`prom.json` is a copy of the PROM's manifest, `investigation/proms/<prom>.json` (`investigation/tools/prom.py`). The manifest gives the image's hashes, its sources and the reference dump it matches, its target chip and its word table. `investigation/tools/promimport.py` writes the manifests and the [PROM inventory](prom-inventory.md). It also rebuilds the images from their pinned public sources into `investigation/proms/images/`, which is git-ignored, so images that may not be redistributed stay out of the repo ([PROM corpus](prom-corpus.md)).

## Per capture

```
investigation/captures/<prom>/<word>/<take>/
  capture.json        # metadata (see below)

  # Original capture format: written once at capture time, never modified
  raw/scope_raw.npz   # raw PicoScope ADC streams (Ch A-D) + range/offset/timebase
                      #   (committed as scope_raw.npz.zip, see "Where it is stored")
  raw/vsm_trace.bin   # layer 1: every VSM command and returned bit, with timestamps

  # Readable formats: regenerated from raw/ by the post-processing scripts
  vsm_trace.csv       # layer 1 as text
  frames.csv          # the frames the chip parsed: bit offset, energy, pitch, K1-K10, repeat flag
  samples.wav         # layer 2: I/O decoded from the scope capture, mono PCM, rate = fosc/80,
                      #   from the first T11 period after SCOPE_TRIG rose to the end of the capture
  samples.csv         # layer 2 as text (sample index, value, T11 timestamp), with the DAC voltage
                      #   and the DAC code recovered from it
  dac.wav             # layer 3: recovered per-sample DAC codes as WAV
  listen.wav          # convenience: filtered and resampled to 48 kHz for listening
```

The `raw/` files are the record of what was measured. The `scope_raw.npz` layout (array names, dtypes, scaling) is documented alongside the scripts, so it can be read without the capture tooling. Every readable file can be rebuilt from `raw/` with one command (`investigation/tools/process.py`), and `capture.json` records the script version that built it, under `processed`, together with the checks.

## WAV conventions

- **`samples.wav`:** 16-bit signed PCM, one WAV sample per chip sample, no resampling. The header records the nominal rate (8000 Hz). The *measured* rate goes in the JSON. The chip's 10-bit code is left-justified (code × 64), so the chip's full scale is the WAV's full scale and the file plays at a normal level. The code is the sample shifted right by 6, and the bit width and scaling are documented in the JSON. (Storing the code in the low bits would be equally exact, but at about −48 dBFS it plays as near silence.) A WAV like this is bit-exact, plays in any tool, and diffs trivially against an emulator's output.
- **`dac.wav`:** the same convention, built from the layer 3 recovered codes.
- **`listen.wav`:** 48 kHz, optionally with the BBC filter applied. Nothing is computed from it. It is for ears only.
- No MP3, FLAC-with-resampling or any other processing is used in the reference files. FLAC is fine for archive size, because it is lossless.

## `capture.json` contents

It records at least:

- The source PHROM (name, file hash), the word number, the VSM address and the command sequence sent.
- The chip: part marking, date code and revision (5220 or 5220C).
- The board revision, the measured `fosc`, the supply voltages and the temperature if it is known.
- The firmware git hash, the host script git hash and the capture timestamp.
- The final status register value, the `INT` events, the count of bits consumed and the total samples.
- The capture procedure ([repeatability](work/repeatability.md)): whether the chip was reset first, and, for the standard procedure, the frame the Speak was timed for and when the chip accepted it.

`investigation/tools/run.py` writes all of these. The chip, supply and temperature details come from `investigation/tools/bench.json`, which must be updated whenever the chip or the board changes. The results of the checks go under `processed` (`investigation/tools/process.py`).

## Where it is stored

The raw scope data is large. Four channels at 2 MS/s is about 8 MB per second of speech uncompressed, so a full PROM with repeats runs to gigabytes. Zip compression brings that down by about 10× (the bench bring-up captures shrank from 305 MB to 26 MB), because the digital channels compress well. There are three kinds of capture:

- **Evidence captures** live in `investigation/captures/phase<N>/<id>/`, are cited by the datasheet ([evidence index](../datasheet/evidence.md)), and are **committed to the repo**. The raw file is committed as `raw/scope_raw.npz.zip`: a zip that holds the original `scope_raw.npz` unchanged, so its SHA-256 still matches the evidence index. The tools read the zip directly, and `.gitignore` excludes the unzipped `.npz`. Each directory's evidence is kept to a few tens of MB.
- **PROM corpus captures** live in `investigation/captures/corpus/<prom>/` and are **kept local**. Only the raw data's zip is kept (`run.py --zip-only`), which makes a take about 2 MB. Apart from each PROM's `prom.json`, `index.md`, `takes.csv` and `sweep.csv`, the directory is git-ignored. The repo holds the scripts, manifests and those index files. `takes.csv` gives the SHA-256 of every take's raw files, so the local copies can be checked, and of its output, so an emulator can be checked against the output without the raw data. The local corpus is the only copy, so it is backed up separately. How to publish it is an [open question](open-questions.md#publishing-the-corpus).
- **Vector captures** live in `investigation/captures/vectors/vectors_<set>/` and are kept local in the same way. Only each set's `index.md` and `takes.csv` are committed; the set's definition and image are in `investigation/captures/phase8/vectors/`, so the vectors can be captured again on any board ([synthetic vectors](work/synthetic-vectors.md)).
