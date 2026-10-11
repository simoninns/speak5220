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
  words.
- **A phrase too large for an empty PHROM** goes in no PHROM until it is
  shortened or re-encoded, and cannot be exported.
- **A phrase not yet encoded** takes no space.
- **Word numbers** start at 127 in every PHROM.
- **The ASCII map** is held once for the project. Each PHROM includes only
  the entries whose phrase it holds; its other ASCII entries point to its
  null word.
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

So far a project exports Acorn PHROM images (datasheet §12.8): one 16 KB
image per PHROM, as separate files or concatenated into one. Export presets
saved in the project are run by `speak5220-cli export`. Every phrase must be
encoded with its current settings first. The other formats (TI images, raw
data, C headers, WAV and the BBC Micro disc) come later.
