# Encoder quality

## The evaluation

The figures on this page are for the encoder at its default options, on the
243 test inputs of the encoder's corpus: 210 TTS files
from nine voices with median f0 87–246 Hz, and 33 recorded sentences
(`arctic_*_si`). The corpus is not in the working tree:
`app/scripts/fetch-corpus.sh` restores it from the repository's history
into `local/corpus/`. `app/scripts/option_sweep.py` encodes them as a project
(from the WAV copies `app/scripts/generate_corpus.py` writes) and measures
them, and `speechqual.py`, a research tool, spoke the same frames on the
application's emulator. It was removed with the research after commit
`c537c1d` ([The research](../index.md#the-research)). Two sets of measures:

- **The application's quality rating** (`speak5220-cli measure`; the
  [user manual](../../application/user-manual/encode.md#the-quality-rating)
  describes it). On the frames a voice activity detector marks as speech, it
  compares the encoding with the input: the log spectral distortion of the
  spectral envelope (LSD), the gross pitch error (GPE, more than 20 % out),
  the voicing decision error (VDE) and ESTOI, scored and combined into an
  overall score from 0 to 1 (*Good* from 0.70, *Poor* below 0.50). The same
  run gives the output's loudness, its peak, its clipping at the DAC and its
  lattice wraps.
- **`speechqual.py`**:
  the frames spoken on the emulator (VSM Speak after a Reset), aligned with
  the input by their level envelopes, and compared by mel-cepstral distortion
  (MCD), the spectrum's balance in three bands, the level error and the
  voicing. Its `--ladder` gives the steps from an unquantised vocoder to the
  chip ([Where the quality goes](#where-the-quality-goes)).

## Results

| Measure | TTS (210) | Recordings (33) | All (243) |
|---|---|---|---|
| Score, median (10th–90th pct of files) | 0.748 (0.638–0.875) | 0.709 (0.610–0.801) | 0.736 (0.635–0.873) |
| Rating: Good / Fair / Poor | | | 170 / 70 / 3 files |
| Log spectral distortion, median | 3.08 dB | 2.87 dB | 3.05 dB |
| MCD on the emulator, median (10th–90th pct) | 2.14 (1.82–2.38) dB | 2.03 (1.95–2.14) dB | 2.11 (1.84–2.36) dB |
| Tilt, output − input: below 800 / 800–2000 / above 2000 Hz | −0.5 / +0.5 / +0.1 dB | −0.4 / +0.6 / −0.2 dB | −0.5 / +0.5 / +0.0 dB |
| Gross pitch error, median | 4.2 % | 5.6 % | 4.4 % |
| Voicing decision error, median | 9.8 % | 19.6 % | 10.9 % |
| ESTOI, median | 0.794 | 0.771 | 0.791 |
| Level error per 10 ms frame, median | 2.0 dB | 2.0 dB | 2.0 dB |
| Output level (RMS) at sounding frames, median | 24.6 codes | 24.9 codes | 24.7 codes |
| Whole output's RMS after the 279 Hz high-pass, pauses included, median | 15.7 codes | 14.5 codes | 15.5 codes |
| Samples clipped at the DAC's ±127, median per file | 0.075 % | 0.091 % | 0.077 % |
| Largest output code (any file) | 160 | 162 | 162 |
| Samples with a lattice wrap | 0 | 0 | 0 |
| Frames: voiced / unvoiced / repeat / silence | 65.3 / 19.7 / 4.2 / 10.8 % | 74.7 / 17.5 / 1.9 / 5.8 % | 66.8 / 19.3 / 3.9 / 10.0 % |
| Bit rate, median per file | 1629 bit/s | 1743 bit/s | 1648 bit/s |

Per voice (median over its files):

| Voice | Files | Score | MCD (dB) | LSD (dB) | GPE (%) | VDE (%) | Level (codes) | Peak |
|---|---|---|---|---|---|---|---|---|
| alba | 33 | 0.734 | 2.31 | 3.14 | 5.3 | 11.8 | 27.6 | 160 |
| cori | 30 | 0.729 | 2.23 | 3.29 | 3.5 | 14.0 | 26.8 | 159 |
| joe | 20 | 0.710 | 1.98 | 3.06 | 6.6 | 6.1 | 20.7 | 160 |
| kristin | 19 | 0.778 | 2.00 | 3.22 | 0.5 | 7.9 | 24.6 | 158 |
| nem | 26 | 0.733 | 2.11 | 3.13 | 5.1 | 10.4 | 22.0 | 160 |
| norman | 19 | 0.761 | 1.99 | 2.97 | 7.4 | 9.1 | 23.3 | 160 |
| sef | 22 | 0.716 | 2.27 | 3.20 | 10.7 | 11.2 | 25.3 | 160 |
| vctk_hi | 20 | 0.773 | 2.34 | 2.85 | 4.3 | 8.5 | 25.8 | 157 |
| vctk_lo | 21 | 0.832 | 1.81 | 2.60 | 1.4 | 8.5 | 21.9 | 155 |
| si (recordings) | 33 | 0.709 | 2.03 | 2.87 | 5.6 | 19.6 | 24.9 | 162 |

The peak column is the largest code in any of the voice's files.

## Observations

- **The envelope is the weakest part.** Its median score is 0.65, against
  0.77 for pitch and 0.89 for intelligibility; a median of 20 % of speech
  frames are more than 4 dB out. The spectral distance includes the inherent
  loss of LPC-10 with a fixed chirp: a natural voice's glottal pulse,
  breathiness and mixed excitation cannot be reproduced, so it never
  approaches 0.
- **Pitch** follows every voice: the median gross pitch error is under 11 % for
  every voice, and under 2 % for kristin and vctk_lo. The rating's own pitch
  tracker can be an octave out on some very low or very high voices, so part
  of these errors are the measure's.
- **Voicing** disagrees most on the recordings (19.6 % against 9.8 % for the
  TTS voices). Their breathy and creaky stretches are where periodicity is
  least clear, and `voicing_threshold` 0.6 voices the doubtful ones, the
  choice that brings the encoder nearest Acorn's own phrases.
- **Level.** `target_rms` sets the loudness, after the 279 Hz high-pass, at
  20 codes over sounding frames. The plain level there comes out higher, a
  median of 25 codes, since the bass counts in it. The limiter
  lowers 13 % of sounding frames and silences 1 %; the median file clips
  0.08 % of its samples at the DAC, within the budget of 0.1 % and below the
  0.21 % of Acorn's PHROM. The encoder holds the peaks of its own synthesis
  to ±160; on the emulator one recording reaches 162.
- **Energy indices** are lower than the originals': full voiced frames centre
  on index 9, against 13 ([§12.10](../datasheet/authoring.md)), at a similar
  loudness. So the encoder's filters have higher gain than TI's, from
  sharper resonances or TI's different pre-emphasis. The lower indices mean
  coarser energy steps on quiet voiced frames.
- **Bit rate** is a median of 1648 bit/s, against the originals' 1372 bit/s.
  Only about 4 % of frames are repeats: `repeat_lambda` 300 keeps a repeat
  only where it costs little, which brings the repeats near those of Acorn's
  own phrases. The share of silence is higher than in the PROM words, because
  the test inputs are sentences with pauses.
- **Speed:** the 243 inputs (673 s of speech) encode in 7.0 s on one core of
  an Apple M5, about 0.01 s per second of speech.

These numbers measure the encoder's frames against the input, on the
emulator, not on the chip. The chip differs from the emulator by about one
code at a few samples ([§8.8](../datasheet/lattice-filter.md)), so they should
carry over. None of this replaces listening on the chip.

## Where the quality goes

`speechqual.py --ladder` started from an unquantised floating-point LPC-10
vocoder at the chip's frame rate (the anchor) and added what the chip imposes
one step at a time.
Steps `q` to `qci` use the anchor's simple analysis. `qci` and `enc` speak
their frames through the reference model's timing, interpolation and
excitation, with a floating-point lattice in place of the chip's arithmetic.
Medians over all 243 inputs; lo, mid and hi are the tilt below 800 Hz,
800–2000 Hz and above 2000 Hz, output minus input:

| Step | MCD | lo | mid | hi | vuv% |
|---|---|---|---|---|---|
| `anchor`: float LPC-10 vocoder | 1.92 | +0.0 | +0.3 | −0.4 | 7.3 |
| `q`: + the chip's K, pitch and energy tables | 2.11 | +0.0 | +0.3 | −0.4 | 8.6 |
| `qc`: + the chip's excitation (the chirp; voiced frames analysed after 1 − 0.9375z⁻¹) | 2.27 | −1.2 | +1.2 | +0.0 | 6.4 |
| `qci`: + the chip's interpolation and timing | 2.22 | −1.0 | +1.1 | −0.1 | 8.4 |
| `enc`: the encoder's frames, same float model | 2.05 | −0.2 | +0.6 | −0.4 | 8.0 |
| `chip`: the encoder's frames on the emulator | 2.11 | −0.5 | +0.5 | +0.0 | 10.3 |

Of the steps in `q`, the K table costs 0.15 dB (`qk`), the pitch table
0.01 dB (`qp`) and the energy table nothing (`qe`).

What this shows:

1. **The format costs about 0.3 dB of MCD.** That is the tables (0.19 dB)
   and the chirp (0.16 dB), less a little back from the interpolation
   (0.05 dB). The encoder cannot recover this, only work around it.
2. **The encoder's analysis recovers half of it.** Its frames, in the same
   model, score 2.05 dB against 2.22 dB for the simple analysis. The chirp's
   inverse as pre-emphasis takes out most of the tilt the chirp leaves (lo
   −1.0 → −0.2 dB, mid +1.1 → +0.6 dB), and the K search, pitch tracker and
   closed-loop level do the rest.
3. **The chip's arithmetic and 8-bit output cost in the low band and in
   voicing.** From `enc` to `chip`, MCD rises 0.06 dB, the low band loses
   0.3 dB and voicing disagreement rises 2.3 points. That fits white noise
   from the output's resolution in the quieter parts.

The encoder ends 0.19 dB of MCD from the unquantised vocoder: what is left is
mostly the K table and the chip's output.
