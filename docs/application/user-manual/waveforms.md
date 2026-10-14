# Waveforms and playback

Below the name and description, the [main window](main-window.md) shows
the selected phrase on three tabs, one for each stage of making it.
Under the tabs are two areas, with a grip between them:

- the **waveform area**: the tab's waveform, with its own scroll bar and
  controls under it;
- the **settings area**: the tab's settings. When they do not fit, the
  area scrolls rather than growing.

Drag the grip to share the height between the two areas. The areas keep
their sizes whichever tab is shown, so the waveforms stay in the same
place: click between the tabs to compare them.

The tabs:

| Tab | Waveform | Settings |
|---|---|---|
| **Original** | **Original (8 kHz)**: the phrase's audio as stored in the project, after resampling to 8 kHz (see [importing audio](projects.md#importing-audio)) | None (the area says *No settings*) |
| **Preprocess** | **Preprocessed (8 kHz)**: that audio as the phrase's [preprocessing](encoder.md#preprocessing) prepares it for the encoder ([below](#the-preprocessed-recording)) | The [preprocessing settings](encoder.md#preprocessing) |
| **Encode** | **Encoded**: what a TMS5220 makes of the phrase's frames. The frames are spoken by the emulated chip as a PHROM word is spoken (datasheet §12.7): loaded into a speech ROM and started with Speak | The [encoder panel](encoder.md) |

Each tab's controls are a play button for its waveform, the play-start
reset, and the zoom controls (with **Frame grid** on the Encode tab).
With several phrases selected, or none, the waveforms are empty, and show the Speak5220 logo faintly in their place. Speak5220 remembers the tab last shown between sessions.

!!! note
    The encoded waveform shows the frames stored in the project. A phrase
    that has not been encoded yet says so; encode it with **Encode** in
    the [encoder panel](encoder.md) below. Frames made with other settings
    are still shown, marked **stale** beside the waveform's title, until the
    phrase is re-encoded.

## The preprocessed recording

