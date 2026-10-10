# Other TMS5220 encoders: what they do, and what they suggest

**Status: notes, 2026-10-10.** This note examines four existing TMS5220 encoders, one at a time, for anything that might improve ours. It is a companion to `ti_encoder_clues.md`, which covers TI's own encoders and measures where the quality goes (`speechqual.py --ladder`). Its §5 found two things:

- the TMS5220 frame format itself costs about 0.33 PESQ against an unquantised vocoder;
- our encoder's analysis already scores about as well as the format allows.

The encoders:

| Encoder | Kind | Source |
|---|---|---|
| BlueWizard | Objective-C, macOS, MIT | <https://github.com/patrick99e99/BlueWizard> |
| python_wizard | Python port of BlueWizard, MIT | <https://github.com/ptwz/python_wizard> |
| TMS-Express | C++, CLI and Qt GUI, GPL-3.0 | <https://github.com/tornupnegatives/TMS-Express> |
| QBoxPro V3.34 | Quadravox, Windows 3.1, closed | furrtek's guide, <https://www.furrtek.org/index.php?a=speakandspell&ss=9&i=2>, and the package it links |

The clones and the QBox package are kept in `local/encoders/`, which is not in the repository. File references below are relative to each project's root there.

QBox was disassembled. `QBOXPRO.exe` still carries Borland's debug symbol table, 1630 names covering both functions and globals, so its code could be read function by function. §4 gives the tooling. Addresses there are `segment:offset`; segment 22 is the data segment.

As with MAME and TI's code, everything here is a hypothesis for our encoder to test with `speechqual.py`. None of it is a specification. Our encoder stays written from the datasheet.

## Summary

**None of the four does anything our encoder does not already do, in some form.** All four are open loop:

- the gain is computed from the analysis, not fitted to the chip's output;
- nothing models the chirp, the interpolation or the chip's output range in the analysis;
- there is no lag window;
- K, energy and pitch are quantised to the nearest value in the linear domain.

QBox, the only professional one of the four, works much as TI's own PSL encoder does (`ti_encoder_clues.md` §2):

- pre-emphasis of 0.99, voiced frames only;
- Le Roux–Gueguen K values;
- gain computed as RMS × Π√(1 − k²) × √pitch;
- nearest-k quantisation;
- repeats only when the quantised K values are equal.

On top of that, it adds three things TI's code does not have:

1. **A voicing score that combines three kinds of evidence.** It adds:
   - the strength of the pitch peak;
   - the spectral tilt, r1/r0;
   - the zero-crossing rate.

   The open-source tools use the first reflection coefficient alone; our encoder uses periodicity alone.
2. **A smoothing pass over the whole file.** It removes isolated voicing flips and pitch jumps.
3. **An optional "compression" pass.** It is a rate-distortion search by dynamic programming over runs of repeat frames, scored by a weighted cepstral distance.

Two practices recur in the hand-tuned tools:

- **Unvoiced frames are turned down.** QBox scales unvoiced energy by 0.5. BlueWizard copies that factor. TMS-Express advises an unvoiced ceiling of 0.8 × the voiced one. These are not three independent findings: BlueWizard's is QBox's.
- **Everyone edits by hand.** Every tool has some of:
  - a fixed-pitch override;
  - a pitch offset;
  - per-frame editing;
  - separate voiced and unvoiced gain limits.

  QBox's help tells the user to correct voiced fricatives and plosive releases by hand.

Two of the open-source tools also use pitch or K tables that are not the chip's (§1, §3).

§5 ranks the ideas as tests for our encoder.

## 1. BlueWizard

**Lineage.** BlueWizard is by Patrick J. Collins, 2015–17, under the MIT licence. It was announced on AtariAge as a "QBoxPro replacement". Its credits name Wiggins, Gevaryahu, Frantz and Palazzolo (`BlueWizard/Credits.rtf`), not Quadravox. It plays its output through MAME's tms5220 core, set to the 5220C variant.

