# How the encoder works

The method is that of [§12.6](../datasheet/authoring.md), with the choices
below. Where the datasheet leaves a point open, [Implementation choices](choices.md) gives
the reasoning.

## Preparation and timing

- The input is resampled to 8 kHz if needed (polyphase, which also
  band-limits it). A 60 Hz high-pass (2nd-order Butterworth, zero-phase)
  removes DC and rumble.
- **Onset and offset.** These are the first and last 10 ms blocks above the
  gate (see [Repeat and silence decisions](#repeat-and-silence-decisions)).
  Leading and trailing quiet is dropped.
- **Frame centres.** Frame *j* describes the input at `onset + 22 + 200·j`
  samples. The chip makes the first frame's energy audible at output sample
  203: it is computed in PC 0 of IP 0 (sample 201) and used two samples later
  ([§8.5](../datasheet/frame-timing.md)). So output sample 203 is the input
  onset, and output sample 225 + 200·*j* corresponds to frame *j*'s centre.
  Sample 225 is the middle of the stretch where frame *j*'s values dominate.
  They are half-way reached at the end of IP 4 of its glide (0.498 of the way,
  §12.2), reached in IP 0, and half-way to the next frame's by IP 4 of the
  next glide. An envelope cross-correlation between the input and the
  synthesis puts 88 % of the files within ±5 ms of this alignment (5 ms is
  that check's resolution), and the rest within ±25 ms.

## Pre-emphasis

Voiced frames are analysed after the inverse of the chirp's spectrum, so that
the filter describes the speech spectrum divided by the chirp's. The
pre-filter is the order-4 LPC inverse of the chirp table (`chirpInverse()`):
[1, −0.938, 0.152, −0.104, −0.006]. Its first-order fit, 1 − 0.888z⁻¹, is
close to §12.3's `s(n) − 0.9·s(n−1)`. The order-4 filter also follows the
chirp's shape between 300 and 2000 Hz, where the first-order one leaves it
about 1 dB weak below 800 Hz and 1 dB strong above. Over the 243 test inputs
it halves the output's spectral tilt error (below 800 Hz −1.05 → −0.68 dB,
800–2000 Hz +0.74 → +0.42 dB) and improves the MCD by 0.03 dB, for no extra
bits. Order 8 was no better.
`preEmphasis=0.9` selects the first-order filter. Unvoiced frames, whose
excitation is flat, are analysed without pre-emphasis.

## Voicing and pitch

- **Candidates.** The signal is low-passed at 1 kHz. Every 5 ms, YIN's
  cumulative mean normalised difference function (CMNDF) is computed over a
  30 ms window, for lags 14–165 samples. The window is placed so that it is
  centred on the hop for a lag of 80, and within 5 ms of it for every lag in
  range. The five deepest local minima, with parabolic refinement, are the
  voiced candidates.
- **Path.** A Viterbi search over the candidates plus an unvoiced state picks
  the path. Its costs are:
  - each candidate's CMNDF value, × 10;
  - a pitch jump between hops, 12 per octave, so an octave jump costs as much
    as a CMNDF difference of 1.2;
  - a voicing switch, 4. The unvoiced state remembers the last voiced lag, so
    a short unvoiced gap (under 40 ms) cannot be used to change octave more
    cheaply than a jump;
  - a prior towards the utterance's median period: 5 per octave beyond
    ±0.6 octave. The median is taken from confident hops, by YIN's first-dip
    rule;
  - a subharmonic penalty, 4, when a clearly periodic candidate (CMNDF below
    0.15) at half or a third of the lag is at least as good.
- **The unvoiced cost** is 10 × a threshold that adapts to the sound. The base
  is 0.3. It rises by up to 0.35 for loud frames whose energy lies mostly
  below 1 kHz (vowels, nasals, creaky voice), and falls by up to 0.15 when
  most of the energy is above 1 kHz (fricatives). Frames more than 34 dB below
  the loud level get a further bias towards unvoiced.
- **Tilt and zero crossings.** The threshold also moves with two features over
  a centred 30 ms window: a tilt score from r1/r0 of the signal pre-emphasised
  by 0.99 (−2 to +4), and the zero crossings per 10 ms. It changes by
  `voicingEvidence` × (tilt − (crossings − 30)/10) / 66.7, so at the default
  weight of 4 a steep low-pass frame is about 0.24 easier to voice, and a
  fricative at 80 crossings per 10 ms 0.3 harder. The features and their scale
  come from Quadravox's QBox encoder.
  They lower the voicing disagreement by about 0.4 points and help most in
  sentences and vowels. The weight was tuned on the test inputs, which are
  mostly synthetic voices.
- **Per frame.** A frame is voiced if most of the three hops nearest its
  centre are voiced, and its period is their median. A final check halves or
  doubles a period that is about 2× off the median of its voiced neighbours.
  The period is mapped to the pitch index whose period (value + 1, Table 8-5)
  is nearest in log terms.

Octave errors were the main thing tuned here. The prior alone either let
octave errors through or forced true high excursions (a 200 Hz stress peak
from a 110 Hz voice) down to the subharmonic. The subharmonic rule and the gap
memory fixed the cases found. Voiced fricatives come out voiced, the choice
§12.5 says the original data made most often.

## Spectrum and K quantisation

- Each frame's spectrum is analysed over a 30 ms Hamming window around its
  centre. Frame 0's window is moved to start near the onset, so it does not
  analyse silence. Separate, longer windows for voiced and unvoiced frames
  (300 and 400 samples) made no measurable difference.
- The autocorrelation (to lag 10) gets a Gaussian lag window (60 Hz) and a
  −40 dB noise floor (r₀ × 1.0001), for stable, not over-sharp resonances.
- Levinson–Durbin gives *k*₁–*k*₁₀ (voiced) or *k*₁–*k*₄ (unvoiced), in the
  convention of §12.3. `test_tms5220enc.py` checks that sign with a one-stage
  lattice.
- **Quantisation.** Each *k* goes to the nearest Table 8-6 entry in log-area
  ratio. A coordinate search then tries ±1 and ±2 indices of each K, and keeps
  changes that lower the prediction error `aᵀRa` of the quantised filter on
  this frame's autocorrelation. This is the LPC criterion itself, measured on
  the quantised filter, so a later K can make up for an earlier K's rounding.

## Energy

- **Target.** The frame's level is the Hann-weighted RMS of the input over
  25 ms around its centre. One gain scales the whole utterance so that the RMS
  over sounding frames is `targetRms`.
- **Level processing (optional, off by default).** Before the gain is set, the
  frame levels can be reshaped, so that the output is louder for the same
  peak:
  - `levelAgc` divides the swings of a slow loudness envelope (the frame
    powers averaged over a 0.75 s Hann window) about its median by the ratio.
    Quiet words and phrases come up; syllables keep their contrast.
  - `compression` then divides each frame's level in dB, above or below the
    median sounding level, by its ratio.

  Both make the output follow the input's level contour less closely.
  Measured over the 243 test inputs, PESQ
  against the unprocessed input changed as follows:

  | Setting | At the default settings | Played as loud as the DAC allows, with a hiss of 2 codes RMS added |
  |---|---|---|
  | `levelAgc` 4 | +0.005 | +0.05 |
  | `compression` 1.5 | −0.03 | +0.10 |
  | Both | −0.05 | +0.13, and about 1 dB louder |

  The hiss level is an assumption, not a measurement of real hardware.
  Compression of 2 or more, or lifting only the quiet frames, cost more than
  it gained. Smoothing the level from frame to frame made both conditions
  worse.
- **First guess.** The open-loop estimate of §12.3 (code ≈ E·x·h/128) gives an
  energy value from the quantised filter's power response: the chirp spectrum
  × |H|² over the period for voiced frames, the flat noise power for unvoiced.
  That value goes to the nearest index in dB.
- **Closed loop.** The utterance is synthesised with the reference algorithm.
  Each frame's output level is measured with the same Hann window around
  output sample 225 + 200·*j*, and each index is corrected by 70 % of its
  error in dB, when the error exceeds 1.5 dB. Up to 5 passes are made, and the
  best pass is kept. Because of the glide, a frame's window also holds part of
  its neighbours. Measuring both sides the same way, and damping the
  correction, keeps this stable. Lowering unvoiced frames below the input's
  level (by 1–6 dB) only lowered PESQ, most of all on the recordings.
- **Too loud at index 1.** A frame still more than 12 dB above its target at
  energy index 1 becomes silence. This happens with strongly resonant filters
  ([Implementation choices](choices.md#settled-by-the-datasheet), the quietest frame).
- **Limiter.** Any frame whose output peaks above `peakLimit` (120) has its
  index lowered by the excess in 3 dB steps. A frame already at index 1
  becomes silence. A final pass lowers every frame by one index until no code
  exceeds the limit and no 14-bit sum wraps anywhere in the lattice. This last
  pass has not been needed on any test input.

## Repeat and silence decisions

- **Silence.** A frame is silence if its level is more than `gateDb` (45 dB)
  below the loudest frame, or below twice the noise floor (the 5th percentile
  of 10 ms levels).
- **Repeat.** Within each run of sounding frames of one voicing, the encoder
  splits the run into segments of up to 8 frames, each a full frame followed
  by repeats, by dynamic programming. A segment costs its bits (50 for a full
  voiced frame, 29 for a full unvoiced one, 11 per repeat) plus
  `repeatLambda` × its distortion. A frame's distortion is how much its
  prediction error rises, in dB, under the segment's K instead of its own
  quantised K: an Itakura-style likelihood ratio. The segment's K is either
  its first frame's or the K quantised from the segment's pooled normalised
  autocorrelation, whichever costs less. At equal bit rate this beats a greedy
  rule (a repeat when the stored K cost at most 0.8 dB) by 0.01–0.06 PESQ, or
  saves about 100 bit/s at equal quality. A repeat never crosses
  silence or a change of voicing.
- **Voicing rules.** A voiced repeat never follows an unvoiced frame, and the
  first voiced frame after unvoiced speech is always full (§12.2). Unvoiced
  repeats are used only after unvoiced frames, apart from the fades.
- **Keeping repeats valid.** After level setting, frames turned to silence
  change what the chip holds. Packing tracks the chip's stored K, and sends a
  repeat whose K are not the stored ones as a full frame.

## Start and end

- **Start.** The first frame is always a full voiced or unvoiced frame
  (§12.2): leading silence frames are dropped. Its level is the input's at the
  onset, which is usually low, so the inhibited jump at the start does not
  click.
- **No dependence on the previous utterance** (§12.2, "K5–K10 before the first
  voiced frame"). Until the first full voiced frame, the stored K5–K10 indices
  are whatever the chip spoke before. A silence frame there would bring them
  back into use. So:
  - a faint opening sound (no voiced frame, every frame more than 36 dB below
    the loudest, followed by silence) is dropped with its silence; on the test
    inputs these are noise blips before the speech;
  - any silence frame still before the first full voiced frame becomes an
    unvoiced repeat at energy 1, which keeps K5–K10 zeroed. An utterance with
    no voiced frame at all therefore has a faint hiss in its pauses.
- **End.** After a voiced frame, the stop fades the energy over one frame
  (§12.2).
- **Fades.** An unvoiced frame before silence or the stop does not fade on the
  chip ([§8.7](../datasheet/excitation-interpolation.md)). The encoder adds
  unvoiced repeat frames, each with a third of the previous energy value,
  until the value is 2 or less. Inside an utterance they replace the following
  silence frames; at the end they are added before the stop.
- **Speak External.** `speakExternalStream()` adds silence frames before the
  stop only when the stream is shorter than 9 bytes (§12.9).