The Preprocess tab shows and plays the recording as the phrase's
[preprocessing](encoder.md#preprocessing) prepares it: what the encoder
aims at, and what the quality is measured against. The note beside the
title names the preprocessing, for example *bass cut 200 Hz, treble tilt
0.4, de-esser 10 dB*, or says *no
preprocessing: the same as the original*.

The preprocessed recording follows the settings below it as they are
moved, before the change is encoded, keeping the zoom; a change stops it
if it is playing. A treble tilt can take loud sounds past full scale;
the preprocessed recording is then shown and played scaled down to fit,
and the note says by how much (*scaled by −2.0 dB to fit*). Only the
display and playback are scaled: the encoder's level comes from its own
target RMS.

## The noise gate

When the phrase has a [noise gate](encoder.md#the-noise-gate), the
stretches it turns down by more than 6 dB (to half their size or less)
are shaded on the preprocessed recording: dimmed and hatched. The
waveform itself shows the gate fading the sound down and back up. The
note gives the gate's level and speed, for example *noise gate 30 dB,
100 ms: shaded stretches are turned down* (or that nothing is quiet
enough to gate). Hovering over a shaded stretch says it is gated. The
shading follows the gate's sliders as they move, before the change is
encoded, so the gate can be set by eye: low enough to cover the breaths,
not so low that it eats into the words. The Original tab still shows and
plays the recording as imported.

## Lining up the tabs

The chip starts speaking about 25 ms after it reads the first frame, and
the encoder leaves out silence at the start of the audio. So that the
same sound is at the same place on every tab, the encoded audio is lined
up with the original by the same search that the quality measures use.
Time on every tab counts from the start of the original audio; the
encoded waveform can show a little before it (the chip's idle start) or
after it.

## Levels

The chip's output is much quieter than a normalised recording: its DAC
codes take a quarter of full scale, and the encoder aims for a moderate
level (`target_rms`, see the [encoder options](../reference/encoder-options.md)).
So that the two compare like with like, the encoded waveform is shown and
played at the original's level: its loudness over the speech is matched
to the original's. The note beside
the waveform's title gives the gain, for example
**level matched to the original (14.6 dB)**. Only the display and playback
change; the frames, and anything exported, are as the chip makes them.

## Chip output

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
pin's codes. Choose which under **Audio > Chip Output**:

| Pin | What you see and hear |
|---|---|
| SPEAKER (8-bit DAC) | Each code clipped to −128 to +127 and converted with the DAC's measured steps, the default. If any samples are clipped, the note beside the encoded waveform's title says how many, for example **12 samples clipped by the DAC** |
| I/O (10-bit codes) | The codes unclipped. The note says **I/O pin, 10-bit** |

The DAC's steps are those measured on one chip (the datasheet's
Table 8-2). Each chip's are a little different: one BBC Micro measured
through its audio stage had a step of about seven between −1 and 0. The
level match above is measured on the unclipped codes, so it is the same
for both pins. Like the output filter below, the choice changes the
encoded waveform and the encoded audio played in the main window and in the
Optimise window, but not the frames, the quality measures or anything
exported. Changing it stops the encoded audio if it is playing.

## Output filter

On its own, the chip's output looks lopsided next to the original. Its
voiced sound starts each pitch period with a pulse that only goes one way,
and nothing removes the steady offset this causes. A real computer sends
the output through an audio stage that removes the offset and smooths the
steps between samples. To hear and see the encoded audio as a particular
computer would play it, choose that computer under
**Audio > Output Filter**:

| Filter | What it models |
|---|---|
| None | The chip's samples as they are, the default |
| BBC Micro | The BBC Model B's speech path: a 279 Hz high-pass, which removes the offset and cuts the bass, and a reconstruction filter that is −3 dB at about 5.5 kHz. Its components are given in the [audio output stage](../../technical/hardware/audio-output-stage.md) |

The filter changes the encoded waveform and the encoded audio played in the
main window and in the Optimise window. The original and preprocessed
recordings are not filtered.
With a filter chosen, the note beside the encoded waveform's title names it,
for example **BBC Micro output filter**. The level is unchanged in the
passband, and is still matched to the original as described above.

The filter comes after the [chip output](#chip-output): with the SPEAKER
pin, the clipped samples are filtered.

With a filter chosen, the encoded audio plays at 48 kHz: each sample is
held for its whole period, as the chip's DAC holds it, then passed through
the filter. So you also hear what is left of the steps between samples
above 4 kHz, as the computer's speaker would play them. The encoded waveform
still draws one point per 8 kHz sample: the filtered output averaged over
that sample's period.

The filter only affects what you see and hear. The frames, the quality
measures and anything exported are unchanged. The window remembers
the choice for next time. Changing it stops the encoded audio if it is
playing.

## Chip clock

The chip's sample rate is its oscillator's rate divided by 80: 8000
samples a second at the nominal 640 kHz
([datasheet §2](../../technical/datasheet/clock.md)). On a real board the
oscillator is set by a resistor and capacitor, so it can run somewhat
fast or slow, and some machines clock the chip off its nominal rate on
purpose. A different clock changes everything at once: the pitch, the
formants and the length of every frame move by the same proportion, like
a tape played at the wrong speed.

To hear the encoded audio as a chip clocked at another rate would play it,
choose the rate under **Audio > Chip Clock**:

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

The clock changes only the encoded audio played, in the main window and in
the Optimise window. With an [output filter](#output-filter), the filter's
corners stay at the same frequencies, as the computer's audio stage does
not change with the chip's clock. The original and preprocessed recordings
play at 8 kHz, and the waveforms, the frames, the quality measures and
anything exported are unchanged. The playhead follows the encoded audio
at its new rate. The window remembers the choice for next time. Changing
it stops the encoded audio if it is playing.

## Frames

The encoded waveform is shaded by the kind of each frame (voiced, unvoiced,
repeat, silence), with a line between one 25 ms frame and the next. Turn
the lines off with **Frame grid** under it. A frame is shaded where the chip
uses its values: from half-way through its change from the previous frame
to half-way through its change to the next (datasheet §8.5, §8.7,
§12.2).

Hover over the encoded waveform to see the frame there, for example:

`Frame 12: voiced, energy 9, pitch 31, K 20 15 9 8 7 6 5 4 3 2`

The numbers are the frame's coded indices (datasheet §8.1): a silence or
stop frame has none, a repeat frame its energy and pitch, an unvoiced frame
its energy and K1 to K4, and a voiced frame all of them.

## Reading the waveform

While the pointer is over any tab's waveform, the left of the
[status bar](main-window.md#the-status-bar) says what is under it:

`0.512 s, sample 4096, amplitude -0.310 to +0.420 · gated`

- the time from the start of the original recording, and the sample
  there (8000 a second; the same in every tab, as their times line up);
- the amplitude there, from -1 to +1 full scale: the lowest and highest
  of the samples under the pointer when it covers several, or the one
  sample when zoomed in close;
- on the encoded audio, the frame there, as in its hover text above; on
  the preprocessed audio, **gated** where the noise gate turns it down.

Beyond the end of the audio it is blank.

## Zoom and scroll

| Control | What it does |
|---|---|
| **+** and **−** under the waveform | Zoom in or out, about the [selection](#selecting-part-of-a-phrase) if there is one, or else the [play start](#the-play-start), which stays in the middle of the waveform |
| Ctrl + mouse wheel | Zoom in or out about the pointer |
| Mouse wheel, or the scroll bar under the waveform | Scroll |
| **Fit** | Show the whole phrase |

With a selection, the first press of **+** zooms in until the
selection fills the waveform; later presses zoom further into its
middle. A phrase with neither (one with no source audio) zooms about the
middle of the waveform.

A waveform fits the whole phrase until you zoom or scroll it, and fits
again when resized. The closest zoom is 16 pixels per sample.

The three tabs always zoom and scroll together, over the span that any
of them covers, whichever tab or control you use. So switching tabs
shows the same stretch of the phrase.

## The play start

The orange line with a flag is where playback starts. It belongs to the
phrase, so it is the same on every tab. Click in any tab's waveform to
move it there; the **|◀** button under each waveform moves it back to
the beginning. Playback goes with it: a waveform that is playing starts
again from the new play start, and one that is paused stops, so pressing
play starts there instead of going on from the pause. Moving it is a
change to the project, saved with it (`play_start` in the [project
file](../reference/project-file.md)), and one step to
[undo](main-window.md#undo-and-redo). A phrase with no source audio has
no play start.

## Selecting part of a phrase

Drag across any tab's waveform to select that stretch: it is tinted,
with a line at each end, on every tab. While there is a selection,
**▶** plays just the selection, and the zoom buttons zoom into it (see
[zoom and scroll](#zoom-and-scroll)). Selecting while a waveform plays
starts it again on the new selection; one that is paused stops.

A click (which moves the play start), Escape in a waveform, or choosing
another phrase clears the selection, and play goes back to playing from
the play start. The selection is not part of the project: it is not
saved, and there is nothing to undo. A phrase with no source audio has
no selection.

## Playing

| Control | What it does |
|---|---|
| **▶** under the waveform | Plays that tab's waveform from the play start, or just the selection; while playing, the button pauses, and pressing it again goes on from where it paused |
| Space | The same, when the waveform has the keyboard focus (click in it, or Tab to it) |

One waveform plays at a time: playing one stops another. The encoded
audio starts from the same sound as the recordings, at the play start. A
green line follows the sound; at the end (of the selection, if there is
one), playback stops and the line goes back to where it started.

Audio plays on the system's default output device, or the one chosen in
[Preferences](main-window.md#preferences). If the device fails while
playing, playback stops and a message says why. If the device does not
take 8 kHz audio, the audio is resampled to the device's own rate first.
With an [output filter](#output-filter), the encoded audio plays at
48 kHz, or is resampled from 48 kHz to the device's own rate. At another
[chip clock](#chip-clock), those rates change with it: 9000 Hz and 54 kHz
at 720 kHz, for example.

## Light and dark

The window follows the desktop's light or dark setting, when it starts and
when the setting changes. The waveforms themselves are drawn the dark
way, in colour on black, so they look the same in either setting, unless
**Waveforms** in [Preferences](main-window.md#preferences) is set to
**As the theme**; the quality rating's bands keep their full red, amber and green. To always show one or the other, choose **View > Theme > Light** or
**Dark**; **Auto** (the default) goes back to following the desktop. The
choice is remembered for next time. On Linux the setting is read from the desktop portal, as GNOME and
KDE Plasma provide it; on Windows and macOS Qt follows the system itself.
