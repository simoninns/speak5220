# Getting started

This page takes you from a recording to a PHROM image: importing a WAV
file, listening to it, encoding it, checking the result, and exporting it.
Each step gives the mouse action and the keys, so the whole walkthrough can
be done with the keyboard alone.

## Installing

Speak5220 is built from source; there are no installers yet. `BUILD.md`
in the repository describes the build on Linux, macOS and Windows. The
build makes two programs: `speak5220`, the application described here,
and `speak5220-cli`, the [command-line tool](user-manual/cli.md).

## Moving around with the keyboard

- **Tab** and **Shift+Tab** move between the phrase list, the phrase's
  name and description, the tabs, the waveform and its buttons, and the
  settings.
- The **arrow keys** move within the phrase list, between the tabs, and
  along a slider.
- **Space** presses the focused button, or plays and pauses the focused
  waveform.
- **Alt** with a menu's underlined letter opens the menu (on macOS, use
  the menu bar or Ctrl+F2); every command is in a menu.

On macOS, Ctrl in the shortcuts below is Cmd.

## Walkthrough

### 1. Start a project

Speak5220 opens with a new, empty project. To start another, choose
**File > New** (Ctrl+N).

New projects start from the encoder's default settings, or those you
choose under **New Projects** in **Edit > Preferences...** (see
[Preferences](user-manual/main-window.md#preferences)).

### 2. Add a recording

Choose **Phrase > Add Phrases...** (Ctrl+Shift+A) and pick one or more WAV
files (Enter picks the selected file). Each becomes a phrase, named after
its file: `hello.wav` becomes `HELLO`. You can also drop WAV files on the
phrase list.

If a file clips or is very quiet, you are asked whether to normalise it;
press Enter for **Yes**. See [importing audio](user-manual/projects.md#importing-audio).

The new phrase is selected in the list. To select another phrase, Tab to
the list and use the arrow keys.

### 3. Name and describe it

Tab to **Name**, type a name (letters are made upper case), and press
Enter. **Description** is free text for your own notes. See
[phrases and their names](user-manual/projects.md#phrases-and-their-names).

### 4. Listen to the recording

Press **Ctrl+1** (**View > Original**). The Original tab shows the
recording, and its waveform has the keyboard focus: press **Space** to
play it, and Space again to pause. See [playing](user-manual/waveforms.md#playing).

### 5. Encode it and listen to the result

Press **Ctrl+3** (**View > Encode**). The Encode tab shows what the
TMS5220 makes of the phrase's frames, with the encoder panel below.

A new phrase has no frames yet: press **Ctrl+E** (**Phrase > Re-encode**)
to encode it. Then press **Space** to hear the encoded audio, as the chip
speaks it.

The encoder panel shows the encoding's size and its quality rating, from
*Poor* to *Good*. See [the encoder panel](user-manual/encoder.md#the-encoder-panel)
and [the quality rating](user-manual/encoder.md#the-quality-rating).

### 6. Improve it

Either:

- change a setting: Tab to its slider and use the arrow keys. The phrase
  is encoded again shortly after each change. See
  [the settings](user-manual/encoder.md#the-settings); or
- let Speak5220 search the settings for you: press **Ctrl+Shift+O**
  (**Phrase > Optimise...**), then Tab to **Start** and press Space
  (Alt+S on Windows and Linux). When the search ends, the listen buttons
  play the phrase before and after to compare; **Accept** (Alt+A) keeps
  the result, and **Close** (Esc) leaves the phrase as it was. See
  [Optimise](user-manual/encoder.md#optimise).

Every change can be undone with **Ctrl+Z** (**Edit > Undo**).

### 7. Save the project

Press **Ctrl+S** (**File > Save**). A new project asks where to save it,
as a `.s5proj` file. See [projects](user-manual/main-window.md#projects).

### 8. Export a PHROM image

Press **Ctrl+Shift+E** (**Export > Export...**). With **Acorn PHROM
images** chosen as the format, Tab to **Folder** to choose where the files
go, then press Enter for **Export**. The image can be used in a BBC
Micro's speech system or an emulator. See [Export](user-manual/export.md),
and [BBC Micro disc](user-manual/bbc-disc.md) for a disc that speaks the
phrases on a BBC Micro.

## Where next

- [Projects and phrases](user-manual/projects.md): how phrases fill
  PHROMs, and importing words from existing PHROM images.
- [The main window](user-manual/main-window.md): every menu, the
  phrase list, Preferences.
- [The encoder and Optimise](user-manual/encoder.md): what each setting
  does.
- [Troubleshooting](user-manual/troubleshooting.md): messages and what to
  do about them.
