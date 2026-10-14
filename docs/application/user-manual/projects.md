# Projects and phrases

A project holds the speech for one or more PHROMs: a list of **phrases**,
each with its audio, its encoder settings and its frames. It is kept in one
`.s5proj` file, or unpacked as a directory; both are described in the
[project file](../reference/project-file.md) reference. Projects are made
and edited in the [main window](main-window.md) or with the
[command-line tool](cli.md). Both encode them (the window's
[encoder panel](encoder.md) also optimises their settings); so far only
the command-line tool exports them.

## Phrases and their names

Each phrase becomes one word in a PHROM, and its name is stored in the
PHROM with it. Names follow the style of Acorn's own PHROM:

- upper-case letters, digits, space, and `' ( ) , - .`, so names such as
  `O'CLOCK`, `TWO,TO,TOO` and `2- (TWEN-)` are allowed;
- at most 16 characters;
- no two phrases with the same name.

Lower case is turned into upper case. A name with any other character is
refused, with a message saying which character and what is allowed.

The order of the phrases is the order of the words in the PHROMs: the first
phrase is word 127 of PHROM 1.

## Importing audio

A phrase starts as a WAV file: any sample rate, any number of channels, 8-
to 32-bit PCM or floating point. The encoder works at 8 kHz, so on import
Speak5220:

1. mixes the channels to mono by averaging them;
2. resamples to 8 kHz with a high-quality sinc converter, or copies the
   audio unchanged if it is already 8 kHz;
3. checks the level. Audio that clips (its peak reaches full scale) or is
   very quiet (its peak is below −30 dBFS) is reported. It is changed only
   if you ask for it to be normalised (the main window asks; the
   command-line tool has `--normalise`), which scales its peak to −1 dBFS;
   otherwise the level is left alone, because the encoder sets the output
   level itself (`target_rms`);
4. stores the 8 kHz copy in the project as 24-bit PCM, with the original
   file's name, rate, channels, length and SHA-256.

The original file is not kept in the project. To resample a phrase again,
import the original file again.

If no name is given, one is made from the file name: `good_morning.wav`
becomes `GOOD MORNING`, cut to 16 characters, with ` 2`, ` 3`, ...
added if the name is already taken.

## Importing PHROM images

The words of an existing Acorn PHROM image, such as Acorn's own PHROM A or
an image exported from another project, can be added to a project with the
command-line tool's [`import-phrom`](cli.md#commands) command, or in the
main window with **File > Import PHROM Image...**. A file may hold one
16 KB PHROM or several concatenated, as the `concatenated` export layout
writes them.

Each word becomes a **locked** phrase at the end of the list, in PHROM then
word order. It has no source audio: its frames, read from the image up to
and including the stop code, are its content, and it is never re-encoded.
It is exported, measured and spoken like any other phrase, so a project
can mix imported words with new ones.

Each phrase takes the word's name from the image (datasheet §12.8): the
bytes stored between the end of the word before it and its data, read for
as long as they hold printable characters, so a name ends at padding or
other data. The name is then made valid as for an imported file: lower
case turned into upper case, other characters turned into spaces, cut to
16 characters, and ` 2`, ` 3`, ... added if it is already taken. A word
whose image holds no name is called `WORD 127`, `WORD 128`, ... after its
word number. Two pointers to the same data give two phrases with the same
frames.

The words are imported from word 127 (after the 95 "ASCII associated"
entries) to the end of the pointer table. With them come:

- **the image's ASCII entries** that point at a word, as entries of the
  project's [ASCII map](#phroms-and-the-project-budget) naming the phrase
  that word became, for each character the project does not already map;
- **the image's header strings**, as each PHROM's own, but only when the
  words go into a project with no phrases that still has the default
  header strings, so a project's own strings are never replaced.

So Acorn's PHROM A, imported into a new project, fills one PHROM and
exports as the same image: the same pointers, names and frames. Only the
unused bits after each word's stop code can differ: Acorn's tools set
them to 1 and Speak5220 leaves them 0, and the chip never reads them.
In the main window, the words and these settings are one step to undo.

An image that is not a whole number of 16 KB PHROMs, does not start with
an Acorn PHROM header, has a pointer into its header or pointer table, or
has a word with no stop code before the end of its PHROM, is refused, and
nothing is added.

## Encoding

A phrase is encoded with the project's default
[encoder options](../reference/encoder-options.md), plus any options it
sets for itself. The frames are saved in the project, so it opens without
re-encoding. They are redone when they go out of date: when the audio, an
option in effect or the encoder itself changes. Encoding the same audio
with the same options always gives the same frames.

A **locked** phrase is never re-encoded. Its frames are its content, as for
a word taken from an existing PHROM image, which has no source audio.

## PHROMs and the project budget

A PHROM holds 16 KB. A project can hold more speech than one PHROM, and
fills as many as it needs:

- **In list order.** Phrases fill PHROM 1 in order. When the next phrase
  does not fit, PHROM 2 starts, and so on. Earlier PHROMs are not filled in
  later, so the result is predictable; reordering the list can move phrases
  between PHROMs.
- **What a phrase costs** is its packed frames, plus one byte per character
  of its name, plus a 2-byte pointer. Each PHROM also spends 256 bytes on
  its header, its 95 ASCII pointers, the end of its pointer table and a
  one-byte null word (datasheet §12.8), which leaves 16 128 bytes for
  words. A PHROM whose phrases have all 95 ASCII entries, as Acorn's
  PHROM A does, needs no null word, and holds 16 129.
- **A phrase too large for an empty PHROM** goes in no PHROM until it is
  shortened or re-encoded, and cannot be exported.
- **A phrase not yet encoded** takes no space.
- **Word numbers** start at 127 in every PHROM.
- **The ASCII map** is held once for the project. Each PHROM includes only
  the entries whose phrase it holds; its other ASCII entries point to its
  null word. The map names phrases, so renaming a phrase leaves its
  entries naming the old name, and they no longer apply.
- **Header strings** are the project's, with the PHROM's number appended to
  the second string (`My PHROM 2`), unless a PHROM has its own.

The **project budget** is 64 PHROMs (1 MB). It is a limit chosen for
testing, not a hardware one: a BBC Micro can select only 16 PHROMs at a
time. No change takes a project past it:

- adding phrases that would need a 65th PHROM is refused;
- a settings change whose new encoding would need a 65th PHROM is not
  applied, and the phrase keeps its previous settings and encoding;
- renaming or reordering phrases that would need a 65th PHROM is refused.

`speak5220-cli encode` reports how many PHROMs the project fills and the
free space in the last one; the main window shows it in its status bar.
[Optimise](encoder.md#optimise) never chooses settings that would take
the project past its budget.

## Exporting

A project exports Acorn PHROM images (datasheet §12.8), C headers of
those images or of each phrase's Speak External stream, and WAV files of
the encoded or original audio, from the [Export dialog](export.md) or
`speak5220-cli export`. Exports can be saved in the project as presets.
The BBC Micro disc comes later.
