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
the reference data. Done 2026-10-13; the sweeps and the reasons are in HLD
§9, "Level and loudness, from the PHROM round trip", and the design in HLD
§8, "Level setting".

1. **Peak limit with a clip budget.** The limiter allows no lattice wrap,
   no frame peak over 160 (`kPeakCeiling`) and at most 1 sample in 1000
   beyond ±127 (`kClipBudget`), lowering the frames that clip most while
   the phrase is over the budget; constants (D2). Unit tests over the
   reference synthesiser, which is pure (the clip budget and ceiling at
   five gains).
   - **Acceptance** (with this change alone, `target_rms` 24 of level):
     ARCTIC median sounding RMS 21.9 (PHROM 21.8; ≤ 1 dB); worst file
     0.100 % clipped (≤ 0.3 %); no wraps; frames lowered 490 to 163
     (more than half fewer); corpus median 0.734 (Phase 2 0.730; within
     0.005 or better). Met.
2. **Loudness-weighted level.** `FrameFeatures::loudness`,
   `MeasureOutputLoudness()` and `GainOf()` with `loudness` take the level
   after `LoudnessHighPass()` (279 Hz, first order, the quality measures'
   `BbcHighPass()` now shares it). Unit tests: 100 Hz and 1 kHz tones of
   equal RMS get loudnesses the high-pass's gain implies, in the features
   and in the output, and the weighted open-loop gain of a flat filter.
   - **Acceptance:** corpus score within 0.005: met (median 0.742 at the
     default). ARCTIC A-weighted RMS within 1.5 dB of the PHROM's 14.5 with
     the *BBC Micro* preset and within 3 dB without: **not met** (12.4 and
     10.6 at the default; 13.2 and 11.0 at 24, which costs ARCTIC score).
     ARCTIC is darker than the PHROM and its pauses lower its whole-talk
     RMS; on the PHROM's own words the default matches the PHROM's
     loudness (A-weighted +0.0 dB), which was taken instead (D8).
3. **`target_rms`.** Now a loudness, so the plan's 24 to 30 (of level)
   no longer applied: swept 16, 18, 20, 24 (and 27, 30 on ARCTIC), with
   and without the preset. Chosen with the user: **20** (D8), the
   reference's number but of loudness. The preset stays off by default
   (D3); `encoder.md` ("The level") tells users of a BBC Micro to choose
   it and why.
   - **Acceptance:** the default reaches the PHROM's loudness on the
     PHROM's own words; the measures CLI reports the loudness;
     `encoder.md` explains the preset. Met, with task 2's ARCTIC bound
     relaxed (D8).
4. **Reference data, version and documentation.** The encoder reference
   data cannot be regenerated from the Python reference, so its frames and
   level-setting intermediates now come from the C++ encoder (D7):
   `encoder.py` runs the new by-hand writer `EncoderReferenceWriter`, the
   Python still writes the stages before. `app/golden/quality/` was
   regenerated from the new frames (with the flake's REAPER tool now
   built without fused multiply-add contraction, as SPTK is, so its
   tracks match on arm64). `kEncoderVersion` 1.3.0; the fixture projects
   with recordings re-encoded and their export hashes recorded again.
   HLD §8 ("Level setting", "Options"), §9, §13, `encoder-options.md`,
   `encoder.md`, `project-file.md`, `app/golden/README.md`, TESTING.md
   updated. The corpus check's `measures.tsv` loudness figures were
   already in `speak5220-cli measure` (Phase 1); the corpus check itself
   was left as it was.
   - **Acceptance:** `corpus-check.sh` passes against the new reference
     (972 of 972 identical; Optimise raises 240 of 243 files): met.
     Encoding takes 0.0079 s per second of speech on one core (Phase 2:
     0.0085). The listening check of five loud and five quiet files is
     the user's.

**Baseline for the later phases.** The defaults after Phases 3 to 5, by
the same commands:

