# Project Settings

**File > Project Settings...** sets what belongs to the project as a
whole rather than to a phrase: its name, the strings in its PHROMs'
headers, and its ASCII map. Nothing changes until you press **OK**, and
then all the changes are one step to [undo](main-window.md#undo-and-redo)
(**Undo Change Project Settings**). **Cancel** leaves the project as it
was.

The project's encoder settings are not here: they are the project
defaults of the [Preprocess](preprocess.md) and [Encode](encode.md) views
(**Settings for: Project defaults**). Export presets are saved and
deleted in the [Export dialog](export.md#presets).

## General

| Setting | Default | What it does |
|---|---|---|
| **Name** | Empty, until the project is first saved: it then takes the file's name | The project's name. It is the title of a [BBC Micro disc](bbc-disc.md) (its first 12 characters) and is written in the comments of the [C headers](export.md#c-headers) |
| **Header strings** | `(C)2026 speak5220`, `Speak5220 PHROM`, `1.00` | The strings in every PHROM's header, one per line. Each PHROM gets its number after the second: `Speak5220 PHROM 2`. A [BBC Micro disc](bbc-disc.md) lists each PHROM under its second string |

The header is 64 bytes, and holds the strings in its bytes 2 to 55
([datasheet §12.8](../../technical/datasheet/authoring.md#128-word-tables)):
54 bytes, with a null after each string. Under the strings, the dialog
says how many of those bytes they take, counted with the longest number
a PHROM can get (` 64`), so the strings fit every PHROM. Each string must
be printable ASCII (letters, digits, space and punctuation) and not
empty, since a computer reading the header stops at an empty string.
Empty lines at the end are left out. **OK** is greyed out, and the reason
shown, while the strings do not fit.

!!! note
    A BBC Micro finds a PHROM only if the first header string starts with
    `(C)` (Acorn Speech System User Guide, Section 7). The dialog says so
    when it does not, but lets you keep it, for a PHROM meant for
    something else.

## PHROMs

A PHROM can have header strings of its own instead of the project's: for
example, a project of numbers and letters whose PHROM 2 should be called
`Letters`.

| Setting | What it does |
|---|---|
| **PHROM** | The PHROM to show, from 1 to 64 |
| **Own header strings** | Gives that PHROM header strings of its own. Ticked, they start as the strings it has now (the project's, with its number), to edit below. Unticked, the PHROM goes back to the project's strings |
| **Header strings** | The PHROM's own strings, one per line, with how many of the 54 bytes they take. Unticked, it shows, greyed out, the strings the PHROM gets from the project |

Below, the dialog lists the PHROMs that have strings of their own. The
strings follow the same rules as the project's, except that no number is
added to them.

Header strings can also come from a PHROM image: importing one into a
new, empty project gives each PHROM the strings of its image (see
[importing PHROM images](phrases.md#importing-phrom-images)).

## ASCII Map

The ASCII map gives a phrase to some of the 95 printable characters,
from space (32) to `~` (126). These are a PHROM's "ASCII associated"
words (word numbers 32 to 126; [datasheet §12.8](../../technical/datasheet/authoring.md#128-word-tables)):
a program can speak a character's phrase by the character's code, for
example to speak the digits of a number.

| Setting | Choices | What it does |
|---|---|---|
| One row per character, such as **A (65)** | **None** (the default), or any phrase of the project | The phrase that character speaks |

Each PHROM includes only the entries whose phrase it holds; its other
ASCII entries point at a one-byte null word, which says nothing. A PHROM
whose phrases have all 95 entries needs no null word, and holds one byte
more (see [PHROMs and the project budget](phrases.md#phroms-and-the-project-budget)).

The map names phrases by name. Renaming a phrase leaves its entries
naming the old name, and they no longer apply: the dialog shows such an
entry as *NAME (no phrase of this name)*, and keeps it unless you choose
another phrase. Importing a PHROM image adds its ASCII entries for the
characters the project does not map yet.

If the new map would take the project past its budget of 64 PHROMs, the
dialog says so and stays open.
