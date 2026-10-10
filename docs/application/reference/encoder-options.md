# Encoder options

The options that control how Speak5220 encodes a phrase's 8 kHz audio into
TMS5220 frames. The encoder reproduces the frames of the project's reference
encoder (`investigation/encoder/tms5220enc.py`) exactly, so these options
mean what they mean there; the method is described in
[How the encoder works](../../technical/encoder/how-it-works.md).

!!! note
    The encoder is built, but nothing reaches it yet: the encoder panel and
    the project file, where these options will be set, come later. The keys
    below are the names they will use.

| Key | Meaning | Type | Default | Range |
|---|---|---|---|---|
| `targetRms` | RMS of the output over sounding frames, in DAC codes | Number | 20 | 1 to 100 |
| `targetPeak` | If set, scale so the loudest output peak reaches this many codes, instead of using `targetRms` | Number, or unset | Unset | 1 to 127 |
| `peakLimit` | No output code goes beyond this (the DAC's range is ±127) | Number | 120 | 1 to 127 |
| `repeats` | Send a frame whose coefficients barely change as an 11-bit repeat of the frame before | Yes or no | Yes | |
| `repeatLambda` | Bits a run of repeats may save per dB of extra prediction error; lower gives fewer bits | Number | 70 | 0 to 1000 |
| `gateDb` | Frames this many dB below the loudest frame become silence | Number | 45 | 10 to 90 |
| `preEmphasis` | Pre-emphasis for the voiced analysis: `chirp` whitens the chip's excitation pulse; a number μ uses 1 − μz⁻¹ | `chirp` or a number | `chirp` | μ from 0 to 0.99 |
| `window` | Length of the analysis window, in samples at 8 kHz | Whole number | 240 (30 ms) | 80 to 480 |
| `lagWindowHz` | Bandwidth of the Gaussian lag window, in Hz, which widens the formant peaks | Number | 60 | 0 to 400 |
| `voicingThreshold` | Periodicity level at which voiced and unvoiced cost the same; higher gives more voiced frames | Number | 0.3 | 0 to 1 |
| `voicingEvidence` | Weight of spectral tilt and zero crossings in the voicing decision; 0 uses periodicity alone | Number | 4 | 0 to 20 |
| `iterations` | Closed-loop passes that adjust each frame's energy against the synthesised output | Whole number | 5 | 1 to 20 |
| `fadeUnvoiced` | Fade hiss out before a silence or the end (datasheet §12.2) | Yes or no | Yes | |
| `levelAgc` | Compression ratio of a slow (0.75 s) loudness envelope, which brings quiet words up; 1 is off | Number | 1 | 1 to 10 |
| `compression` | Per-frame level compression ratio around the median level; 1 is off | Number | 1 | 1 to 10 |

A value outside its range is refused, with a message naming the option.
The encoder takes audio at 8000 Hz only; importing a phrase resamples it.