| Measure | Phase 2 defaults | Phase 3 defaults | Phase 4 defaults | Phase 5 defaults |
|---|---|---|---|---|
| ARCTIC (33): score median / mean | 0.698 / 0.695 | 0.708 / 0.701 | 0.709 / 0.706 | 0.709 / 0.708 |
| ARCTIC: envelope distortion, GPE, VDE, ESTOI | 2.97 dB, 5.5 %, 18.4 %, 0.764 | 2.88 dB, 5.7 %, 18.9 %, 0.765 | 2.87 dB, 5.6 %, 20.4 %, 0.771 | 2.87 dB, 5.6 %, 19.6 %, 0.771 |
| ARCTIC: bytes; sounding RMS, peak, clipped (mean), BBC Micro RMS, A-weighted RMS | 20,298; 19.6, 118, 0, 11.6, 8.5 | 20,452; 24.7, 146, 0.082 %, 14.5, 10.6 | 20,771; 24.9, 147, 0.084 %, 14.5, 10.6 | 20,756; 24.9, 147, 0.084 %, 14.5, 10.6 |
| Corpus (243): score median / mean; bytes | 0.730 / 0.732; 122,016 | 0.742 / 0.742; 122,687 | 0.738 / 0.742; 124,477 | 0.736 / 0.741; 124,387 |
| TTS (64): score median / mean | 0.747 / 0.726 | 0.730 / 0.741 | 0.735 / 0.742 | 0.749 / 0.755 |
| Round trip: agreement; inside / onset / offset / unvoiced-made-voiced | 97.2 %; 17 / 28 / 7 / 26 | 97.2 %; 16 / 28 / 7 / 26 | 97.9 %; 10 / 14 / 6 / 27 | 98.1 %; 10 / 14 / 4 / 26 |
| Round trip: repeats; fade-outs; voiced shape error; energy bias voiced / unvoiced | 395; 25; 1.98 dB; +0.36 / +0.78 | 393; 23; 1.99 dB; +0.58 / +0.80 | 358; 4; 1.99 dB; +0.58 / +0.79 | 358; 4; 1.99 dB; +0.58 / +0.78 |
| Round trip: sounding RMS, peak, A-weighted (PHROM / ours) | 21.8 / 20.2, 124 / 109, 14.5 / 13.4 | 21.8 / 21.5, 124 / 126, 14.5 / 14.6 | 21.8 / 21.6, 124 / 126, 14.5 / 14.5 | 21.8 / 21.6, 124 / 126, 14.5 / 14.5 |
| Phoneme set (`say`): loud onset frames; files over 20 % GPE; consonant frames voiced | 13; 23 of 152; 81 of 553 | 17; 22 of 152; 81 of 553 | 10; 20 of 152; 99 of 553 | 8; 19 of 152; 99 of 553 |

## Phase 4: Voicing onsets and fades

Depends on: Phase 3. Changes C4 and C7. One regeneration of the reference
data. Done 2026-10-13; the figures are in HLD §9, "Onsets and fades, from
the PHROM round trip", and the design in HLD §8, "Onsets and fades".

1. **Onset rule.** `VoicingClassifier::Classify()` keeps the centred
   majority of hops −1…+1 (`CentredVoicing()`) and, when the next frame's
   centred vote is voiced, also votes hops 0…+4 (`VoiceOnsets()`); the
   frame is voiced if either says so, with the median period of the
   voiced hops that voted, and the octave fix (`CorrectOctaves()`)
   follows. Unit tests with hand-made pitch tracks for an onset inside the
   frame (and one too late in it), an offset, an island, and a frame
   voiced by the rule not voicing the one before.
   - **Acceptance:** round trip: onsets on time 80 of 106 (≥ 80; Phase 3
     72), late by one frame 0 (≤ 15; 8), offsets late by one 72
     (unchanged; the bound of 65 was already passed in Phase 3);
     unvoiced-made-voiced 27 (≤ 26: **not met** by one frame); corpus
     median 0.738 against 0.742 (within 0.005). The corpus VDE rises from
     9.1 % to 11.1 % and the pitch difference on the round trip from 0.40
     to 0.43 semitones, both from this rule alone (the fades alone leave
     them as they were). Whether to accept the miss is the user's.
