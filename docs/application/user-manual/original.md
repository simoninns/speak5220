# The Original view

The **Original** tab (**View > Original**, Ctrl+1) shows and plays the
selected phrase's recording as it was imported. It is the first of the
three stages of making a phrase, and the only one that never changes:
[Preprocess](preprocess.md) and [Encode](encode.md) work from it.

## The waveform

The waveform, titled **Original (8 kHz)**, is the phrase's audio as the
project stores it: the imported file mixed to mono and resampled to
8 kHz, and normalised if you chose that when it was imported (see
[importing audio](phrases.md#importing-audio)). The original file itself is
not kept in the project.

A phrase with no source audio, such as a word imported from a PHROM image,
has no original: the view says *No source audio: the phrase is made of
frames alone*.

## The controls

| Control | What it does |
|---|---|
| **▶** | Plays the recording from the play start, or just the selection; while playing it pauses (Space does the same when the waveform has the keyboard focus) |
| **\|◀** | Moves the play start back to the beginning |
| **−** and **+** | Zoom out and in |
| **Fit** | Shows the whole phrase |

Click in the waveform to move the play start, drag across it to select a
stretch, and use the mouse wheel to scroll (with Ctrl, to zoom). These
controls, and the play start and selection, are the same on every view,
and described in [Waveforms](waveforms.md).

## The settings

The Original view has no settings: its settings area says *No settings*.
To change the recording, change its [preprocessing](preprocess.md); to
replace it, import the file again as a new phrase (see
[importing audio](phrases.md#importing-audio)).

## Exporting the original

The **WAV audio** export can write each phrase's original, as a 24-bit,
8 kHz WAV file (**Audio: Original**). See [Export](export.md).
