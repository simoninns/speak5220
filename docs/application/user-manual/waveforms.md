# Waveforms and playback

Below the name and description, the [main window](main-window.md) shows
the selected phrase as two waveforms, one above the other:

- **Original (8 kHz)**: the phrase's audio as stored in the project,
  after resampling to 8 kHz (see [importing audio](projects.md#importing-audio)).
- **Encoded**: what a TMS5220 makes of the phrase's frames.
  The frames are spoken by the emulated chip as a PHROM word is spoken
  (datasheet §12.7): loaded into a speech ROM and started with Speak.

The two are the same height. Under both are a scroll bar and one row of
controls: a play button for each pane, labelled **Original** and
**Encoded**, then the play-start reset, Frame grid and the zoom
controls. With several
phrases selected, or none, the panes are empty.

!!! note
    The encoded pane shows the frames stored in the project. A phrase
    that has not been encoded yet says so; encode it with **Encode** in
    the [encoder panel](encoder.md) below. Frames made with other settings
    are still shown, marked **stale** beside the pane's title, until the
    phrase is re-encoded.

## Lining up the two

The chip starts speaking about 25 ms after it reads the first frame, and
the encoder leaves out silence at the start of the audio. So that the
same sound is at the same place in both panes, the encoded audio is lined
up with the original by the same search that the quality measure (PESQ)
uses. Time in both panes counts from the start of the original audio; the
encoded pane can show a little before it (the chip's idle start) or after
it.

## Levels

The chip's output is much quieter than a normalised recording: its DAC
codes take a quarter of full scale, and the encoder aims for a moderate
level (`target_rms`, see the [encoder options](../reference/encoder-options.md)).
So that the two compare like with like, the encoded pane is shown and
played at the original's level: its loudness over the speech is matched
to the original's, as the quality measure (PESQ) does. The note beside
the pane's title gives the gain, for example
**level matched to the original (14.6 dB)**. Only the display and playback
change; the frames, and anything exported, are as the chip makes them.

## Frames

The encoded pane is shaded by the kind of each frame (voiced, unvoiced,
repeat, silence), with a line between one 25 ms frame and the next. Turn
the lines off with **Frame grid** under the panes. A frame is shaded where the chip
uses its values: from half-way through its change from the previous frame
to half-way through its change to the next (datasheet §8.5, §8.7,
§12.2).

Hover over the encoded pane to see the frame there, for example:

`Frame 12: voiced, energy 9, pitch 31, K 20 15 9 8 7 6 5 4 3 2`

The numbers are the frame's coded indices (datasheet §8.1): a silence or
stop frame has none, a repeat frame its energy and pitch, an unvoiced frame
its energy and K1 to K4, and a voiced frame all of them.

## Zoom and scroll

| Control | What it does |
|---|---|
| **+** and **−** under the panes | Zoom in or out, about the middle |
| Ctrl + mouse wheel | Zoom in or out about the pointer |
| Mouse wheel, or the scroll bar under the panes | Scroll |
| **Fit** | Show the whole phrase |

A pane fits the whole phrase until you zoom or scroll it, and fits again
when resized. The closest zoom is 16 pixels per sample.

The two panes always zoom and scroll together, over the span that either
one covers, whichever pane or control you use.

## The play start

The orange line with a flag is where playback starts. Click in either pane
to move it there; the **|◀** button under the panes moves it back to the
beginning. Playback goes with it: a pane that is playing starts again from
the new play start, and one that is paused stops, so pressing play starts
there instead of going on from the pause. Moving
it is a change to the project, saved with it (`play_start` in the
[project file](../reference/project-file.md)), and one step to
[undo](main-window.md#undo-and-redo). A phrase with no source audio has no
play start.

## Playing

| Control | What it does |
|---|---|
| **▶ Original**, **▶ Encoded** under the panes | Plays that pane from the play start; while playing, the button pauses, and pressing it again goes on from where it paused |
| Space | The same, for the pane that has the keyboard focus (click in it, or Tab to it) |

One pane plays at a time: playing one stops the other. The encoded audio
starts from the same sound as the original, at the play start. A green
line follows the sound; at the end, playback stops and the line goes back
to the play start.

Audio plays on the system's default output device. If the device does not
take 8 kHz audio, the audio is resampled to the device's own rate first.

## Light and dark

The window follows the desktop's light or dark setting, when it starts and
when the setting changes, and the waveform colours have a light and a dark
form. On Linux the setting is read from the desktop portal, as GNOME and
KDE Plasma provide it; on Windows and macOS Qt follows the system itself.
