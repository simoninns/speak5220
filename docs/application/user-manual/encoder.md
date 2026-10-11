# The encoder and Optimise

Under the [waveforms](waveforms.md), with a splitter between them, the
[main window](main-window.md) shows the selected phrase's **encoder
panel**: what its frames add up to, how good they sound, and the
settings they are made with. **Optimise** searches those settings for
the best quality the project has room for.

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
    (frames whose energy was lowered to stay under `peak_limit`, and
    frames made silent) is known only for encodings made in this session.
- **The quality scale** (below).
- **The settings** (below).
- **Re-encode** (Encode for a phrase with no frames) encodes the phrase
  now with the settings shown. It is available only when there is
  something to redo: the phrase has no frames, they are stale, or a
  settings change is waiting to be applied. **Optimise...** opens the
  [Optimise](#optimise) window for the phrase. **Reset to Project
  Defaults** removes the phrase's own settings, so it follows the
  project's defaults again, and re-encodes it.

## The quality scale

PESQ (ITU-T P.862, as MOS-LQO) compares the encoded speech with the
original and scores it from 1.0 to 4.5. No TMS5220 encoding comes near
4.5: a typical encoding scores about 2.0, and an ideal LPC coder with
nothing quantised only about 2.3. So the score is shown against the
phrase's own reference, the **anchor**: the score of an unquantised
LPC-10 vocoder of the same recording.

`[ Bad | Poor | Fair |  Good ▼  | Best ]`

`PESQ 2.08 (LPC reference 2.31): Good`

The score relative to the anchor is (score − 1.0) / (anchor − 1.0): 0
for PESQ's floor, 1 for an encoding that matches the anchor. The anchor
is a reference, not a limit: an encoding can score above it, and after
Optimise about a quarter do. So the bar runs on a quarter
past the anchor: the **Best** band starts at the anchor, four fifths of
the way along, and a score beyond it shows how far (1.25 or more is at
the right end).

The bar is divided into five bands, each named inside it (the top one as
**Best**), and the line under the bar gives the band in full, so the
scale reads the same without colour. The bar is at most 520 pixels
wide, so it can be taken in at a glance:

| Band | Score relative to the anchor |
|---|---|
| Bad | below 0.40 |
| Poor | 0.40 to below 0.60 |
| Fair | 0.60 to below 0.75 |
| Good | 0.75 to below 1.00 |
| As good as LPC or better | 1.00 (the anchor) and above |

With the default settings a typical encoding is *Good*; Optimise moves
most encodings up within *Good*, and some past the anchor. PESQ was not
validated for vocoders, so use the scale to rank encodings of a phrase,
not to predict what a listener would say; hover over it for a reminder.

PESQ treats breaths and room noise as part of the speech, and gives
quiet stretches a little more weight than loud ones. If a recording has
audible breaths, Optimise may favour settings that reproduce them; use
the [squelch](#the-squelch) to leave them out.

The score and the anchor are measured when a phrase is encoded in the
window, and saved with its frames. For frames made elsewhere (by
`speak5220-cli encode`, for example) they are measured in the background
for display, and the line says *Measuring quality...* meanwhile.

## The settings

One row per [encoder option](../reference/encoder-options.md), in two
columns. Hover over a label for what the option does and its default.
Numbers are set with sliders, with the value and its unit beside them
(`20 codes`, `45 dB`, `30 ms`; the ratios read `off` at 1). A small tick
under each slider marks the default, so an option moved from it is easy
to see. A slider covers the range where useful values lie (the reference
gives each one); a value outside it, from the project file or Optimise,
widens the slider to include it. Click a slider and use the arrow keys
for single steps, Page Up and Page Down for larger ones. On/off options
are check boxes; the target peak and the [squelch](#the-squelch) have a
check box that turns them on, and the pre-emphasis a choice of chirp or
first order, whose coefficient the slider beside it sets.
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

## The squelch

**Squelch**, off by default, makes the quiet stretches of the recording
silent before it is encoded: everything more than the set number of dB
below the recording's loudest sound, such as breaths, lip noise and room
noise. Tick its check box to turn it on (it starts at `30 dB`). The
quality scale then compares the encoding with the squelched recording,
so an encoding that reproduces a breath scores *lower*, and Optimise
looks for settings that keep it out.

- A lower number squelches more: `20 dB` mutes anything a tenth as loud
  as the loudest sound; `40 dB` only sounds a hundredth as loud.
- Sound is kept for 60 ms after it falls below the level and from 10 ms
  before it rises above it, with a short fade, so word endings and
  onsets are not clipped. Loudness alone cannot tell a breath from a soft
  *h* or a trailing *s*, though: if a word loses its ending, raise the
  number.
- Changing the squelch changes what the quality is measured against, so
  scores made with different squelches do not compare, and the anchor is
  measured again. Optimise keeps the phrase's squelch as it is.
- The squelch is not the **Silence gate**, which only decides which
  frames of the encoding are silent (45 dB below the loudest frame by
  default) and leaves the quality measured against the whole recording.

The stretches the squelch mutes are [shaded on the original
waveform](waveforms.md#the-squelch), and the shading follows the slider as
it moves, so the level can be set by eye: breaths are often 25 to 35 dB
below the loudest sound, so a breath that `30 dB` leaves unshaded may
need `25 dB` or less. The recording itself is not changed: the waveform
shows and plays it as imported.

## Quiet pauses

A pause inside a phrase is made of silence frames, which stop the sound
but leave the chip's filter as the sound before it set it. On a real
TMS5220 the filter then never quite comes to rest: it keeps flicking a
code or two either side of zero, and the chip turns that into a faint buzz
at about the pitch of the voice, roughly 23 dB below the speech, for the
whole pause (datasheet §12.2, measured on the chip).

**Quiet pauses**, on by default, settles the filter: in every pause of
three silence frames or more, the second becomes one voiced frame at the
lowest energy with neutral filter settings, and the rest of the pause is
then truly silent. The first frame of the pause is left alone, so the
sound before it fades out as it did. It costs 46 bits a pause, a few
percent of a phrase with pauses. Shorter pauses are left as they are:
they ring for at most 50 ms. Optimise leaves the setting as it is.

The [squelch](#the-squelch) makes long pauses common, so leave quiet
pauses on unless the frames must match another encoder's exactly. Phrases
encoded before quiet pauses existed are shown as *stale*; re-encode them
(**Phrase > Re-encode**) to quieten their pauses.

!!! warning "The pauses you hear in the window are not the chip's"
    The waveforms and **Listen** play the emulator, and at this level, a
    code or two, the emulator cannot be trusted: how the chip rounds its
    smallest values is not known (datasheet §8.8). A pause can buzz on the
    chip and be silent in the window, or the other way round. Measured on
    a real TMS5220, a pause with quiet pauses on was silent every time, even
    where the window plays a faint whine in it (datasheet §12.2). Judge
    the pauses on a real chip, not by ear in the window.

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
searches each phrase's settings for the encoding with the best PESQ.

Before it starts, two limits can be set:

- **Limit each phrase to ... bytes**: only settings whose frames fit are
  chosen. It starts at the largest of the phrases' sizes now, so ticking
  it alone finds better settings that are no bigger.
- **Trade quality for size**: choose the settings with the best PESQ less
  this much for each kbit/s, so a smaller encoding can win over a
  slightly better one. With 0.1, saving 1 kbit/s is worth 0.1 of PESQ.

Whatever the limits, a result that would take the project past its 64
PHROMs is never chosen. Optimise does not change the
[squelch](#the-squelch): each phrase keeps its own, and its encodings are
scored against its squelched recording. Each phrase is checked with the earlier phrases'
results in place, so the results fit together.

**Start** begins the search. While it runs, the window shows:

- the phrase being optimised and the candidates tried so far;
- the best so far on the phrase's quality scale, with a dashed mark at
  the score it started from;
- the best so far's size and the settings it changes.

**Show every candidate** lists every set of settings tried, with its
size, bit rate, PESQ and what became of it (*scored*, *over budget*,
*over limit* or *failed*).

When it finishes with better settings, **Listen** compares them by ear:

- **▶ Original** plays the phrase's frames now, which Revert keeps;
- **▶ Optimised** plays the frames Optimise found, which Accept keeps.

Each plays the whole phrase at the recording's level, as the encoded
waveform pane plays it, so the two compare fairly. Pressing a button while
it plays pauses it; pressing the other stops it and plays from the
beginning. When several phrases changed, choose which to hear beside
**Listen**. A phrase with no frames before Optimise has only **Optimised**.

**Accept** keeps the results: each phrase's new
settings and frames, with their PESQ, as one step to undo (**Undo
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
