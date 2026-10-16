# BBC Micro disc

The **BBC Micro disc** export (`bbc-dsd`) writes a disc image that plays a
project's phrases on a BBC Micro with the Acorn speech system. The disc
holds the phrases and a player program, so no PHROM needs to be made: the
player sends each phrase to the speech chip with Speak External
([datasheet §12.9](../../technical/datasheet/authoring.md#129-speak-external-streams)).
The disc's layout is in the
[export formats](../reference/export-formats.md#bbc-micro-disc) reference.

## What you need

- A BBC Micro Model B with the speech system fitted (the TMS5220 speech
  chip). The player needs no PHROM.
- Acorn DFS. The disc is a 40-track double-sided image (`.dsd`), which BBC
  Micro emulators and floppy emulators such as the Gotek read.
- A project of at most 8 PHROMs, or that many phrases chosen from it. A
  disc holds the phrases of 8 PHROMs, 4 on each side.

!!! note
    The player has been checked on an emulated BBC Micro (MAME's Model B
    with the speech system and Acorn DFS), but not yet on a real one.

## Exporting the disc

Choose **BBC Micro disc** in the [Export dialog](export.md#the-export-dialog).
It has no options of its own: choose all phrases or the selected ones, a
name and a folder, and Export writes `NAME.dsd`. From the command line:

```bash
speak5220-cli export words.s5proj --output out --format bbc-dsd
```

The phrases are placed in PHROMs as the
[project budget](phrases.md#phroms-and-the-project-budget) places them,
and each PHROM is listed under the second of its header strings, such as
`Speak5220 PHROM 1`. If the phrases fill more than 8 PHROMs, the export is
refused; choose fewer phrases. The disc's title is the project's name, cut
to 12 characters.

## Playing it

Insert the disc and press **Shift+Break**: the disc boots into the
player. (Typing `CHAIN "SPEAK"` does the same.) The player runs in
MODE 7.

1. It lists the PHROMs on the disc, with the number of phrases in each.
   Press a PHROM's number to choose it.
2. The player loads that PHROM's phrases (a full PHROM takes about 3
   seconds), then lists them, 16 to a page, each with a letter.
3. Press a phrase's letter to speak it.

| Key | In the phrase list |
|---|---|
| **A**-**P** | Speak that phrase |
| **.** or **>** | The next page |
| **,** or **<** | The previous page |
| **RETURN** | Play all: speak every phrase of the PHROM in order |
| **ESCAPE** | Stop play all, after the phrase being spoken; otherwise, go back to the PHROM list |

The bottom line shows the phrase being spoken.

A disc of 4 PHROMs or fewer uses only drive 0, so it also works in a
single-sided drive. With more, the player reads PHROMs 5 to 8 from drive 2,
the disc's other side.

## Memory

The player keeps a 16 KB buffer below the MODE 7 screen for the chosen
PHROM's phrases, so a phrase as large as a PHROM holds can be spoken. With
Acorn DFS (PAGE at &1900), 8.75 KB is left beside the buffer: the program
takes about 3.5 KB, and its variables, stack and speech routine about
1.1 KB more, leaving over 4 KB to spare. If a filing system or other ROM
has raised PAGE too far, the player says how low PAGE must be and stops.
