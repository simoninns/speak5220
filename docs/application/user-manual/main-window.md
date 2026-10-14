# The main window

`speak5220` opens a window with the project's phrases on the left, the
selected phrase's name and description on the right with its three tabs
below them, and the project's size in the status bar (with, on its left,
what is under the pointer in a waveform). The tabs follow
the stages of making a phrase, each with its [waveform](waveforms.md)
and controls in the waveform area, and that stage's settings in the
scrolling settings area below it:

- **Original**: the recording as imported.
- **Preprocess**: the recording as preprocessed, with the
  [preprocessing settings](encoder.md#preprocessing).
- **Encode**: what the chip makes of the phrase's frames, with the
  [encoder panel](encoder.md).

The two areas keep their sizes, and the play start, the zoom and the
scroll position are shared by the tabs, so switching tabs shows the same
place in the phrase at the same place on screen. To open a project at start-up, give it on the command line:
`speak5220 greetings.s5proj`. The window follows the desktop's light or
dark setting unless you choose one under **View > Theme** (see
[light and dark](waveforms.md#light-and-dark)).

## Projects

The **File** menu works on the project as a whole:

| Command | Shortcut | What it does |
|---|---|---|
| New | Ctrl+N | Starts an empty project, called `Untitled` until it is saved |
| Open... | Ctrl+O | Opens a `.s5proj` file. To open an unpacked project (a directory), choose its `project.yaml` |
| Open Recent | | The last eight projects opened or saved. A project that no longer exists is removed when chosen. Clear Menu empties the list |
| Save | Ctrl+S | Saves the project where it came from, in the same form (file or directory). A new project asks where to save it |
| Save As... | Ctrl+Shift+S | Saves the project as a new `.s5proj` file (the extension is added if missing). A project with no name takes the file's name |
| Import PHROM Image... | Ctrl+I | Adds the words of Acorn PHROM images as locked phrases (see [importing PHROM images](projects.md#importing-phrom-images)) |
| Quit | Ctrl+Q | Closes the window |

A new project starts from the settings chosen under **New Projects** in
[Preferences](#preferences).

The title bar shows the project's file name, marked (`*`, or as the
platform shows it) while there are unsaved changes. Starting a new
project, opening another, or closing the window with unsaved changes asks
first: **Save**, **Discard** or **Cancel**. If saving fails or is
cancelled, the project stays open.

Saving is atomic and keeps the previous version beside the project, as
described in the [project file](../reference/project-file.md) reference.

## The phrase list

The list shows the phrases in PHROM order: the first phrase is word 127 of
PHROM 1, and so on (see [PHROMs and the project budget](projects.md#phroms-and-the-project-budget)).
Each row shows:

- the phrase's name;
- its size in its PHROM, in bytes, once it has been encoded;
- **stale** if its frames were made with other settings (re-encode it with
  **Phrase > Re-encode**), **not encoded** if it has no frames yet, or
  **locked** if its frames are never re-encoded.

A phrase changed since the project was last saved has `*` after its name,
as the title bar marks the project. That covers its name, description,
[play start](waveforms.md#the-play-start), its own settings and its
encoding, and a phrase added since the last save. Moving a phrase does not
mark it, and changing the project's defaults marks none (though phrases
that follow them may become stale). Saving clears every mark.

A row with a **PHROM 2**, **PHROM 3**, ... band above it is the first
phrase of that PHROM. A phrase too large for an empty PHROM is shown in
red; its tooltip gives its size. Hovering over any row shows its PHROM,
word number and size.

The buttons above the list, and the **Phrase** menu:

| Button | Command | Shortcut | What it does |
|---|---|---|---|
| + | Add Phrases... | Ctrl+Shift+A | Imports WAV files as phrases, after the last selected phrase or at the end |
| − | Remove | Delete | Removes the selected phrases, after asking |
| ↑ | Move Up | Ctrl+Up | Moves the selected phrases up one place |
| ↓ | Move Down | Ctrl+Down | Moves the selected phrases down one place |
| A-Z | Sort A-Z | | Sorts every phrase by name |
| | Re-encode | Ctrl+E | Encodes the selected phrases that are not encoded or are stale, with their settings (see [re-encoding several phrases](encoder.md#re-encoding-several-phrases)) |
| | Optimise... | Ctrl+Shift+O | Searches the selected phrases' settings for the best quality (see [Optimise](encoder.md#optimise)) |
| | Revert to Saved | | Puts the selected phrases marked `*` back as they were when the project was last saved or opened: name, description, play start, settings and encoding. Each keeps its place in the list. One step, which Undo takes back |

Select several phrases with Shift or Ctrl and a click. Moving several
phrases keeps them in their order and gathers them together: Move Up puts
them all just above the first of them.

Revert to Saved is offered when a selected phrase has been saved and has
changed since. A phrase added since the last save has nothing to revert
to; remove it instead. If a phrase's saved name has since been given to
another phrase that is not being reverted, nothing is reverted and the
clash is reported: rename the other phrase, or select both.

**Drag and drop.** Drag phrases to another place in the list to move them.
Drop WAV files from a file manager onto the list to import them where they
are dropped.

A change that would take the project past its budget of 64 PHROMs is
refused, and a message says why (see [troubleshooting](troubleshooting.md#the-project-budget)).

## Adding phrases

Importing works as described in [importing audio](projects.md#importing-audio):
each file is resampled to 8 kHz and stored in the project, and is named
after its file (`good_morning.wav` becomes `GOOD MORNING`). Files are read
in the background, so the window stays responsive.

If any file clips, or is very quiet (its peak is below −30 dBFS), you are
asked whether to normalise those files to a peak of −1 dBFS. Otherwise
they are added unchanged. Files that cannot be read are listed in a
message; the others are still added. All the files added at once are one
step to undo.

## Name and description

With one phrase selected, its name and description can be edited on the
right. A change is applied when you press Return or leave the field.

The name follows the [phrase name rules](projects.md#phrases-and-their-names)
as you type:

- lower case becomes upper case;
- a character a name cannot have is not taken, and the message below the
  field names it;
- the field holds at most 16 characters.

If the name is already taken (or a longer name would need a 65th PHROM),
the message says so and the name goes back to what it was. The
description is free text, kept only in the project.

## Importing PHROM images

**File > Import PHROM Image...** (Ctrl+I) adds every word of one or more
Acorn PHROM images to the project, after the last selected phrase or at
the end. A file may hold several 16 KB images one after another, as the
**All PHROMs in one file** export writes them. The words become
[locked](projects.md#importing-phrom-images) phrases: their frames are
kept exactly as in the image and never re-encoded, so they have no
recording, and the Encode tab shows their statistics only. A name already
in the project gets a number added. All the words added at once are one
step to undo; images that cannot be read are listed in a message, and the
others are still added.

## The View menu

**View > Original**, **Preprocess** and **Encode** (Ctrl+1, Ctrl+2 and
Ctrl+3) show that tab and put the keyboard focus on its waveform, so
Space plays it.

**View > Theme** chooses how the window looks: **Auto** (the default)
follows the desktop's light or dark setting, **Light** and **Dark** always
show that one. The change is immediate, and the window remembers it for
next time. See [light and dark](waveforms.md#light-and-dark).

## Preferences

**Edit > Preferences...** (Ctrl+, ; on macOS, **Speak5220 > Settings...**,
Cmd+,) sets what belongs to you rather than to a project. Nothing changes
until you press **OK**; **Restore Defaults** puts every tab back as it
was when Speak5220 was first run. The choices are kept for next time.

| Tab | Setting | What it does |
|---|---|---|
| Appearance | Theme | **Follow the desktop** (the default), **Light** or **Dark**, as **View > Theme** |
| Appearance | Waveforms | **Dark, in either theme** (the default): the waveforms are drawn on black, as in [light and dark](waveforms.md#light-and-dark). **As the theme**: drawn in the window's own colours |
| Audio | Output device | Where phrases are played: **System default** (the default), or one of the devices connected now. If the chosen device is later disconnected, phrases play on the system's default, and the list shows it as *not connected* until another is chosen |
| New Projects | (the settings form) | The [preprocessing and encoder settings](encoder.md) a new project starts from, in the same form as the Encode tab, with each setting that differs from the encoder's default in bold. A project keeps the settings it was made with: changing these does not change open or saved projects |
| Logging | Log level | **Info** (the default), **Debug** or **Trace**, as `--log-level` (see [logging](cli.md#logging)) |
| Logging | Log file | A file the log is also written to, replaced at each start; empty for none, as `--log-file` |

The logging settings take effect the next time Speak5220 starts. A
`--log-level` or `--log-file` given on the command line overrides them
for that run. If the log file cannot be written, Speak5220 still starts,
logging to the console, and says why in the log.

## The Help menu

**Help > About Speak5220** shows the version, the licence (the GNU
General Public License, version 3 or later), and every library and other
project whose code Speak5220 uses, each with its licence and copyright.

## Using the keyboard

Every command is in a menu, and the main ones have shortcuts (above).
Tab and Shift+Tab move between the phrase list, the name and description,
the tabs, the waveform and its buttons, and the settings; the arrow keys
move within the list, the tabs and a slider; Space presses the focused
button, or plays and pauses the focused waveform. Each control has a name
that a screen reader reads, including the buttons that show only a
symbol (+, −, the arrows, play). The [getting started](../getting-started.md)
walkthrough lists the keys for each step.

## The Export menu

**Export > Export...** (Ctrl+Shift+E) opens the Export dialog, which
writes PHROM images, C headers or WAV files. See [Export](export.md). It is available once
the project has a phrase.

## The Audio menu

**Audio > Output Filter** chooses the computer's audio stage that the
encoded audio is heard and shown through: **None** (the default) or
**BBC Micro**. See [output filter](waveforms.md#output-filter).

**Audio > Chip Output** chooses which of the chip's pins the encoded audio
is taken from: **SPEAKER (8-bit DAC)** (the default), whose DAC clips
loud samples and has the real DAC's uneven steps, or **I/O (10-bit
codes)**, unclipped. See
[chip output](waveforms.md#chip-output).

**Audio > Chip Clock** chooses the clock the chip's output is played at:
**640 kHz (nominal)** (the default), or a rate from 560 kHz to 720 kHz in
20 kHz steps. See [chip clock](waveforms.md#chip-clock).

The window remembers these choices for next time.

## Undo and redo

**Edit > Undo** (Ctrl+Z) and **Edit > Redo** (Ctrl+Shift+Z, or Ctrl+Y on
some platforms) step through every change to the phrases: adding,
removing, moving, sorting, renaming, changing a description, moving
the [play start](waveforms.md#the-play-start), changing a phrase's
settings or the project's defaults, re-encoding, accepting
[Optimise](encoder.md#optimise)'s results and reverting to saved. The menu
names the step, for example **Undo Rename to HELLO**. Undoing back to the
last save clears the unsaved-changes mark. Opening or starting a project
clears the history.

## The status bar

On its left, while the pointer is over a waveform, the status bar shows
what is under it (see [Waveforms](waveforms.md#reading-the-waveform)), for
example:

`0.512 s, sample 4096, amplitude -0.310 to +0.420 · Frame 20: voiced, energy 9, ...`

A message, such as one about an export, takes its place for a while.

On its right it shows the project's size, for example:

`37.2 KB · 3 PHROMs of 64 · 4.1 KB free in PHROM 3`

- the size of every PHROM in use, headers included, in KB of 1024 bytes;
- the number of PHROMs the phrases fill, and the budget;
- the free space in the last PHROM.

It adds **over budget** for a project that needs more than 64 PHROMs (one
made elsewhere; edits never take a project past the budget), and the
number of phrases too large for a PHROM or not yet encoded.
