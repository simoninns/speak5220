# Encoder: implementation choices

What the [reference datasheet](../datasheet/index.md) leaves unclear,
ambiguous or missing for someone writing an encoder from it alone, and what
`tms5220enc.py` chooses. What is not known about the chip itself is listed in
the datasheet's [Appendix B: Known gaps](../datasheet/gaps.md). Section numbers are the datasheet's. The points the
datasheet does settle are listed at the end, with the answer and where it is
given.

## Synthesis (§8.5–§8.9)

`synthesize()` follows [§8.9](../datasheet/reference-algorithm.md) step by
step. It gives the reference emulator's codes sample for sample on random
speech-like frame lists and on encoded speech, through the VSM path
(`test_tms5220enc.py`).

1. **Which noise state to use.** `synthesize()` models one VSM Speak with the
   standard procedure: register 0x00EB at slot 0 of sample 1
   ([§7](../datasheet/power-up.md)), from power-up state. It does not model
   the Speak External path, which runs 2 samples earlier with other noise
   (§7, [§8.5](../datasheet/frame-timing.md)). The encoder only needs levels,
   so this does not matter for encoding. It does mean the synthesis of
   unvoiced frames is exact for one start time only.

## Timing, for an author (§12.2, §12.6)

2. **Measuring a frame's level (§12.6 steps 3 and 5).** Because of the glide,
   a 25–40 ms output window around a frame also holds a large share of its
   neighbours. A per-frame closed loop then mistakes a neighbour's level for
   the frame's own, and pushes decaying tails down to energy 1 and then to
   silence. *Chose:* the same Hann-weighted 25 ms window on both sides (input
   centred on the frame centre, output on sample 225 + 200·*j*), with a damped
   closed loop through the reference algorithm. §12.6 step 5 could warn about
   this.

## Levels (§12.3, §12.6)

3. **The silence threshold (§12.6 step 6).** "Below a level threshold" gives
   no threshold. *Chose:* 45 dB below the loudest frame (4 dB below what
   energy index 1 can reach from index 14), or twice the noise floor (the 5th
   percentile of 10 ms levels), whichever is higher.
4. **Wrapping inside the lattice.** [§8.8](../datasheet/lattice-filter.md)
   says every sum wraps at 14 bits, while §12.3 discusses only the output
   (±512 codes). It does not say whether internal *u*ᵢ and *b*ᵢ can wrap while
   *u*₁ stays small. *Chose:* the synthesis checks every sum. The encoder
   lowers energies until none wraps. No encoded input wrapped at a 120-code
   peak limit.
5. **Margin for the chip.** The chip differs from the reference algorithm by
   about one code, more in resonant frames, and has upsets (§7, §8.8). The
   datasheet does not say how much margin an author should leave below ±127.
   *Chose:* a 120-code peak limit.

## Frame kinds (§12.2, §12.5)

