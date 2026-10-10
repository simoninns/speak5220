# Encoder usage

## Command line

    nix develop -c python3 investigation/encoder/tms5220enc.py encode IN.flac OUT.bin [--csv OUT.csv] [--wav OUT.wav]
                                                 [--rms CODES] [--peak CODES] [--no-repeat]
                                                 [--repeat-lambda L] [--pre-emphasis chirp|MU]
                                                 [--voicing-evidence W] [--voicing-threshold T]
                                                 [--agc RATIO] [--compression RATIO]

- `OUT.bin` holds the packed bytes: frames to the stop code, then zero
  padding to the end of its byte ([§12.1](../datasheet/authoring.md)).
- `--csv` writes one row per frame: `frame,kind,energy,repeat,pitch,k1,...,k10`,
  with blanks where a field is absent. `kind` is `voiced`, `unvoiced`,
  `repeat`, `silence` or `stop`.
- `--wav` writes the reference-algorithm synthesis: codes × 64, 16-bit, 8 kHz.
  It starts at the sample period of the first frame read, so it holds about
  25 ms of idle (code −1) before the sound.
- `--rms` sets the level target (default 20 codes). `--peak` normalises the
  loudest peak instead. `--no-repeat` disables repeat frames.
- The other switches set the options of the same names (see
  [`encode()` options](#encode-options)), with the same defaults:
  - `--repeat-lambda` (`repeatLambda`, 70): lower gives fewer bits, higher
    better quality;
  - `--pre-emphasis` (`preEmphasis`, `chirp`): `chirp`, or a coefficient such
    as `0.9` for the first-order filter of §12.3;
  - `--voicing-evidence` (`voicingEvidence`, 4): `0` decides voicing on
    periodicity alone;
  - `--voicing-threshold` (`voicingThreshold`, 0.3): higher gives more voiced
    frames;
  - `--agc` (`levelAgc`, 1: off) and `--compression` (`compression`, 1: off):
    level processing for playback on real hardware
    ([Energy](how-it-works.md#energy)). `--agc 4 --compression 1.5` is a
    reasonable start.

Input of any sample rate is resampled to 8 kHz; stereo is mixed to mono.

## API

    import sys; sys.path.insert(0, "encoder"); import tms5220enc as enc

    frames = enc.encode(samples, rate=8000, **options)   # float samples in -1..1; ends with a stop
    data = enc.pack(frames)                              # bytes, as the chip reads them
    frames, bitsUsed = enc.parse(data, maxFrames=100000)
    codes = enc.synthesize(frames)                       # numpy int32 output codes (§8.9)
    stream = enc.speakExternalStream(frames)             # at least 9 bytes (§12.9)
    image, table = enc.acornPhrom(words, strings=(...), asciiMap=None)
    image, table = enc.tiIndexed(words)

- **Frames** are dicts of coded indices:
  - silence is `{"energy": 0}`, and stop is `{"energy": 15}`;
  - a repeat frame is `{"energy", "repeat": 1, "pitch"}`;
  - a full frame is `{"energy", "repeat": 0, "pitch", "k": [...]}`, with 4 K
    indices when unvoiced (pitch 0) and 10 when voiced.

  `pack()` validates every field and stops after the first stop frame.
  `parse()` returns the frames up to and including the stop code. If the bits
  run out first, it returns the complete frames read so far, and `bitsUsed`
  counts only their bits.
- **`synthesize(frames)`** runs the [§8.9 reference algorithm](../datasheet/reference-algorithm.md)
  for one VSM Speak from power-up state. Sample 0 is the sample period of the
  first frame read. The output is code −1 until the sound starts at about
  sample 203–208. Frame *j*'s own stretch is centred on sample 225 + 200·*j*.
  The array ends once the output has been idle for 25 samples after the stop.
  On every frame list tried, it gives the same codes as the reference
  emulator `investigation/emulator/tms5220emu`, sample for sample, through the VSM path. The
  Speak External path runs 2 samples earlier with other noise
  ([§7](../datasheet/power-up.md), [§8.5](../datasheet/frame-timing.md));
  `synthesize()` does not model that.
- **`encodeDetailed()`** is `encode()` that also returns its analysis (the
  filtered signal, frame centres, level gain and limiter statistics).
  `evaluate.py` uses it.
- **`acornPhrom(words, ...)`**: `words` is a list of `(name, frames)`,
  numbered from 127. `asciiMap` maps characters, or codes 32–126, to word
  names. It returns a 16 KB image, and a table of
  `{"number", "name", "offset", "bytes", "frames"}`, with `frames` the number
  of frames, stop included. ASCII entries with no word point to a shared
  one-byte stop code. It raises `ValueError` if the words do not fit.
- **`tiIndexed(words)`**: the same, in TI's indexed format (§12.8). Words are
  numbered from 1, and each word is followed by one spare byte.
- Helpers: `frameKind`, `frameBits`, `booth` (the
  [§8.8](../datasheet/lattice-filter.md) multiplier, step by step),
  `boothFast`, `wrap14`, `levinson`, `stepUp`, `bitReverse`, `framesCsv`, and
  the tables `ENERGY`, `PITCH`, `KTABLES`, `CHIRP`.

## `encode()` options

| Option | Default | Meaning |
|---|---|---|
| `targetRms` | 20.0 | Output RMS over the sounding frames, in codes (§12.3: the originals have 19–20) |
| `targetPeak` | None | If set, scale so that the loudest output peak is this many codes, instead of an RMS |
| `peakLimit` | 120 | No output code beyond ±this. Frames that would exceed it are turned down |
| `repeats` | True | Use repeat frames |
| `repeatLambda` | 70 | Bits a run of repeats may trade per dB of extra prediction error ([Repeat and silence decisions](how-it-works.md#repeat-and-silence-decisions)). Lower gives fewer bits |
| `gateDb` | 45 | Frames this far below the loudest frame are silence |
| `preEmphasis` | `"chirp"` | Pre-emphasis for voiced analysis: `"chirp"` (the chirp's inverse, see [Pre-emphasis](how-it-works.md#pre-emphasis)), or μ for 1 − μz⁻¹ (§12.3) |
| `window` | 240 | Analysis window, in samples (30 ms) |
| `lagWindowHz` | 60 | Gaussian lag-window bandwidth |
| `voicingThreshold` | 0.3 | Base CMNDF level at which voiced and unvoiced cost the same |
| `voicingEvidence` | 4 | Weight of spectral tilt and zero crossings in the voicing threshold (0: off) |
| `iterations` | 5 | Closed-loop level passes |
| `fadeUnvoiced` | True | Fade a hiss before silence and before the stop |
| `levelAgc` | 1 | Slow gain control: ratio by which a 0.75 s loudness envelope is compressed (1: off) |
| `compression` | 1 | Ratio by which each frame's level in dB is compressed about the median (1: off) |

`encode()` is deterministic: it uses no random numbers, and the same input
always gives the same frames.

## Self-test

`nix develop -c python3 investigation/encoder/test_tms5220enc.py` exits non-zero on failure.
It checks:

- **Frames and bits:** the lengths of every frame kind; pack/parse round trips
  on 300 random frame lists and a hand-made one with every kind; truncated
  data and `maxFrames`; invalid fields rejected; packing stops at the stop
  code; bytes hand-assembled from the §8.1/§12.1 bit order; the §6 Read Byte
  reversal examples.
- **Arithmetic:** every row of the §8.8 multiplier examples (digits and
  products); the closed form against the step-by-step multiplier on 20,000
  random pairs; the 14-bit wrap; the tables (chirp energy 65,546, monotonic K
  tables, pitch periods).
- **Sign convention (§12.3):** a one-stage chip lattice against
  1/(1 + *k*₁*z*⁻¹), and Levinson recovering the same *k*₁; in the full
  synthesis, a large negative K1 is low-pass and a large positive K1 is
  high-pass.
- **Synthesis:** the start of speech (unvoiced energy at sample 203, voiced
  output within 205–209) and its end; a lone stop gives no output; and, if
  `investigation/emulator/tms5220emu` is built, three random speech-like frame lists give
  the emulator's codes sample for sample.
- **Packaging:** the ≥9-byte rule (padding only when needed, silence before
  the stop); the PHROM layout read back (header through bit reversal, counts,
  pointers, the 0xFF, names backwards before the data, ASCII aliases, the
  null word, overflow); the TI indexed layout read back, with its spare bytes
  and overflow.
- **Predecessor:** the §12.2 pattern (unvoiced, silence, before any voiced
  frame) depends on the stored indices, and the encoder's guard removes the
  dependence.
- **`encode()`** on `w05_vctk_lo.flac` ("marmalade"): a valid frame list that
  round-trips, a single stop at the end, a full first frame, no voiced repeat
  after unvoiced, no wraps, peaks within ±127, a sensible level, independence
  from the predecessor, determinism, no repeats when they are disabled, fewer
  bits at a lower `repeatLambda`, and a narrower spread of frame levels with
  `levelAgc` and `compression`.
- **Pre-emphasis and repeats:** the chirp inverse filter (its coefficients,
  and that it flattens the chirp), and the repeat search merging identical
  frames and splitting at a change.
