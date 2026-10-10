# How TI encoded speech: clues for the encoder

**Status: notes, 2026-10-10.** This note covers how TI turned recorded speech into TMS5220 frames in the early 1980s. It answers one question: what in TI's own process might improve our encoder? The current encoder scores a median PESQ of 2.00 over the Phase 11 inputs (`tools/speechqual.py`, see `tools/README.md` "Encoding quality"). An unquantised LPC-10 vocoder at the same frame rate scores 2.30.

There are two sources:

- **The Portable Speech Lab (PSL, also called PASS), 1982.** This is Stuart Conner's page, <https://www.stuartconner.me.uk/ti_portable_speech_lab/ti_portable_speech_lab.htm>. It reproduces Helms and Petersen's article from *Electronics*, 8 Sep 1982, and links a dump of the system's EPROMs. We disassembled the dump. §2 below is taken from that code and cites addresses in it. The dump, the TMS9900 disassembler written for it and the listings are kept in `local/ti_psl/`, which is not in the repository.
- **TI's minicomputer analysis program, 1981.** It is described in "Speech-synthesizer software generated from text or speech", *Electronic Design*, 20 Aug 1981, pp. 107–112. That article is part of TI's reprint collection "Leading Electronics Press Coverage of Speech Synthesis Products and Technology", at <http://ftp.whtech.com/datasheets%20and%20manuals/Speech%20Synthesis/Articles%20on%20TI%20Speech%20Synthesis.pdf>. The article shows the program's option screen and its flowchart. Section 1 is taken from these.

The encoder is written from the datasheet alone (`encoder/README.md`). Everything here is a hypothesis for it to test, measured with `speechqual.py`, in the same way MAME is used for the chip. TI's process is not a specification. Its output was also hand-edited and re-recorded until it sounded right, so its algorithms were never meant to stand alone.

## 1. The minicomputer program (Electronic Design, 1981)

The option screen lists these defaults:

| Option | Default |
|---|---|
| Energy analysis window | Hamming. Rectangular and "pitch dependent" are also offered. |
| Energy window length | 25 ms |
| Pre-emphasis constant α | 0.9375 |
| LPC order | 10 (2 to 15 allowed) |
| Frame update period | 25 ms |
| Sample rate | 8000 Hz (7000–12000 allowed) |

The output formats include "voiced & unvoiced parameters per frame / uncoded RMS". So the program computed both a voiced and an unvoiced parameter set for every frame. The choice between them could then be made, or changed, afterwards.

The flowchart ("LPC-analysis logic flow", fig. 3) runs in this order:

1. Read in the digitised speech.
2. **Calculate the pitch (Gold-Rabiner).**
3. Calculate the unvoiced energy, then **unvoiced K1–K4**, then code the unvoiced energy and K1–K4.
4. Calculate the voiced energy **from pre-emphasised data**, then voiced K1–K10, then code the voiced energy, the pitch and K1–K10.
5. Write the result as a listing, as synthesiser data, or for one of two editors. Then audition it and edit it again if needed.

The flowchart marks pre-emphasis on the voiced branch only. The unvoiced energy and K values come from the signal without pre-emphasis.

Gold-Rabiner is the 1969 time-domain pitch detector (B. Gold and L. Rabiner, "Parallel processing techniques for estimating pitch periods of speech in the time domain", JASA 46, 1969). It works like this:

- It takes six pulse trains from the peaks and valleys of low-passed speech.
- It feeds each train to a simple period estimator. Each estimator has a blanking interval and an exponentially decaying threshold.
- It chooses the final period by a coincidence vote among the six estimators' current and recent estimates.
- That vote also gives the voicing decision.

TI's other 1981 articles in the same collection make two more points:

- They say the 5220 has 10 poles and that "more than 12 is superfluous".
- They say quality is improved by "extending hand editing of the input and output data" (*Electronic Design*, 25 June 1981, p. 126).
- TI's text-to-speech program tapered energies toward zero at both ends of a string of frames, to avoid "pops, squeaks, squeals" (p. 125). That applies to joining words. It is not part of analysis.

## 2. The PSL's encoder (EPROM dump)

### 2.1 What the hardware does

The PSL's analysis hardware is described in the article and on the page; none of it is in the code.

- **Two input paths.**
  - The spectral path is Hamming-windowed by a multiplying DAC, then low-passed at 4 kHz.
  - The pitch path is low-passed at 800 Hz.
