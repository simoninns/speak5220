# Encoder quality: why encodings sound worse than the original PHROMs, and what to change

## 1. Scope

Encoded speech sounds distorted and robotic next to the words of Acorn's
PHROM A, its S, C and CH sounds come out as noise, and it is quieter. This
document records the investigation of the encoder (`app/src/core/encoder/`,
the design in [HLD §8](design/high-level-design.md#8-the-encoder)), ranks the
causes found, proposes the implementation changes, and specifies an automated
diagnostic harness so that each cause can be measured again after a change.

Everything below was measured with the application's encoder as built from
`8420177` (`speak5220-cli encode`, the defaults of `EncoderOptions`), the
Python reference encoder where an option the application fixes had to be
moved, and the application's own quality measures (`core/quality/`,
[HLD §9](design/high-level-design.md#9-quality-measurement)).

## 2. Summary

| Symptom | Cause | Evidence (§) | Change (§5) |
|---|---|---|---|
| Quieter than the PHROM | The peak limiter at 120 codes sets the level, not `target_rms`; the PHROM drives the DAC into clipping (half its words peak over 127). Recordings are bass-heavy, so after the BBC Micro's bass cut the gap grows to 3–5 dB | 4.1 | C1, C2 |
| Vowels interrupted by bursts of hiss; "S-like noise" in the middle of words | Loud voiced frames classed unvoiced: `voicing_threshold` 0.4 is too low for clear, bright vowels, and the zero-crossing/tilt evidence pushes open vowels (/æ/, /ɛ/) to unvoiced. On the PHROM's own audio 5.4 % of its voiced frames become unvoiced, 78 of 90 inside vowels at energy 8–12 | 4.2 | C3 |
| Noise burst at the start of vowels | The voicing of a frame is decided at its centre ±5 ms, but the chip plays a frame's voicing for the 25 ms *after* its centre; onsets are a frame late in 20 % of the PHROM's onsets, and the late frame carries the vowel's level as noise | 4.2 | C4 |
| Buzzing, wrong-pitched vowels in some words | The pitch tracker locks on a harmonic (470 Hz for a 117 Hz voice in "six") and its utterance prior then holds the lock for the whole word; 3 of 64 TTS words have a gross pitch error on every frame | 4.3 | C5 |
| Robotic, frozen formants | `repeat_lambda` 70 marks 40 % of sounding frames as repeats on the PHROM's own audio, where the PHROM has 14 %; 30 % of the TTS vowels sit in filters frozen for 75 ms or more | 4.4 | C6 |
| Hiss trailing every word-final S, CH, K | `AddFades()` appends decaying noise frames after unvoiced sound before silence (37 fade-outs, 43 frames, in 157 PHROM words re-encoded; the PHROM has 3) | 4.5 | C7 |
| Consonants a little loud against vowels | `target_rms` 24 against the PHROM's 21.8, and `level_agc` 1.25 lifting the quiet consonant stretches: unvoiced frames land one energy step (3 dB) above the PHROM's on the same audio | 4.1 | C2 |

Not at fault (checked, §4.6): the LPC analysis and K quantisation (the
filter shapes match the PHROM's to 1.6–2.1 dB RMS per frame), the lag
window, the pre-emphasis, the closed-loop level setting itself, the frames
silenced as too loud at energy 1, and the output spectrum's balance against
the source.

## 3. Method and materials

### 3.1 The PHROM round trip (ground truth)

Every word of Acorn PHROM A (`investigation/proms/acorn_phrom_a.json`, the
image in `investigation/firmware/src/romdata_acorn.h`) was parsed into
frames, synthesised with the reference algorithm (speak5220 datasheet §8.9;
`tms5220enc._synth()`, which the chip matches sample for sample) and written
as 8 kHz WAV. The 157 words of more than 8 frames that are not the two tones
were then encoded by `speak5220-cli` with the default options, and the
encoder's frames compared with the PHROM's. Because the audio *is* a
TMS5220 rendering of known frames, every difference is the encoder's.

Frames were aligned by the shift of −3…+3 frames that minimises the
difference of the two output level envelopes; repeats were expanded to the
K they hold; the filter shape of each matched frame was compared as the
gain-normalised dB response of 1/A(z) from the quantised K.

### 3.2 Test speech

- **TTS**: 16 prompts rich in S, CH and K (`six`, `cheese`, `church`,
  `kick`, `sister`, `she sells sea shells`, `Charlie chose cheap cherries
  and chips`, `Kate kicked the cold cup across the kitchen`, …) in four
  macOS voices (Daniel, Fred, Eddy and Flo, en_GB/en_US), resampled to
  8 kHz with peaks at −3 dBFS: 64 files.
- **Recordings**: the 33 ARCTIC files in `local/arctic-project/generated-corpus/`
  (a project of the corpus of HLD §9).

### 3.3 Measures

- **Frame statistics**: kinds, energy index per kind, K1 index, repeat
  run lengths, fade-outs, and (for the round trip) voicing agreement with
  the PHROM split by position (inside a voiced run, at its onset, at its
  offset), onset and offset lag in frames, pitch difference in semitones,
  energy index difference, filter-shape difference.
- **Level**: the RMS over sounding frames as `target_rms` defines it
  (Hann-weighted 25 ms levels at output sample 225 + 200 j); the peak code;
  the share of samples the DAC clips (|code| > 127); RMS after the BBC
  Micro's speech path, a first-order high-pass at 279 Hz (HLD §16); and
  A-weighted RMS, as a loudness estimate. The last two matter because the
  score of HLD §9 scales the output to the source's power and cannot see
  loudness at all.
- **Quality**: the application's measures and overall score
  (`MeasureQuality()`, `ScoreQuality()`: envelope distortion, REAPER gross
  pitch error (GPE), F0 RMSE, voicing decision error (VDE), ESTOI), run on
  any (WAV, frames) pair through a small driver linked against `libs5core`;
  it reproduces the `MeasuresCorpusCheck` rows exactly.

## 4. Findings

### 4.1 Level and loudness

**What the PHROM does.** Over its 157 speech words:

| Quantity | PHROM A | Ours, same audio, defaults |
|---|---|---|
| Sounding-frame RMS, codes (median) | 21.8 | 20.3 |
| Peak code (median; quartiles) | 124 (101–154; max 297) | 108 |
| Words with a peak over 120 / over 127 | 84 / 74 of 157 | 0 / 0 |
| Samples clipped by the DAC (mean) | 0.21 % | 0 |
| RMS after the 279 Hz high-pass | 17.1 | 15.4 |
| A-weighted RMS | 14.5 | 13.5 |
| Voiced energy index (median) | 12 (27 % of voiced frames at 14) | 11 |
| Unvoiced energy index (median) | 4 (none above 9) | 5 |

TI's data was authored to a loudness that clips the DAC on the peaks of
half the words: its crest factor is about 15 dB. Ours is capped at a peak
of 120 (`kPeakLimit`, `level_setter.cpp`), so with speech's crest factor
the limiter, not `target_rms`, sets the level: on the ARCTIC and TTS sets
the median peak is 114–119 (maximum 120) in every file, and the median
sounding RMS 20.0–20.8 against the target of 24.

**Why it sounds much quieter than the 0.8 dB of RMS suggests.** The
recordings keep their bass; the PHROM's spectrum does not (HLD §9, "Voicing
and source shaping"). Long-term band power relative to the total, in dB
(0–250, 250–500, 500–1k, 1–2k, 2–3k, 3–4k Hz):

| Signal | Bands |
|---|---|
| PHROM A output | −7.8, −6.8, −9.1, −10.1, −13.1, −17.2 |
| ARCTIC source / our encoding | −3.3, −4.9, −9.0, −15.1, −21.3, −23.6 / −3.2, −4.9, −9.3, −14.4, −19.5, −22.9 |
| TTS source / our encoding | −5.7, −4.1, −10.1, −15.9, −18.0, −23.0 / −5.2, −4.4, −10.3, −15.2, −16.9, −22.0 |

The encoding follows its source to within about 1 dB per band, so the
encoder is faithful; but a bass-heavy signal carries less loudness per code
of RMS, and the BBC Micro's path removes the bass. Median over the files:

| Encoding | RMS over the talk | After the 279 Hz high-pass | A-weighted |
|---|---|---|---|
| PHROM A | 20.9 | 17.1 | 14.5 |
| ARCTIC, defaults | 19.0 | 11.6 | 8.4 |
| TTS, defaults | 19.6 | 12.4 | 9.3 |
| ARCTIC, *BBC Micro* preset (bass cut 200 Hz, tilt 0.4) | 16.8 | 13.1 | 11.3 |

Our encodings are about 3 dB quieter through a BBC Micro and 4–5 dB quieter
A-weighted. The *BBC Micro* preset closes a third of the gap but loses
plain RMS to the limiter, since the brighter signal has sharper peaks.

**What the peak limit costs, and what allowing the PHROM's clipping would
give** (the Python reference encoder with the application's defaults, 12
ARCTIC files; `peakLimit` is fixed at 120 in the application):

| Options | Sounding RMS | Peak (median / max) | Clipped samples (median / max) | Frames limited | A-weighted |
|---|---|---|---|---|---|
| Defaults (peak 120) | 20.1 | 119 / 120 | 0 / 0 | 161 of 1237 | 8.4 |
| Peak 127 | 20.7 | 126 / 127 | 0 / 0 | 118 | 8.7 |
| Peak 160 | 22.7 | 147 / 160 | 0.10 % / 0.23 % | 41 | 9.4 |
| Peak 200 | 23.2 | 168 / 196 | 0.23 % / 0.39 % | 7 | 9.9 |
| No limit (511) | 23.2 | 168 / 233 | 0.23 % / 0.39 % | 0 | 9.9 |
| Peak 160, `targetRms` 30 | 25.7 | 156 / 160 | 0.25 % / 0.75 % | 101 | 11.0 |
| Bass cut 200 Hz + tilt 0.4, peak 120 | 18.1 | 118 / 119 | 0 / 0 | 111 of 1248 | 11.9 |
| Bass cut + tilt, peak 160 | 19.8 | 156 / 159 | 0.09 % / 0.22 % | 29 | 12.6 |
| Bass cut + tilt, peak 160, `targetRms` 30 | 22.6 | 156 / 159 | 0.15 % / 0.34 % | 60 | 14.4 |

No lattice sum wrapped in any of these. A peak limit of 160 with a 1 in
1000 clip budget is what the PHROM does (0.21 % of samples on average),
and with the PHROM's spectral shaping and `target_rms` 30 the A-weighted
loudness matches the PHROM's 14.5. The overall quality score does not move
with the peak limit (0.636 → 0.639 → 0.641 on 16 files for 120, 160, and
160 with `targetRms` 30), because it scales the output to the source: a
loudness check must be a separate measure (§6).

**Consonants against vowels.** On the PHROM's own audio our unvoiced
frames come out a mean 0.80 energy steps above the PHROM's, our voiced
frames 0.35 above. With `level_agc` 1 and `target_rms` 22 (the PHROM's
level) the differences fall to 0.41 and 0.15: the slow gain control lifts
the quiet stretches, which are mostly consonants, and the limiter takes
back from the loud vowels what the higher target gave them. Net: hiss
1.5 dB louder against vowels than in the PHROM.

The frames the level setter silences as too loud at energy 1 (32 of 1237
on ARCTIC) are all 29 dB or more below the loudest frame (median 40 dB),
unvoiced, with K1 near −1: breaths and rumble. Harmless.

### 4.2 Voicing

On the PHROM's own audio (2737 matched sounding frames), the voicing
agrees on 93.9 %. The disagreements are one-sided: 148 of the PHROM's
voiced frames are unvoiced in ours, 20 the other way. Where the 148 fall:

| Position in the PHROM's voicing | Frames | Our energy there |
|---|---|---|
| Inside a voiced run (vowel) | 90 | median index 10; 78 are at 8 or more; our level 28 codes against the PHROM's 21 |
| The first voiced frame (onset) | 44 | median 4; 13 at 8 or more |
| The last voiced frame (offset) | 14 | |

The 90 inside vowels are the "noise instead of speech": a loud frame with
the vowel's spectrum and the noise excitation. They cluster in open, bright
vowels (HAVE, THAT, THANK, CASSETTE, AND, PENCE, SEVEN). Two mechanisms,
seen hop by hop with the reference tracker's internals:

- **The threshold.** In `HAVE`, frames 3–4: the CMNDF dips to 0.07–0.25
  at the period (clear periodicity), but the low-band ratio is under 0.5
  so the threshold falls by 0.15, and the zero-crossing evidence (26–33
  crossings per 10 ms in /æ/) takes a further 0.04–0.09 off: the unvoiced
  state costs 0.29–0.39 while the voiced candidate pays its CMNDF plus the
  prior (the word's median period is 88 samples, this vowel's 44–51, an
  octave away, so 2 units) and ends up dearer.
- **Pitch glides.** In `CASSETTE`, frames 14–16 are driven by the chip
  from period 45 to 73 in four frames (Kendall's falling intonation); the
  30 ms YIN window sees a chirped period and the CMNDF stays at 0.54–0.76.
  Natural speech glides more slowly, but the same loss shows at every fast
  fall.

What moves it, on the same audio:

| Options | Agreement | Voiced → unvoiced inside / onset / offset | Unvoiced → voiced |
|---|---|---|---|
| Defaults (`voicing_threshold` 0.4, `voicing_evidence` 5) | 93.9 % | 90 / 44 / 14 | 20 |
| `voicing_evidence` 0 | 91.8 % | 122 / 53 / 24 | 25 |
| `voicing_evidence` 2.5 | 92.9 % | 107 / 45 / 18 | 23 |
| `voicing_threshold` 0.5 | **96.7 %** | **28 / 33 / 4** | 24 |
| Threshold 0.5, evidence 0 | 95.0 % | 60 / 35 / 15 | 26 |

The evidence term helps on this audio; the threshold is what is too low.
(HLD §9 chose 0.4 from a comparison of 0.3 and 0.4 only.)

**Onsets.** Of the PHROM's 106 voicing onsets, ours is on time in 69,
one frame late in 14, two or three late in 7, and never voiced in 11; of
its 204 offsets, ours is on time in 99 and one frame late in 61. The chip
plays frame j's voicing from the moment its values are reached (output
sample 225 + 200 j) for the 25 ms that follow (speak5220 datasheet §8.6:
the excitation follows the flag a frame later; §8.7: a voicing change is
inhibited and jumps in IP 0), that is, over the input from the frame's
centre to the next frame's centre, while `VoicingClassifier::Classify()`
takes the majority of the three hops at the centre ±5 ms. A vowel that
starts 10 ms after the centre is therefore voiced a frame late, and the
late frame is coded as a noise burst at the vowel's level. TI's encoder
voiced such frames (§3.1's data shows it), which is also the less audible
error: a chirp a few ms early at a rising level against 25 ms of loud
hiss.

Deciding from the hops after the centre, on the reference analysis of the
PHROM audio (2747 frames):

| Hops that vote (5 ms each; 0 is the centre) | Agreement | Inside / onset / offset | Unvoiced → voiced |
|---|---|---|---|
| −1…+1 (current), threshold 0.4 | 93.7 % | 95 / 46 / 13 | 20 |
| 0…+4 | 93.9 % | 98 / 26 / 38 | 6 |
| −1…+3 | 94.4 % | 88 / 34 / 18 | 13 |
| −1…+1, or 0…+4 when the next frame's centre is voiced | 94.9 % | 78 / 26 / 13 | 22 |
| −1…+1, threshold 0.5 | 96.5 % | 32 / 36 / 7 | 22 |
| −1…+1 or onset rule, threshold 0.5 | **97.2 %** | **25 / 20 / 7** | 24 |

A plain forward window fixes onsets but ends voicing early at offsets
(38); the onset rule (vote forward only when the following frame is
voiced) keeps the offsets and halves the onset lag.

On the TTS and ARCTIC encodings, 7 % and 2 % of voicing onsets carry an
unvoiced frame at energy 8 or more (value 16, a loud hiss) immediately
before the vowel.

### 4.3 Pitch

On the PHROM's own audio the pitch is right: median difference 0
semitones, mean |difference| 0.33, 0.2 % of frames more than 6 semitones
off. On natural and TTS speech the tracker can lock on a harmonic:

- `six` (Daniel): the vowel's spectral peaks are 117 Hz apart (period
  68), the encoder codes period 17–18 (pitch index 2–3, 470 Hz, the 4th
  harmonic, which /ɪ/'s first formant boosts). On the 1 kHz low-passed
  signal the CMNDF at lag 17 is 0.36–0.45 and at lag 72 0.65; the
  utterance prior (the median of each loud hop's *first* dip below 0.15,
  `kConfidentDip`) comes out at 18 samples, and the prior cost
  (`kPrior` 5 per octave beyond 0.6) then holds every frame of the word
  at the harmonic even where the true period's dip is deeper (frame 8:
  lag 92 at 0.26 against lag 17 at 0.39).
- REAPER gross pitch error of the default encodings against the source:
  `six` (Daniel), `church` (Eddy) and `cat` (Flo) 100 %; `science` (Flo)
  63 %; `seven` (Flo) 49 %; `sixty six` (Daniel) 49 %; 10 of the 64 TTS
  files over 20 %. ARCTIC: median 5.7 %, worst 15 %.

Two quick changes were tried on the reference tracker and did not help:
a prior from the deepest dip of the confident hops (no change: the
harmonic *is* the deepest dip for most of the vowel), and a spectral
subharmonic check that promotes 2×, 3× or 4× the lag when the spectrum
holds energy at the intermediate harmonics (worse overall: more files
over 20 %). The lock is not a matter of a constant. Note also that the
subharmonic rule in `pitch_tracker.cpp` (`kSubharmonic`) penalises the
*longer* of two candidates when the shorter is clearly periodic, which
favours harmonics by design; and that with the median prior wrong the
Viterbi's jump cost (`kJump` 12 per octave) makes a correction within the
word impossible.

### 4.4 Repeats

| Material | Repeats of sounding frames | Repeat runs (1 / 2 / 3 / 4+) |
|---|---|---|
| PHROM A | 13.6 % (393 of 2891) | 229 / 27 / 10 / 2 |
| Ours on the PHROM audio, `repeat_lambda` 70 | 40 % (1164 of 2934) | 508 / 176 / 63 / 23 (longest 9) |
| TTS, defaults | 34.7 % | 30 % of voiced frames in filters frozen for 75 ms or more |
| ARCTIC, defaults | 19.0 % | 8 % of voiced frames frozen for 75 ms or more |

The search in `repeat_search.cpp` trades bits against the sum, over the
run, of the dB increase of the *prediction error* when the run's pooled
K replace each frame's own. That measure barely moves for a formant that
drifts 100 Hz, so vowels that TI coded as four changing full frames
become one full frame and three repeats, and `church` (Daniel) holds one
filter for 125 ms through a diphthong. Against the PHROM's frames on its
own audio, with the rest at the defaults:

| `repeat_lambda` | Repeats | Voiced filter-shape error per frame (median dB RMS) |
|---|---|---|
| 70 (default) | 1164 | 2.11 |
| 140 | 715 | 1.94 |
| 300 | 394 (the PHROM's 393) | 1.87 |
| 1000 | 161 | 1.87 |

The quality score agrees (HLD §9 already found repeats off worth +0.014
on the mean score, the largest effect of any fixed option):

| Set | Options | Score median / mean | Envelope dist. (dB) | GPE | VDE | ESTOI | Bytes |
|---|---|---|---|---|---|---|---|
| ARCTIC (33) | Defaults | 0.679 / 0.682 | 3.10 | 5.7 % | 13.9 % | 0.746 | 16,857 |
| | `repeat_lambda` 140 | 0.682 / 0.690 | 3.08 | 5.7 % | 14.4 % | 0.754 | 18,856 |
| | `repeat_lambda` 300 | 0.682 / 0.691 | 3.05 | 5.7 % | 14.2 % | 0.754 | 19,752 |
| | `voicing_threshold` 0.5 | 0.681 / 0.682 | 3.08 | 5.7 % | 16.0 % | 0.752 | 17,100 |
| | Threshold 0.5 + lambda 300 | **0.699** / 0.691 | 3.01 | 5.4 % | 16.0 % | 0.759 | 20,033 |
| | Threshold 0.5, lambda 300, evidence 2.5 | 0.700 / 0.692 | 3.02 | 5.7 % | 16.0 % | 0.759 | 19,953 |
| TTS (64) | Defaults | 0.707 / 0.684 | 3.07 | 0.9 % | 6.0 % | 0.808 | 9,982 |
| | `repeat_lambda` 140 | 0.733 / 0.702 | 3.01 | 1.3 % | 5.6 % | 0.811 | 11,112 |
| | `repeat_lambda` 300 | 0.722 / 0.702 | 2.96 | 1.3 % | 5.8 % | 0.811 | 12,061 |
| | `voicing_threshold` 0.5 | 0.707 / 0.691 | 3.07 | 1.1 % | 6.0 % | 0.807 | 10,062 |
| | Threshold 0.5 + lambda 300 | 0.728 / 0.708 | 2.96 | 1.3 % | 6.0 % | 0.811 | 12,146 |
| | Threshold 0.5, lambda 300, evidence 2.5 | **0.747** / 0.719 | 2.93 | 0.9 % | 6.0 % | 0.811 | 12,151 |

Per TTS voice, defaults → threshold 0.5, lambda 300, evidence 2.5:
Daniel 0.599 → 0.651, Eddy 0.699 → 0.779, Flo 0.846 → 0.851, Fred
0.713 → 0.731. The cost is size: +17 % on ARCTIC, +21 % on TTS at lambda
300 (+11–12 % at 140); the PHROM itself spends 118,804 bits on the words our
defaults code in 92,192 (78 %).

### 4.5 Fades

`AddFades()` (`frame_endings.cpp`) follows every unvoiced frame that
precedes silence or the stop with repeat frames at a third of the energy
each until the value is 2 or less: one to three frames of decaying hiss.
Re-encoding the PHROM's words produced 37 such fade-outs (43 frames) in
157 words; the PHROM has 3 (4 frames). TI's words end on an unvoiced frame at full
level in 48 of 157 cases, and where an S decays it is coded as full
frames at falling energy (SIX ends 6, 6, 4, 3, 2, 1, 1). The fade was
added so that hiss does not stop abruptly (datasheet §12.2), but the
natural decay is already in the frames; the appended tail lengthens every
final S, CH and K by 25–75 ms, and after a pause it fills the start of the
gap with hiss.

### 4.6 Checked and not at fault

- **LPC analysis and K quantisation.** Against the PHROM's own K on its
  audio, the filter shapes agree to a median 2.1 dB RMS per voiced frame
  (1.9 with the repeats of §4.4 removed) and 1.6 dB per unvoiced frame,
  with a mean within ±1.3 dB at every frequency; unvoiced K1 spreads a
  little lower than the PHROM's (its S frames sit at index 31, +0.85)
  but the resulting shapes match. The chirp inverse pre-emphasis and the
  60 Hz lag window do what they should on chip-made speech.
- **Level setting.** The closed loop's mean error after five passes is
  1.7 dB (3 dB steps); frame-to-frame energy roughness is the PHROM's
  (mean |Δ index| 1.50 against 1.45). Per-frame level against the PHROM's
  is unbiased (median +0.06 dB) but spreads to −3.4 / +4.4 dB at the 10th
  and 90th percentile, mostly where the kinds differ.
- **Spectral balance.** The encodings follow their source to within about
  1 dB per band (§4.1).
- **Pitch on chip-made speech** (§4.3) and the quiet pauses.

## 5. Recommended implementation changes

In order of audible effect. Every change that alters the frames of the
reference option sets invalidates `app/golden/encoder/` and needs the
user's agreement, a regeneration in its own commit with the quality before
and after, and a new `kEncoderVersion` (AGENTS.md §3.1; HLD §8,
"Results"). Changes to defaults alone (C2, C3, C6) touch no reference
data, since the reference is compared with its own defaults written out.

**C1. Let the level reach the PHROM's: a peak limit with a clip budget**
(`level_setter.cpp`, `kPeakLimit`; HLD §8 "Fixed options"). Raise the
limit from 120 to about 160 and make the limiter's criterion the share of
samples beyond ±127 (at most 1 in 1000, the PHROM's own 0.2 % mean), while
keeping the wrap check at ±512 absolute (datasheet §12.3). Expected: +2.6 dB
sounding RMS, three quarters fewer limited frames, no score change (§4.1).
Alternatively expose it again as an advanced option `peak_limit` with the
new default. Acceptance: on the ARCTIC set the median sounding RMS is
within 1 dB of the PHROM's 21.8; clipped samples under 0.3 % in every
file; no wraps.

**C2. Loudness-aware level target.** Two parts:

- Raise `target_rms` to 30 once C1 lands (the PHROM's loudness needs the
  headroom C1 gives; §4.1 last table).
- Measure the level the encoder aims at after a 279 Hz first-order
  high-pass (the BBC Micro's path, HLD §16) or with A-weighting, in
  `FrameAnalyser::Features()` (`level`) and `MeasureOutputLevels()`, so
  that bass does not count as loudness and a bass-heavy source is not
  held down by its own rumble. With the *BBC Micro* preset on, the
  encoding then reaches the PHROM's A-weighted 14.5 (§4.1). Consider
  making the preset's shaping the default for new projects, with the
  score shown against the preprocessed source as it is now.
- Lower `level_agc` to 1 or 1.1, or exclude unvoiced frames from the
  envelope it lifts, to bring the consonant level back to the PHROM's.

Acceptance: a loudness measure (A-weighted RMS of the chip output over the
talk) within 1.5 dB of the PHROM reference (14.5) on the ARCTIC set with
the preset, and unvoiced energy indices within 0.3 of the PHROM's on the
round trip.

**C3. `voicing_threshold` 0.5** (`encoder_options.h`). Keep the evidence
term. Expected: voiced frames lost inside vowels on the PHROM round trip
from 90 to 28 (§4.2); ARCTIC score unchanged, TTS +0.00 to +0.04 with C6.
Confirm on the full corpus with `OptionsCorpusCheck` as for 0.4 (HLD §9),
and check the voicing decision error does not rise beyond the ARCTIC
figure (16 %) on the corpus. Acceptance: PHROM round-trip voicing
agreement ≥ 96 %, with voiced-to-unvoiced misses inside runs ≤ 30.

**C4. Decide onsets from the hops the chip will play**
(`voicing_classifier.cpp`, `VoicingClassifier::Classify()`). Keep the
centred vote, but when the following frame's centre is voiced also take
the majority of hops 0…+4 (the 25 ms the frame is heard for): voiced if
either says so. Expected: onset lag halved (46 → 20–26 late onsets of 106
on the round trip), unvoiced-to-voiced errors unchanged (§4.2). Changes
the reference frames. Acceptance: round-trip onsets on time ≥ 80 of 106;
no rise in offsets late.

**C5. A pitch tracker that cannot lock on a harmonic**
(`pitch_tracker.cpp`). The constants are not the problem (§4.3). Options,
in order of preference:

1. Use the REAPER estimator already in the tree
   (`quality::SptkPitchEstimator`, `PitchAlgorithm::kReaper`, 50–400 Hz,
   10 ms) to produce the voicing and the F0 track for the encoder, with
   the present 5 ms YIN only refining the period inside REAPER's voiced
   stretches. REAPER's track of the source is what the GPE measure trusts,
   and it is right on the words our tracker loses.
2. Failing that, replace the utterance prior: run the Viterbi once without
   it, take the median period of its voiced path weighted by level, and
   re-run with the prior around that; add a harmonic test on the
   *full-band* normalised autocorrelation at 2×, 3× and 4× the candidate
   lag; and cap the voiced search at 350 Hz for a frame whose spectrum
   has peaks below the candidate F0.

Acceptance: REAPER GPE of the default encoding against the source under
20 % on every file of the TTS set (today 10 of 64 fail) and median ≤ 5 %
on the ARCTIC set; round-trip pitch agreement with the PHROM unchanged
(mean |difference| ≤ 0.4 semitones).

**C6. Fewer repeats** (`encoder_options.h`, `repeat_search.cpp`). Raise
the default `repeat_lambda` to 300 (the PHROM's repeat rate on its own
audio; +0.02 median score on ARCTIC, +0.04 on TTS with C3, +17–21 %
bytes) or 140 as a compromise (+12 % bytes). Better, change the distortion
in `SegmentCost()` from the prediction-error ratio to the log spectral
distance between the candidate K's envelope and each frame's own (the
measure of HLD §9, which is sensitive to formant movement), so a repeat is
chosen only where the spectrum is stationary; then a lower lambda keeps
the size. Acceptance: ≤ 20 % repeats on the PHROM round trip; no voiced
run frozen for more than 75 ms unless the shape distance stays under 2 dB.

**C7. Fade only what would otherwise stop abruptly** (`frame_endings.cpp`,
`AddFades()`). Add fade frames only after an unvoiced frame whose energy
index is 6 or more (value 8) and whose own frame was still loud, and then
one frame; otherwise end as the PHROM does. Changes the reference frames.
Acceptance: fade-outs on the PHROM round trip ≤ 10 of 157 words.

Together, C3 + C6 lift the TTS median score from 0.707 to 0.747 and the
ARCTIC median from 0.679 to 0.699 without touching the reference data;
C1 + C2 restore the loudness; C4, C5 and C7 remove the remaining artefacts
the score cannot measure (onset bursts, harmonic locks, hiss tails) and
are judged by the round trip and the GPE.

## 6. The diagnostic harness

The measurements of §4 were made with scratch scripts around the CLI, the
Python reference and a driver over `libs5core`. They are now part of the
repository (Phase 1 of [encoder-quality-plan.md](encoder-quality-plan.md),
which also holds the phased implementation of §5; HLD §9, "Diagnostics";
TESTING.md):

| Tool | Does | Replaces |
|---|---|---|
| `app/scripts/phrom-check.sh` (`phrom_roundtrip.py`, `PhromRoundTripCheck`) | Writes PHROM A's words as frames and as the chip's output (`speak5220-cli synth`), encodes them and compares the encodings with the PHROM's frames, per word and in total | §3.1's round trip, the voicing, onset, repeat, fade and loudness comparisons of §4 |
| `speak5220-cli measure PROJECT --output DIR` | The quality measures and score of every phrase, and the loudness figures of §4.1 (sounding RMS, peak, clipping, wraps, BBC Micro and A-weighted RMS) | The driver over `libs5core` |
| `app/scripts/option_sweep.py` | Encodes a WAV directory with each option set of a table, measures it, optionally runs the round trip, one row per set | The tables of §4.2 and §4.4 |
| `app/scripts/phoneme-check.sh` (`phoneme_corpus.py`, `PhonemeCheck`) | Renders the phoneme test set (Piper, or macOS `say` with the voices of §3.2), encodes it and reports per file how its consonant and vowel came out | §3.2's TTS words, §4.2's onsets, §4.3's harmonic locks |

They reproduce §4's figures: the round trip run on this investigation's
WAVs gives its totals exactly, and the sweep gives the ARCTIC rows of
§4.4's table. Two of §4's figures were corrected on the way: the
fade-outs (§4.5) and the PHROM's own count. The round trip's own WAVs come
from the emulator, which runs one idle frame longer than the reference
synthesiser did, and that tail alone moves the defaults' round trip a
little (93.7 % agreement, 42 fade-outs); the plan records the baseline.

## 7. Reference values from Acorn PHROM A

For the harness's thresholds; 157 speech words, 2891 sounding frames.

| Quantity | Value |
|---|---|
| Frame kinds, of sounding frames | voiced 70.5 %, unvoiced 15.9 %, repeat 13.6 % |
| Repeat runs | 1: 229, 2: 27, 3: 10, 4+: 2 |
| Energy index median | voiced 12, unvoiced 4 (max 9), repeat 9 |
| Voiced K1 index | mostly 18–27 (−339…+287); a cluster of 87 frames at 8 (−478) |
| Unvoiced K1 index | 121 of 459 at 31 (+0.85), the rest 17–30 |
| Sounding RMS (codes) | median 21.8, quartiles 17.8–25.3 |
| Peak code | median 124, quartiles 101–154, max 297; 74 of 157 words over 127 |
| Clipped samples | mean 0.21 %, median 0 |
| RMS after the 279 Hz high-pass / A-weighted | 17.1 / 14.5 |
| Band power (0–250, 250–500, 500–1k, 1–2k, 2–3k, 3–4k Hz), dB re total | −7.8, −6.8, −9.1, −10.1, −13.1, −17.2 |
| Pitch | periods 45–138 samples (5th–95th percentile), falling through most words |
| Words ending (frame before the stop) | voiced 69, unvoiced 48, repeat 31, silence 9; fade-outs 3 (4 frames) |
| Bits | 118,804 for the 157 words (3125 frames) |

## 8. Reproducing the measurements

From the repository root, after building the application and its tests
(BUILD.md):

```bash
nix develop --command app/scripts/phrom-check.sh            # §3.1, §4.1-§4.5 on PHROM A
nix develop --command python3 app/scripts/option_sweep.py \
    --input local/arctic-project/generated-corpus --phrom   # §4.2, §4.4
nix develop --command app/scripts/phoneme-check.sh --engine say   # §3.2, §4.2-§4.3
```

`SPEAK5220_PHROM_OPTIONS` and `SPEAK5220_PHONEME_OPTIONS` (as
`repeat_lambda=300,voicing_threshold=0.5`) set the options of the round
trip and the phoneme check. Options the application fixes (`peakLimit`)
were moved only in the Python reference encoder (in git history, commit
`6abe9ed`):
`tms5220enc.encodeDetailed(x, 8000, targetRms=24, window=256,
voicingEvidence=5, levelAgc=1.25, voicingThreshold=0.4, peakLimit=160)`.
