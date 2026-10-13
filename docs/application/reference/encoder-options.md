# Encoder options

The options that control how Speak5220 makes a phrase's frames, in two
groups, as the [encoder panel](../user-manual/encoder.md#the-settings)
shows them:

- **[Preprocessing](#preprocessing)**: how the phrase's 8 kHz recording
  is prepared before it is encoded (`noise_gate_db`, `noise_gate_ms`,
  `bass_cut_hz`, `treble_tilt`). The encoding aims at the preprocessed
  recording, and its
  [quality](../user-manual/encoder.md#the-quality-rating) is measured
  against it. All are off by default.
- **[Encoder](#encoder)**: how the preprocessed recording is encoded into
  TMS5220 frames.

The encoder reproduces the frames of the project's reference encoder
(`investigation/encoder/tms5220enc.py`) exactly, so its options mean
what they mean there; the method is described in
[How the encoder works](../../technical/encoder/how-it-works.md). The
exceptions are `resonance_hold_db` and `voicing_min_run`, and the
preprocessing, which the reference does not have. Five defaults differ
from the reference's. Four give the best typical
[quality rating](../user-manual/encoder.md#the-quality-rating) over a
corpus of 243 recordings: `target_rms` 24 (the reference's is 20),
`window` 256 (240), `voicing_evidence` 5 (4) and `level_agc` 1.25 (1).
The fifth, `voicing_threshold` 0.4 (0.3), with `voicing_min_run` 2,
brings the encoder's voicing nearer that of Acorn's own PHROM phrases,
and raises the typical rating over the corpus too.

The reference's other options are fixed at its defaults (below). The
encoder also always gives pauses a quiet frame
([quiet pauses](../user-manual/encoder.md#quiet-pauses)), which the
reference does not. So with no preprocessing, the reference's values,
and the options the reference does not have off (`resonance_hold_db`
unset, `voicing_min_run` 1), the encoder gives the reference's frames
with their pauses quietened.

Options are set in the [project file](project-file.md):
`default_preprocess` and `default_encoder` hold the project's values,
every option written out, and a phrase's `preprocess` and `encoder`
mappings hold only the options it sets for itself. So a project keeps
its settings, and its encodings, when a later version of Speak5220
changes a default. In the main window they are set in the
[encoder panel](../user-manual/encoder.md#the-settings), for a phrase or
for the project, and [Optimise](../user-manual/encoder.md#optimise)
searches the encoder's, all but `resonance_hold_db` and
`voicing_min_run`. It never changes the preprocessing, since that
changes what the quality is measured against.

In both tables the key is the option's name in project files. The second
column is its name in the reference encoder and its documentation (the
options the reference does not have, such as `noiseGateDb`, are named in
its style). The last column is what the encoder panel's slider covers:
where useful values lie. Any value in the full range can be set in the
project file, and the slider widens to show it.

## Preprocessing

The panel always shows these four, with a chooser of
[presets](../user-manual/encoder.md#presets): **None** (all off) and
**BBC Micro** (`bass_cut_hz` 200, `treble_tilt` 0.4). Neither preset
gates, and `noise_gate_ms` does not count towards a match while
`noise_gate_db` is unset. The noise gate comes first, then the bass cut,
then the treble tilt. The encoder finds the
pitch and the voicing in the recording before the bass cut and treble
tilt, and everything else in the preprocessed recording.

| Key | Reference encoder | Meaning | Type | Default | Range | Slider |
|---|---|---|---|---|---|---|
| `noise_gate_db` | none (`noiseGateDb`) | If set, sound more than this many dB below the recording's loudest (breaths, room noise) is turned down by 3 dB for each dB under the level, and made silent from about 27 dB under it ([the noise gate](../user-manual/encoder.md#the-noise-gate)) | Number, or `null` (unset) | Unset | 6 to 80 | 12 to 60 dB, steps of 1 |
| `noise_gate_ms` | none (`noiseGateMs`) | How long the noise gate takes to close, from open to silent, after the sound falls under its level; it opens in a quarter of this, ending as the sound rises. No effect while `noise_gate_db` is unset | Number | 100 | 5 to 2000 | 20 to 500 ms, steps of 10 |
| `bass_cut_hz` | none (`bassCutHz`) | If set, the bass is cut below this frequency: a zero-phase order-2 Butterworth high-pass, −6 dB at the cut and 24 dB per octave below it ([bass cut and treble tilt](../user-manual/encoder.md#bass-cut-and-treble-tilt)) | Number, or `null` (unset) | Unset | 50 to 600 | 100 to 400 Hz, steps of 10 |
| `treble_tilt` | none (`trebleTilt`) | The treble is lifted by 1 − μz⁻¹; 0 is off | Number | 0 | 0 to 0.95 | 0 to 0.9, steps of 0.05 |

## Encoder

The encoder panel shows four of these at first, the **basic** options:
`target_rms`, `repeat_lambda`, `pre_emphasis` and `voicing_threshold`.
The rest, marked *Advanced* below, appear when the panel's advanced
settings are shown ([the settings](../user-manual/encoder.md#the-settings)).
Project files and the command line treat the two groups alike.

| Key | Reference encoder | Meaning | Type | Default | Range | Slider |
|---|---|---|---|---|---|---|
| `target_rms` | `targetRms` | RMS of the output over sounding frames, in DAC codes | Number | 24 | 1 to 100 | 5 to 50 codes, steps of 0.5 |
| `repeat_lambda` | `repeatLambda` | Bits a run of repeats may save per dB of extra prediction error; lower gives fewer bits | Number | 70 | 0 to 1000 | 0 to 300, steps of 5 |
| `gate_db` (*Advanced*) | `gateDb` | Frames this many dB below the loudest frame become silence | Number | 45 | 10 to 90 | 20 to 70 dB, steps of 0.5 |
| `pre_emphasis` | `preEmphasis` | Pre-emphasis for the voiced analysis: `"chirp"` whitens the chip's excitation pulse; a number μ uses 1 − μz⁻¹ | `"chirp"` or a number | `"chirp"` | μ from 0 to 0.99 | μ from 0.5 to 0.99, steps of 0.01 |
| `window` (*Advanced*) | `window` | Length of the analysis window, in samples at 8 kHz | Whole number | 256 (32 ms) | 80 to 480 | 120 to 400 (15 to 50 ms), steps of 8 |
| `lag_window_hz` (*Advanced*) | `lagWindowHz` | Bandwidth of the Gaussian lag window, in Hz, which widens the formant peaks | Number | 60 | 0 to 400 | 0 to 200 Hz, steps of 5 |
| `resonance_hold_db` (*Advanced*) | none (`resonanceHoldDb`) | If set, where a fading voiced sound moves from a strongly resonant filter to one with more than this many dB less gain, the resonant filter is kept for one more frame, so the level does not drop suddenly with a pop ([the resonance hold](../user-manual/encoder.md#the-resonance-hold)) | Number, or `null` (unset) | 14 | 3 to 40 | 6 to 30 dB, steps of 0.5 |
| `voicing_threshold` | `voicingThreshold` | Periodicity level at which voiced and unvoiced cost the same; higher gives more voiced frames | Number | 0.4 | 0 to 1 | 0.05 to 0.7, steps of 0.01 |
| `voicing_evidence` (*Advanced*) | `voicingEvidence` | Weight of spectral tilt and zero crossings in the voicing decision; 0 uses periodicity alone | Number | 5 | 0 to 20 | 0 to 12, steps of 0.25 |
| `voicing_min_run` (*Advanced*) | none (`voicingMinRun`) | A run of voiced or unvoiced frames shorter than this, with runs of the other kind on both sides, takes its neighbours' voicing; 1 is off ([voicing smoothing](../user-manual/encoder.md#voicing-smoothing)) | Whole number | 2 | 1 to 8 | 1 to 6 frames, steps of 1 |
| `level_agc` (*Advanced*) | `levelAgc` | Compression ratio of a slow (0.75 s) loudness envelope, which brings quiet words up; 1 is off | Number | 1.25 | 1 to 10 | 1 (off) to 6, steps of 0.05 |

## Fixed settings

The reference encoder has seven more options, which Speak5220 fixes at
the values below. Changing them made no difference worth keeping over the
corpus of 243 recordings, or did what another option already does:

| Reference option | Fixed at | Why |
|---|---|---|
| `targetPeak` | Unset: the level is set by `target_rms` | A second way to set the loudness. A peak of 100 lowered the median score from 0.718 to 0.673 |
| `peakLimit` | 120 | A safety limit within the DAC's ±127 |
| `repeats` | On | `repeat_lambda` already sets how many repeat frames are used |
| `iterations` | 5 passes | 3 or 8 passes moved no file's score by more than 0.02 |
| `fadeUnvoiced` | On | Hiss fades out instead of stopping abruptly |
| `compression` | Off | It lowered the score on average, and badly on some recordings; `level_agc` brings quiet words up better |
| `quietPauses` (Speak5220's own) | On | Without it the chip buzzes faintly through every pause |

A project saved before these were fixed may still list them. It opens
as usual: the lines are kept, as a key Speak5220 does not know is kept
([project file](project-file.md#keys-and-files-from-newer-versions)), but
have no effect.

Numbers are written with a `.` as the decimal point, whatever the
computer's language settings. A value outside its range, or of the wrong
type, is refused, with a message naming the option. The encoder takes audio
at 8000 Hz only; importing a phrase resamples it.
