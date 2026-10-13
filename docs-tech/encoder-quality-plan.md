# Encoder quality: implementation plan

The phased work that follows from the findings and recommendations of
[encoder-quality-investigation.md](encoder-quality-investigation.md)
(its §5 for the changes C1–C7, §6 for the harness, §7 for the PHROM A
reference values). The encoder's design is in
[HLD §8](design/high-level-design.md#8-the-encoder) and the quality
measures in [HLD §9](design/high-level-design.md#9-quality-measurement);
the rules for tests, reference data and documentation are in
[AGENTS.md](../AGENTS.md) §3 and §9 and [TESTING.md](../TESTING.md).

## Ground rules

- **Measure first.** Phase 1 builds the harness; every later phase is
  accepted by its figures, never by ear alone. A listening check closes
  each phase, as in HLD §9.
- **Two kinds of change.** A changed *default* (Phase 2) alters no
  reference data, since `app/golden/encoder/` is compared with the
  reference's own defaults written out. A changed *algorithm* (Phases 3–5)
  changes the frames of the reference option sets: it needs the user's
  agreement first, a regeneration of `app/golden/encoder/` and
  `app/golden/quality/` in its own commit recording the quality before and
  after, and a new `kEncoderVersion` so cached encodings are redone
  (AGENTS.md §3.1; HLD §8, "Results"). Phases 3–5 are each one such
  regeneration.
- **Documentation in the same task**: `docs/application/reference/encoder-options.md`
  and `docs/application/user-manual/encoder.md` for every option or default
  that changes, the HLD for every stage that changes, with the numbers that
  justified it (as HLD §9 records today's defaults).
- **Determinism and tests**: unit tests over mocked stages for every new
  rule; functional tests only where the corpus, the PHROM audio or the
  real synthesiser is needed (TESTING.md).

## Phase 1: The diagnostic harness

Depends on: nothing. Makes the investigation's scratch measurements part
of the repository.

1. **PHROM round-trip corpus.** `app/scripts/phrom_roundtrip.py`, run by
   hand like `generate_corpus.py`: reads a PROM manifest and image
   (`investigation/proms/`, the Acorn image in
   `investigation/firmware/src/romdata_acorn.h`), parses each word with the
   frame format of speak5220 datasheet §8.1, and writes `NAME.frames.yaml`,
   `NAME.bin` (the word's bytes) and `NAME.wav` into `--output`, with
   `index.tsv`. The WAV comes from the application: `speak5220-cli synth
   FRAMES.yaml --output WAV`, the emulator's VSM path, I/O codes × 64.
   - **Acceptance:** 165 words of PHROM A (157 speech words); each word's
     frames re-pack to the image's bytes (the round trip of datasheet
     §12.11). The bench captures cannot serve as a check: their takes
     differ from each other and from a render from power-up even for words
     with no unvoiced frame, at any lead, so the emulator's agreement with
     the chip stays the business of `app/golden/emulator/`.
2. **Round-trip report.** `PhromRoundTripCheck.DISABLED_WritesTheReport`
   (`app/tests/functional/encoder/phrom_roundtrip_test.cpp`), run by
   `app/scripts/phrom-check.sh`, driven by `SPEAK5220_PHROM_DIR` and
   `SPEAK5220_PHROM_REPORT`: checks each word's frames against its bytes
   with `FrameCodec`, encodes each speech word's WAV with the defaults (or
   `SPEAK5220_PHROM_OPTIONS`) and compares with its frames after aligning
   them by output level. Per word and in total: voicing agreement split
   into inside / onset / offset / unvoiced-made-voiced; onset and offset
   lag histograms; energy index difference per voicing; repeats and run
   lengths; filter-shape RMS dB per voicing; pitch difference in
   semitones; sounding RMS, peak, clipped samples, wraps, BBC Micro and
   A-weighted RMS; fade-outs. The comparison is
   `core/quality/frame_track_comparison` (HLD §9, "Diagnostics"), with
   unit tests on hand-made frame pairs.
   - **Acceptance:** on the investigation's WAVs (the reference
     synthesiser's output) the defaults reproduce its totals exactly
     (§4.1–4.5: 2737 frames, agreement 93.9 %, 90 / 44 / 14 / 20, repeats
     1164, energy +0.35 / +0.80, shape 2.11 dB, 92,192 bits). Fade-outs
     are 37 (43 frames; the PHROM's 3, 4 frames): §4.5's "43 fade-outs"
     counted every frame of a two-frame fade as a fade-out.
3. **Measures for any encoding.** `speak5220-cli measure PROJECT --output
   DIR` writes `DIR/measures.tsv`, one row per phrase: the figures of
   `QualityMeasurement` and `ScoreQuality()` against the phrase's
   preprocessed source, and the loudness figures of
   `core/quality/loudness` (sounding RMS in codes, peak code, share of
   samples beyond ±127, wrapped samples, RMS after a 279 Hz first-order
   high-pass, A-weighted RMS), with unit tests. `speak5220-cli new
   --option KEY=VALUE` sets a project's default options. Documented in
   `cli-options.md` and `cli.md`.
   - **Acceptance:** on `local/arctic-project/generated-corpus/` the
     `app-default` rows of `MeasuresCorpusCheck` are reproduced to ten
     digits; PHROM A's words give the §7 reference values (RMS 21.8, peak
     124, A-weighted 14.5, clipped 0.21 %) in the round trip.
4. **Sweep driver.** `app/scripts/option_sweep.py`: encodes a directory
   with each option set of a table (`app/scripts/sweeps/encoder-quality.tsv`
   by default; options as `OptionsCorpusCheck` takes them) through the CLI,
   measures it, with `--phrom` runs the round trip, and prints one row per
   set (median and mean score, components, envelope distortion, GPE, VDE,
   ESTOI, bytes, loudness, round-trip agreement, repeats and fade-outs).
   Phases 2–5 are judged with it.
   - **Acceptance:** reproduces the ARCTIC rows of the investigation's §4.4
     table for its six option sets.
5. **Phoneme test set.** `app/scripts/phoneme_prompts.csv` (words and
   short sentences isolating S, SH, CH, K, T and F beside bright vowels,
   and the investigation's TTS prompts) rendered by
   `app/scripts/phoneme_corpus.py`, modelled on
   `investigation/tools/ttsgen.py`: with Piper and four of its pinned
   voices (Linux), or with macOS's `say` and the investigation's four
   voices, the files' hashes recorded in `index.tsv`, into
   `local/phoneme-corpus/ENGINE/`. `PhonemeCheck.DISABLED_WritesTheReport`,
   run by `app/scripts/phoneme-check.sh`, reports per file: score, GPE and
   VDE; the consonant's frames (before the first or after the last frame
   REAPER finds voiced in the source) and how many the encoding voices;
   their K1; the consonant's level against the vowel in source and
   encoding; loud unvoiced frames before voicing; fade frames; the share of
   repeats in voiced sound.
   - **Acceptance:** with `say`, the investigation's artefacts appear as
     rows: the 100 % GPE words (`six` Daniel, `church` Eddy, `cat` Flo),
     unvoiced frames at energy 8 or more before voicing, fade frames after
     final S and CH.

**Baseline for the later phases.** The defaults, from `option_sweep.py
--input local/arctic-project/generated-corpus --phrom` and `phoneme-check.sh
--engine say`:

| Measure | Defaults |
|---|---|
| ARCTIC (33): score median / mean | 0.679 / 0.682 |
| ARCTIC: envelope distortion, GPE, VDE, ESTOI | 3.10 dB, 5.7 %, 13.9 %, 0.746 |
| ARCTIC: bytes; sounding RMS, peak, BBC Micro RMS, A-weighted RMS | 16,857; 20.0, 119, 11.6, 8.4 |
| Round trip: agreement; inside / onset / offset / unvoiced-made-voiced | 93.7 %; 94 / 43 / 19 / 18 |
| Round trip: repeats; fade-outs (frames); voiced shape error | 1184 of 3011; 42 (49); 2.17 dB |
| Round trip: sounding RMS, peak, A-weighted (PHROM / ours) | 21.8 / 20.2, 124 / 109, 14.5 / 13.6 |
| Phoneme set (`say`): loud onset frames; files over 20 % GPE | 18; 29 of 152 |

The round trip's WAVs are the emulator's output, which runs one idle
frame (200 samples of code −1) longer than the reference synthesiser's
that the investigation encoded; that tail alone moves the defaults'
round trip from 93.9 % to 93.7 % agreement and from 37 to 42 fade-outs.
The encoder's sensitivity to a trailing idle tail is worth a look in
Phase 4.

## Phase 2: Defaults that need no reference data

Depends on: Phase 1. Changes C3 and C6, and the `level_agc` part of C2.
Done 2026-10-13; the sweeps and the reasons are in HLD §9, "The voicing
threshold and the repeat trade-off, from the PHROM round trip".

1. **`voicing_threshold`.** Swept from 0.4 to 0.7 over the corpus of 243
   (`option_sweep.py`, the measures `OptionsCorpusCheck` gives) with the
   round trip, the TTS set and the phoneme set, alone and with each
   `repeat_lambda` of task 2, since the two interact. Chosen with the
   user: **0.6**. 0.5, expected, missed the inside misses (33).
   - **Acceptance:** round-trip voicing agreement 97.2 % (≥ 96 %) with
     17 voiced-made-unvoiced misses inside runs (≤ 30); corpus median
     0.730 (today's was 0.722, not the 0.718 written here before: that
     was the median before the voicing smoothing), mean 0.712 to 0.732;
     corpus median VDE 8.4 % to 9.0 %. The bound of 16 % median VDE, which
     was ARCTIC's figure at 0.5, is not met on ARCTIC (18.4 %; 0.5 gives
     16.0 %) and was relaxed with the user: VDE is judged against a
     tracker's voicing of the source, the round trip against the ground
     truth. HLD §9 and `encoder-options.md` record the figures and the
     new default.
2. **`repeat_lambda` 300** (D1). Swept 70, 140, 200 and 300 at threshold
   0.6 over the corpus, the TTS set and the round trip; the default
   became 300, and the slider's typical range 0 to 600.
   - **Acceptance:** round-trip repeats 395 (13 % of sounding frames;
     ≤ 20 %); TTS median score 0.747 (≥ 0.72; 0.705 today on the
     phoneme set's render of the TTS set, 0.707 on the investigation's);
     corpus size +20.1 % (101,566 to 122,016 bytes; ARCTIC +20.4 %).
3. **`level_agc`.** 1 and 1.1 at the new defaults lower the round trip's
   unvoiced energy bias from +0.78 to +0.60 and +0.67, never to +0.5,
   and lower the corpus median to 0.727 and 0.728 with more files down
   than up (85 against 53 at 1, the worst −0.26). Left at 1.25; the bias
   is Phase 3's, through the level setting.
   - **Acceptance:** not met (no value reaches +0.5), so the default is
     unchanged, as the task allowed.
4. **Documentation and fixtures.** `encoder-options.md`, `encoder.md`,
   `project-file.md`, HLD §8 "Options" and §9 updated; the fixture
   projects in `app/projects/` list every option, so their frames are
   unchanged; the default-settings tests follow the new defaults, and
   `ReferenceEncoderOptions()` writes out the reference's
   `repeat_lambda` 70. `option_sweep.py --only` takes the TTS set
   (`app/scripts/sweeps/investigation-tts.txt`) out of the phoneme set.
   - **Acceptance:** unit and GUI tests pass (1088); `mkdocs build
     --strict` passes; the corpus check passes unchanged (972 encodings,
     all identical to the reference's).

**Baseline for the later phases.** The defaults after Phase 2, by the
same commands as Phase 1's baseline:

| Measure | Phase 1 defaults | Phase 2 defaults |
|---|---|---|
| ARCTIC (33): score median / mean | 0.679 / 0.682 | 0.698 / 0.695 |
| ARCTIC: envelope distortion, GPE, VDE, ESTOI | 3.10 dB, 5.7 %, 13.9 %, 0.746 | 2.97 dB, 5.5 %, 18.4 %, 0.764 |
| ARCTIC: bytes; sounding RMS, peak, BBC Micro RMS, A-weighted RMS | 16,857; 20.0, 119, 11.6, 8.4 | 20,298; 19.6, 118, 11.6, 8.5 |
| Corpus (243): score median / mean; bytes | 0.722 / 0.712; 101,566 | 0.730 / 0.732; 122,016 |
| TTS (64): score median / mean | 0.705 / 0.685 | 0.747 / 0.726 |
| Round trip: agreement; inside / onset / offset / unvoiced-made-voiced | 93.7 %; 94 / 43 / 19 / 18 | 97.2 %; 17 / 28 / 7 / 26 |
| Round trip: repeats; fade-outs; voiced shape error; energy bias voiced / unvoiced | 1184; 42; 2.17 dB; +0.37 / +0.79 | 395; 25; 1.98 dB; +0.36 / +0.78 |
| Round trip: sounding RMS, peak, A-weighted (PHROM / ours) | 21.8 / 20.2, 124 / 109, 14.5 / 13.6 | 21.8 / 20.2, 124 / 109, 14.5 / 13.4 |
| Phoneme set (`say`): loud onset frames; files over 20 % GPE; consonant frames voiced | 18; 29 of 152; 69 of 553 | 13; 23 of 152; 81 of 553 |

## Phase 3: Level and loudness

Depends on: Phase 2. Changes C1 and the rest of C2. One regeneration of
the reference data.

1. **Peak limit with a clip budget.** In `LevelSetter::SetLevels()` the
   limiter's criterion becomes: no lattice wrap (±512, datasheet §12.3),
   and the share of samples beyond ±127 within a budget (1 in 1000, the
   PHROM's 0.21 % mean), in place of the hard `kPeakLimit` 120. The
   budget and a ceiling (about 160) are constants (D2). Unit tests over a
   mocked synthesis.
   - **Acceptance:** on ARCTIC the median sounding RMS is within 1 dB of
     the PHROM's 21.8 with `target_rms` 24; clipped samples ≤ 0.3 % in
     every file; no wraps; limited frames fall by more than half; the
     corpus median score within 0.005 of Phase 2's.
2. **Loudness-weighted level.** The frame level the encoder aims at
   (`FrameAnalyser::Features()`) and the level it measures of its output
   (`MeasureOutputLevels()`) are taken after the same 279 Hz first-order
   high-pass, so bass does not count as loudness. Unit tests: a 100 Hz
   tone and a 1 kHz tone of equal RMS get the energies their weighted
   levels imply.
   - **Acceptance:** ARCTIC encodings' A-weighted RMS within 1.5 dB of the
     PHROM's 14.5 with the *BBC Micro* preset, and within 3 dB without it;
     corpus score within 0.005.
3. **`target_rms`.** With tasks 1–2 in, sweep `target_rms` 24, 27 and 30
   on loudness, clipped samples and score, and set the default (expected
   30). The *BBC Micro* preset stays off by default (D3); the sweep records
   the loudness with and without it for the user manual.
   - **Acceptance:** the chosen default reaches task 2's loudness target
     on ARCTIC and TTS; the measures CLI reports it; `encoder.md` tells
     users of a BBC Micro to choose the preset and why.
4. **Reference data, version and documentation.** Regenerate
   `app/golden/encoder/` and `app/golden/quality/` in their own commit with
   the before-and-after quality table; bump `kEncoderVersion`; update HLD
   §8 ("Fixed options", the limiter), §9, `encoder-options.md`, and
   `encoder.md`; add the loudness figures to the corpus check's
   `measures.tsv`.
   - **Acceptance:** `corpus-check.sh` passes against the new reference;
     a listening check of five loud and five quiet files confirms no
     audible clipping.

## Phase 4: Voicing onsets and fades

Depends on: Phase 3. Changes C4 and C7. One regeneration of the reference
data.

1. **Onset rule.** `VoicingClassifier::Classify()` keeps the centred
   majority of hops −1…+1 and, when the next frame's centre is voiced,
   also votes hops 0…+4; the frame is voiced if either says so. The
   period of a frame voiced by the forward vote is the median of the
   voiced hops that voted. Unit tests with hand-made pitch tracks for an
   onset inside the frame, an offset, and an island.
   - **Acceptance:** round trip: onsets on time ≥ 80 of 106, late by one
     frame ≤ 15, offsets late unchanged (≤ 65); unvoiced-made-voiced ≤ 26;
     corpus score within 0.005.
2. **Fades only where needed.** `AddFades()` appends a fade only after an
   unvoiced frame of energy index 6 or more that precedes silence or the
   stop, and then one frame at a third of its energy; quieter endings end
   as the PHROM's do. Unit tests for both branches and for the K5–K10
   guard that follows.
   - **Acceptance:** round-trip fade-outs ≤ 10 of 157 words; the phoneme
     report shows no fade after a final S or CH whose own frames already
     decay; corpus score within 0.005.
3. **Reference data, version and documentation** as Phase 3 task 4; HLD
   §8's description of the voicing classifier and of `frame_endings`
   updated.
   - **Acceptance:** as Phase 3 task 4, with a listening check of ten
     words with final fricatives and ten with vowel onsets after S or K.

## Phase 5: A pitch tracker that cannot lock on a harmonic

Depends on: Phase 4. Change C5. One regeneration of the reference data.

1. **Prototype and decide.** Two candidates, built behind the
   `IPitchTracker` interface so each is measured by the Phase 1 harness
   on the TTS set, the phoneme set, ARCTIC and the round trip:
   (a) REAPER (`quality::SptkPitchEstimator`, `PitchAlgorithm::kReaper`,
   50–400 Hz every 10 ms) gives voicing and F0, and the present YIN
   refines the period at 5 ms inside REAPER's voiced stretches;
   (b) the present tracker with a two-pass prior (the median period of a
   first Viterbi path without the prior, weighted by level), a harmonic
   test on the full-band normalised autocorrelation at 2×, 3× and 4× the
   candidate lag, and the subharmonic rule reconsidered. REAPER is taken
   unless it slows Optimise by more than about a third (D4); encoding time
   from `benchmark.sh` (AGENTS.md §5).
   - **Acceptance:** a comparison table: GPE and F0 RMSE per set, round-
     trip pitch agreement, voicing agreement, encoding time per second of
     speech.
2. **Implement the chosen tracker.** As the `IPitchTracker` the encoder
   uses, deterministic whatever the thread count, with unit tests on
   synthetic pulse trains (16 to 160 samples), a pulse train with a
   boosted 4th harmonic (the `six` case), and a glide of an octave over
   100 ms.
   - **Acceptance:** REAPER GPE of the default encoding against the source
     under 20 % on every TTS file (10 of 64 fail today) and ≤ 5 % median
     on ARCTIC; round-trip pitch mean |difference| ≤ 0.4 semitones;
     encoding time ≤ 0.02 s per second of speech on one core (0.011
     today).
3. **Reference data, version and documentation** as Phase 3 task 4; HLD
   §8's pitch stage rewritten; `pitch_tracker` names what it re-implements
   or replaces (AGENTS.md §4.3.6).
   - **Acceptance:** as Phase 3 task 4, with a listening check of the
     words that failed before (`six`, `cat`, `church`, `science`, `seven`,
     `sixty six`).

## Decisions

Agreed with the user on the investigation's figures.

| Decision | Options considered | Decided |
|---|---|---|
| D1. Repeat rate (Phase 2 task 2) | `repeat_lambda` 300 (the PHROM's rate, +17–21 % bytes) or 140 (+11–12 %); a spectral-distance cost in `SegmentCost()` | 300. A spectral-distance cost is considered after Phase 5, as an algorithm change with its own reference-data regeneration |
| D2. Peak limit (Phase 3 task 1) | A constant clip budget and ceiling, or `peak_limit` back as an advanced option | Constants. An option only if listening on hardware shows material clipping audibly within the budget |
| D3. Default shaping (Phase 3 task 3) | *BBC Micro* preset on by default for new projects, or off | Off by default; the loudness-weighted level (Phase 3 task 2) closes part of the gap for every target, and the preset stays the user's choice for a small speaker. Revisited only if listening on a BBC Micro after Phase 3 still finds encodings quiet |
| D4. Pitch tracker (Phase 5 task 1) | REAPER-led, or the present tracker made robust | Prototype both behind `IPitchTracker`; REAPER unless it slows Optimise by more than about a third |
| D5. Reference data | Regenerate once per phase (3, 4, 5), or once after Phase 5 | Once per phase, so each change's quality is recorded on its own commit |
| D6. Voicing threshold (Phase 2 task 1; 2026-10-13) | 0.5 (expected), 0.55 (the least meeting the round-trip bounds) or 0.6 | 0.6: the best corpus mean and fewest Poor files, 97.2 % round-trip agreement; ARCTIC's median VDE (18.4 %) over the 16 % bound accepted, as VDE is against a tracker's voicing of the source |

## Risks

- The corpus score cannot hear loudness, onset bursts, harmonic locks or
  hiss tails; the round trip, the GPE on TTS and the listening checks are
  the acceptance for those, and a change must not be dropped because the
  score alone does not move.
- Raising the repeat threshold and the peak ceiling grows encodings; the
  PHROM budget per phrase (plan §4.1) and the BBC disc export need
  re-checking with `run-projects.sh`.
- REAPER in the encoder adds its cost per encoding and to Optimise, which
  encodes hundreds of candidates; Phase 5 task 1 measures it before the
  choice.
- A changed frame timing of voicing (Phase 4) may interact with the quiet
  pauses and the K5–K10 guard; the round trip and the pause tests cover
  both.