**Tables.** The K tables are the chip's (`CodingTable.m:5-43`). The other two are not:

- **Pitch** (`CodingTable.m:49-51`):
  - index 1 is 1, where the chip has 15;
  - indices 22 and 23 are both 36, where the chip has 36 and 37;
  - above index 40 most entries are 1–2 below the chip's (157 at the top, against 159).
- **Energy** (`CodingTable.m:45-47`): a uniform 3 dB geometric ladder in 16-bit units, 52, 87, 123 … 7789. It is QBox's gain table (§4.5). Entry *i* stands for chip code *i* + 1. It runs at about 43 × the chip's table (1, 2, 3, 4, 6, 8, 11, 16, 23, 33, 47, 63, 85, 114) to within about 1 dB, except at the top: 5514 / 114 is 48.

**Input rate.** The input is not resampled. Analysis runs at the file's own rate (`Input.m:49`), so the pitch is only right for 8 kHz files.

**Pre-processing.**
- Pre-emphasis is 1 − 0.9375z⁻¹, in the time domain, over the whole signal, voiced and unvoiced alike, then rescaled to the original energy (`PreEmphasizer.m`).
- It is **off by default** (`UserSettings.m:14-19`, `ViewController.m:50-55`).
- Optional AudioUnit low-pass and high-pass filters, off by default.

**Analysis.**
- A 400-sample Hamming window (two frames), starting at the frame start (`Segmenter.m:37-49`). Each frame's parameters are therefore centred on the frame's end.
- Autocorrelation, then Le Roux–Gueguen to order 10 (`Reflector.m`).
- No lag window.

**Pitch.**
- Normalised cross-correlation on an "800 Hz" low-passed copy, over 50–500 Hz, with parabolic refinement.
- An octave fix that takes P/m when every sub-multiple lag reaches 0.9 × the peak (`PitchEstimator.m:49-79`).
- No smoothing between frames.
- The filter's AudioUnit stream format is never set (`Filterer.m`), so the cut-off may really be scaled for 44.1 kHz. That is unverified.

**Voicing.** Unvoiced when K1 ≥ 0.3, in its sign convention k1 = −r1/r0 (`Reflector.m:89-95`). Pitch plays no part.

**Quantisation.** K, energy and period each go to the nearest table value, linearly. The RMS is the final Le Roux error of the unquantised filter over the windowed 400 samples: √(E/N)·32768, with no correction for the window's power (`Reflector.m:47-53`). Energy is capped at index 14.

**Frame decisions.**
- No repeat frames.
- Silence only when the energy rounds to 0.
- Unvoiced frames send K1–K4 of the same order-10 solution.
- **Unvoiced energy × 0.5** (`Processor.m:74`, `UserSettings.m:119-123`). This is the same factor as QBox's `_unvoiced_ratio` (§4.4).
- Optional normalisation of the loudest voiced and unvoiced frames to index 14.

**Editing.** Every parameter of every frame is editable. There are also a pitch override and offset, an RMS limit, the unvoiced multiplier, and trimming.

**Bugs.**
- The pitch buffer is allocated 7 bytes short (`PitchEstimator.m:85`).
- A negative pitch offset clamps to 63 (`FrameData.m:180-181`).

## 2. python_wizard

**Lineage.** python_wizard is ptwz's port of BlueWizard (2017), extended by sintech (2020) with a preview player and a choice of chip tables. The pipeline is BlueWizard's. The differences:

**Tables.**
- The K and pitch tables come from MAME's `tms5110r.inc`, for the 5100, 5110A, 5200 and 5220.
- The CLI defaults to the 5220, but the settings and GUI default to the 5100 (`python_wizard:58-61`, `pywizard/userSettings.py:28`).
- The energy table is still BlueWizard's 3 dB ladder for every chip (`CodingTable.py:7`); `chip.energytable` is never used.

**Pre-processing.**
- Input is resampled to 8 kHz with a whole-file FFT resample (`Buffer.py:31-47`), which wraps at the file's ends.
- Pre-emphasis is −0.9373 (sic) over the whole file. It is off by default on the CLI, because `-p` is a store_true flag.