- **Sampling.** Both paths are sampled by one 12-bit ADC at 8 kHz. Stuart Conner found that his unit records at 10 kHz whatever the playback jumper says. The code also contains a "TEN KHZ" warning, so the rate in use is uncertain. A frame is always 200 samples.
- **Autocorrelation.** A custom board computes it for each 25 ms frame:
  - 12 lags of the spectral path;
  - 128 lags of the pitch path.
- **Windows.** The window is the frame, with no overlap.

The TMS9900 CPU then does everything below, in real time, one frame behind. The CRU interface to the analysis board is at base >1000:

| Bit | Function |
|---|---|
| 0 | Chip family (1 = 52xx) |
| 1–2 | Variant (TMS5220 = 2) |
| 3 | RPT |
| 4 | The Analyse button |
| 5 | The frame clock |

The autocorrelation buffers are at E000 and E400.

### 2.2 Pitch and voicing (37E2)

- **Input.** The 32-bit autocorrelation of the 800 Hz path. The software does no centre clipping; the code cannot tell whether the hardware does.
- **Silence gate.** If the pitch path's R0 ≤ 0x00400000, the frame is unvoiced.
- **Peak search.**
  - Lags 25 to 124 are searched (64–320 Hz at 8 kHz).
  - Only strict local maxima count.
  - A negative R(k) resets the search.
  - The search starts in a "falling" state, so the tail of the lag-0 lobe is never taken.
- **Voicing rule.** Take the largest local maximum. The frame is **voiced if R(k)/R0 ≥ 48/164 ≈ 0.29**.
  - An optional table (39CA, off by default) divides out the autocorrelation's bias, (200 − k)/200.
- **The pitch** is the integer lag of that peak.
- **What is not there:** interpolation, smoothing or tracking across frames, and octave checks.

### 2.3 Pre-emphasis (3B58)

- **Voiced frames only.** If the frame's pitch is 0, the routine returns at once (3B60–3B66).
- **The filter is a full first difference, 1 − z⁻¹ (μ = 1)**, applied to the autocorrelation rather than to the signal:
  - R′(0) = 2R0 − 2R1 (3B68–3B98);
  - R′(k) = 2R(k) − R(k−1) − R(k+1) for k = 1–10 (loop 3B9C–3BD8).
- That is why 12 lags are computed for a 10th-order analysis.

This μ = 1 differs from the 0.9375 default of the 1981 program. Both apply pre-emphasis to voiced frames only.

### 2.4 Reflection coefficients (33D8)

- **Method.** Block-normalise R to 16 bits (3676), then run a 10-stage Le Roux-Gueguen recursion in Q15 arithmetic.
- **What is absent:**
  - no lag window;
  - no white-noise correction;
  - no bandwidth expansion;
  - no stability check.
- **Unvoiced frames.** K1–K10 are all computed, but only K1–K4 are used.

### 2.5 Energy (316C, 4042–413A)

- **RMS.** It is √R0 of the spectral path (Hamming-windowed). For voiced frames this is the R0 after pre-emphasis, 2R0 − 2R1.
- **The gain is computed after the K values are quantised, from the quantised values.** The prediction-error factor is Π√(1 − k̂ᵢ²) over K1–K10 for a voiced frame, or K1–K4 for an unvoiced one.
  - Unvoiced: e = RMS · Π√(1 − k̂ᵢ²).
  - Voiced: e = RMS · Π√(1 − k̂ᵢ²) · √P / 2. Here P is the period in samples, and a table at 7544 gives 2048·√P.
- **Quantisation.** A threshold table at 75C4: 0, 26, 69, 104, 147, 207, 293, 414, 584, 825, 1165, 1646, 2325, 3285, 4640.
  - Above 104 the steps are 3 dB apart, which is nearest in the log domain.
  - Code 0 is a silence frame.

### 2.6 K and pitch quantisation (3044)

- **K rule.** Nearest value in the linear k domain, with ties rounding up. Neither arcsine nor LAR is used.
- **K tables** (TMS5220 set at 75FC).
  - K1 is the chip's ROM value × 64.
  - K2–K10 are TI's decimal design values, which agree with the chip's tables to within rounding.
- **The pitch table is the chip's table plus one** (0, 16, 17, … 154, 160). So TI treated the period as the stored value + 1 samples. This is what our measurements found independently (datasheet §8.6), and what `pitchIndex` in the encoder already uses.

