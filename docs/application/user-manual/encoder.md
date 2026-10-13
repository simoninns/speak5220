# The encoder and Optimise

On the **Encode** tab of the [main window](main-window.md), under the
encoded [waveform](waveforms.md), is the selected phrase's **encoder
panel**: what its frames add up to, how good they sound, and the
settings they are made with. The [preprocessing](#preprocessing)
settings are on the **Preprocess** tab, under the preprocessed waveform.
**Optimise** searches the settings for the best quality the project has
room for.

A phrase is made in three stages:

1. **The original**: the recording as imported, never changed.
2. **The preprocessed recording**: the original with the
   [preprocessing](#preprocessing) applied (a squelch, a bass cut, a
   treble tilt), or the original itself when there is none.
3. **The encoding**: the frames the encoder makes from the preprocessed
   recording.

The quality is measured between the last two: how close the encoding is
to what it was asked to make. The main window has a tab for each stage,
**Original**, **Preprocess** and **Encode**, each with its
[waveform](waveforms.md), so you can see and hear what the encoding is
aiming at and what it made.

## The encoder panel

From the top:

- **The state**: *Up to date*; *Stale* (the frames were made with other
  settings, or by another version of the encoder); *Not encoded yet*;
  *Encoding...*; or *Locked* (the frames are never re-encoded, and only
  the statistics are shown).
- **The statistics**, for example:

    `104 frames, 3063 bits (383 B), 1178 bit/s`

    `voiced 41% · unvoiced 20% · repeat 19% · silence 19%`

    `peak code 120 of 127 · limiter lowered 3 frames, silenced 0`

    The frames before the stop, every frame's bits with the stop, the
    bytes they pack to, and the bits per second of speech (each frame is
    25 ms); the share of each kind of frame; and the loudest code the
    chip's output reaches (its DAC's range is ±127). What the limiter did
    (frames whose energy was lowered to keep every code within ±120,
    and frames made silent) is known only for encodings made in this session.
- **The quality rating** (below).
- **The settings** (below): the encoder's. The preprocessing settings
  are on the Preprocess tab.