2. **Fades only where needed.** `AddFades()` appends a fade only after an
   unvoiced frame of energy index 6 or more that precedes silence or the
   stop (`kFadeFromEnergy`), and then one frame at a third of its energy;
   quieter endings end as the PHROM's do. Unit tests for both branches,
   the threshold, a decay already in the frames, and the K5–K10 guard
   that follows.
   - **Acceptance:** round-trip fade-outs 4 of 157 words (≤ 10; Phase 3
     23); on the phoneme set 11 fade frames (45), of which one is a fade
     the encoder added, after a lone final SH at energy 6, and the rest
     decays coded from the speech: no fade after a final S or CH whose
     frames already decay. Corpus median 0.741 with this change alone
     (within 0.005). Met.
3. **Reference data, version and documentation** as Phase 3 task 4:
   `app/golden/encoder/` (the "encode" sections of the intermediates and
   the frames, from the C++ encoder; the Python's voicing in the
   intermediates is without the onset rule, and the intermediates test
   checks it against `CentredVoicing()` and `CorrectOctaves()`) and
   `app/golden/quality/` regenerated; `kEncoderVersion` 1.4.0; the fixture
   projects with recordings re-encoded and their export hashes recorded
   again. HLD §8 (the stage table, "Onsets and fades", "Options"), §9 and
   §13, `encoder-options.md` (`fadeUnvoiced`), `encoder.md` ("Vowel
   starts and word endings"), `app/golden/README.md`, TESTING.md and
   `encoder.py` updated.
   - **Acceptance:** `corpus-check.sh` passes against the new reference
     (972 of 972 identical; Optimise raises 242 of 243 files): met.
     Encoding takes 0.0080 s per second of speech on one core (Phase 3:
     0.0079). The listening check of ten words with final fricatives and
     ten with vowel onsets after S or K is the user's.

## Phase 5: A pitch tracker that cannot lock on a harmonic

Depends on: Phase 4. Change C5. One regeneration of the reference data.
Done 2026-10-13; the design is in HLD §8, "The pitch anchor", and the
figures in HLD §9, "The pitch anchor".

1. **Prototype and decide.** Five candidates were built behind
   `IPitchTracker` and measured on the TTS set, the phoneme set, ARCTIC,
   the corpus and the round trip: (a) REAPER's voicing and F0 with YIN
   refining the period; (b) the present tracker with a two-pass prior, a
   full-band harmonic test at 2×, 3× and 4× the lag and the subharmonic
   rule kept only where the full band agrees; and the present tracker
   with its prior held near an anchor's F0 where the anchor finds voicing,
   with REAPER, Harvest or RAPT as the anchor. At 8 kHz REAPER rates whole
   words unvoiced and drops an octave on low and high voices, and the GPE
   measure, which compares REAPER's tracks, inherits both; so each
   candidate was also judged by its coded GPE (each voiced frame's coded
   period against the source's F0 from REAPER and from Harvest) and by
   the voices' harmonics. (a) whispers whole words; (b) cannot move `six`
   (Daniel); REAPER as the anchor takes on REAPER's octave drops; Harvest
   as the anchor encodes at 0.052 s per second of speech, over the
   application's 0.05. RAPT as the anchor was chosen with the user (D9).
   - **Acceptance:** the comparison table, with GPE and F0 RMSE per set,
     round-trip pitch and voicing agreement and encoding time, is in HLD
     §9, "The pitch anchor". Met.
2. **Implement the chosen tracker.** `YinPitchTracker` with an
   `IPitchEstimator` anchor (`AnchorPeriods()`, `kAnchorPrior`,
   `kAnchorFree`); `MakeEncoder()` gives it `SptkPitchEstimator` with the
   new `PitchAlgorithm::kRapt`. Without an anchor the tracker is the
   reference's. Unit tests, with the anchor mocked: pulse trains of 16 to
   160 samples; a pulse train whose narrow first formant sits on its 4th
   harmonic, with its period wandering by 3 % (`six`), locked on the
   harmonic without the anchor and on the period with it; a glide of an
   octave over 100 ms; an anchor that fails or finds no voicing changes
   nothing; an anchor does not voice noise; `AnchorPeriods()`.
   Functional tests: RAPT on pulse trains and a vowel, the encoder coding
   the `six` signal at its period, and encodings on eight threads at once
   matching one.
   - **Acceptance:** REAPER GPE of the default encoding under 20 % on
     every TTS file: **not met**, 7 of 64 over (8 in Phase 4), none of
     them over 20 % by the coded GPE against Harvest. ARCTIC median GPE
     ≤ 5 %: **not met**, 5.6 % (5.6 %). Round-trip pitch mean |difference|
     ≤ 0.4 semitones: **not met**, 0.43 (0.43). These bounds count
     REAPER's own errors (HLD §9); whether to accept them is the user's.
     Encoding time ≤ 0.02 s per second of speech: met, 0.0084 (0.0081).
     Round trip 98.1 % agreement, unvoiced-made-voiced 26 (27).
3. **Reference data, version and documentation** as Phase 3 task 4:
   `app/golden/encoder/` (the frames and the "encode" sections from the
   C++ encoder; the pitch tracks in the intermediates are the Python's,
   without the anchor, and are checked against `YinPitchTracker` without
   one) and `app/golden/quality/` regenerated; `kEncoderVersion` 1.5.0;
   the fixture projects with recordings re-encoded and their export
   hashes recorded again. HLD §8 (the stage table, "The pitch anchor",
   "Performance"), §9 and §13, TESTING.md, `app/golden/README.md` and
   `encoder.py` updated.
   - **Acceptance:** `corpus-check.sh` passes against the new reference
     (972 of 972 identical; Optimise raises 242 of 243 files, mean gain
     0.058): met. The listening check of the words
     that failed before (`six`, `cat`, `church`, `science`, `seven`,
     `sixty six`) is the user's: `six` and `sixty six` (Daniel) now code
     at 107 and 116 Hz (471 and 182), and the others are unchanged.

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
| D7. Reference data after an algorithm change (Phase 3 task 4; 2026-10-13) | Advanced options whose old values reproduce the Python reference, leaving the data unchanged; changing the Python reference in `investigation/`; or the C++ encoder's own output as the reference | The C++ encoder's own frames and level-setting intermediates; the Python still checks the stages before the level setting. Phases 4 and 5 regenerate the same way |
| D8. `target_rms` (Phase 3 task 3; 2026-10-13) | 16 (closest energy indices on the round trip, but the PHROM's words 1 dB quieter than the PHROM), 18, 20 (the PHROM's loudness on its own words), 24 | 20. The ARCTIC A-weighted bounds of task 2 relaxed: they need 24 to 30, which lowers the ARCTIC score and leaves the consonants louder still |
| D9. Pitch tracker (Phase 5 task 1; 2026-10-13) | REAPER as the encoder's voicing and F0 (D4); the present tracker with a two-pass prior and a harmonic test; the present tracker held near an anchor's F0, the anchor REAPER, Harvest or RAPT | The present tracker held near RAPT's F0 where RAPT finds voicing, YIN keeping the voicing. REAPER loses whole words' voicing and drops an octave on low and high voices at 8 kHz; Harvest, first chosen, encodes at 0.052 s per second of speech, over the application's 0.05; RAPT is as right on the octave for no measurable time |

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