**Pitch.**
- A 5th-order Butterworth band-pass at 50–500 Hz, zero phase, replaces BlueWizard's low-pass (`Processor.py:56`).
- The correlation is `abs(corrcoef)`, so anti-correlation peaks also count (`Buffer.py:104-127`).
- Search over lags 15–161.

**Unvoiced level.** BlueWizard's unvoiced × 0.5 is gone, so its fricatives are 6 dB hotter than BlueWizard's.

**Bugs.**
- `RMSNormalizer` is never called and would crash, so `-U`/`-V` do nothing.
- `fd.stopFrame = True` overwrites a method, so the stop frame is followed by stray bits (`FrameData.py:12`).
- `isVoiced` is tested without being called (`FrameData.py:57`).
- A final odd nibble is dropped (`HexConverter.py:33`).
- The preview player resets its noise LFSR on every sample (`lpcplayer/player.py:122`).

**Timing.** Both wizards centre the analysis window on the frame's end, which suits the chip's interpolation: a frame's values are reached only after its glide. Our encoder already places its frames by when the chip reaches each frame's values (`REACHED = 225`, `encoder/tms5220enc.py`). There is nothing to take from this.

## 3. TMS-Express

**Lineage.** TMS-Express is by Joseph Bellahcen, 2023, under GPL-3.0. Its analysis is its own code. Its tables do not come from the chip:

- the energy ladder is python_wizard's 52 … 7789;
- K3–K10 are linear approximations up to about 0.002 off the ROM;
- K9 entry 3 has the wrong sign, +0.02857 for −15/512 (`src/encoding/CodingTable.hpp:84`).

Its preview synthesiser is derived from Talkie (`Synthesizer.cpp:2-3`), with the **TMS5100** chirp and energy tables. It has no interpolation and no integer arithmetic.

**Defaults.** The CLI (`src/ui/cli/CommandLineApp.hpp:61-95`) and the GUI defaults differ:

| Setting | CLI | GUI |
|---|---|---|
| Frame | 25 ms, non-overlapping; the window is the frame | same |
| High-pass on the LPC path | 1 kHz | 100 Hz |
| Low-pass on the pitch path | 800 Hz | off |
| α | −0.9375 | +0.9375 |
| Gain shift | +2 | off |
| Voiced / unvoiced gain ceiling | 37.5 / 30 "dB" | 37.5 / 37.5 |

**Pre-processing.** The filter is `y = x − αx[n−1]` (`src/audio/AudioFilter.cpp:57-62`). With the CLI's α = −0.9375 that is a low-pass. Followed by the 1 kHz high-pass, it removes F1. The README describes α as pre-emphasis.

**Analysis.** Hamming window over the 200-sample segment (none in the GUI path), biased autocorrelation, Levinson to order 10, no lag window.

**Pitch.** The autocorrelation's minimum in lags 16–160, then the maximum after it. No threshold, smoothing or octave check.

**Voicing.** Voiced when k1 < 0, i.e. from the sign of r1 alone (`BitstreamGenerator.cpp:152`).

**Energy.** Energy is effectively one bit:

- the gain is computed as |10·log10(e/10⁻¹²)|, from the 9th-order error (`LinearPredictor.cpp:58,64-75`);
- it is scaled so the peak is 37.5 "dB";
- that is then quantised against the linear RMS ladder.

Only index 0 or 1 is reachable, or 2 or 3 after the +2 shift, which also removes all silence frames. The reviewing agent reproduced this in numpy on `local/wav_examples`: indices {2: 43, 3: 71}.

**Repeats.** Marked when the K1 index changes by exactly 1 (`FramePostprocessor.cpp:49-72`).

**Hints.** The README's advice to set the unvoiced ceiling at 0.8 × the voiced one (`README.md:116-121`) is the only point of interest.