- **Re-encode** (Encode for a phrase with no frames) encodes the phrase
  now with the settings shown. It is available only when there is
  something to redo: the phrase has no frames, they are stale, or a
  settings change is waiting to be applied. **Optimise...** opens the
  [Optimise](#optimise) window for the phrase. **Reset to Project
  Defaults** removes the phrase's own settings, so it follows the
  project's defaults again, and re-encodes it.

## The quality rating

The encoded speech is compared with the preprocessed recording (the
original, when there is no preprocessing) on the frames that hold
speech. Silence, pauses, breaths and clicks are left out, found by a
voice activity detector. Three things are measured:

- **Envelope**: how far the encoding's spectral envelope (the shape the
  chip's ten K parameters give the sound) is from the original's, as log
  spectral distortion in dB, and the share of frames more than 4 dB out.
  Under 2 dB is close; over 4 dB is a clear difference.
- **Pitch**: the share of frames whose pitch is wrong by more than 20%
  (gross errors, such as an octave out), how far it is out otherwise, in
  cents, and the share of frames where voiced and unvoiced sounds are
  swapped.
- **Intelligibility**: ESTOI, from 0 to 1, which compares the loudness of
  the speech in each band over time. It needs about 0.4 s of speech; a
  shorter phrase is rated on the other two.

Each is scored from 0 to 1, and the **overall score** is their geometric
mean, so one weak part pulls it down and cannot be hidden by the others.
The rating divides the overall score into three bands:

| Rating | Overall score |
|---|---|
| Poor | below 0.50 |
| Fair | 0.50 to below 0.70 |
| Good | 0.70 and above |

`[    Poor    |  Fair  ▼ |  Good  ]`

`Fair · 0.62`

The bar names each band inside it, and the line under it gives the rating
and the score, so it reads the same without colour. Under that, a small
bar for each of the three parts shows its score, so you can see which one
holds an encoding back, and the figures behind them follow:

`envelope 3.34 dB, 28% over 4 dB · pitch 5.6% gross, 60 cents RMS ·
voicing 9.9% · ESTOI 0.76`

The rating means the same on every phrase. An ideal LPC coder with
nothing quantised is *Good* on nearly every phrase; with the default
settings encodings are *Good* or *Fair* in about equal parts, and *Poor*
on a few. The envelope is usually the weakest part, since the chip's K
parameters are coarsely quantised. Hover over the rating for a reminder
of what each part measures.

The quality is measured when a phrase is encoded in the window, and saved
with its frames. For frames made elsewhere (by `speak5220-cli encode`, for
example), or saved by an earlier version of the measures, it is measured
in the background for display, and the line says *Measuring quality...*
meanwhile.

## The settings

One row per [option](../reference/encoder-options.md), in two columns,
in two sections: **Preprocessing** (with its [preset](#presets)
chooser) and **Encoder**. Hover over a label for what the option does and its default.
Numbers are set with sliders, with the value and its unit beside them
(`24 codes`, `45 dB`, `32 ms`; the ratios read `off` at 1, and the
treble tilt and the shortest voicing run read `off` at 0 and 1). A small tick
under each slider marks the default, so an option moved from it is easy
to see. A slider covers the range where useful values lie (the reference
gives each one); a value outside it, from the project file or Optimise,
widens the slider to include it. Click a slider and use the arrow keys
for single steps, Page Up and Page Down for larger ones. The
[squelch](#the-squelch), the [bass cut](#bass-cut-and-treble-tilt) and
the [resonance hold](#the-resonance-hold) have a check box that turns
them on or off, and the pre-emphasis a choice of chirp or first order,
whose coefficient the slider beside it sets.

The **Preprocessing** section, on the Preprocess tab, always shows its
three settings: the [squelch](#the-squelch), and the [bass cut and
treble tilt](#bass-cut-and-treble-tilt). The **Encoder** section, on
the Encode tab, first shows
its four **basic settings**, the ones that change the sound or the size
most clearly: the target RMS (loudness), the repeat trade-off (bytes
against quality), the pre-emphasis (brighter or more muffled) and the
voicing threshold (buzz against hiss on sounds between the two). Click
**Advanced settings** below them for the other seven, which fine-tune the
encoding; click it again to hide them. Speak5220 remembers
the choice. Hidden settings still apply: when any of them differ from the
default, the button says how many, for example **Advanced settings (2
changed)**, as it can after Optimise or with a project from elsewhere.
The [option reference](../reference/encoder-options.md) says which
options are which.

**Settings for** chooses what the form edits:

- **This phrase**: the phrase's settings. The ticks mark the project's
  defaults; a row in bold differs from its default, and its ↺ button
  puts it back. Only the options a
  phrase changes are stored with it; the rest follow the project's
  defaults.
- **Project defaults**: the options every phrase starts from. The ticks
  mark the encoder's own defaults, and a row in bold differs from them. Changing a default makes
  every phrase that follows it *stale*: their frames are not redone until
  they are re-encoded (**Phrase > Re-encode**). The button at the bottom
  becomes **Reset to Encoder Defaults**.

A change is applied 0.6 s after the last one, so typing a number or
holding an arrow does not encode every step. For a phrase, it is encoded
and measured in the background, and the new settings and frames are
stored together as one step to undo (**Undo Change Encoder Settings**).
For the project's defaults, they are set as one step (**Undo Change
Project Defaults**). Selecting another phrase applies a change still
waiting.

A new encoding that would take the project past its 64 PHROMs is not
stored: a message says so, and the form shows the phrase's settings again
(see [troubleshooting](troubleshooting.md#the-encoder-and-optimise)).

## Preprocessing

The preprocessing prepares the recording for the chip before it is
encoded. It is off by default, so the encoder takes the original as it
is. It is set in the **Preprocessing** section on the **Preprocess**
tab, for a phrase or for the project's defaults like any other setting:
the tab has its own **Settings for** chooser, which always matches the
Encode tab's. A change there re-encodes the phrase after a pause, as a
change on the Encode tab does.

- The quality is measured against the preprocessed recording, which is
  what the encoding aims at. So a preprocessing that suits the speaker
  the phrases will be played on is not counted against the encoding, and
  scores made with different preprocessing do not compare. Optimise keeps
  the phrase's preprocessing as it is.
- The Preprocess tab's waveform shows and plays the recording as
  preprocessed, following the settings as they move
  ([the preprocessed recording](waveforms.md#the-preprocessed-recording)).
  The original is never changed.
- **Export** can write the preprocessed recordings as WAV files
  ([export](export.md)).

### Presets

**Preset** at the top of the section sets all three settings at once:

| Preset | Squelch | Bass cut | Treble tilt | For |
|---|---|---|---|---|
| None | off | off | off | The recording as it is |
| BBC Micro | off | 200 Hz | 0.4 | Phrases played on a BBC Micro's small speaker, balanced like Acorn's own |

Choosing a preset replaces the three settings; the encoder's settings
are left as they are. When the settings match no preset, for example
after moving a slider, the chooser shows **Custom**.

### The squelch

**Squelch**, off by default, makes the quiet stretches of the recording
silent: everything more than the set number of dB below the recording's
loudest sound, such as breaths, lip noise and room noise. Tick its check
box to turn it on (it starts at `30 dB`).

- A lower number squelches more: `20 dB` mutes anything a tenth as loud
  as the loudest sound; `40 dB` only sounds a hundredth as loud.
- Sound is kept for 60 ms after it falls below the level and from 10 ms
  before it rises above it, with a short fade, so word endings and
  onsets are not clipped. Loudness alone cannot tell a breath from a soft
  *h* or a trailing *s*, though: if a word loses its ending, raise the
  number.
- The squelch is not the **Silence gate**, which only decides which
  frames of the encoding are silent (45 dB below the loudest frame by
  default) and does not change the preprocessed recording.

On the Preprocess tab's waveform the stretches the squelch mutes are
silent and [shaded](waveforms.md#the-squelch), and the shading follows
the slider as it moves, so the level can be set by eye: breaths are
often 25 to 35 dB below the loudest sound, so a breath that `30 dB`
leaves unshaded may need `25 dB` or less.

### Bass cut and treble tilt

**Bass cut** and **Treble tilt**, both off by default, make the
recording brighter: its bass cut below the set frequency (24 dB per
octave below it), and its treble lifted. The speech then has less of its
energy in the bass, where a small speaker such as a BBC Micro's gives
little back and where the chip's loudest peaks come from, and more in
the band where speech is understood. The encoded sound can be played
louder for the same peaks.

Acorn's own phrases, a voice that TI recorded and encoded, have this
balance: compared with a natural recording of a similar voice, they have
about 11 dB less at 125 Hz and a few dB more treble. **Bass cut** at
`200 Hz` and **Treble tilt** at `0.4`, the **BBC Micro** preset, come
close to it.

The encoder takes the encoding's spectra and loudness from the
preprocessed recording. It still finds the pitch and the voicing (buzz
or hiss) in the recording before its bass cut and treble tilt, since
brightening would make voiced sounds look like hiss.

## Voicing smoothing

A voiced sound (a vowel's buzz) and an unvoiced one (an *s*'s hiss) sound
very different on the chip, and a frame or two of hiss inside a vowel, or
of buzz inside a hiss, is heard as roughness. **Shortest voicing run**, an
advanced setting at `2 frames` by default, gives any run of voiced or
unvoiced frames shorter than that, with the other kind on both sides,
the voicing of its neighbours. A lone unvoiced frame in a vowel becomes
voiced, at the pitch around it; a lone voiced frame in a hiss becomes
unvoiced. Runs at the start and end of a phrase are kept. `1` turns it
off.

So by default a single frame of the other kind is smoothed away. On
Acorn's own phrases, this and the voicing threshold of `0.4` bring the
encoder's voicing nearer the phrases' own: re-encoded by Speak5220, they
change between buzz and hiss 2.7 times a second instead of 3.8 (the
phrases themselves: 1.6), and score 0.928 instead of 0.890. `3 frames`
comes nearer still (2.2 times a second), but over the corpus of 243
recordings it lowers more scores than it raises, so it is not the
default.

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
they ring for at most 50 ms. The [squelch](#the-squelch) makes long
pauses common.

!!! warning "The pauses you hear in the window are not the chip's"
    The waveforms and **Listen** play the emulator, and at this level, a
    code or two, the emulator cannot be trusted: how the chip rounds its
    smallest values is not known (datasheet §8.8). A pause can buzz on the
    chip and be silent in the window, or the other way round. Measured on
    a real TMS5220, a pause quietened this way was silent every time, even
    where the window plays a faint whine in it (datasheet §12.2). Judge
    the pauses on a real chip, not by ear in the window.

## The resonance hold

A high voice can make the encoder give a voiced frame a filter with one
very strong, narrow resonance, low in pitch, and a low energy to match.
The chip glides from one voiced frame's settings to the next, but out of
such a filter most of the change in loudness comes at the very start of
the glide: when the next frame's filter is an ordinary one, the sound
drops suddenly, heard as a pop.

**Resonance hold**, an advanced setting on at 14 dB by default, finds
these places: where the sound is fading and the next frame's filter has
more than that many dB less gain, the resonant filter is kept for that
frame instead, and the sound fades out through it. The filter changes a
frame later, when the sound is already quieter. It costs no bits (the
held frame is a repeat, which is shorter) and changes about one encoding
in ten. A lower value catches more places, at a small cost in how closely
the sound follows the recording; switch it off only when the frames must
match another encoder's exactly. Optimise leaves the setting as it is.

## Re-encoding several phrases

**Phrase > Re-encode** (Ctrl+E) encodes, in the background, every
selected phrase that is not encoded yet or is stale (and not locked, with
audio) with its own settings, and stores them as one step (**Undo
Re-encode 5 Phrases**). It is available only when the selection holds at
least one such phrase.
Use it after changing the project's defaults, or after adding phrases,
which are added unencoded. If the new encodings together would take the
project past its budget, none is stored.

## Optimise

**Optimise...** in the encoder panel, or **Phrase > Optimise...**
(Ctrl+Shift+O) for the selected phrases, opens the Optimise window. It
searches each phrase's settings for the encoding with the best overall
quality score.

Before it starts, two limits can be set:

- **Limit each phrase to ... bytes**: only settings whose frames fit are
  chosen. It starts at the largest of the phrases' sizes now, so ticking
  it alone finds better settings that are no bigger.
- **Trade quality for size**: choose the settings with the best overall
  score less this much for each kbit/s, so a smaller encoding can win
  over a slightly better one. With the default 0.03, saving 1 kbit/s is
  worth 0.03 of the score.

Whatever the limits, a result that would take the project past its 64
PHROMs is never chosen. Optimise does not change the
[preprocessing](#preprocessing): each phrase keeps its own, and its
encodings are scored against its preprocessed recording. Each phrase is
checked with the earlier phrases' results in place, so the results fit
together.

**Start** begins the search. While it runs, the window shows:

- the phrase being optimised and the candidates tried so far;
- the best so far on the quality rating's bar, with a dashed mark at the
  score it started from;
- the best so far's size and the settings it changes.

**Show every candidate** lists every set of settings tried, with its
size, bit rate, overall score and what became of it (*scored*, *over budget*,
*over limit* or *failed*).

When it finishes with better settings, **Listen** compares them by ear:

- **▶ Original** plays the phrase's frames now, which Revert keeps;
- **▶ Optimised** plays the frames Optimise found, which Accept keeps.

Each plays the whole phrase at the recording's level, as the Encode
tab's waveform plays it, so the two compare fairly. Pressing a button while
it plays pauses it; pressing the other stops it and plays from the
beginning. When several phrases changed, choose which to hear beside
**Listen**. A phrase with no frames before Optimise has only **Optimised**.

**Accept** keeps the results: each phrase's new
settings and frames, with their quality, as one step to undo (**Undo
Optimise HELLO**, or **Undo Optimise 3 Phrases**). **Revert** keeps the
current settings. **Cancel**, or closing the window, stops the search and
changes nothing. While the window is open the main window cannot be
edited.

### How the search works

Each phrase's search starts from its current settings, then tries:

1. **A coarse grid** around them over the options with the most effect:
   the other kind of pre-emphasis, the voicing threshold ±0.05, the
   voicing evidence ±2, the repeat trade-off halved and doubled, and the
   target RMS ±4 (or the target peak ±16, if the phrase sets one): 162
   combinations, or 54 with repeat frames off.
2. **Steps along each option** from the best so far: one step down and
   one up along every option but the peak limit (and the target it does
   not use: the RMS when a target peak is set, the peak otherwise), all
   tried together; on/off options are switched. It moves to the best of
   them if that is better, and otherwise halves the steps, until they are
   as small as they go.

It tries at most 400 settings per phrase, never the same settings twice.
Candidates run in parallel on every core, and the result is the same
whatever the number of cores: the best has the highest score, and a tie
goes to the settings tried first.

Optimise takes about 12 seconds per second of speech on one core, and
about 1.4 seconds on 16 cores: a phrase of a few seconds takes a few
seconds on a computer with many cores, and under a minute on one.
