# The Encode view

The **Encode** tab (**View > Encode**, Ctrl+3) shows what the TMS5220
makes of the selected phrase's frames, and below it the **encoder panel**:
the encoding's state, size and quality, and the encoder's settings.

Encoding is the last of the three stages of making a phrase: the encoder
turns the [preprocessed](preprocess.md) recording into TMS5220 frames,
25 ms each, and the quality rating measures how close they come to it.
How the encoder works is described in
[How the encoder works](../../technical/encoder/how-it-works.md), and what
it always does, whatever the settings, in
[encoder behaviour](../reference/encoder-behaviour.md).

## The waveform

The waveform, titled **Encoded**, is what a TMS5220 makes of the phrase's
frames. The frames are spoken by the emulated chip as a PHROM word is
spoken (datasheet §12.7): loaded into a speech ROM and started with Speak.
Its controls (play, play start, zoom) are those of every view, described
in [Waveforms](waveforms.md), with one more:

| Control | What it does |
|---|---|
| **Frame grid** | On by default: draws a line between one 25 ms frame and the next. Untick it to hide the lines; the frames' shading stays |

The waveform shows the frames stored in the project. A phrase not yet
encoded says *Not encoded yet: press Encode below*; frames made with other
settings are still shown, marked **stale** beside the title, until the
phrase is re-encoded. A [locked](phrases.md#importing-phrom-images) phrase
is marked **locked**.

**Frames.** The waveform is shaded by the kind of each frame (voiced,
unvoiced, repeat, silence). A frame is shaded where the chip uses its
values: from half-way through its change from the previous frame to
half-way through its change to the next (datasheet §8.5, §8.7, §12.2).
Hover over it to see the frame there, for example:

`Frame 12: voiced, energy 9, pitch 31, K 20 15 9 8 7 6 5 4 3 2`

The numbers are the frame's coded indices (datasheet §8.1): a silence or
stop frame has none, a repeat frame its energy and pitch, an unvoiced frame
its energy and K1 to K4, and a voiced frame all of them. The status bar
shows the same, after the time and amplitude.

**Lining up.** The chip starts speaking about 25 ms after it reads the
first frame, and the encoder leaves out silence at the start of the audio.
So that the same sound is at the same place on every tab, the encoded audio
is lined up with the original by the same search that the quality measures
use. The encoded waveform can show a little before the original starts
(the chip's idle start) or after it ends.

**Level.** The chip's output is much quieter than a normalised recording:
its DAC codes take a quarter of full scale. So that the two compare like
with like, the encoded waveform is shown and played at the original's
level: its loudness over the speech is matched to the original's. The note
beside the title gives the gain, for example **level matched to the
original (14.6 dB)**. Only the display and playback change; the frames,
and anything exported, are as the chip makes them.

The other notes beside the title come from the
[listening options](#listening-options) below: **12 samples clipped by
the DAC**, **I/O pin, 10-bit** or **BBC Micro output filter**.

## The encoder panel

From the top:

| Part | What it shows or does |
|---|---|
| **State** | *Up to date*; *Stale: made with other settings; re-encode to update* (the frames were made with other settings, or by another version of the encoder); *Not encoded yet*; *Settings changed: applied after a pause...*; *Encoding...*; *Locked: imported frames are never re-encoded* (only the statistics are shown); *No source audio to encode* |
| **Statistics** | [Size, frame kinds and level](#the-statistics) |
| **Quality rating** | [How close the encoding is to the preprocessed recording](#the-quality-rating) |
| **Settings for** | **This phrase** or **Project defaults**, as on the Preprocess view. See [phrase settings and project defaults](phrases.md#phrase-settings-and-project-defaults) |
| **Encoder** settings | The three [basic settings](#basic-settings) |
| **Advanced settings** | Shows or hides the seven [advanced settings](#advanced-settings) |
| **Re-encode** | Encodes the phrase now with the settings shown. Called **Encode** for a phrase with no frames. Available only when there is something to redo: the phrase has no frames, they are stale, or a settings change is waiting to be applied (which it applies at once) |
| **Optimise...** | Opens the [Optimise](optimise.md) window for the phrase |
| **Reset to Project Defaults** | With **This phrase**: removes the phrase's own settings, of both views, so it follows the project's defaults again, and re-encodes it |
| **Reset to Encoder Defaults** | With **Project defaults**: puts every project default, of both views, back to the encoder's own default. Phrases that follow them become stale |

### The statistics

For example:

`104 frames, 3063 bits (383 B), 1178 bit/s`

`voiced 41% · unvoiced 20% · repeat 19% · silence 19%`

`peak code 120 of 127 · limiter lowered 3 frames, silenced 0`

- The frames before the stop, every frame's bits with the stop, the bytes
  they pack to (what the phrase costs in a PHROM, before its name and
  pointer), and the bits per second of speech.
- The share of each kind of frame.
- The loudest code the chip's output reaches (its DAC's range is ±127),
  and what the encoder's limiter did: the frames whose energy it lowered
  so the output does not clip too much ([the level](#target-rms)), and
  the frames it made silent. *limiter idle* when it did nothing. This part
  is known only for encodings made in this session.

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
  swapped. The pitch tracker the rating uses can itself be an octave out
  on some very low or very high voices, so a phrase whose pitch sounds
  right can still be marked down here.
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

*ESTOI too short* means the phrase has too little speech for it. Hover
over the rating for a reminder of what each part measures.

The rating means the same on every phrase. An ideal LPC coder with
nothing quantised is *Good* on nearly every phrase; with the default
settings encodings are *Good* or *Fair* in about equal parts, and *Poor*
on a few. The envelope is usually the weakest part, since the chip's K
parameters are coarsely quantised. How the measures work is in
[Quality](../../technical/encoder/quality.md).

The quality is measured when a phrase is encoded in the window, and saved
with its frames. For frames made elsewhere (by `speak5220-cli encode`, for
example), or saved by an earlier version of the measures, it is measured
in the background for display, and the line says *Measuring quality...*
meanwhile. A locked phrase, with no recording to compare with, has no
rating.

## The settings

The encoder's ten settings are in two groups. The three **basic
settings** change the sound or the size most clearly, and are always
shown. The seven **advanced settings** fine-tune the encoding: click
**Advanced settings** to show them, and again to hide them. Speak5220
remembers the choice. Hidden settings still apply: when any of them
differ from their default, the button says how many, for example
**Advanced settings (2 changed)**, as it can after Optimise or with a
project from elsewhere.

Each setting is a row with a slider, its value, and a **↺** button that
puts it back to its default; **Resonance hold** also has a check box that
turns it on and off. How the rows work, and when a change is applied, is
described under [the settings rows](phrases.md#the-settings-rows).

### At a glance

The key is the setting's name in the [project file](../reference/project-file.md)
and in `speak5220-cli new --option` ([command-line tool](cli.md)). The
slider covers the range where useful values lie; any value in the full
range can be set in the project file, and the slider widens to show it.
[Optimise](optimise.md) searches all but `resonance_hold_db` and
`voicing_min_run`.

| Key | Setting | Type | Default | Range | Slider |
|---|---|---|---|---|---|
| `target_rms` | Target RMS | Number | 20 | 1 to 100 | 5 to 50 codes, steps of 0.5 |
| `repeat_lambda` | Repeat trade-off | Number | 300 | 0 to 1000 | 0 to 600, steps of 5 |
| `voicing_threshold` | Voicing threshold | Number | 0.6 | 0 to 1 | 0.05 to 0.7, steps of 0.01 |
| `gate_db` | Silence gate (*advanced*) | Number | 45 | 10 to 90 | 20 to 70 dB, steps of 0.5 |
| `window` | Analysis window (*advanced*) | Whole number | 256 (32 ms) | 80 to 480 | 120 to 400 (15 to 50 ms), steps of 8 |
| `lag_window_hz` | Lag window (*advanced*) | Number | 60 | 0 to 400 | 0 to 200 Hz, steps of 5 |
| `resonance_hold_db` | Resonance hold (*advanced*) | Number, or `null` (off) | 14 | 3 to 40 | 6 to 30 dB, steps of 0.5 |
| `voicing_evidence` | Voicing evidence (*advanced*) | Number | 5 | 0 to 20 | 0 to 12, steps of 0.25 |
| `voicing_min_run` | Shortest voicing run (*advanced*) | Whole number | 2 | 1 to 8 | 1 to 6 frames, steps of 1 |
| `level_agc` | Slow gain control (*advanced*) | Number | 1.25 | 1 to 10 | 1 (off) to 6, steps of 0.05 |

How the defaults were chosen is in
[encoder behaviour](../reference/encoder-behaviour.md#how-the-defaults-were-chosen).

### Basic settings

#### Target RMS

**Target RMS** (`target_rms`, `20 codes` by default) sets how loud the
encoding is: the RMS of the chip's output over the sounding frames, in DAC
codes, measured after a 279 Hz high-pass, the one in the BBC Micro's
speech path. Bass does not count, since a small speaker gives little of it
back: a recording with a lot of bass is encoded louder than one without
for the same setting, so the two sound about as loud on a BBC Micro. At
`20`, Acorn's own phrases, encoded from the chip's output, come back as
loud as they are.

The encoder scales the whole phrase to this level, then chooses each
frame's energy code. The loudest peaks may clip a little, as Acorn's
phrases do (they drive the DAC past its ±127 on half their words): the
encoder's limiter lets at most 1 sample in 1000 pass ±127, and none pass
160. Where a phrase would clip more, it turns down the frames that clip
most. A higher Target RMS makes it turn down more frames, which flattens
the loud syllables, and the quality rating falls; from `24` up it falls on
many recordings. The [statistics](#the-statistics) show the peak and what
the limiter did.

For phrases to be played on a BBC Micro, choose the **BBC Micro**
[preset](preprocess.md#preset) on the Preprocess view too, which makes
them louder to the ear for the same Target RMS.

#### Repeat trade-off

**Repeat trade-off** (`repeat_lambda`, `300` by default) sets how many
**repeat frames** the encoding uses. A repeat frame keeps the previous
frame's filter (its K parameters) and sends only a new energy and pitch:
11 bits instead of 50 for a voiced frame, or 29 for an unvoiced one.

The encoder splits each run of voiced or unvoiced frames into groups of
up to 8 frames, each a full frame followed by repeats that share its
filter (the first frame's own, or one fitted to the whole group). Sharing
a filter saves bits but takes each frame a little further from the
recording: its prediction error rises. The encoder chooses the groups with
the least cost, the bits plus this setting times the extra error in dB,
so the setting is how many bits a repeat must save for each dB of error it
adds.

A lower value accepts repeats more readily: fewer bytes and a lower
quality. A higher value sends a new filter more often. At `0` the error
does not count, and the encoding is as small as the groups allow. The
default, `300`, gives about as many repeat frames as Acorn's own phrases
have.

#### Voicing threshold

**Voicing threshold** (`voicing_threshold`, `0.6` by default) decides
between buzz and hiss. Each frame of speech is either **voiced**, a buzz
at the voice's pitch (vowels, *m*, *n*), or **unvoiced**, a hiss (*s*,
*f*, *sh*). The encoder measures how periodic the sound is (the YIN
periodicity measure, CMNDF, where lower is more periodic), and the
threshold is the level at which voiced and unvoiced cost the same in its
search for the best path of pitch and voicing through the phrase. The
threshold is moved for each frame by the [voicing
evidence](#voicing-evidence).

A higher value gives more voiced frames: a sound between the two, such as
a *z* or a breathy vowel, is more likely to buzz. A lower value gives more
hiss. With the default, Acorn's own phrases, encoded from the chip's
output, come back with nearly the same voicing as their own frames (97 %
of frames agree).

### Advanced settings

#### Silence gate

**Silence gate** (`gate_db`, `45 dB` by default) decides which frames of
the encoding are silent, and where the speech starts and ends. The encoder
measures the level of the preprocessed recording, after a 60 Hz high-pass
that removes hum, every 2.5 ms over 10 ms. The gate is this many dB below
the loudest 25 ms frame, or twice the background noise (the level the
quietest 5 % of the recording is under) if that is higher. The speech
starts and ends where the level first and last passes the gate, and the
frames between at or under it become silence frames.

A higher value keeps quieter sounds, such as soft endings and breaths; a
lower one makes more of the phrase silent. It does not change the
recording: to take breaths and noise out of the recording itself, use the
[noise gate](preprocess.md#noise-gate) on the Preprocess view.

#### Analysis window

**Analysis window** (`window`, `256 samples`, shown as `32 ms`, by
default) is the length of the stretch of recording each frame's filter is
worked out from. The encoder takes a Hamming window of this many samples,
centred on each 25 ms frame, and finds the frame's ten K parameters from
it by linear prediction (autocorrelation and the Levinson recursion).

A longer window gives a steadier filter that follows quick changes, such
as a consonant's start, less closely; a shorter one follows them more
closely but is noisier, especially on low voices, whose pitch periods are
long.

#### Lag window

**Lag window** (`lag_window_hz`, `60 Hz` by default) widens the peaks of
each frame's filter (the formants). Before the filter is worked out, the
recording's autocorrelation is multiplied by a Gaussian lag window of this
bandwidth. Wider peaks are less sensitive to the coarse steps of the
chip's K parameters, and to a peak landing on one harmonic of a high
voice. `0` turns it off.

#### Resonance hold

**Resonance hold** (`resonance_hold_db`), on at `14 dB` by default,
prevents a pop as a high voice fades out. A high voice can make the
encoder give a voiced frame a filter with one very strong, narrow
resonance, low in pitch, and a low energy to match. The chip glides from
one voiced frame's settings to the next, but out of such a filter most of
the change in loudness comes at the very start of the glide: when the next
frame's filter is an ordinary one, the sound drops suddenly, heard as a
pop.

Where the sound is fading, from one voiced frame to the next, and the next
frame's filter has more than this many dB less gain, the encoder keeps the
resonant filter for that frame instead (it becomes a repeat frame), and the
sound fades out through it. The filter changes a frame later, when the
sound is already quieter. It costs no bits, since the held frame is a
repeat, which is shorter, and changes about one encoding in ten.

A lower value catches more places, at a small cost in how closely the
sound follows the recording. Untick it to turn it off; that is only
useful to compare the frames with another encoder's. Optimise leaves it
as it is.

#### Voicing evidence

**Voicing evidence** (`voicing_evidence`, `5` by default) is how much
the voicing decision weighs two other clues besides periodicity: the
spectral tilt (voiced sounds have more of their energy low, hiss high) and
the zero crossings (hiss crosses zero far more often). For each frame they
move the [voicing threshold](#voicing-threshold) up where the sound looks
voiced and down where it looks like hiss, by this much weight. `0` uses
periodicity alone.

#### Shortest voicing run

**Shortest voicing run** (`voicing_min_run`, `2 frames` by default)
smooths the voicing. A voiced sound (a vowel's buzz) and an unvoiced one
(an *s*'s hiss) sound very different on the chip, and a frame or two of
hiss inside a vowel, or of buzz inside a hiss, is heard as roughness. Any
run of voiced or unvoiced frames shorter than this, with the other kind on
both sides, takes the voicing of its neighbours. A lone unvoiced frame in
a vowel becomes voiced, at the pitch around it; a lone voiced frame in a
hiss becomes unvoiced. Runs at the start and end of a phrase are kept.
`1`, shown as *off*, turns it off.

So by default a single frame of the other kind is smoothed away. `3
frames` brings the encoder's voicing nearer Acorn's own phrases, but over
a corpus of 243 recordings it lowers more scores than it raises.
Optimise leaves it as it is.

#### Slow gain control

**Slow gain control** (`level_agc`, `1.25:1` by default) brings quiet
words up, so that a phrase whose words were spoken at different levels
comes out more even. The encoder follows the phrase's loudness with a
slow envelope, smoothed over 0.75 s (30 frames) of sounding frames, and
compresses it by this ratio about its median: at `2:1`, a word 10 dB
quieter than the median comes out 5 dB quieter, and a louder word is
brought down in the same way. It acts on whole
words, not on each syllable, so the speech keeps its own rise and fall.
`1`, shown as *off*, turns it off.

## Listening options

The **Audio** menu sets how the encoded audio is heard and shown, in the
Encode view and in the [Optimise](optimise.md) window. These choices
change only what you see and hear: not the frames, the quality rating or
anything exported. They are not part of the project; the window
remembers them for next time. Changing one stops the encoded audio if it
is playing.

| Menu | Choices | Default |
|---|---|---|
| **Audio > Chip Output** | **SPEAKER (8-bit DAC)**, **I/O (10-bit codes)** | SPEAKER |
| **Audio > Output Filter** | **None**, **BBC Micro** | None |
| **Audio > Chip Clock** | **560 kHz** to **720 kHz** in 20 kHz steps | 640 kHz (nominal) |

### Chip output

The chip gives the same sound on two pins. Its **I/O** pin sends each
sample as a 10-bit code, −512 to +511. Its **SPEAKER** pin drives a DAC
that takes only 8 bits, so it clips every code to −128 to +127: loud
speech that goes past that range is flattened at the top and bottom. The
DAC's steps are also uneven
([datasheet §8.2](../../technical/datasheet/dac-and-audio.md#82-da-conversion)):
the step between codes −1 and 0, where all eight of its input bits
change, is about six normal steps. Quiet sound, which keeps crossing
between −1 and 0, is coarsened by that step into a gritty hiss once an
audio stage such as the BBC Micro's removes the offset. Most computers
play the SPEAKER pin, but the sound can also be regenerated from the I/O
pin's codes.

| Choice | What you see and hear |
|---|---|
| **SPEAKER (8-bit DAC)** | Each code clipped to −128 to +127 and converted with the DAC's measured steps. If any samples are clipped, the note beside the title says how many, for example **12 samples clipped by the DAC** |
| **I/O (10-bit codes)** | The codes unclipped. The note says **I/O pin, 10-bit** |

The DAC's steps are those measured on one chip (the datasheet's
Table 8-2). Each chip's are a little different: one BBC Micro measured
through its audio stage had a step of about seven between −1 and 0. The
level match is measured on the unclipped codes, so it is the same for
both pins.

### Output filter

On its own, the chip's output looks lopsided next to the original. Its
voiced sound starts each pitch period with a pulse that only goes one way,
and nothing removes the steady offset this causes. A real computer sends
the output through an audio stage that removes the offset and smooths the
steps between samples.

| Choice | What it models |
|---|---|
| **None** | The chip's samples as they are |
| **BBC Micro** | The BBC Model B's speech path, after Acorn's sound schematic for the Model B: a first-order 279 Hz high-pass, which removes the offset and cuts the bass, and a reconstruction low-pass (two order-2 Butterworth sections) that is −3 dB at about 5.5 kHz. The note beside the title says **BBC Micro output filter** |

The filter comes after the [chip output](#chip-output): with the SPEAKER
pin, the clipped samples are filtered. The level is unchanged in the
passband, and is still matched to the original.

With a filter chosen, the encoded audio plays at 48 kHz: each sample is
held for its whole period, as the chip's DAC holds it, then passed through
the filter. So you also hear what is left of the steps between samples
above 4 kHz, as the computer's speaker would play them. The waveform
still draws one point per 8 kHz sample: the filtered output averaged over
that sample's period.

### Chip clock

The chip's sample rate is its oscillator's rate divided by 80: 8000
samples a second at the nominal 640 kHz
([datasheet §2](../../technical/datasheet/clock.md)). On a real board the
oscillator is set by a resistor and capacitor, so it can run somewhat
fast or slow, and some machines clock the chip off its nominal rate on
purpose. A different clock changes everything at once: the pitch, the
formants and the length of every frame move by the same proportion, like
a tape played at the wrong speed.

| Clock | Sample rate | Change |
|---|---|---|
| 560 kHz | 7000 Hz | −12.5% |
| 580 kHz | 7250 Hz | −9.4% |
| 600 kHz | 7500 Hz | −6.3% |
| 620 kHz | 7750 Hz | −3.1% |
| 640 kHz (nominal) | 8000 Hz | the default |
| 660 kHz | 8250 Hz | +3.1% |
| 680 kHz | 8500 Hz | +6.3% |
| 700 kHz | 8750 Hz | +9.4% |
| 720 kHz | 9000 Hz | +12.5% |

The clock changes only the encoded audio played. With an
[output filter](#output-filter), the filter's corners stay at the same
frequencies, as the computer's audio stage does not change with the
chip's clock; the audio then plays at 48 kHz times the clock's change
(54 kHz at 720 kHz). The waveform is unchanged, and the playhead follows
the audio at its new rate. The original and preprocessed recordings play
at 8 kHz.

!!! warning "The pauses you hear in the window are not the chip's"
    The waveforms and the Optimise window play the emulator, and at the
    level of a code or two, the emulator cannot be trusted: how the chip
    rounds its smallest values is not known (datasheet §8.8). A pause can
    buzz on the chip and be silent in the window, or the other way round.
    Judge the pauses on a real chip, not by ear in the window (see
    [quiet pauses](../reference/encoder-behaviour.md#quiet-pauses)).