6. **"Close" K indices for a repeat (§12.5).** This is not quantified.
   *Chose:* repeats are chosen by a rate-distortion search within each run of
   one voicing: bits + `repeatLambda` (70) × the rise in each frame's
   prediction error, in dB, under the repeated K. This is a likelihood-ratio
   (Itakura) measure. Repeats only follow a full frame of the same voicing
   ([Repeat and silence decisions](how-it-works.md#repeat-and-silence-decisions)).
7. **Unvoiced repeat after a voiced frame.** §12.5 says this is allowed (K1–K4
   kept, K5–K10 zeroed), but it never says when it is useful. *Chose:* never,
   apart from the fades below.
8. **How to fade a hiss (§12.2).** "Step its energy down over a few unvoiced
   repeat frames first" leaves the rate open. *Chose:* each fade frame has a
   third of the previous energy value, until the value is 2 or less (for
   example 33 → 11 → 4 → 1). Inside an utterance, fade frames replace the
   silence frames after the hiss. At the end, they are added before the stop.
9. **Leading silence.** §12.2 says to start with a voiced or unvoiced frame.
   *Chose:* after level setting, the encoder drops any leading silence frames,
   so the first frame always loads every index.

## Analysis (§12.3, §12.6)

10. **Pre-emphasis (§12.3).** The stated chirp spectrum (+3.9, −8.8, −11.7,
    −17.6 dB at 100, 500, 1000 and 2000 Hz relative to 250 Hz) is right to
    0.1 dB, and 1 − 0.9z⁻¹ inverts it well. With that filter, 0.8 and 0.95
    changed the spectral distance by less than 0.05 dB. *Chose:* the order-4
    LPC inverse of the chirp table, because it also matches the chirp's shape
    from 300 to 2000 Hz and halves the output's tilt error
    ([Pre-emphasis](how-it-works.md#pre-emphasis)).
11. **Window (§12.6 step 3: 25–40 ms).** *Chose:* 30 ms Hamming. For frame 0,
    whose centre is at the onset, the spectral window is moved to start near
    the onset, so it does not analyse mostly silence.
12. **Quantisation (§12.6 step 4).** The text says nearest in LAR. *Chose:*
    nearest in LAR, then a coordinate search over ±2 indices of each K that
    minimises the prediction error of the quantised filter on the frame's
    autocorrelation.
13. **Voicing and pitch (§12.6 step 3).** The datasheet gives no method and no
    warning about octave errors. That is encoder craft, not a gap in the
    chip's description. The choices are in
    [Voicing and pitch](how-it-works.md#voicing-and-pitch).

## Packaging (§12.7–§12.9)

14. **Acorn PHROM: ASCII entries with no word.** The format reserves numbers
    32–126 for "ASCII associated" entries, but the datasheet does not say what
    an image without a word for a character should hold. *Chose:* always 95
    ASCII entries, so word 127's pointer is at byte 254 as in the Acorn PHROM
    A. Unmapped entries point to one shared byte holding a lone stop code
    (0x0F), placed after the last word. Speaking it does nothing
    ([§4](../datasheet/vsm-interface.md): a first-frame stop gives no output).
15. **Acorn PHROM: where a name starts.** Names are stored backwards before
    the data, "shifted left by one, then bit-reversed". The datasheet does not
    say how a reader finds the first byte of a name (the far end), or what the
    freed bit is for. It may be an end-of-name flag. *Chose:* no flag bits.
    Names directly follow the previous word's stop byte, or the pointer
    table's 0xFF for the first word, as described. No spare byte is left after
    a word's stop code, since the datasheet mentions one only for TI images.
    Unused bytes are 0x00.
16. **Acorn PHROM: untested.** Whether a BBC Micro reads a new image in this
    format has not been tested ([known gaps](../datasheet/gaps.md#the-acorn-phrom-format-and-the-bbc-micro)).
17. **Speak External padding (§12.9).** Silence frames before the stop keep
    the stream at 9 bytes or more. If the last sound frame is unvoiced, those
    silence frames end it abruptly, because unvoiced to silence is inhibited
    ([§8.7](../datasheet/excitation-interpolation.md)). The encoder's own
    output already fades a final hiss. A stream that is only a stop code
    becomes 16 silence frames and a stop: 0.4 s of nothing, starting with a
    silence frame, whose start rule is approximate (§7). The datasheet could
    say whether padding is better placed somewhere else.

## Checked against the text

These statements were checked and are correct: the glide fractions of §12.2
(0.125 … 0.906); the energy-step figures of §12.3; the chirp energy of 65,546
and its RMS at *P* = 16, 100 and 160; the lattice sign convention of §12.3
(`test_tms5220enc.py`, one-stage check).

## Settled by the datasheet

Each of these is stated in the datasheet, and the encoder follows it.

- **Unvoiced frames and the K5–K10 indices** (§8.9 "Loading a frame", §8.7).
  An unvoiced frame writes idx[2..5] only; idx[6..11] keep their values.
  Reading it as setting K5–K10 to index 0 gives output that differs from the
  emulator after the next silence frame.
- **The pitch counter step** (§8.9 step 6) runs in every sample period, not
  only in frame-read samples.
- **Writes due "in this sample"** (§8.9 step 3): when not talking, a K zeroed
  at n + d(p) − 1 is applied before the filter (step 4).
- **"oldU as it was in sample n − 1"** (§8.9 step 2.4) is the value during
  sample n − 1, before that sample's step 6. This gives the "from sample 2 on"
  of §8.5.
- **Which output samples belong to which frame** (§12.2): sample
  225 + 200·*j* from the first frame read stands for frame *j*, the middle of
  the stretch where its values dominate (half-way into its own glide at the
  end of IP 4, sample 125; half-way to the next frame's values by sample 325).
  The input frame centres are then `onset + 22 + 200·j`, because the sound
  starts at output sample 203 (energy computed at 201, used from 203, §8.5).
- **RMS target and peak limit** (§12.3): the peak limit wins over an RMS
  target, and 15–20 codes RMS at ±120 peaks is the range. On the test inputs,
  a single scale that puts the loudest output peak at 120 gives a median
  output RMS of 15.9 codes (40 files, `targetPeak=120`). The encoder uses an
  RMS target of 20 codes with a per-frame limiter: any frame whose output
  peaks above 120 has its energy index lowered. This acts on a minority of
  frames ([Quality](quality.md)). `targetPeak` gives the single-scale reading.
- **The quietest frame depends on the filter** (§12.3, "The energy steps"): a
  resonant filter can make index 1 too loud, so each frame is checked by
  synthesis. An unvoiced frame with K1 = −501 (a near-DC resonance) peaked at
  179 codes at energy index 1. The encoder makes a frame silence if it is
  still more than 12 dB above its target at index 1, or if the limiter cannot
  bring it below the peak limit at index 1. A repeat frame that relied on a
  silenced frame's K is sent as a full frame.
- **Pitch steps** (§12.4): indices 1–28 step one sample at a time (periods
  16–43, steps of 6.3 % down to 2.4 %). Index 29 is 45 and index 30 is 47
  samples; periods 16–31 are indices 1–16.
- **Median pitch of the originals** (§12.10): index 44, a period of 79 samples
  (101 Hz).
- **Acorn PHROM pointer count** (§12.8): it includes the ASCII entries
  (260 = 95 + 165). The encoder writes 95 + the number of words.
- **Acorn PHROM bit reversal** (§12.8): every header byte is stored
  bit-reversed, the 16-bit counts and bytes 60–63 (0x00, 0x02, 0x00, 0x02 as
  the host reads them) included.
- **TI indexed count byte** (§12.8): stored as is, not bit-reversed. Words are
  numbered from 1, at most 255. Pointers are 14-bit offsets, low byte first.
  The image is 16 KB, zero-filled.
- **K5–K10 before the first voiced frame** (§12.2). An unvoiced frame writes
  only K1–K4 indices. Until the first full voiced frame, the stored K5–K10
  indices are the previous utterance's. A silence frame ends the K5–K10
  zeroing, so an utterance that opens with an unvoiced sound and then has
  silence before any voiced frame filters its decay with the predecessor's
  K5–K10. A synthesis that always starts from power-up does not show this. On
  the reference emulator it showed in about 20 of 347 utterances, by up to 27
  codes. With two extreme stored states in the encoder's synthesis, 15 of the
  243 test inputs depended on the predecessor, by up to 80 codes. The encoder
  guards against it
  ([Start and end](how-it-works.md#start-and-end)):
    - a faint opening (no voiced frame, every frame more than 36 dB below the
      loudest, followed by silence) is dropped, with its silence; this drops
      151 frames in 10 files, all noise blips at −40 to −46 dB before the
      speech;
    - any silence frame still before the first full voiced frame becomes an
      unvoiced repeat at energy 1, which keeps the zeroing (3 frames in 2
      files; most such silences are already hiss fades);
    - a stop after unvoiced frames was checked and does not depend on the
      predecessor.

  No test input depends on its predecessor. `evaluate.py` measures this
  ("predecessor-dependent"), and the self-test checks the pattern and one
  encoded input.