**Caution.** Do not use Talkie-derived tools as a reference for the 5220: their chirp and energy tables are the 5100's.

## 4. QBox / QBoxPro (Quadravox)

### 4.1 What it is, and how it was read

**The product.** Qbox was sold by Quadravox Inc., Richardson, Texas, around 1996 (<https://mi.eng.cam.ac.uk/comp.speech/Section1/Labs/quadravox.html>).

- It is a Windows 3.1 analysis and editing system for TI's TSP50C1x chips (the "D6" format), with an OTP programmer. It can also code for the TMS5220.
- It is built with Borland C++ for the 386, with floating point through `WIN87EM`.
- furrtek found it the only workable way to make Speak & Spell data, running it under DOSBox (ss=9). Being from 1992 or later, it was not used for the original products.

**Tooling.** In `local/encoders/qbox/dis/`:

| File | What it does |
|---|---|
| `ne.py` | Reads the NE segments and relocations |
| `syms.py` | Reads the Borland debug symbols (magic 0x52FB, inside an `NB02` wrapper at the end of the file) into `syms.txt` |
| `qdis.py` | Disassembles a function by name with Capstone; `--seg` writes `segNN.asm` for a whole segment |
| `notes_analysis.md`, `notes_pitch.md`, `notes_coding.md` | Working notes, with an address for every claim |

`qdis.py` also:

- turns the floating-point emulator's `INT 34h`–`3Dh` sequences back into x87 instructions, at the OS-fixup relocations only;
- names call targets and data references from the symbols;
- prints floating-point constants with their values.

Nothing was run; this is a reading of the code.

**What the symbol names suggest but the code does not do.** Several symbols name features that are never used. The code shows:

- **The Gold–Rabiner detector is dead code.** `_GRPITCH`, `_GSCORE` and their helpers in segment 15 are never called.
- **So are the FFT and comb path** (`_pre_process`, `_flatten`, `_fft`, `_module`, `_order`) and `_compute_pitch_score`.
- **Peakiness is computed but never read.** It is stored in `_peakiness`.
- **The pre-emphasis file is never read.** `lpcpre.dat` only appears in the LPC dialog. `_read_preemp` (16:0300) always sets one coefficient, 0.99 (22:2348).
- **`_pitch_bias` is always 1.0.** A histogram search computes it, but 16:4aad then overwrites it.
- **The `_gain_distortion` and similar totals are statistics only.** No decision uses them.

### 4.2 Settings in effect

At startup, `_initialize_system` reads `qboxpro.ini` (13:12aa–1450, through `GetPrivateProfileInt`). The hard-coded set in `_initialize_lpc_defaults` (12:a8f0) is used only if the INI gives no order or frame length (4:14cd), or when the user presses Defaults (10:10f6).

| Setting | INI default if the key is missing | Defaults button | furrtek's use |
|---|---|---|---|
| Order `LPCNB` | 12 | 12 | 10 |
| Voiced window `LPCWL` | 150 % of a frame | 150 % | default |
| Unvoiced window `LPCWLU` | 200 % | 200 % | default |
| Pitch range `LPCMINP`–`LPCMAXP` | 80–300 Hz | 50–500 Hz | default |
| Silence `LPCDBS` | 3 | 3 | default |
| Voicing score `LPCSCORE` | 25 | 15 | default |
| Compression weight, threshold | 0.5, 50 | 0.3, 100 | not used |

**Frame length.** The frame length follows the coding table, not `LPCFL`. `_initialize_lpc_variables` (10:4bd0) sets 200 samples for the TMS5220 project type and 160 for D6. It also sets the pitch oversampling to 1 and 16 respectively (`_init_table_vars`, 10:4cd5). So furrtek's `LPCFL = 160` (20 ms) does not affect TMS5220 data: one 25 ms analysis frame becomes one chip frame.

### 4.3 Analysis (`_lpc_analyze_frame`, 16:03a7)

**Input.** The file's mean is subtracted once (`_remove_bias`, 16:3b28). There is no other input filtering.

**Windows.** Hamming windows, centred on each 200-sample frame (16:032c, `_get_frame` 12:3910):

- voiced: 300 samples (s − 50 … s + 249);
- unvoiced: 400 samples (s − 100 … s + 299).

**Voiced path.**

1. Pre-emphasis 1 − 0.99z⁻¹, with its memory carried across frames (16:1374).
2. The 300-sample window.
3. Autocorrelation r0 … r12, with no lag window, white-noise correction or bandwidth expansion.
4. Le Roux–Gueguen (16:14b9). If |k| ≥ 1, the remaining K values are zeroed.

**Unvoiced path.** No pre-emphasis, the 400-sample window, the same 12-pole analysis. Only K1–K4 are coded.

**Both analyses are kept for every frame.** The voicing decision then picks which one to code.

**Energy** (16:0558 onwards):

- voiced: Ev = √(r0/300) × 2.0, where r0 is of the pre-emphasised, windowed signal (`_new_scale`, 22:0230);
- unvoiced: Euv = √(r0/300) × 2.0 × 0.5 (`_unvoiced_ratio`, 22:0234).

The unvoiced r0 is over 400 samples but is divided by 300. That looks like a bug, and it makes Euv √(4/3) (+1.2 dB) higher.

**Energy to gain.** In `_code_lpc_frame` (12:518f), with pki = Π(1 − k²):

| Frame | Gain | Code |
|---|---|---|
| Voiced | G = 0.651687 × Ev × √(pki × period) | `_energy_to_gain`, 16:4393; constant at 22:23c0 |
| Unvoiced | G = Euv × √pki, over K1–K4 only | `_energy_to_gain_unv`, 16:446c |

This is the same form as TI's PSL formula, RMS × Π√(1 − k̂²) × √P/2 (`ti_encoder_clues.md` §2.5).

The constant 0.651687 does not come from the chip's chirp, whose energy is exactly 256², nor from QBox's own preview pulse. Its origin is unknown.

After quantisation, the energy is recomputed from the chosen gain (16:4545).

### 4.4 Pitch and voicing

**Detector.** One detector runs: Rabiner's centre-clipped autocorrelation (`_do_pitch_tracking` 16:2568 → `_rabiner` 15:0c9f).

1. **Buffer:** the raw signal, 2·fs/minp samples long (200 at 80 Hz, 320 at 50 Hz), centred on the frame, with no taper.
2. **Low-pass:** a 32-tap FIR with a nominal 800 Hz edge (coefficients at 22:22f8). Its stopband only reaches about −4 dB; its two outer taps look ten times too large.
3. **Centre clip:** the clip level is 0.68 (22:22e8) × the smaller of the peaks in the first and last thirds of the buffer.
4. **Autocorrelation:** over lags fs/maxp … fs/minp. It takes the first maximum, with no octave check, then refines it with a parabola (16:28d6).
5. **Strength:** ρ′ = (R[p]/R[0]) × 10/0.3 (22:22a8, 22:22f0).

**Voicing** (`_compute_rab_pitch_score`, 16:31ad; the decision at 16:2841):

  score = 2ρ′ + T − (zc − 30)/10

- zc is the number of zero crossings per 10 ms of the raw frame (`_compute_zero_crossings`, 16:3a3f).
- T comes from a jump table on r1/r0, from the voiced analysis (16:325e):

  | r1/r0 | T |
  |---|---|
  | ≤ −0.5 | −2 |
  | −0.5 to 0.25 | −1 |
  | 0.25 to 0.5 | +1 |
  | 0.5 to 0.75 | +2 |
  | ≥ 0.75 | +4 |

The frame is voiced if the score ≥ `_lpc_score`. With the INI default of 25, a frame with steep tilt (T = +4) and 30 crossings per 10 ms needs ρ = R[p]/R[0] ≥ 0.32. A flat frame (T = −1) needs ρ ≥ 0.39, and each extra 10 crossings per 10 ms raises that by 0.015. Energy plays no part in the decision, and there is no hysteresis.

**Smoothing** (`_smoothe_pitch`, 16:083a). This runs once, over the whole file, after the last frame:

1. **Silence.** A frame becomes silent (pitch and both energies zeroed) if Ev < 10^(LPCDBS/10) × the quietest non-zero Ev in the file. With the default of 3 that is a factor of 2.0 in RMS (6 dB), relative to the quietest frame, not the loudest.
2. **Isolated voicing flips, ±2 frames.** A voiced frame with unvoiced frames two on each side becomes unvoiced. An unvoiced frame with voiced frames two on each side becomes voiced, with the mean of its neighbours' pitch.
3. **The same with ±1 frame.**
4. **Pitch jumps.** Where two adjacent pitches differ by a ratio above 1.6 (22:2370), the one farther from the file's mean pitch takes the other's value.

### 4.5 Coding to the TMS5220

**Coding table.** The .COD file (`QV5220.COD`, parsed at 13:5e0c) lists each parameter's method, bit count and values.

- The 5220 table uses the `direct` method throughout. K1–K10 are the chip's ×64, to within ±1.
- Pitch is the chip's table + 1, as in the datasheet and TI's PSL.
- The gain table is the 3 dB ladder 52 … 7789. Gain index *i* is written as chip energy code *i* + 1, capped at 14, with 0 for silence (13:e77f–e7cf). The ladder is about 43 × the chip's energy table, to within about 1 dB.
- The last value appears twice, so index 15 is never chosen.

**Quantisation.** Every parameter goes to the nearest table value, in the linear domain: K in k, gain in amplitude, pitch in period (`_code_parameter`, 16:4271). The log-area-ratio code (`_log_area`, the `coef01` method) only spaces a table evenly; it plays no part in quantisation.

**Frames.** The writer (`_normal_coding`, 13:e4eb) writes each frame as:

- energy;
- then the repeat bit and pitch;
- then 10 or 4 K values, unless the frame is a repeat.

The stream ends with the stop code. Without compression, a frame is a repeat only when all its K indices equal the previous frame's (13:89d9).

### 4.6 Compression (`_do_compression`, segment 11)

This is the optional "Low bit rate" step after "Medium".

1. **Regions.** The selection is split into runs of non-silent frames of the same voicing (11:0979).
2. **Segments.** For each start frame, a segment of frames is grown while its distortion stays below the threshold.
3. **Distortion of a segment** (11:048f):
   1. average the frames' K, each weighted by max(1, 10·log10 E);
   2. quantise the average;
   3. convert it to cepstrum (`_stepupm`, then `_a2cp` 11:190f);
   4. sum the L1 cepstral distance to each frame's own K, times that frame's weight.
4. **Best path.** `_find_best_path` (11:0d72) chooses the segmentation that minimises bits + weight × distortion. The bit counts (`_compute_bits`, 11:16bc) are:

   | Frame | Bits |
   |---|---|
   | Repeat | 11 |
   | Unvoiced | 29 |
   | Voiced | 50 |
   | Silent | 5 |
5. **Output.** Each segment becomes one coded frame with the averaged K, followed by repeat frames.
6. **Gain and pitch stay per frame.** After merging, `_correction_gain_pitch` (11:10cc) re-fits each frame's gain to the new K and quantises it again, and quantises the pitch again.

The help file says compression "may degrade quality slightly (sometimes it actually improves it!)".

### 4.7 Preview, workflow and level

**Preview.** QBox's preview synthesiser (`_lpc_frame_synthesis`, 16:3bed) is not a TMS5220 model:

- a 160-entry symmetric pulse, not the chirp;
- parameters moved halfway at each pitch period.

Nothing feeds back from it into the parameters.

**Workflow.** The help file (`SAMPLPRO.HLP`) expects the user to correct the pitch track by hand. It names the usual faults:

- voiced fricatives coded unvoiced (/z/);
- stray pitch at plosive releases (P→I, D);
- K values voiced in error.

QBox's voicing score has no feature aimed at either of the first two faults, so those are the cases its users fixed by hand.

**Level.** In the sample set, a synthesised file (`SAMPLE18`) has an RMS of 6679 against 4119 for its input, and peaks at 32767. `_lpc_boost` (default 4) is a playback gain only (3:19f5). Our own test of louder encoding lowered PESQ (`ti_encoder_clues.md` §5).

## 5. Ideas for our encoder, ranked

For each idea: what to try, why, and what the encoder does now.

1. **An inverse-chirp pre-emphasis filter for voiced frames.**
   - **Try:** the order-4 inverse-chirp filter, [1, −0.938, 0.152, −0.104, −0.006], in place of 1 − 0.9z⁻¹.
   - **Why:** our own measurement halves the tilt error and improves MCD by 0.05, for +0.02 PESQ (`ti_encoder_clues.md` §5). None of the four tools does anything like it: QBox and TI use 0.99 and 1.0, and the others 0.9375 or nothing.
   - **Now:** 0.9.
2. **More evidence for voicing.**
   - **Try:** add QBox's two extra features to the voiced/unvoiced cost in our Viterbi path: the tilt r1/r0 and the zero-crossing rate per 10 ms. Start with QBox's weights: the score above, with the pitch strength replaced by our CMNDF.
   - **Check:** the voicing column, and the sibilant, fricative and voiced/unvoiced-pair prompts in `prompts.csv`.
   - **Why:** the chip's 8-bit output costs 3.5 points of voicing agreement (`ti_encoder_clues.md` §5). Voicing is where every tool's users intervened by hand.
   - **Now:** CMNDF only (`voicingThreshold`).
3. **A small unvoiced energy offset.**
   - **Try:** after the closed loop, lower unvoiced frames by 1, 2, 3 and 6 dB. Score with PESQ, the levelErr and tilt columns, and listening.
   - **Why:** QBox (copied by BlueWizard) uses 0.5, and TMS-Express 0.8. Our closed loop matches the input's RMS, so any gain would be perceptual, or headroom for the voiced frames.
   - **Now:** none.
4. **Repeats by rate-distortion search.**
   - **Try:** QBox's scheme with our own measures. Within each voiced or unvoiced run, choose the repeat runs by dynamic programming on bits + λ × distortion, using our prediction-error measure or a cepstral distance. Re-fit the gain after merging. Sweep λ to trace bits against PESQ, and compare with the greedy `repeatThreshold` curve.
   - **Why:** the search chooses repeats over a whole run rather than frame by frame.
   - **Room:** small. `repeatThreshold 0` already gained only +0.03 PESQ for 18 % more bits (`ti_encoder_clues.md` §4). The gain, if any, is in bits per quality rather than quality.
   - **Now:** greedy, by prediction error.
5. **Longer windows, of different lengths.**
   - **Try:** 300 samples voiced and 400 unvoiced, centred, as QBox does.
   - **Why:** a 200-sample window made no difference (`ti_encoder_clues.md` §4). Longer windows smooth K over the interpolation. Expect little.
   - **Now:** one 240-sample window.

Not worth testing, and why:

- **Voicing from K1 alone:** it ignores periodicity.
- **Linear-domain quantisation:** our LAR search with the prediction-error check is better.
- **Open-loop gain** (`RMS × Π√(1 − k²) × √P`, QBox and TI): our closed loop replaces it.
- **No lag window:** already measured; `lagWindowHz 0` scores +0.02 PESQ for about 2 % more bits (`ti_encoder_clues.md` §4).
- **Pre-emphasis 0.99 or 1.0:** already measured (pre 1.0, §4 there); no change.
- **The wizards' late window:** our frame placement already accounts for the interpolation (`REACHED`).
- **Rabiner centre-clip pitch with first-peak picking, and QBox's file-level pitch smoothing:** our YIN with a Viterbi path and octave check covers the same ground.
- **Loud output:** already measured; it lowers PESQ.