### 2.7 Frame decisions

- **Endpoints (4E00).** Leading and trailing frames with energy code below 2 are dropped.
- **Repeat frames (41E4).** A frame gets the repeat bit only if it has the same voicing and **exactly** the same quantised K indices as the frame before. The energy and pitch are still sent.
- **Stop.** A stop frame, then zero padding to a byte boundary.
- **What is not there:** smoothing of K or energy, and any change made because of the chip's interpolation.

## 3. How the encoder compares

| | TI (PSL code, or 1981 program) | Our encoder (`encoder/tms5220enc.py`) |
|---|---|---|
| Pre-emphasis | Voiced frames only. μ = 1.0 (PSL) or 0.9375 (1981) | Voiced frames only, 0.9 |
| Unvoiced analysis | No pre-emphasis, K1–K4 | The same |
| Window | 200-sample Hamming, the frame itself | 240-sample Hamming, centred on the frame |
| Lag window | None | 60 Hz Gaussian |
| Pitch | Autocorrelation peak on the 800 Hz path (PSL); Gold-Rabiner (1981) | YIN candidates on a 1 kHz path with a Viterbi path at 5 ms hops (`trackPitch`), and an octave fix against neighbours |
| Voicing | Peak ≥ 0.29·R0, frame by frame | YIN's normalised difference (CMNDF) at a threshold of 0.3, smoothed by the Viterbi path |
| K quantisation | Nearest k | Nearest log-area ratio, then a search that minimises the prediction error of the quantised filter (`quantiseK`) |
| Energy | Open loop: RMS · Π√(1 − k̂²) (· √P/2 if voiced), from quantised K | Closed loop on the synthesised output, 5 passes |
| Repeats | Exactly equal K only | Allowed when the prediction error rises by less than 0.8 dB |
| Period | Value + 1 | Value + 1 |

The encoder already does what TI did on the main structural points:

- pre-emphasis on voiced frames only;
- unvoiced frames analysed without it and reduced to K1–K4;
- the period taken as value + 1.

Where the two differ, the encoder is the more elaborate:

- its pitch tracking is smoothed across frames;
- its gain is fitted to the synthesised output rather than estimated;
- its K quantisation minimises the prediction error instead of taking the nearest k.

The TI code gives no reason to think any of these is the cause of the low score.

## 4. TI's settings measured

These are medians over all 243 Phase 11 inputs, from `speechqual.py --set all --opt ...`, run on 2026-10-10:

