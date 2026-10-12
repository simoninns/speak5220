# Export

A project's phrases are exported as Acorn PHROM images, as C headers for
programs that drive the TMS5220 themselves, as WAV files, or as a BBC
Micro disc. Export
writes the frames the project holds, so every phrase exported must be
encoded with its current settings first. The only exception is a WAV export
of the original audio. The byte layout of each format is in the
[export formats](../reference/export-formats.md) reference.

## The formats

| Format | Writes | For |
|---|---|---|
| **Acorn PHROM images** | One 16 KB image per PHROM the phrases fill, as separate files or one concatenated file | A PHROM in a BBC Micro's speech system, or an emulator (datasheet §12.8) |
| **C header, PHROM images** | One `.h` file with each image as an array, and a table of every phrase's PHROM, word number, offset and length | Firmware that reads PHROM images from its own memory |
| **C header, Speak External streams** | One `.h` file with each phrase's bytes as an array, ready to write to the chip with Speak External, with its length and a table | Firmware that speaks phrases with Speak External (datasheet §12.9) |
| **WAV audio** | One WAV file per phrase: the chip's output (16-bit, 8 kHz) or the phrase's 8 kHz original (24-bit) | Listening, comparing, or using the sound elsewhere |
| **BBC Micro disc** | One double-sided DFS disc image (`.dsd`) holding the phrases of up to 8 PHROMs and a player program | Speaking the phrases on a BBC Micro with the speech system; see [BBC Micro disc](bbc-disc.md) |

The PHROM images, and the PHROM header, hold the phrases in PHROM order:
the phrases are placed as the [project budget](projects.md#phroms-and-the-project-budget)
places them, from word 127 of PHROM 1, each PHROM with the project's
header strings (with its number) and its own ASCII entries. When only some
phrases are exported, they are placed afresh in the same way, so their word
numbers can differ from the whole project's.

The encoded WAV audio is what the chip's SPEAKER pin gives through the
VSM path, as the encoded waveform shows it with the Audio menu at its
defaults: no output filter, and loud samples clipped by the DAC. Its
length is the chip's whole output for the phrase, from the first frame
until the output has been quiet for a while.

## The Export dialog

**Export > Export...** (Ctrl+Shift+E) opens the dialog.

| Field | What it sets |
|---|---|
| Format | One of the formats above |
| Phrases | **All phrases**, or **Selected phrases**: those selected in the phrase list when the dialog opened |
| Layout | Acorn PHROM images: **One file per PHROM** (`NAME_1.bin`, `NAME_2.bin`, ...) or **All PHROMs in one file** (`NAME.bin`) |
| Licence | C headers: the SPDX licence identifier written at the top of the header. Empty for `GPL-3.0-or-later` |
| Audio | WAV: **Encoded** (the chip's output) or **Original** (the 8 kHz source) |
| Name | What the files are named after: `NAME.h`, `NAME_1.bin`, `NAME_HELLO.wav`. It starts as the project file's name |
| Folder | Where the files are written. **Browse...** chooses it. It starts as the project's folder, then the last folder exported to |

The BBC Micro disc has no options of its own.

Below the fields the dialog lists the files the export will write, or
says why it cannot run: a phrase not encoded with its current settings, a
phrase too large for a PHROM, more PHROMs than a BBC Micro disc holds, or
a name that cannot name files. **Export**
is available only when the export can run and a folder is chosen. The
export runs in the background, and the status bar says how many files
were written.

If any of the files already exist in the folder, **Export** lists them
and asks before replacing them. **Cancel** returns to the Export dialog
with its settings as they were, so a different name or folder can be
chosen; nothing is written until the export goes ahead.

!!! note
    `speak5220-cli export` does not ask: it replaces files of the same
    names in its `--output` folder.

!!! note
    A WAV file is named after its phrase's C identifier (the name with
    other characters made `_`, as in a C header), so `O'CLOCK` gives
    `NAME_O_CLOCK.wav`.

## Presets

A preset is an export written in the project file (`exports:` in the
[project file](../reference/project-file.md#exports)): its format, scope,
phrases and options, under a name. Presets are for repeatable exports
from the command line: `speak5220-cli export PROJECT --output DIR` runs
every preset of a project, or those named by `--preset`. The Export
dialog neither loads nor saves them, and a project keeps its presets when
the GUI saves it. A preset's files are named after
the preset. See the [command-line tool](cli.md).

## C headers

Both headers are plain C, for C99 and later and for C++. They start with
an `SPDX-License-Identifier` line, the project's name and a list of each
phrase's name with its C identifier, and have an include guard. Every name
in the header starts with the header's name as a C identifier: exported
as `speech`, the header defines `speech_phrom_1`, `SPEECH_WORD_HELLO`,
`SPEECH_H_` and so on. Arrays are `static const`, so the header can be
included in more than one file. The header holds no date, so the same
project always exports the same header.
