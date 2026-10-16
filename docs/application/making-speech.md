# Making speech

This page walks through making speech with Speak5220, from a recording to
a PHROM image that a BBC Micro can speak:

1. [Start a project](#1-start-a-project)
2. [Add recordings](#2-add-recordings)
3. [Name and order the phrases](#3-name-and-order-the-phrases)
4. [Listen to the original](#4-listen-to-the-original)
5. [Preprocess the recording](#5-preprocess-the-recording)
6. [Encode it](#6-encode-it)
7. [Improve the encoding](#7-improve-the-encoding)
8. [Check that the phrases fit](#8-check-that-the-phrases-fit)
9. [Save the project](#9-save-the-project)
10. [Export it](#10-export-it)

Each step links to the page that describes its part of the application
in full, with every option. Each also gives the keys, so the whole
walkthrough can be done with the keyboard alone (on macOS, Ctrl in the
shortcuts is Cmd).

## Before you start

Record each word or phrase as its own WAV file. Any sample rate and any
number of channels will do: Speak5220 makes an 8 kHz mono copy. A quiet
room matters more than a good microphone; breaths and room noise can be
taken out later with the noise gate.

**Moving around with the keyboard:**

- **Tab** and **Shift+Tab** move between the phrase list, the phrase's
  name and description, the tabs, the waveform and its buttons, and the
  settings.
- The **arrow keys** move within the phrase list, between the tabs, and
  along a slider.
- **Space** presses the focused button, or plays and pauses the focused
  waveform.
- **Alt** with a menu's underlined letter opens the menu (on macOS, use
  the menu bar or Ctrl+F2); every command is in a menu.

## 1. Start a project

Speak5220 opens with a new, empty project. To start another, choose
**File > New** (Ctrl+N).

A project holds the phrases of one or more PHROMs, with their recordings,
settings and frames, in one `.s5proj` file. New projects start from the
encoder's default settings, or those you choose under **New Projects** in
[Preferences](user-manual/preferences.md#new-projects).

See [the main window](user-manual/main-window.md).

## 2. Add recordings

Choose **Phrase > Add Phrases...** (Ctrl+Shift+A) and pick one or more WAV
files (Enter picks the selected file), or drop WAV files on the phrase
list. Each becomes a phrase, named after its file: `hello.wav` becomes
`HELLO`.

If a file clips or is very quiet, you are asked whether to normalise it;
press Enter for **Yes**. The new phrases are not encoded yet.

See [importing audio](user-manual/phrases.md#importing-audio). To start
from an existing PHROM image instead, such as Acorn's PHROM A, use **File
> Import PHROM Image...** (see
[importing PHROM images](user-manual/phrases.md#importing-phrom-images)).

## 3. Name and order the phrases

Select a phrase in the list (Tab to the list and use the arrow keys). Tab
to **Name**, type a name (letters are made upper case, at most 16
characters), and press Enter. **Description** is free text for your own
notes.

The order of the list is the order of the words in the PHROM: the first
phrase is word 127. Move phrases with **Phrase > Move Up** and **Move
Down** (Ctrl+Up, Ctrl+Down), by dragging, or sort them with **Phrase >
Sort A-Z**.

See [phrases and their names](user-manual/phrases.md#phrases-and-their-names)
and [the phrase list](user-manual/main-window.md#the-phrase-list).

## 4. Listen to the original

Press **Ctrl+1** (**View > Original**). The Original view shows the
recording as imported, and its waveform has the keyboard focus: press
**Space** to play it, and Space again to pause. Click in the waveform to
play from there; drag across it to play just that stretch.

See [the Original view](user-manual/original.md) and
[waveforms and playback](user-manual/waveforms.md).

## 5. Preprocess the recording

Press **Ctrl+2** (**View > Preprocess**). The Preprocess view shows the
recording as the encoder will take it, with the preprocessing settings
below it. They are all off at first.

- For phrases to be played on a **BBC Micro**, choose **Preset: BBC
  Micro**. It cuts the bass and lifts the treble, so the phrases sound
  louder and clearer on its small speaker.
- If there are **breaths or room noise** between the words, tick **Noise
  gate**. The stretches it silences are shaded: lower the level until the
  breaths are shaded but the words are not.
- If the ***s*** **sounds are harsh**, tick **De-esser**.

Press Space to hear the result. To set these for every phrase at once,
choose **Settings for: Project defaults** first.

See [the Preprocess view](user-manual/preprocess.md).

## 6. Encode it

Press **Ctrl+3** (**View > Encode**). The Encode view shows what the
TMS5220 makes of the phrase's frames, with the encoder panel below.

A new phrase has no frames yet: press **Ctrl+E** (**Phrase > Re-encode**)
to encode it (select several phrases first to encode them all). Then press
**Space** to hear the encoded audio, as the chip speaks it.

The encoder panel shows the encoding's size and its quality rating, from
*Poor* to *Good*: how close the encoding comes to the preprocessed
recording. To hear it as a BBC Micro plays it, choose **Audio > Output
Filter > BBC Micro**.

See [the encoder panel](user-manual/encode.md#the-encoder-panel),
[the quality rating](user-manual/encode.md#the-quality-rating) and
[listening options](user-manual/encode.md#listening-options).

## 7. Improve the encoding

Either:

- **change a setting**: Tab to its slider and use the arrow keys. The
  phrase is encoded again shortly after each change. **Target RMS** sets
  the loudness, **Repeat trade-off** trades bytes against quality, and
  **Voicing threshold** decides between buzz and hiss; **Advanced
  settings** shows seven more. See
  [the Encode view's settings](user-manual/encode.md#the-settings); or
- **let Speak5220 search the settings for you**: press **Ctrl+Shift+O**
  (**Phrase > Optimise...**), then press **Start** (Alt+S on Windows and
  Linux). When the search ends, the listen buttons play the phrase before
  and after; **Accept** (Alt+A) keeps the result, and **Revert** leaves the
  phrase as it was. See [Optimise](user-manual/optimise.md).

Every change can be undone with **Ctrl+Z** (**Edit > Undo**).

## 8. Check that the phrases fit

A PHROM holds 16 KB. The status bar shows how many PHROMs the phrases
fill, and the free space in the last one, for example `3 PHROMs of 64 ·
4.1 KB free in PHROM 3`; the phrase list marks where each PHROM starts.
A phrase marked **stale** in the list was encoded with other settings, for
example after a change to the project defaults: select it and press
Ctrl+E.

To make phrases smaller, lower the **Repeat trade-off**, or use Optimise
with **Limit each phrase to** or **Trade quality for size**.

See [PHROMs and the project budget](user-manual/phrases.md#phroms-and-the-project-budget).

## 9. Save the project

Press **Ctrl+S** (**File > Save**). A new project asks where to save it,
as a `.s5proj` file. See [projects](user-manual/main-window.md#projects).

## 10. Export it

First, give the PHROM its name: choose **File > Project Settings...** and
change the **Header strings**, whose second line names the PHROM (keep the
first starting with `(C)`, which a BBC Micro looks for). See
[Project Settings](user-manual/project-settings.md).

Press **Ctrl+Shift+E** (**Export > Export...**). With **Acorn PHROM
images** chosen as the format, Tab to **Folder** to choose where the files
go, then press Enter for **Export**. The image can be used in a BBC
Micro's speech system or an emulator.

Other formats write C headers for your own firmware, WAV files, or a
**BBC Micro disc** that speaks the phrases on a BBC Micro with the speech
system but no PHROM. **Save as Preset** keeps the export's choices in the
project, to run again later. See [Export](user-manual/export.md) and
[BBC Micro disc](user-manual/bbc-disc.md).

## Where next

- [The command-line tool](user-manual/cli.md) does the same from scripts:
  creating projects, adding recordings, encoding and exporting.
- [Troubleshooting](user-manual/troubleshooting.md) lists the messages
  and what to do about them.
