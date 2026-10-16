# Encoder behaviour

What the encoder does whatever its settings, and why its defaults are what
they are. The settings themselves are described on the
[Preprocess view](../user-manual/preprocess.md#the-settings) and the
[Encode view](../user-manual/encode.md#the-settings) pages, and the method
in [How the encoder works](../../technical/encoder/how-it-works.md).

## The order of the stages

1. **Preprocessing**: the noise gate, the bass cut, the treble tilt and
   the de-esser, in that order.
2. **Finding the speech**: the start, the end and the silent frames, in the
   recording after the noise gate, by the **Silence gate**.
3. **Pitch and voicing**: the pitch and the voicing of each frame, found in
   the recording after the noise gate but before the other preprocessing
   (a brighter recording makes voiced sounds look like hiss), by the
   **Voicing threshold** and **Voicing evidence**, and checked against a
   second pitch tracker ([below](#the-voices-pitch)). The voicing is then
   smoothed by **Shortest voicing run**.
4. **The speech found again** in the fully preprocessed recording, when
   any filter is on.
5. **Each frame's filter**, from the fully preprocessed recording, by the
   **Analysis window** and **Lag window**.
6. **Repeat frames**, by the **Repeat trade-off**.
7. **A faint start trimmed**: an unvoiced opening more than 36 dB below
   the loudest frame is left out.
8. **Resonance hold**.
9. **The level**: the **Slow gain control**, then the **Target RMS** and
   the limiter, which choose each frame's energy.
10. **The endings and pauses**: [loud hiss faded](#vowel-starts-and-word-endings)
    and [quiet pauses](#quiet-pauses).

## Fixed behaviour

These are not settings: the encoder always works this way. Making them
settable made no difference worth keeping over the corpus of 243
recordings, or did what a setting already does:

| Behaviour | Why |
|---|---|
| The level is set by **Target RMS**, never by a target peak | A second way to set the loudness. Aiming the peaks at 100 lowered the median score from 0.718 to 0.673 |
| No output code beyond ±160, and at most 1 sample in 1000 beyond the DAC's ±127, which clips it | Holding every peak inside the DAC's range let the peaks rather than Target RMS set the level, and phrases came out quieter than Acorn's own, which clip 0.21 % of their samples ([Target RMS](../user-manual/encode.md#target-rms)) |
| Repeat frames are always used | **Repeat trade-off** already sets how many there are |
| 5 closed-loop level passes | 3 or 8 passes moved no file's score by more than 0.02 |
| Loud hiss fades out over one frame | Instead of stopping abruptly; quieter hiss ends as the recording has it, as on Acorn's own PHROMs ([below](#vowel-starts-and-word-endings)) |
| No level compression | It lowered the score on average, and badly on some recordings; **Slow gain control** brings quiet words up better |
| Quiet pauses | Without them the chip buzzes faintly through every pause ([below](#quiet-pauses)) |
| Voiced frames are analysed after the inverse of the chirp's spectrum | First-order pre-emphasis gave a higher spectral distortion at every coefficient tried ([pre-emphasis](../../technical/encoder/how-it-works.md#pre-emphasis)) |

A project saved by an earlier version may still list a setting that is
now fixed, such as `pre_emphasis`. It opens as usual: the lines are kept,
as a key Speak5220 does not know is kept
([project file](project-file.md#keys-and-files-from-newer-versions)), but
have no effect.

## How the defaults were chosen

The defaults were chosen on a corpus of 243 recordings and against Acorn's
own PHROM phrases:

- Three give the best typical
  [quality rating](../user-manual/encode.md#the-quality-rating) over the
  corpus: **Analysis window** 256, **Voicing evidence** 5 and **Slow gain
  control** 1.25.
- Two, **Voicing threshold** 0.6, with **Shortest voicing run** 2, and
  **Repeat trade-off** 300, bring the encoder's voicing and its repeat
  frames near those of Acorn's phrases, and raise the typical rating over
  the corpus too. Encoded from the chip's output, Acorn's phrases come back
  with nearly the same voicing as their own frames (97 % of frames agree)
  and about as many repeat frames, and recordings score higher on average,
  at about a fifth more bytes.
- **Target RMS** 20 is a loudness, the output's level after the BBC
  Micro's 279 Hz high-pass so that bass does not count, and gives Acorn's
  own phrases their own loudness back.

On Acorn's phrases, voicing smoothing and the voicing threshold bring the
encoder's voicing nearer the phrases' own: re-encoded by Speak5220 with
the threshold at 0.4, they changed between buzz and hiss 2.7 times a
second instead of 3.8 (the phrases themselves: 1.6), and scored 0.928 instead of 0.890. A shortest run of 3
comes nearer still (2.2 times a second), but over the corpus it lowers
more scores than it raises, so it is not the default. The threshold's
default has since risen to 0.6, which leaves fewer bursts of hiss inside
vowels still.

A project keeps the settings it was made with: every project default is
written in its file, so a later version of Speak5220 that changes a
default does not change the project's encodings. A new project starts
from these defaults, or from those chosen in
[Preferences](../user-manual/preferences.md#new-projects).

## Vowel starts and word endings

Two things the encoder always does, so that phrases start and end as
Acorn's own do:

- **Vowels start on time.** The chip plays each frame's buzz or hiss for
  the 25 ms after the moment the frame stands for, so a vowel that starts
  a little after that moment would be played a frame late, with a short
  burst of hiss at the vowel's loudness in front of it (after an *s* or a
  *k*, for example). The encoder decides such a frame by the sound the
  chip will play it over, and makes it voiced. On Acorn's phrases,
  re-encoded, 80 of their 106 vowel starts now come on the same frame as
  in the phrases themselves, and none a frame late (72 and 8 before).
- **Hiss ends as it is.** A final *s*, *sh* or *ch* already dies away in
  its own frames, so the encoder adds a fade only after hiss that is
  still loud when the sound stops (energy 8 or more), and then one frame
  of it. Acorn's phrases, re-encoded, now end in a fade 4 times in 157
  words, against 23 before (the phrases themselves: 3); each of those
  fades lengthened the word by 25 to 75 ms of hiss.

## The voice's pitch

The encoder finds the pitch of each voiced frame from how the sound
repeats. A vowel whose lowest resonance sits on a multiple of the voice's
pitch (the *i* of *six*, for example) can repeat more strongly at that
multiple than at the pitch itself, and would come out as a high, thin
buzz. So the encoder checks its pitch against a second pitch tracker,
which listens differently, and keeps to it wherever that tracker hears a
voice; which frames are voiced is still the encoder's own decision, set
by the voicing settings.

## Quiet pauses

A pause inside a phrase is made of silence frames, which stop the sound
but leave the chip's filter as the sound before it set it. On a real
TMS5220 the filter then never quite comes to rest: it keeps flicking a
code or two either side of zero, and the chip turns that into a faint buzz
at about the pitch of the voice, roughly 23 dB below the speech, for the
whole pause (datasheet §12.2, measured on the chip).

So the encoder always settles the filter: in every pause of three
silence frames or more, the second becomes one voiced frame at the
lowest energy with neutral filter settings, and the rest of the pause is
then truly silent. The first frame of the pause is left alone, so the
sound before it fades out as it did. It costs 46 bits a pause, a few
percent of a phrase with pauses. Shorter pauses are left as they are:
they ring for at most 50 ms. The
[noise gate](../user-manual/preprocess.md#noise-gate) makes long pauses
common.

!!! warning "The pauses you hear in the window are not the chip's"
    The waveforms and the Optimise window play the emulator, and at this
    level, a code or two, the emulator cannot be trusted: how the chip
    rounds its smallest values is not known (datasheet §8.8). A pause can
    buzz on the chip and be silent in the window, or the other way round.
    Measured on a real TMS5220, a pause quietened this way was silent
    every time, even where the window plays a faint whine in it
    (datasheet §12.2). Judge the pauses on a real chip, not by ear in the
    window.

## Values in project files and on the command line

Numbers are written with a `.` as the decimal point, whatever the
computer's language settings. A value outside its range, or of the wrong
type, is refused, with a message naming the setting. The encoder takes
audio at 8000 Hz only; importing a phrase resamples it.
