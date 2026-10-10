# TMS5220 speech encoder

An encoder that turns 8 kHz speech into TMS5220 frames, and packs the frames into the chip's bit stream, a Speak External stream, or an Acorn PHROM or TI indexed VSM image.

It was written from the reference datasheet alone (`docs-tech/tms5220_reference_datasheet/tms5220_reference_datasheet.md`, mainly §8 and §12): no other encoder, emulator source or original PROM data was used. The project's reference emulator binary was run only as a black box, to check `synthesize()`. Whatever the datasheet left open is listed in [`GAPS.md`](GAPS.md).

| File | Contents |
|---|---|
| `tms5220enc.py` | The encoder, packer, synthesis and packaging (importable as `tms5220enc`) |
| `test_tms5220enc.py` | Self-test: `nix develop -c python3 encoder/test_tms5220enc.py` |
| `evaluate.py` | Encodes the Phase 11 test inputs and measures the result (the numbers below) |
| `GAPS.md` | Datasheet gaps and the choices made |

## Command line

    nix develop -c python3 encoder/tms5220enc.py encode IN.flac OUT.bin [--csv OUT.csv] [--wav OUT.wav]
                                                 [--rms CODES] [--peak CODES] [--no-repeat]
                                                 [--repeat-lambda L] [--pre-emphasis chirp|MU]
                                                 [--voicing-evidence W] [--voicing-threshold T]

- `OUT.bin` holds the packed bytes: frames to the stop code, then zero padding to the end of its byte (§12.1).
- `--csv` writes one row per frame: `frame,kind,energy,repeat,pitch,k1,...,k10`, with blanks where a field is absent. `kind` is `voiced`, `unvoiced`, `repeat`, `silence` or `stop`.
- `--wav` writes the reference-algorithm synthesis: codes × 64, 16-bit, 8 kHz. It starts at the sample period of the first frame read, so it holds about 25 ms of idle (code −1) before the sound.
- `--rms` sets the level target (default 20 codes). `--peak` normalises the loudest peak instead. `--no-repeat` disables repeat frames.
- The other switches set the options of the same names (see the table under API), with the same defaults:
  - `--repeat-lambda` (`repeatLambda`, 70): lower gives fewer bits, higher better quality;
  - `--pre-emphasis` (`preEmphasis`, `chirp`): `chirp`, or a coefficient such as `0.9` for the older first-order filter;
  - `--voicing-evidence` (`voicingEvidence`, 4): `0` decides voicing on periodicity alone;
  - `--voicing-threshold` (`voicingThreshold`, 0.3): higher gives more voiced frames.

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
  - a full frame is `{"energy", "repeat": 0, "pitch", "k": [...]}`, with 4 K indices when unvoiced (pitch 0) and 10 when voiced.

  `pack()` validates every field and stops after the first stop frame. `parse()` returns the frames up to and including the stop code. If the bits run out first, it returns the complete frames read so far, and `bitsUsed` counts only their bits.
- **`synthesize(frames)`** runs the §8.9 reference algorithm for one VSM Speak from power-up state. Sample 0 is the sample period of the first frame read. The output is code −1 until the sound starts at about sample 203–208. Frame *j*'s own stretch is centred on sample 225 + 200·*j*. The array ends once the output has been idle for 25 samples after the stop. On every frame list tried, it gives the same codes as the reference emulator `emulator/tms5220emu`, sample for sample, through the VSM path. The Speak External path runs 2 samples earlier with other noise (§7, §8.5); `synthesize()` does not model that.
- **`encodeDetailed()`** is `encode()` that also returns its analysis (the filtered signal, frame centres, level gain and limiter statistics). `evaluate.py` uses it.
- **`acornPhrom(words, ...)`**: `words` is a list of `(name, frames)`, numbered from 127. `asciiMap` maps characters, or codes 32–126, to word names. It returns a 16 KB image, and a table of `{"number", "name", "offset", "bytes", "frames"}`, with `frames` the number of frames, stop included. ASCII entries with no word point to a shared one-byte stop code. It raises `ValueError` if the words do not fit.
- **`tiIndexed(words)`**: the same, in TI's indexed format (§12.8). Words are numbered from 1, and each word is followed by one spare byte.
- Helpers: `frameKind`, `frameBits`, `booth` (the §8.8 multiplier, step by step), `boothFast`, `wrap14`, `levinson`, `stepUp`, `bitReverse`, `framesCsv`, and the tables `ENERGY`, `PITCH`, `KTABLES`, `CHIRP`.

