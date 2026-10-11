# Encoder options

The options that control how Speak5220 encodes a phrase's 8 kHz audio into
TMS5220 frames. The encoder reproduces the frames of the project's reference
encoder (`investigation/encoder/tms5220enc.py`) exactly, so these options
mean what they mean there; the method is described in
[How the encoder works](../../technical/encoder/how-it-works.md). The
exceptions are `squelch_db` and `quiet_pauses`, which the reference does
not have: with the squelch off (the default) and quiet pauses off, the
encoder gives the reference's frames.

Options are set in the [project file](project-file.md): `default_encoder`
holds the project's values, and a phrase's `encoder` mapping holds only the
options it sets for itself. In the main window they are set in the
[encoder panel](../user-manual/encoder.md#the-settings), for a phrase or
for the project, and [Optimise](../user-manual/encoder.md#optimise)
searches them, all but `squelch_db` and `quiet_pauses`.

| Key | Reference encoder | Meaning | Type | Default | Range | Slider |
|---|---|---|---|---|---|---|
| `target_rms` | `targetRms` | RMS of the output over sounding frames, in DAC codes | Number | 20 | 1 to 100 | 5 to 50 codes, steps of 0.5 |
| `target_peak` | `targetPeak` | If set, scale so the loudest output peak reaches this many codes, instead of using `target_rms` | Number, or `null` (unset) | Unset | 1 to 127 | 30 to 127 codes, steps of 1 |
| `peak_limit` | `peakLimit` | No output code goes beyond this (the DAC's range is ±127) | Number | 120 | 1 to 127 | 60 to 127 codes, steps of 1 |
| `repeats` | `repeats` | Send a frame whose coefficients barely change as an 11-bit repeat of the frame before | `true` or `false` | `true` | | |
| `repeat_lambda` | `repeatLambda` | Bits a run of repeats may save per dB of extra prediction error; lower gives fewer bits | Number | 70 | 0 to 1000 | 0 to 300, steps of 5 |
| `gate_db` | `gateDb` | Frames this many dB below the loudest frame become silence | Number | 45 | 10 to 90 | 20 to 70 dB, steps of 0.5 |
| `squelch_db` | none (`squelchDb`) | If set, stretches of the recording this many dB below its loudest sound (breaths, room noise) are made silent before encoding, and the quality is measured against the squelched recording ([the squelch](../user-manual/encoder.md#the-squelch)) | Number, or `null` (unset) | Unset | 6 to 80 | 12 to 50 dB, steps of 1 |
| `pre_emphasis` | `preEmphasis` | Pre-emphasis for the voiced analysis: `"chirp"` whitens the chip's excitation pulse; a number μ uses 1 − μz⁻¹ | `"chirp"` or a number | `"chirp"` | μ from 0 to 0.99 | μ from 0.5 to 0.99, steps of 0.01 |
| `window` | `window` | Length of the analysis window, in samples at 8 kHz | Whole number | 240 (30 ms) | 80 to 480 | 120 to 400 (15 to 50 ms), steps of 8 |
| `lag_window_hz` | `lagWindowHz` | Bandwidth of the Gaussian lag window, in Hz, which widens the formant peaks | Number | 60 | 0 to 400 | 0 to 200 Hz, steps of 5 |
| `voicing_threshold` | `voicingThreshold` | Periodicity level at which voiced and unvoiced cost the same; higher gives more voiced frames | Number | 0.3 | 0 to 1 | 0.05 to 0.7, steps of 0.01 |
| `voicing_evidence` | `voicingEvidence` | Weight of spectral tilt and zero crossings in the voicing decision; 0 uses periodicity alone | Number | 4 | 0 to 20 | 0 to 12, steps of 0.25 |
| `iterations` | `iterations` | Closed-loop passes that adjust each frame's energy against the synthesised output | Whole number | 5 | 1 to 20 | 1 to 10 |
| `fade_unvoiced` | `fadeUnvoiced` | Fade hiss out before a silence or the end (datasheet §12.2) | `true` or `false` | `true` | | |
| `quiet_pauses` | none (`quietPauses`) | Start each pause of three or more silence frames with one quiet frame that settles the chip's filter, so the pause is silent instead of buzzing ([quiet pauses](../user-manual/encoder.md#quiet-pauses); datasheet §12.2) | `true` or `false` | `true` | | |
| `level_agc` | `levelAgc` | Compression ratio of a slow (0.75 s) loudness envelope, which brings quiet words up; 1 is off | Number | 1 | 1 to 10 | 1 (off) to 6, steps of 0.1 |
| `compression` | `compression` | Per-frame level compression ratio around the median level; 1 is off | Number | 1 | 1 to 10 | 1 (off) to 6, steps of 0.1 |

The key is the option's name in project files. The second column is its
name in the reference encoder and its documentation (`squelchDb` and
`quietPauses`, which the reference does not have, are named in its style). The last column is
what the [encoder panel](../user-manual/encoder.md#the-settings)'s
slider covers: where useful values lie. Any value in the full range can
be set in the project file, and the slider widens to show it.

Numbers are written with a `.` as the decimal point, whatever the
computer's language settings. A value outside its range, or of the wrong
type, is refused, with a message naming the option. The encoder takes audio
at 8000 Hz only; importing a phrase resamples it.