| Setting | PESQ | MCD (dB) | Tilt below 800 Hz (dB) | Voicing disagreement (%) | bits/s |
|---|---|---|---|---|---|
| Default (pre-emphasis 0.9, window 240) | 2.00 | 2.29 | −1.1 | 11.7 | 1342 |
| `preEmphasis=0.9375` (1981 default) | 2.01 | 2.30 | −1.2 | 11.7 | 1340 |
| `window=200` (TI's window) | 2.00 | 2.30 | −1.0 | 11.6 | 1359 |
| both | 2.00 | 2.31 | −1.2 | 11.9 | 1353 |
| `preEmphasis=1.0` (PSL) | 2.02 | 2.31 | −1.4 | 11.6 | 1345 |
| `preEmphasis=1.0`, `window=200` | 2.01 | 2.32 | −1.3 | 11.6 | 1362 |
| `lagWindowHz=0` (no lag window, as PSL) | 2.02 | 2.27 | −1.1 | 11.9 | 1370 |
| `repeatThreshold=0` (repeats only when K do not change) | 2.03 | 2.26 | −1.1 | 11.8 | 1588 |

None of TI's settings moves the median PESQ by more than 0.03. That is small beside the spread between files (p10–p90 1.79–2.32) and far short of the 0.3 between the encoder and the unquantised vocoder.

- **Pre-emphasis.** The PSL's μ = 1 makes the low band slightly worse (−1.4 dB), as expected from a stronger pre-emphasis.
- **Lag window.** Dropping it helps MCD a little and voicing not at all.
- **Exact repeats.** `repeatThreshold=0` cuts out almost all repeats. It costs 18 % more bits for +0.03, the same as `repeats=False`.

**Conclusion:** TI's analysis choices are not where the encoder loses quality. The encoder already shares the parts of TI's design that matter, and its own refinements are at least as good.

## 5. Where the quality goes (`speechqual.py --ladder`)

The gap to the unquantised vocoder (0.3 PESQ, 0.35 dB MCD, a low band about 1 dB short and 4 points more voicing disagreement) has to come from what the chip adds:

- quantisation of K, energy and pitch;
- the chirp excitation in place of a pulse;
- interpolation between frames;
- the chip's arithmetic and output range.

`--ladder` adds these to the anchor one at a time; `tools/README.md` describes each step. Steps `q` to `qci` use the anchor's simple analysis. `qci` and `enc` speak their frames through the reference model's timing, interpolation and excitation, with a floating-point lattice in place of the chip's arithmetic.

These are medians over all 243 inputs, run on 2026-10-10:

| Step | PESQ | Change | MCD | lo | mid | hi | vuv% |
|---|---|---|---|---|---|---|---|
| `anchor`: float LPC-10 vocoder | 2.29 | | 1.92 | +0.0 | +0.3 | −0.4 | 7.3 |
| `q`: + the chip's K, pitch and energy tables | 2.11 | −0.16 | 2.11 | +0.0 | +0.3 | −0.4 | 8.6 |
| `qc`: + the chip's excitation (chirp, pre-emphasis voiced only) | 1.99 | −0.12 | 2.27 | −1.2 | +1.2 | +0.0 | 6.4 |
| `qci`: + the chip's interpolation and timing | 1.94 | −0.05 | 2.22 | −1.0 | +1.1 | −0.1 | 8.4 |
| `enc`: the encoder's frames, same float model | 1.95 | +0.03 | 2.19 | −0.5 | +0.9 | −0.3 | 8.2 |
| `chip`: the encoder's frames on the emulator | 2.00 | +0.00 | 2.29 | −1.1 | +0.7 | +0.3 | 11.7 |

The quantisation step breaks down like this, each table applied to the anchor alone:

| Table | PESQ change |
|---|---|
| K | −0.12 |
| Pitch | −0.04 |
| Energy | −0.01 |

The "Change" column is the median of each file's change, so the steps do not add up exactly to the difference of the medians.

What this shows:

1. **The format costs about 0.33.** That is the tables (0.16, mostly K), the chirp (0.12) and the interpolation (0.05). The encoder cannot recover this, only work around it.
2. **The encoder's analysis is not the problem.** Its frames score as well as the simple analysis does (+0.03), so its pitch tracker, closed-loop energy and K search are about as good as the format allows. Tuning them further (§4) moves the score by hundredths.
3. **The chirp shifts the spectrum.** It moves the low band by −1.2 dB and the mid band by +1.2 dB. Its spectrum falls off much like de-emphasis by 0.9, but it is about 1 dB weaker from 300 to 800 Hz and about 1 dB stronger from 800 to 2000 Hz. So a fixed first-difference pre-emphasis (TI's or ours) leaves that difference in the output.

   **Tried:** pre-filtering voiced frames with the inverse of an order-4 fit to the chirp, [1, −0.938, 0.152, −0.104, −0.006], in place of 1 − 0.9375z⁻¹. On 40 files the tilt error halves (lo −1.0 → −0.4 dB, mid +1.2 → +0.7 dB at `qci`) and MCD improves by 0.05. PESQ moves only +0.02.

   This is the one analysis change found that the encoder could take up. It costs nothing in bits.
4. **The chip's arithmetic and 8-bit output hardly change PESQ, but they cost elsewhere.** From `enc` to `chip`, the low band loses 0.6 dB and voicing disagreement rises 3.5 points. That fits white noise from the output's resolution in the quieter parts.

   A louder encoding tests this. `targetRms=40` brings voicing disagreement down to 9.6 % and lo to −0.9 dB. But peaks then reach `peakLimit` and the level error rises from 1.8 to 2.8 dB, so PESQ falls to 1.90 (1.97 at `targetRms=30`; `targetPeak=120` gives 2.00). Level is a trade-off here, not a free gain.

One TI practice the encoder does not have is the human in the loop. Both TI systems relied on listening, editing energy and pitch by hand, and re-recording. The 1981 article draws its flowchart around an "audition, edit again" loop. Steve Petersen, who wrote the PSL software, recalls "a LOT of goofing around with the data until it sounded right" (interview on the PSL page). Its quality figures should be read with that in mind.