### `encode()` options

| Option | Default | Meaning |
|---|---|---|
| `targetRms` | 20.0 | Output RMS over the sounding frames, in codes (§12.3: the originals have 19–20) |
| `targetPeak` | None | If set, scale so that the loudest output peak is this many codes, instead of an RMS |
| `peakLimit` | 120 | No output code beyond ±this. Frames that would exceed it are turned down |
| `repeats` | True | Use repeat frames |
| `repeatLambda` | 70 | Bits a run of repeats may trade per dB of extra prediction error (below). Lower gives fewer bits |
| `gateDb` | 45 | Frames this far below the loudest frame are silence |
| `preEmphasis` | `"chirp"` | Pre-emphasis for voiced analysis: `"chirp"` (the chirp's inverse, below), or μ for 1 − μz⁻¹ (§12.3) |
| `window` | 240 | Analysis window, in samples (30 ms) |
| `lagWindowHz` | 60 | Gaussian lag-window bandwidth |
| `voicingThreshold` | 0.3 | Base CMNDF level at which voiced and unvoiced cost the same |
| `voicingEvidence` | 4 | Weight of spectral tilt and zero crossings in the voicing threshold (0: off) |
| `iterations` | 5 | Closed-loop level passes |
| `fadeUnvoiced` | True | Fade a hiss before silence and before the stop |

`encode()` is deterministic: it uses no random numbers, and the same input always gives the same frames.

## How it works

The method is §12.6, with these choices.

### Preparation and timing

- The input is resampled to 8 kHz if needed (polyphase, which also band-limits it). A 60 Hz high-pass (2nd-order Butterworth, zero-phase) removes DC and rumble.
- **Onset and offset.** These are the first and last 10 ms blocks above the gate (below). Leading and trailing quiet is dropped.
- **Frame centres.** Frame *j* describes the input at `onset + 22 + 200·j` samples. The chip makes the first frame's energy audible at output sample 203: it is computed in PC 0 of IP 0 (sample 201) and used two samples later (§8.5). So output sample 203 is the input onset, and output sample 225 + 200·*j* corresponds to frame *j*'s centre. Sample 225 is the middle of the stretch where frame *j*'s values dominate. They are half-way reached at the end of IP 4 of its glide (0.498 of the way, §12.2), reached in IP 0, and half-way to the next frame's by IP 4 of the next glide. An envelope cross-correlation between the input and the synthesis puts 88 % of the files within ±5 ms of this alignment (5 ms is that check's resolution), and the rest within ±25 ms.

### Pre-emphasis

Voiced frames are analysed after the inverse of the chirp's spectrum, so that the filter describes the speech spectrum divided by the chirp's. The pre-filter is the order-4 LPC inverse of the chirp table (`chirpInverse()`): [1, −0.938, 0.152, −0.104, −0.006]. Its first-order fit, 1 − 0.888z⁻¹, is close to §12.3's `s(n) − 0.9·s(n−1)`, which was used before. The order-4 filter also follows the chirp's shape between 300 and 2000 Hz, where the first-order one leaves it about 1 dB weak below 800 Hz and 1 dB strong above. Over the 243 test inputs it halves the output's spectral tilt error (below 800 Hz −1.05 → −0.68 dB, 800–2000 Hz +0.74 → +0.42 dB) and improves the MCD by 0.03 dB, for no extra bits (`docs-tech/third_party_encoders.md` §6). Order 8 was no better. `preEmphasis=0.9` restores the first-order filter. Unvoiced frames, whose excitation is flat, are analysed without pre-emphasis.

### Voicing and pitch

- **Candidates.** The signal is low-passed at 1 kHz. Every 5 ms, YIN's cumulative mean normalised difference function (CMNDF) is computed over a 30 ms window, for lags 14–165 samples. The window is placed so that it is centred on the hop for a lag of 80, and within 5 ms of it for every lag in range. The five deepest local minima, with parabolic refinement, are the voiced candidates.
- **Path.** A Viterbi search over the candidates plus an unvoiced state picks the path. Its costs are:
  - each candidate's CMNDF value, × 10;
  - a pitch jump between hops, 12 per octave, so an octave jump costs as much as a CMNDF difference of 1.2;
  - a voicing switch, 4. The unvoiced state remembers the last voiced lag, so a short unvoiced gap (under 40 ms) cannot be used to change octave more cheaply than a jump;
  - a prior towards the utterance's median period: 5 per octave beyond ±0.6 octave. The median is taken from confident hops, by YIN's first-dip rule;
  - a subharmonic penalty, 4, when a clearly periodic candidate (CMNDF below 0.15) at half or a third of the lag is at least as good.
- **The unvoiced cost** is 10 × a threshold that adapts to the sound. The base is 0.3. It rises by up to 0.35 for loud frames whose energy lies mostly below 1 kHz (vowels, nasals, creaky voice), and falls by up to 0.15 when most of the energy is above 1 kHz (fricatives). Frames more than 34 dB below the loud level get a further bias towards unvoiced.
- **Tilt and zero crossings.** The threshold also moves with two features over a centred 30 ms window: a tilt score from r1/r0 of the signal pre-emphasised by 0.99 (−2 to +4), and the zero crossings per 10 ms. It changes by `voicingEvidence` × (tilt − (crossings − 30)/10) / 66.7, so at the default weight of 4 a steep low-pass frame is about 0.24 easier to voice, and a fricative at 80 crossings per 10 ms 0.3 harder. The features and their scale come from Quadravox's QBox encoder (`docs-tech/third_party_encoders.md` §4.4). They lower the voicing disagreement by about 0.4 points and help most in sentences and vowels. The weight was tuned on the test inputs, which are mostly synthetic voices.
- **Per frame.** A frame is voiced if most of the three hops nearest its centre are voiced, and its period is their median. A final check halves or doubles a period that is about 2× off the median of its voiced neighbours. The period is mapped to the pitch index whose period (value + 1, Table 8-5) is nearest in log terms.

Octave errors were the main thing tuned here. The prior alone either let octave errors through or forced true high excursions (a 200 Hz stress peak from a 110 Hz voice) down to the subharmonic. The subharmonic rule and the gap memory fixed the cases found. Voiced fricatives come out voiced, the choice §12.5 says the original data made most often.

### Spectrum and K quantisation

- Each frame's spectrum is analysed over a 30 ms Hamming window around its centre. Frame 0's window is moved to start near the onset, so it does not analyse silence.
- The autocorrelation (to lag 10) gets a Gaussian lag window (60 Hz) and a −40 dB noise floor (r₀ × 1.0001), for stable, not over-sharp resonances.
- Levinson–Durbin gives *k*₁–*k*₁₀ (voiced) or *k*₁–*k*₄ (unvoiced), in the convention of §12.3. `test_tms5220enc.py` checks that sign with a one-stage lattice.
- **Quantisation.** Each *k* goes to the nearest Table 8-6 entry in log-area ratio. A coordinate search then tries ±1 and ±2 indices of each K, and keeps changes that lower the prediction error `aᵀRa` of the quantised filter on this frame's autocorrelation. This is the LPC criterion itself, measured on the quantised filter, so a later K can make up for an earlier K's rounding.

### Energy

- **Target.** The frame's level is the Hann-weighted RMS of the input over 25 ms around its centre. One gain scales the whole utterance so that the RMS over sounding frames is `targetRms`.
- **First guess.** The open-loop estimate of §12.3 (code ≈ E·x·h/128) gives an energy value from the quantised filter's power response: the chirp spectrum × |H|² over the period for voiced frames, the flat noise power for unvoiced. That value goes to the nearest index in dB.
- **Closed loop.** The utterance is synthesised with the reference algorithm. Each frame's output level is measured with the same Hann window around output sample 225 + 200·*j*, and each index is corrected by 70 % of its error in dB, when the error exceeds 1.5 dB. Up to 5 passes are made, and the best pass is kept. Because of the glide, a frame's window also holds part of its neighbours. Measuring both sides the same way, and damping the correction, keeps this stable.
- **Too loud at index 1.** A frame still more than 12 dB above its target at energy index 1 becomes silence. This happens with strongly resonant filters (GAPS.md item 9).
- **Limiter.** Any frame whose output peaks above `peakLimit` (120) has its index lowered by the excess in 3 dB steps. A frame already at index 1 becomes silence. A final pass lowers every frame by one index until no code exceeds the limit and no 14-bit sum wraps anywhere in the lattice. This last pass has not been needed on any test input.

### Repeat and silence decisions

- **Silence.** A frame is silence if its level is more than `gateDb` (45 dB) below the loudest frame, or below twice the noise floor (the 5th percentile of 10 ms levels).
- **Repeat.** Within each run of sounding frames of one voicing, the encoder splits the run into segments of up to 8 frames, each a full frame followed by repeats, by dynamic programming. A segment costs its bits (50 for a full voiced frame, 29 for a full unvoiced one, 11 per repeat) plus `repeatLambda` × its distortion. A frame's distortion is how much its prediction error rises, in dB, under the segment's K instead of its own quantised K: an Itakura-style likelihood ratio. The segment's K is either its first frame's or the K quantised from the segment's pooled normalised autocorrelation, whichever costs less. At equal bit rate this beats the earlier greedy rule (a repeat when the stored K cost at most 0.8 dB) by 0.01–0.06 PESQ, or saves about 100 bit/s at equal quality (`docs-tech/third_party_encoders.md` §6). A repeat never crosses silence or a change of voicing.
- **Voicing rules.** A voiced repeat never follows an unvoiced frame, and the first voiced frame after unvoiced speech is always full (§12.2). Unvoiced repeats are used only after unvoiced frames, apart from the fades.
- **Keeping repeats valid.** After level setting, frames turned to silence change what the chip holds. Packing tracks the chip's stored K, and sends a repeat whose K are not the stored ones as a full frame.

### Start and end

- **Start.** The first frame is always a full voiced or unvoiced frame (§12.2): leading silence frames are dropped. Its level is the input's at the onset, which is usually low, so the inhibited jump at the start does not click.
- **No dependence on the previous utterance** (§12.2, "K5–K10 before the first voiced frame"). Until the first full voiced frame, the stored K5–K10 indices are whatever the chip spoke before. A silence frame there would bring them back into use. So:
  - a faint opening sound (no voiced frame, every frame more than 36 dB below the loudest, followed by silence) is dropped with its silence; on the test inputs these are noise blips before the speech;
  - any silence frame still before the first full voiced frame becomes an unvoiced repeat at energy 1, which keeps K5–K10 zeroed. An utterance with no voiced frame at all therefore has a faint hiss in its pauses.
- **End.** After a voiced frame, the stop fades the energy over one frame (§12.2).
- **Fades.** An unvoiced frame before silence or the stop does not fade on the chip (§8.7). The encoder adds unvoiced repeat frames, each with a third of the previous energy value, until the value is 2 or less. Inside an utterance they replace the following silence frames; at the end they are added before the stop.
- **Speak External.** `speakExternalStream()` adds silence frames before the stop only when the stream is shorter than 9 bytes (§12.9).

## Self-test

`nix develop -c python3 encoder/test_tms5220enc.py` exits non-zero on failure. It checks:

- **Frames and bits:** the lengths of every frame kind; pack/parse round trips on 300 random frame lists and a hand-made one with every kind; truncated data and `maxFrames`; invalid fields rejected; packing stops at the stop code; bytes hand-assembled from the §8.1/§12.1 bit order; the §6 Read Byte reversal examples.
- **Arithmetic:** every row of the §8.8 multiplier examples (digits and products); the closed form against the step-by-step multiplier on 20,000 random pairs; the 14-bit wrap; the tables (chirp energy 65,546, monotonic K tables, pitch periods).
- **Sign convention (§12.3):** a one-stage chip lattice against 1/(1 + *k*₁*z*⁻¹), and Levinson recovering the same *k*₁; in the full synthesis, a large negative K1 is low-pass and a large positive K1 is high-pass.
- **Synthesis:** the start of speech (unvoiced energy at sample 203, voiced output within 205–209) and its end; a lone stop gives no output; and, if `emulator/tms5220emu` is built, three random speech-like frame lists give the emulator's codes sample for sample.
- **Packaging:** the ≥9-byte rule (padding only when needed, silence before the stop); the PHROM layout read back (header through bit reversal, counts, pointers, the 0xFF, names backwards before the data, ASCII aliases, the null word, overflow); the TI indexed layout read back, with its spare bytes and overflow.
- **Predecessor:** the §12.2 pattern (unvoiced, silence, before any voiced frame) depends on the stored indices, and the encoder's guard removes the dependence.
- **`encode()`** on `w05_vctk_lo.flac` ("marmalade"): a valid frame list that round-trips, a single stop at the end, a full first frame, no voiced repeat after unvoiced, no wraps, peaks within ±127, a sensible level, independence from the predecessor, determinism, no repeats when they are disabled, and fewer bits at a lower `repeatLambda`.
- **Pre-emphasis and repeats:** the chirp inverse filter (its coefficients, and that it flattens the chirp), and the repeat search merging identical frames and splitting at a change.

## Quality

`nix develop -c python3 encoder/evaluate.py [--set tts|recordings|all] [--csv OUT]` encodes every Phase 11 input (210 TTS files from nine voices with median f0 87–246 Hz, and the 33 recordings), synthesises the frames with `synthesize()`, and compares the result with the input. The measures:

- **Level error:** per frame, the output level minus the input level × gain (both Hann-weighted 25 ms, aligned as above), over frames within 30 dB of the loudest. The median offset per file is removed, and the mean absolute error and its 90th percentile are reported.
- **Spectral distance:** per frame (same frames), the RMS difference in dB between level-normalised 20-band mel spectra (100–3800 Hz, 32 ms Hamming) of the input and the synthesis.
- **Clipping and wraps:** output codes outside −128…+127, and samples in which any lattice sum wrapped.
- **Predecessor dependence:** the largest difference between the synthesis started from two extreme stored-index states (all K indices 0, or all at their maximum).
- **Pitch:** the median f0 of voiced frames against the index's `f0_p50`, and octave-like jumps (period ratio above 1.6) between adjacent voiced frames.

All results with the default options, over all 243 files:

| Measure | TTS (210) | Recordings (33) | All (243) |
|---|---|---|---|
| Level error, mean \|dB\| per file: median (90th pct of files) | 1.13 (1.67) | 1.24 (1.62) | 1.16 (1.65) |
| Level error, 90th percentile within a file: median | 2.1 dB | 2.6 dB | 2.2 dB |
| Level bias (output − target), median | −0.37 dB | −0.36 dB | −0.37 dB |
| Spectral distance per frame: median (10th–90th pct of files) | 3.38 (2.95–3.85) dB | 3.29 (3.04–3.57) dB | 3.38 (2.95–3.83) dB |
| Output RMS over speech, median | 18.6 codes | 18.2 codes | 18.6 codes |
| Largest output code (any file) | 120 | 120 | 120 |
| Codes outside −128…+127 / samples with a wrap | 0 / 0 | 0 / 0 | 0 / 0 |
| Files whose output depends on the predecessor's stored indices | 0 | 0 | 0 (15 files, up to 80 codes, before the fix) |
| Sounding frames turned down by the limiter / made silence | 6.8 % / 4.7 % | 6.9 % / 4.6 % | 6.8 % / 4.7 % |
| Median f0 / index `f0_p50`: median (range) | 1.001 (0.96–1.13) | 1.003 (0.97–1.07) | 1.001 (0.96–1.13) |
| Octave-like jumps between adjacent voiced frames | 4 (in 3 files, 21,458 frames) | 0 (3,905 frames) | 4 |
| Voiced share − index `voiced_pct`: median (median \|difference\|) | +0.4 (3.3) points | −1.1 (2.0) points | 0.0 (2.9) points |
| Frames: voiced / unvoiced / repeat / silence | 45.6 / 19.0 / 19.5 / 14.9 % | 51.3 / 22.1 / 14.9 / 10.8 % | 46.5 / 19.5 / 18.8 / 14.3 % |
| Bit rate, median per file | 1334 bit/s | 1400 bit/s | 1342 bit/s |
| Synthesis-to-input alignment within ±5 ms (envelope check) | 86 % of files | 100 % | 88 % |

Per voice (median over its files; "jumps" as above):

| Voice | Files | Index f0 (Hz) | Encoded f0 (Hz) | Level error (dB) | Spectral distance (dB) | RMS (codes) | Peak | Jumps |
|---|---|---|---|---|---|---|---|---|
| alba | 33 | 200 | 205 | 1.05 | 3.66 | 18.6 | 120 | 0 |
| cori | 30 | 205 | 211 | 1.01 | 3.43 | 18.9 | 120 | 0 |
| joe | 20 | 94 | 94 | 1.52 | 3.32 | 16.3 | 120 | 2 |
| kristin | 19 | 148 | 148 | 1.03 | 3.33 | 18.7 | 120 | 0 |
| nem | 26 | 109 | 110 | 1.32 | 3.44 | 16.9 | 120 | 0 |
| norman | 19 | 98 | 99 | 1.39 | 3.32 | 18.1 | 120 | 2 |
| sef | 22 | 200 | 200 | 1.06 | 3.44 | 19.6 | 120 | 0 |
| vctk_hi | 20 | 246 | 246 | 0.91 | 3.23 | 20.1 | 119 | 0 |
| vctk_lo | 21 | 87 | 87 | 1.47 | 3.12 | 17.8 | 120 | 0 |
| si (recordings) | 33 | 122 | 121 | 1.23 | 3.29 | 18.2 | 120 | 0 |

The encoded f0 is quantised to Table 8-5's periods, so high voices land a few per cent off (for example 205 Hz for 200).

These numbers measure the encoder against the reference algorithm, not the chip. The chip differs from the algorithm by about one code at a few samples (§8.8), so they should carry over. The spectral distance includes the inherent loss of LPC-10 with a fixed chirp: a natural voice's glottal pulse, breathiness and mixed excitation cannot be reproduced, so this distance never approaches 0. None of this replaces listening on the chip.

Observations:

- **Pitch** follows every voice: the per-file median f0 is within −4 % to +13 % of the index's in all 243 files. Four octave-like flips remain, in three TTS files (joe, norman), at creaky vowel ends where the signal itself favours the shorter lag (CMNDF about 0.3 at both lags). Each lasts 1–4 frames.
- **Voicing:** the share of voiced frames among sounding frames matches the index's `voiced_pct` to a median of ±3 points. The index uses another estimator over the whole file, so this is only a sanity check.
- **Level:** the error is within the energy table's own resolution (±1.5 dB at 3 dB steps). The p90 includes onsets and offsets, where the glide smears the level over a frame.
- **Limiting:** the RMS target of 20 needs the limiter on about 7 % of sounding frames to keep peaks at 120. Normalising to the peak instead (`targetPeak=120`) avoids limiting, but gives a median RMS of 15.9 codes against 18.6 (40 files).
- **Energy indices** are lower than the originals': voiced frames centre on 9, against 13 (§12.10), at a similar RMS. So my filters have higher gain than TI's. The likely cause is sharper resonances or a different pre-emphasis. The lower indices mean coarser energy steps on quiet voiced frames.
- **Bit rate** is about 1330 bit/s, close to the originals' 1372 bit/s. About 20 % of frames are repeats. The share of silence is higher than in the PROM words, because the test inputs are sentences with pauses.
- **Speed:** 260 s of computing for 673 s of input (about 0.4 s per second of speech), almost all of it in the pure-Python synthesis of the closed loop.
