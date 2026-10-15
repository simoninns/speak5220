# How TI encoded speech

This page covers how TI turned recorded speech into TMS5220 frames in the
early 1980s. It answers one question: what in TI's own process might improve
the encoder? The measurements are of the encoder at its default options,
over the 243 test inputs, scored by the application's quality rating
([Quality](quality.md)).

There are two sources:

- **The Portable Speech Lab (PSL, also called PASS), 1982.** This is Stuart
  Conner's page,
  <https://www.stuartconner.me.uk/ti_portable_speech_lab/ti_portable_speech_lab.htm>.
  It reproduces Helms and Petersen's article from *Electronics*, 8 Sep 1982,
  and links a dump of the system's EPROMs. We disassembled the dump.
  [The PSL's encoder](#the-psls-encoder-eprom-dump) below is taken from that
  code and cites addresses in it. The dump, the TMS9900 disassembler written
  for it and the listings are kept in `investigation/local/ti_psl/`, which is not in the
  repository.
- **TI's minicomputer analysis program, 1981.** It is described in
  "Speech-synthesizer software generated from text or speech", *Electronic
  Design*, 20 Aug 1981, pp. 107–112. That article is part of TI's reprint
  collection "Leading Electronics Press Coverage of Speech Synthesis Products
  and Technology", at
  <http://ftp.whtech.com/datasheets%20and%20manuals/Speech%20Synthesis/Articles%20on%20TI%20Speech%20Synthesis.pdf>.
  The article shows the program's option screen and its flowchart.
  [The minicomputer program](#the-minicomputer-program-electronic-design-1981)
  is taken from these.

The encoder is written from the datasheet alone ([Encoder](index.md)).
Everything here is a hypothesis for it to test, measured, in the same way
other emulators are used for the chip. TI's process is not a
specification. Its output was also hand-edited and re-recorded until it
sounded right, so its algorithms were never meant to stand alone.

## The minicomputer program (Electronic Design, 1981)

The option screen lists these defaults:

| Option | Default |
|---|---|
| Energy analysis window | Hamming. Rectangular and "pitch dependent" are also offered. |
| Energy window length | 25 ms |
| Pre-emphasis constant α | 0.9375 |
| LPC order | 10 (2 to 15 allowed) |
| Frame update period | 25 ms |
| Sample rate | 8000 Hz (7000–12000 allowed) |

The output formats include "voiced & unvoiced parameters per frame / uncoded
RMS". So the program computed both a voiced and an unvoiced parameter set for
every frame. The choice between them could then be made, or changed,
afterwards.

The flowchart ("LPC-analysis logic flow", fig. 3) runs in this order:

1. Read in the digitised speech.
2. **Calculate the pitch (Gold-Rabiner).**
3. Calculate the unvoiced energy, then **unvoiced K1–K4**, then code the
   unvoiced energy and K1–K4.
4. Calculate the voiced energy **from pre-emphasised data**, then voiced
   K1–K10, then code the voiced energy, the pitch and K1–K10.
5. Write the result as a listing, as synthesiser data, or for one of two
   editors. Then audition it and edit it again if needed.

The flowchart marks pre-emphasis on the voiced branch only. The unvoiced
energy and K values come from the signal without pre-emphasis.

Gold-Rabiner is the 1969 time-domain pitch detector (B. Gold and L. Rabiner,
"Parallel processing techniques for estimating pitch periods of speech in the
time domain", JASA 46, 1969). It works like this:

- It takes six pulse trains from the peaks and valleys of low-passed speech.
- It feeds each train to a simple period estimator. Each estimator has a
  blanking interval and an exponentially decaying threshold.
- It chooses the final period by a coincidence vote among the six estimators'
  current and recent estimates.
- That vote also gives the voicing decision.

TI's other 1981 articles in the same collection make more points:

- They say the 5220 has 10 poles and that "more than 12 is superfluous".
- They say quality is improved by "extending hand editing of the input and
  output data" (*Electronic Design*, 25 June 1981, p. 126).
- TI's text-to-speech program tapered energies toward zero at both ends of a
  string of frames, to avoid "pops, squeaks, squeals" (p. 125). That applies
  to joining words. It is not part of analysis.

## The PSL's encoder (EPROM dump)

### What the hardware does

The PSL's analysis hardware is described in the article and on the page; none
of it is in the code.

- **Two input paths.**
  - The spectral path is Hamming-windowed by a multiplying DAC, then
    low-passed at 4 kHz.
  - The pitch path is low-passed at 800 Hz.
- **Sampling.** Both paths are sampled by one 12-bit ADC at 8 kHz. Stuart
  Conner found that his unit records at 10 kHz whatever the playback jumper
  says. The code also contains a "TEN KHZ" warning, so the rate in use is
  uncertain. A frame is always 200 samples.
- **Autocorrelation.** A custom board computes it for each 25 ms frame:
  - 12 lags of the spectral path;
  - 128 lags of the pitch path.
- **Windows.** The window is the frame, with no overlap.

The TMS9900 CPU then does everything below, in real time, one frame behind.
The CRU interface to the analysis board is at base >1000:

| Bit | Function |
|---|---|
| 0 | Chip family (1 = 52xx) |
| 1–2 | Variant (TMS5220 = 2) |
| 3 | RPT |
| 4 | The Analyse button |
| 5 | The frame clock |

The autocorrelation buffers are at E000 and E400.

### Pitch and voicing (37E2)

- **Input.** The 32-bit autocorrelation of the 800 Hz path. The software does
  no centre clipping; the code cannot tell whether the hardware does.
- **Silence gate.** If the pitch path's R0 ≤ 0x00400000, the frame is
  unvoiced.
- **Peak search.**
  - Lags 25 to 124 are searched (64–320 Hz at 8 kHz).
  - Only strict local maxima count.
  - A negative R(k) resets the search.
  - The search starts in a "falling" state, so the tail of the lag-0 lobe is
    never taken.
- **Voicing rule.** Take the largest local maximum. The frame is **voiced if
  R(k)/R0 ≥ 48/164 ≈ 0.29**.
  - An optional table (39CA, off by default) divides out the
    autocorrelation's bias, (200 − k)/200.
- **The pitch** is the integer lag of that peak.
- **What is not there:** interpolation, smoothing or tracking across frames,
  and octave checks.

### Pre-emphasis (3B58)

- **Voiced frames only.** If the frame's pitch is 0, the routine returns at
  once (3B60–3B66).
- **The filter is a full first difference, 1 − z⁻¹ (μ = 1)**, applied to the
  autocorrelation rather than to the signal:
  - R′(0) = 2R0 − 2R1 (3B68–3B98);
  - R′(k) = 2R(k) − R(k−1) − R(k+1) for k = 1–10 (loop 3B9C–3BD8).
- That is why 12 lags are computed for a 10th-order analysis.

This μ = 1 differs from the 0.9375 default of the 1981 program. Both apply
pre-emphasis to voiced frames only.

### Reflection coefficients (33D8)

- **Method.** Block-normalise R to 16 bits (3676), then run a 10-stage
  Le Roux-Gueguen recursion in Q15 arithmetic.
- **What is absent:**
  - no lag window;
  - no white-noise correction;
  - no bandwidth expansion;
  - no stability check.
- **Unvoiced frames.** K1–K10 are all computed, but only K1–K4 are used.

### Energy (316C, 4042–413A)

- **RMS.** It is √R0 of the spectral path (Hamming-windowed). For voiced
  frames this is the R0 after pre-emphasis, 2R0 − 2R1.
- **The gain is computed after the K values are quantised, from the quantised
  values.** The prediction-error factor is Π√(1 − k̂ᵢ²) over K1–K10 for a
  voiced frame, or K1–K4 for an unvoiced one.
  - Unvoiced: e = RMS · Π√(1 − k̂ᵢ²).
  - Voiced: e = RMS · Π√(1 − k̂ᵢ²) · √P / 2. Here P is the period in samples,
    and a table at 7544 gives 2048·√P.
- **Quantisation.** A threshold table at 75C4: 0, 26, 69, 104, 147, 207, 293,
  414, 584, 825, 1165, 1646, 2325, 3285, 4640.
  - Above 104 the steps are 3 dB apart, which is nearest in the log domain.
  - Code 0 is a silence frame.

### K and pitch quantisation (3044)

- **K rule.** Nearest value in the linear k domain, with ties rounding up.
  Neither arcsine nor LAR is used.
- **K tables** (TMS5220 set at 75FC).
  - K1 is the chip's ROM value × 64.
  - K2–K10 are TI's decimal design values, which agree with the chip's tables
    to within rounding.
- **The pitch table is the chip's table plus one** (0, 16, 17, … 154, 160). So
  TI treated the period as the stored value + 1 samples. This is what the
  project's measurements found independently
  ([§8.6](../datasheet/excitation-interpolation.md)), and what the encoder
  uses.

### Frame decisions

- **Endpoints (4E00).** Leading and trailing frames with energy code below 2
  are dropped.
- **Repeat frames (41E4).** A frame gets the repeat bit only if it has the
  same voicing and **exactly** the same quantised K indices as the frame
  before. The energy and pitch are still sent.
- **Stop.** A stop frame, then zero padding to a byte boundary.
- **What is not there:** smoothing of K or energy, and any change made because
  of the chip's interpolation.

## How the encoder compares

| | TI (PSL code, or 1981 program) | The encoder |
|---|---|---|
| Pre-emphasis | Voiced frames only. μ = 1.0 (PSL) or 0.9375 (1981) | Voiced frames only: the order-4 inverse of the chirp |
| Unvoiced analysis | No pre-emphasis, K1–K4 | The same |
| Window | 200-sample Hamming, the frame itself | 256-sample Hamming, centred on the frame |
| Lag window | None | 60 Hz Gaussian |
| Pitch | Autocorrelation peak on the 800 Hz path (PSL); Gold-Rabiner (1981) | YIN candidates on a 1 kHz path with a Viterbi path at 5 ms hops, held near RAPT's track, and an octave fix against neighbours |
| Voicing | Peak ≥ 0.29·R0, frame by frame | YIN's normalised difference (CMNDF) against an adaptive threshold (base 0.6), with tilt and zero-crossing evidence from QBox, smoothed by the Viterbi path and over runs of frames |
| K quantisation | Nearest k | Nearest log-area ratio, then a search that minimises the prediction error of the quantised filter |
| Energy | Open loop: RMS · Π√(1 − k̂²) (· √P/2 if voiced), from quantised K | Closed loop on the synthesised output's loudness, 5 passes, with a limiter |
| Repeats | Exactly equal K only | Rate-distortion search on the rise in prediction error |
| Period | Value + 1 | Value + 1 |

The encoder does what TI did on the main structural points:

- pre-emphasis on voiced frames only;
- unvoiced frames analysed without it and reduced to K1–K4;
- the period taken as value + 1.

Where the two differ, the encoder is the more elaborate:

- its pitch tracking is smoothed across frames;
- its gain is fitted to the synthesised output rather than estimated;
- its K quantisation minimises the prediction error instead of taking the
  nearest k.

## TI's settings measured

Each of TI's choices that the encoder has an option for, set on its own, over
the 243 test inputs (medians, and the total bytes):

| Setting | Score | LSD (dB) | GPE (%) | VDE (%) | ESTOI | Bytes |
|---|---|---|---|---|---|---|
| Defaults | 0.736 | 3.05 | 4.4 | 10.9 | 0.791 | 124,387 |
| `window=200` (TI's window) | 0.725 | 3.10 | 4.6 | 10.8 | 0.787 | 124,927 |
| `lag_window_hz=0` (no lag window, as the PSL) | 0.734 | 3.05 | 4.7 | 11.1 | 0.787 | 124,888 |
| `voicing_evidence=0` (voicing from periodicity alone, as TI) | 0.743 | 3.04 | 4.4 | 11.2 | 0.789 | 124,090 |
| `repeat_lambda=1000` (the fewest repeats, nearest TI's exact rule) | 0.737 | 3.05 | 4.5 | 10.9 | 0.792 | 127,215 |

- **Window.** TI's 25 ms window is worse on every measure but the voicing.
- **Lag window.** Dropping it changes the envelope little, and the pitch and
  voicing slightly for the worse.
- **Voicing evidence.** Periodicity alone raises the median score a little
  (0.743) but lowers the mean (0.740 against 0.741), and voices worse.
- **Exact repeats.** The fewest repeats cost 2 % more bytes for no change in
  quality: at its default the encoder already uses few.
- **Pre-emphasis.** TI's first-order pre-emphasis (μ = 0.9375, the 1981
  default) was measured against the chirp's inverse before the encoder dropped
  the option ([Pre-emphasis](how-it-works.md#pre-emphasis)): on 64 TTS
  phrases and 33 recorded sentences it raised the spectral distortion from
  2.88 to 3.07 dB and from 2.87 to 3.01 dB.

**Conclusion:** none of TI's analysis choices beats the encoder's by more
than the spread between near-equal settings. The encoder shares the parts of
TI's design that matter, and its own refinements are at least as good. Where
the remaining quality goes is on the [Quality](quality.md#where-the-quality-goes)
page: mostly the K table and the chip's output.

## The human in the loop

One TI practice the encoder does not have is the human in the loop. Both TI
systems relied on listening, editing energy and pitch by hand, and
re-recording. The 1981 article draws its flowchart around an "audition, edit
again" loop. Steve Petersen, who wrote the PSL software, recalls "a LOT of
goofing around with the data until it sounded right" (interview on the PSL
page). Its quality figures should be read with that in mind.
