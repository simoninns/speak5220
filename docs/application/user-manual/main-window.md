# The main window

`speak5220` opens a window with the project's phrases on the left, the
selected phrase's name and description on the right with its
[waveforms](waveforms.md) below them, and the project's size in the status
bar. To open a project at start-up, give it on the command line:
`speak5220 greetings.s5proj`. The window follows the desktop's light or
dark setting (see [light and dark](waveforms.md#light-and-dark)).

!!! note
    The encoder panel and export come later. So far, phrases are encoded
    and exported with the [command-line tool](cli.md).

## Projects

The **File** menu works on the project as a whole:

| Command | Shortcut | What it does |
|---|---|---|
| New | Ctrl+N | Starts an empty project, called `Untitled` until it is saved |
| Open... | Ctrl+O | Opens a `.s5proj` file. To open an unpacked project (a directory), choose its `project.yaml` |
| Open Recent | | The last eight projects opened or saved. A project that no longer exists is removed when chosen. Clear Menu empties the list |
| Save | Ctrl+S | Saves the project where it came from, in the same form (file or directory). A new project asks where to save it |
| Save As... | Ctrl+Shift+S | Saves the project as a new `.s5proj` file (the extension is added if missing). A project with no name takes the file's name |
| Quit | Ctrl+Q | Closes the window |

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
  `speak5220-cli encode`), **not encoded** if it has no frames yet, or
  **locked** if its frames are never re-encoded.

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

Select several phrases with Shift or Ctrl and a click. Moving several
phrases keeps them in their order and gathers them together: Move Up puts
them all just above the first of them.

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

## Undo and redo

**Edit > Undo** (Ctrl+Z) and **Edit > Redo** (Ctrl+Shift+Z, or Ctrl+Y on
some platforms) step through every change to the phrases: adding,
removing, moving, sorting, renaming, changing a description and moving
the [play start](waveforms.md#the-play-start). The menu
names the step, for example **Undo Rename to HELLO**. Undoing back to the
last save clears the unsaved-changes mark. Opening or starting a project
clears the history.

## The status bar

The status bar shows the project's size, for example:

`37.2 KB · 3 PHROMs of 64 · 4.1 KB free in PHROM 3`

- the size of every PHROM in use, headers included, in KB of 1024 bytes;
- the number of PHROMs the phrases fill, and the budget;
- the free space in the last PHROM.

It adds **over budget** for a project that needs more than 64 PHROMs (one
made elsewhere; edits never take a project past the budget), and the
number of phrases too large for a PHROM or not yet encoded.
