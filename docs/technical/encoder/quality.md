# Encoder quality

## The evaluation

The figures on this page were measured by the research encoder's
`evaluate.py`. It encoded every test input in `investigation/captures/phase11/inputs/` (210 TTS
files from nine voices with median f0 87–246 Hz, and the 33 recordings),
synthesised the frames with the encoder's reference-algorithm synthesis
(`synthesize()`), and compared the result with the input. The measures:

- **Level error:** per frame, the output level minus the input level × gain
  (both Hann-weighted 25 ms, aligned as in
  [Preparation and timing](how-it-works.md#preparation-and-timing)), over
  frames within 30 dB of the loudest. The median offset per file is removed,
  and the mean absolute error and its 90th percentile are reported.
- **Spectral distance:** per frame (same frames), the RMS difference in dB
  between level-normalised 20-band mel spectra (100–3800 Hz, 32 ms Hamming) of
  the input and the synthesis.
- **Clipping and wraps:** output codes outside −128…+127, and samples in which
  any lattice sum wrapped.
- **Predecessor dependence:** the largest difference between the synthesis
  started from two extreme stored-index states (all K indices 0, or all at
  their maximum).
- **Pitch:** the median f0 of voiced frames against the index's `f0_p50`, and
  octave-like jumps (period ratio above 1.6) between adjacent voiced frames.

Objective measures of the output spoken on the emulator (mel-cepstral
distortion, the spectrum's balance, level and voicing) come from
`investigation/tools/speechqual.py`, described in the
[tools pages](../tools/encoder-speech.md#encoding-quality); the figures on
the [TI encoder](ti-encoder.md) page come from it.

## Results

All results with the default options, over all 243 files:

| Measure | TTS (210) | Recordings (33) | All (243) |
|---|---|---|---|
| Level error, mean \|dB\| per file: median (90th pct of files) | 1.09 (1.51) | 1.23 (1.44) | 1.11 (1.51) |
| Level error, 90th percentile within a file: median | 2.0 dB | 2.4 dB | 2.1 dB |
| Level bias (output − target), median | −0.40 dB | −0.32 dB | −0.38 dB |
| Spectral distance per frame: median (10th–90th pct of files) | 3.19 (2.70–3.66) dB | 3.13 (2.92–3.38) dB | 3.17 (2.70–3.60) dB |
| Output RMS over speech, median | 18.5 codes | 18.6 codes | 18.5 codes |
| Largest output code (any file) | 120 | 120 | 120 |
| Codes outside −128…+127 / samples with a wrap | 0 / 0 | 0 / 0 | 0 / 0 |
| Files whose output depends on the predecessor's stored indices | 0 | 0 | 0 (4 files, up to 16 codes, without the guard in [Start and end](how-it-works.md#start-and-end)) |
| Sounding frames turned down by the limiter / made silence | 6.4 % / 4.0 % | 6.8 % / 3.7 % | 6.4 % / 3.9 % |
| Median f0 / index `f0_p50`: median (range) | 1.001 (0.91–1.13) | 1.001 (0.97–1.06) | 1.001 (0.91–1.13) |
| Octave-like jumps between adjacent voiced frames | 8 (in 7 files, 21,120 frames) | 0 (3,846 frames) | 8 |
| Voiced share − index `voiced_pct`: median (median \|difference\|) | +2.5 (3.9) points | +2.2 (2.7) points | +2.4 (3.7) points |
| Frames: voiced / unvoiced / repeat / silence | 44.8 / 20.3 / 20.8 / 13.0 % | 49.9 / 22.9 / 17.8 / 8.5 % | 45.6 / 20.7 / 20.4 / 12.3 % |
| Bit rate, median per file | 1311 bit/s | 1391 bit/s | 1326 bit/s |
| Synthesis-to-input alignment within ±5 ms (envelope check) | 89 % of files | 100 % | 91 % |

Per voice (median over its files; "jumps" as above):

| Voice | Files | Index f0 (Hz) | Encoded f0 (Hz) | Level error (dB) | Spectral distance (dB) | RMS (codes) | Peak | Jumps |
|---|---|---|---|---|---|---|---|---|
| alba | 33 | 200 | 200 | 1.00 | 3.49 | 18.6 | 120 | 1 |
| cori | 30 | 205 | 209 | 0.97 | 3.30 | 19.1 | 120 | 0 |
| joe | 20 | 94 | 92 | 1.40 | 3.07 | 16.7 | 120 | 3 |
| kristin | 19 | 148 | 148 | 1.04 | 3.16 | 18.3 | 120 | 0 |
| nem | 26 | 109 | 110 | 1.27 | 3.29 | 16.9 | 120 | 0 |
| norman | 19 | 98 | 99 | 1.23 | 3.13 | 18.0 | 120 | 3 |
| sef | 22 | 200 | 200 | 0.99 | 3.24 | 19.3 | 120 | 1 |
| vctk_hi | 20 | 246 | 248 | 0.96 | 2.97 | 20.4 | 119 | 0 |
| vctk_lo | 21 | 87 | 87 | 1.44 | 2.87 | 18.3 | 120 | 0 |
| si (recordings) | 33 | 122 | 121 | 1.23 | 3.13 | 18.6 | 120 | 0 |

The encoded f0 is quantised to Table 8-5's periods, so high voices land a few
per cent off (for example 209 Hz for 205).

These numbers measure the encoder against the reference algorithm, not the
chip. The chip differs from the algorithm by about one code at a few samples
([§8.8](../datasheet/lattice-filter.md)), so they should carry over. The
spectral distance includes the inherent loss of LPC-10 with a fixed chirp: a
natural voice's glottal pulse, breathiness and mixed excitation cannot be
reproduced, so this distance never approaches 0. None of this replaces
listening on the chip.

## Observations

- **Pitch** follows every voice: the per-file median f0 is within −9 % to
  +13 % of the index's in all 243 files. The four files more than 5 % low are
  the low male voices (three joe, one norman). Eight octave-like flips remain,
  in seven TTS files (joe, norman, sef, alba), typically at creaky vowel ends
  where the signal itself favours the shorter lag.
- **Voicing:** the share of voiced frames among sounding frames is a median
  of 2.4 points above the index's `voiced_pct` (median difference 3.7 points). The index uses another
  estimator over the whole file, so this is only a sanity check.
- **Level:** the error is within the energy table's own resolution (±1.5 dB at
  3 dB steps). The p90 includes onsets and offsets, where the glide smears the
  level over a frame.
- **Limiting:** the RMS target of 20 needs the limiter on about 6 % of
  sounding frames to keep peaks at 120. Normalising to the peak instead
  (`targetPeak=120`) avoids limiting, but gives a median RMS of 15.7 codes
  against 18.1 (the same 40 TTS files).
- **Energy indices** are lower than the originals': voiced frames centre on
  8–9 (median 8), against 13 ([§12.10](../datasheet/authoring.md)), at a similar RMS. So the
  encoder's filters have higher gain than TI's. The likely cause is sharper
  resonances or a different pre-emphasis. The lower indices mean coarser
  energy steps on quiet voiced frames.
- **Bit rate** is a median of 1326 bit/s, close to the originals' 1372 bit/s.
  About 20 % of frames are repeats. The share of silence is higher than in the PROM
  words, because the test inputs are sentences with pauses.
- **Speed:** 404 s of encoding for 673 s of input (about 0.6 s per second of
  speech), almost all of it in the pure-Python synthesis of the closed loop.
