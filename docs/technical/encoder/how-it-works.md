# How the encoder works

The method is that of [§12.6](../datasheet/authoring.md), with the choices
below. Where the datasheet leaves a point open, [Implementation choices](choices.md) gives
the reasoning. The encoder is the application's, in `app/src/core/encoder/`;
its options are in the [option reference](../../application/reference/encoder-options.md),
and the [user manual](../../application/user-manual/encoder.md) says what each
one does to the sound.

## The stages

One call encodes one phrase, in this order:

1. **Preprocessing** (optional, off by default): the noise gate, then the bass
   cut, treble tilt and de-esser ([Preprocessing](#preprocessing)).
2. **Segmentation**: the onset, the offset and the frame centres
   ([Preparation and timing](#preparation-and-timing)).
3. **Pitch and voicing**, every 5 ms, then per frame
   ([Voicing and pitch](#voicing-and-pitch)).
4. **Spectra**: each frame's autocorrelation and K
   ([Spectrum and K quantisation](#spectrum-and-k-quantisation)).
5. **The plan**: silence, full frames and repeats
   ([Repeat and silence decisions](#repeat-and-silence-decisions)), then the
   faint-opening trim and the resonance hold.
6. **Level setting**: the energy indices, by closed loop on the encoder's own
   synthesis, with the limiter ([Energy](#energy)).
7. **The ends**: fades, the guard on the stored K5–K10, the stop and the quiet
   pauses ([Start and end](#start-and-end)).

A phrase with no sound above the gate, or whose plan is all silence, is a
lone stop frame.

## Preprocessing

The preprocessing gives two signals. The **gated** one (the noise gate alone)
sets the extent of the speech and is what the pitch tracker and voicing
decisions hear. The **filtered** one (gated, then bass cut, treble tilt and
de-esser) is what the spectra and levels are taken from, and what the
encoding's quality is measured against. With none of the filters on, the two
are the same.

- **Noise gate** (`noise_gate_db`, `noise_gate_ms`): sound more than
  `noise_gate_db` below the loudest (a centred 20 ms RMS) is made silent. The
  gain closes over `noise_gate_ms` and opens in a quarter of that, ending as
  the sound rises.
- **Bass cut** (`bass_cut_hz`): an order-2 Butterworth high-pass run forwards
  and backwards, −6 dB at the cut.
- **Treble tilt** (`treble_tilt`): 1 − μz⁻¹ on the recording.
- **De-esser** (`de_ess_db`): the band above 2.5 kHz is turned down by its
  share of the sound plus `de_ess_db`, at most 1 dB per ms.

The "BBC Micro" preset (a 200 Hz bass cut and a treble tilt of 0.4) is the
difference between Acorn's PHROM and the corpus's recordings, within 3.7 dB
RMS.

## Preparation and timing

- **Input.** The encoder takes 8 kHz audio only. Importing a phrase mixes it
  to mono and resamples it (libsamplerate's best sinc converter), and can
  normalise one that clips or is very quiet. A 60 Hz high-pass (order-2
  Butterworth, zero-phase) removes DC and rumble.
- **Onset and offset.** A level envelope (an 81-sample Hann-weighted RMS,
  about 10 ms) is taken every 2.5 ms. The onset and offset are the first and
  last points above the gate (see [Repeat and silence decisions](#repeat-and-silence-decisions)).
  Leading and trailing quiet is dropped.
- **Frame centres.** Frame *j* describes the input at `onset + 22 + 200·j`
  samples. The chip makes the first frame's energy audible at output sample
  203: it is computed in PC 0 of IP 0 (sample 201) and used two samples later
  ([§8.5](../datasheet/frame-timing.md)). So output sample 203 is the input
  onset, and output sample 225 + 200·*j* corresponds to frame *j*'s centre.
  Sample 225 is the middle of the stretch where frame *j*'s values dominate.
  They are half-way reached at the end of IP 4 of its glide (0.498 of the way,
  §12.2), reached in IP 0, and half-way to the next frame's by IP 4 of the
  next glide.

## Pre-emphasis

Voiced frames are analysed after the inverse of the chirp's spectrum, so that
the filter describes the speech spectrum divided by the chirp's. The
pre-filter is the order-4 LPC inverse of the chirp table's 21 non-zero
samples: [1, −0.938, 0.152, −0.104, −0.006]. Its first-order fit,
1 − 0.888z⁻¹, is close to §12.3's `s(n) − 0.9·s(n−1)`, but the order-4
filter also follows the chirp's shape between 300 and 2000 Hz, where a
first-order one leaves the output about 1 dB weak below 800 Hz and 1 dB
strong above.

Against first-order pre-emphasis at μ = 0.85, 0.9, 0.9375 (TI's 1981 default)
and 0.97, on 64 TTS phrases and 33 recorded sentences, the chirp's inverse
gave the lowest log spectral distortion on both sets (2.88 and 2.87 dB,
against 2.97–3.07 and 2.97–3.10 dB) and the best mean score (0.755 and 0.708,
against at most 0.745 and 0.697), for the same bits. No first-order setting
gave a recorded sentence a lower spectral distortion, so it is the only
pre-emphasis the encoder has. Unvoiced frames, whose excitation is flat, are
analysed without pre-emphasis.

## Voicing and pitch

- **Candidates.** The gated signal is low-passed at 1 kHz (order-4
  Butterworth, zero-phase). Every 5 ms, YIN's cumulative mean normalised
  difference function (CMNDF) is computed over a 30 ms window, placed so that
  it is centred on the hop for a lag of 80. The five deepest local minima
  between lags 15 and 161 samples (50–533 Hz), with parabolic refinement, are
  the voiced candidates.
- **Path.** A Viterbi search over the candidates plus an unvoiced state picks
  the path. Its costs are:
  - each candidate's CMNDF value, × 10;
  - a pitch jump between hops, 12 per octave, so an octave jump costs as much
    as a CMNDF difference of 1.2;
  - a voicing switch, 4. Each state remembers the last voiced lag, and a
    voiced hop after an unvoiced gap of under 40 ms pays the jump from that
    lag, tapering to nothing as the gap reaches 40 ms, so a short gap cannot be
    used to change octave cheaply;
  - a prior on the period. Where RAPT (SPTK's, on the signal band-limited to
    80–3600 Hz, 50–400 Hz) voices the hop, it is 20 per octave beyond
    ±0.2 octave of RAPT's period. Elsewhere it is 5 per octave beyond
    ±0.6 octave of the phrase's median period, taken from confident hops by
    YIN's first-dip rule. RAPT never decides the voicing: it only moves the
    voiced candidates' costs, which keeps the track off harmonics;
  - a subharmonic penalty, 4, on a candidate at about 2× or 3× the lag of a
    clearly periodic one (CMNDF below 0.15) that is no more than 0.01 worse.
- **The unvoiced cost** is 10 × a threshold that adapts to the sound. The base
  is `voicing_threshold` (0.6). It rises by up to 0.35 for loud hops whose
  energy lies mostly below 1 kHz (vowels, nasals, creaky voice), and falls by
  up to 0.15 when most of the energy is above 1 kHz (fricatives). Hops more
  than 34 dB below the loud level add 4 to every voiced candidate.
- **Tilt and zero crossings.** The threshold also moves with two features over
  a centred 30 ms window of the full-band signal: a tilt score from r1/r0 of
  the signal pre-emphasised by 0.99 (−2 to +4), and the zero crossings per
  10 ms. It changes by `voicing_evidence` × (tilt − (crossings − 30)/10) / 66.7,
  so at the default weight of 5 a steep low-pass hop is 0.30 easier to voice,
  and a fricative at 80 crossings per 10 ms 0.375 harder. The features and
  their scale come from Quadravox's QBox encoder.
- **Per frame.** A frame is voiced if most of the three hops nearest its
  centre are voiced, and its period is their median.
- **Onsets.** An unvoiced frame just before a voiced one is voted again over
  the five hops from its centre, the 25 ms in which the chip plays it, and
  becomes voiced if most of those are.
- **Octaves.** A period more than 1.7× the median of its voiced neighbours
  (two either side, at least two of them) is halved, and one under 0.6× is
  doubled, within the chip's 16–160 samples.
- **Short runs.** A run of voiced or unvoiced frames shorter than
  `voicing_min_run` (2), with the other kind on both sides, takes its
  neighbours' voicing: a one-frame unvoiced gap is filled with the mean of
  its neighbours' periods, and a one-frame voiced island becomes unvoiced.
- **The pitch index** is the one whose period (value + 1, Table 8-5) is
  nearest in log terms.

## Spectrum and K quantisation

- Each frame's spectrum is analysed over a Hamming window of `window`
  samples (256, 32 ms) around its centre. Frame 0's window starts no more
  than 5 ms before the onset, so it does not analyse silence.
- The autocorrelation (to lag 10) gets a Gaussian lag window
  (`lag_window_hz`, 60 Hz) and a −40 dB noise floor (r₀ × 1.0001), for stable,
  not over-sharp resonances.
- Levinson–Durbin gives *k*₁–*k*₁₀ (voiced) or *k*₁–*k*₄ (unvoiced), in the
  convention of §12.3, each held within ±0.9999.
- **Quantisation.** Each *k* goes to the nearest Table 8-6 entry in log-area
  ratio. A coordinate search then tries −1, +1, −2 and +2 indices of each K,
  in up to three passes, and keeps changes that lower the prediction error
  `aᵀRa` of the quantised filter on this frame's autocorrelation. This is the
  LPC criterion itself, measured on the quantised filter, so a later K can
  make up for an earlier K's rounding.

## Repeat and silence decisions

- **Silence.** A frame is silence if its level is more than `gate_db` (45 dB)
  below the loudest frame, or below twice the noise floor (the 5th percentile
  of the 10 ms levels).
- **Repeat.** Within each run of sounding frames of one voicing, the encoder
  splits the run into segments of up to 8 frames, each a full frame followed
  by repeats, by dynamic programming. A segment costs its bits (50 for a full
  voiced frame, 29 for a full unvoiced one, 11 per repeat) plus
  `repeat_lambda` × its distortion. A frame's distortion is how much its
  prediction error rises, in dB, under the segment's K instead of its own
  quantised K: an Itakura-style likelihood ratio. The segment's K is either
  its first frame's or the K quantised from the segment's pooled normalised
  autocorrelation, whichever costs less. A repeat never crosses silence or a
  change of voicing. At the default of 300, about 4 % of the corpus's frames
  are repeats; the most the option allows, 1000, adds 2 % to the bytes for no
  change in the score.
- **Voicing rules.** Each run starts with a full frame, so a voiced repeat
  never follows an unvoiced frame, and the first voiced frame after unvoiced
  speech is always full (§12.2).
- **Faint openings.** An opening sound with no voiced frame, no louder than
  36 dB below the loudest frame and followed by silence, is dropped with its
  silence (see [Start and end](#start-and-end)).
- **Resonance hold** (`resonance_hold_db`, 14 dB). The chip interpolates K in
  equal steps, so moving out of a strongly resonant filter drops the gain in
  the first step, which is heard as a pop. Where a fading voiced frame's
  filter has more than this many dB less gain than the voiced frame before
  it, it repeats that frame's filter, with its own pitch. A held frame is not
  held again.
- **Keeping repeats valid.** After level setting, frames turned to silence
  change what the chip holds. The frames are built tracking the chip's
  stored K, and a repeat whose K are not the stored ones is sent as a full
  frame.

## Energy

- **Target.** Each frame has a level, the Hann-weighted RMS of the input over
  25 ms around its centre, and a loudness, the same after a first-order
  279 Hz high-pass (the BBC Micro's speech path, C21/R31), so bass does not
  count. One gain scales the whole phrase so that the RMS of the loudness over
  sounding frames is `target_rms` (20 codes). The level is still what the
  silence gate, the faint-opening trim and the resonance hold use.
- **Slow gain control** (`level_agc`, 1.25). Before the gain is set, the
  swings of a slow loudness envelope (the frame powers averaged over a 0.75 s
  Hann window) about its median are divided by this ratio. Quiet words come
  up; syllables keep their contrast. 1 is off.
- **First guess.** The open-loop estimate of §12.3 (code ≈ E·x·h/128) gives an
  energy value from the quantised filter's power response, weighted by the
  loudness high-pass: the chirp spectrum × |H|² over the period for voiced
  frames, the flat noise power for unvoiced. A repeat uses the K of the full
  frame it repeats. That value goes to the nearest index in log terms.
- **Closed loop.** The phrase, with its fades and stop, is synthesised with
  the reference algorithm. Each frame's output loudness is measured with the
  same Hann window around output sample 225 + 200·*j*, and each index is
  corrected by 70 % of its error in dB, when the error exceeds 1.5 dB. Up to 5
  passes are made, and the pass with the lowest mean error is kept. Because of
  the glide, a frame's window also holds part of its neighbours. Measuring
  both sides the same way, and damping the correction, keeps this stable.
- **Too loud at index 1.** A frame after the first that is still more than
  12 dB above its target at energy index 1 becomes silence. This happens with
  strongly resonant filters ([Implementation choices](choices.md#settled-by-the-datasheet), the quietest frame).
- **Limiter.** The output must have no lattice wrap, no code beyond ±160, and
  at most one sample in 1000 beyond the DAC's ±127, which clips it. Acorn's
  own PHROM clips on the peaks of half its words, 0.21 % of its samples. In up
  to 30 passes, a frame peaking above 160 is lowered by the excess in 3 dB
  steps, and while too many samples clip, the frames with most clipped samples
  are lowered by one index. A frame after the first already at index 1
  becomes silence. A final net lowers every frame by one index until the
  limits hold. Over the corpus the limiter lowers 13 % of sounding frames and
  silences 1 %.

## Start and end

- **Start.** The first frame is always a full voiced or unvoiced frame
  (§12.2): leading silence frames are dropped, and level setting never
  silences the first frame. Its level is the input's at the onset, which is
  usually low, so the inhibited jump at the start does not click.
- **No dependence on the previous utterance** (§12.2, "K5–K10 before the first
  voiced frame"). Until the first full voiced frame, the stored K5–K10 indices
  are whatever the chip spoke before. A silence frame there would bring them
  back into use. So:
  - a faint opening sound is dropped with its silence (above); on the
    corpus these are noise blips before the speech;
  - any silence frame still before the first full voiced frame becomes an
    unvoiced repeat at energy 1, which keeps K5–K10 zeroed. A phrase with no
    voiced frame at all therefore has a faint hiss in its pauses.
- **End.** After a voiced frame, the stop fades the energy over one frame
  (§12.2). The encoder adds the stop frame.
- **Fades.** An unvoiced frame before silence or the stop does not fade on the
  chip ([§8.7](../datasheet/excitation-interpolation.md)). After loud hiss (an
  unvoiced frame at energy index 6 or more), the encoder adds one unvoiced
  repeat at a third of its energy value: inside a phrase it replaces the
  following silence frame, and at the end it is added before the stop. Quieter
  hiss ends as it is, as it does in Acorn's PHROM.
- **Quiet pauses.** With no excitation the lattice settles into a limit cycle,
  and the DAC's step between −1 and 0 turns it into a buzz about 23 dB below
  the speech (§12.2, "Silence inside an utterance rings"). In every pause of
  three or more silence frames, the second becomes a voiced frame at energy 1
  with every K at its index nearest zero, which stops the ringing. A repeat
  after the pause is sent as a full frame with the K it would have had. The
  one exception is a voiced repeat after an unvoiced frame, which relies on
  the zeroed K5–K10 no full frame can give; that pause is left as it is.
- **Speak External.** A Speak External stream gets silence frames before the
  stop only when it is shorter than 9 bytes (§12.9).

## Determinism

The same audio and options always give the same frames, on every platform
and whatever the number of threads. The RAPT anchor adds Gaussian noise
before tracking, as SPTK does, from a fixed seed. The encoder's version
(`kEncoderVersion`, in `app/src/core/encoder/encoder.h`) is stored with every
cached encoding, and changes whenever the same audio and options can give
different frames, so a project re-encodes its phrases when it does.
