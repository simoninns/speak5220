# Waveforms and playback

Below the name and description, the [main window](main-window.md) shows
the selected phrase on three tabs, one for each stage of making it:

| Tab | Waveform | Settings | Page |
|---|---|---|---|
| **Original** (Ctrl+1) | The recording as imported | None | [The Original view](original.md) |
| **Preprocess** (Ctrl+2) | The recording as preprocessed for the encoder | The preprocessing | [The Preprocess view](preprocess.md) |
| **Encode** (Ctrl+3) | What the TMS5220 makes of the phrase's frames | The encoder panel | [The Encode view](encode.md) |

This page describes what the three have in common: the layout, and the
waveform's controls. Each view's own page describes its waveform and
settings.

## The layout

Under the tabs are two areas, with a grip between them:

- the **waveform area**: the tab's waveform, with its own scroll bar and
  controls under it;
- the **settings area**: the tab's settings. When they do not fit, the
  area scrolls rather than growing.

Drag the grip to share the height between the two areas. The areas keep
their sizes whichever tab is shown, and the play start, the selection,
the zoom and the scroll position are shared by the tabs, so switching
tabs shows the same place in the phrase at the same place on screen:
click between the tabs to compare them. Speak5220 remembers the tab last
shown between sessions.

With several phrases selected, or none, the waveforms are empty and say
*Select one phrase to see it*, with the Speak5220 logo faintly in their
place.

Time on every tab counts from the start of the original recording, and
the tabs are lined up so the same sound is at the same place on each (see
[lining up](encode.md#the-waveform) on the Encode view).

## The controls

Under each waveform:

| Control | What it does |
|---|---|
| **▶** | Plays the tab's waveform from the [play start](#the-play-start), or just the [selection](#selecting-part-of-a-phrase); while playing, the button pauses, and pressing it again goes on from where it paused |
| **\|◀** | Moves the play start back to the beginning |
| **−** | Zooms out, about the selection if there is one, or else the play start |
| **+** | Zooms in, about the selection if there is one, or else the play start |
| **Fit** | Shows the whole phrase |
| **Frame grid** | The Encode tab only: see [the Encode view](encode.md#the-waveform) |

In the waveform:

| Action | What it does |
|---|---|
| Click | Moves the play start there, and clears the selection |
| Drag | Selects that stretch |
| Mouse wheel, or the scroll bar under it | Scrolls |
| Ctrl + mouse wheel | Zooms in or out about the pointer |
| Hover | Shows what is under the pointer in the status bar ([below](#reading-the-waveform)) |
| Space | Plays or pauses, as ▶, when the waveform has the keyboard focus (click in it, Tab to it, or choose its tab in the View menu) |
| Escape | Clears the selection |

## Zoom and scroll

Each step of **+** or **−** zooms by two. With a selection, the first
press of **+** zooms in until the selection fills the waveform; later
presses zoom further into its middle. Without one, the play start stays
in the middle of the waveform. A phrase with neither (one with no source
audio) zooms about the middle.

A waveform fits the whole phrase until you zoom or scroll it, and fits
again when resized. The closest zoom is 16 pixels per sample.

The three tabs always zoom and scroll together, over the span that any
of them covers, whichever tab or control you use.

## The play start

The orange line with a flag is where playback starts. It belongs to the
phrase, so it is the same on every tab. Click in any tab's waveform to
move it there; **|◀** moves it back to the beginning. Playback goes with
it: a waveform that is playing starts again from the new play start, and
one that is paused stops, so pressing play starts there instead of going
on from the pause.

Moving it is a change to the project, saved with it (`play_start` in the
[project file](../reference/project-file.md)), and one step to
[undo](main-window.md#undo-and-redo). A phrase with no source audio has
no play start.

## Selecting part of a phrase

Drag across any tab's waveform to select that stretch: it is tinted,
with a line at each end, on every tab. While there is a selection, **▶**
plays just the selection, and the zoom buttons zoom into it. Selecting
while a waveform plays starts it again on the new selection; one that is
paused stops.

A click (which moves the play start), Escape in a waveform, or choosing
another phrase clears the selection, and play goes back to playing from
the play start. The selection is not part of the project: it is not
saved, and there is nothing to undo. A phrase with no source audio has
no selection.

## Playing

One waveform plays at a time: playing one stops another. The encoded
audio starts from the same sound as the recordings, at the play start. A
green line follows the sound; at the end (of the selection, if there is
one), playback stops and the line goes back to where it started.

Audio plays on the system's default output device, or the one chosen in
[Preferences](preferences.md#audio). If the device fails while playing,
playback stops and a message says why. If the device does not take
8 kHz audio, the audio is resampled to the device's own rate first.

The encoded audio can be heard through a chosen pin of the chip, a
computer's audio stage, or at another chip clock: see
[listening options](encode.md#listening-options).

## Reading the waveform

While the pointer is over any tab's waveform, the left of the
[status bar](main-window.md#the-status-bar) says what is under it:

`0.512 s, sample 4096, amplitude -0.310 to +0.420 · gated`

- the time from the start of the original recording, and the sample
  there (8000 a second; the same in every tab, as their times line up);
- the amplitude there, from -1 to +1 full scale: the lowest and highest
  of the samples under the pointer when it covers several, or the one
  sample when zoomed in close;
- on the encoded audio, the [frame](encode.md#the-waveform) there; on
  the preprocessed audio, **gated** where the
  [noise gate](preprocess.md#noise-gate) turns it down.

Beyond the end of the audio it is blank.

## Light and dark

The waveforms are drawn the dark way, in colour on black, so they look the
same whether the window is light or dark, unless **Waveforms** in
[Preferences](preferences.md#appearance) is set to **As the theme**. The
quality rating's bands keep their full red, amber and green. The window's
own theme is chosen with **View > Theme** (see
[the main window](main-window.md#the-view-menu)).
