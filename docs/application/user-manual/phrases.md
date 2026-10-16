# Phrases and PHROMs

A project holds the speech for one or more PHROMs: a list of **phrases**,
each with its audio, its settings and its frames. It is kept in one
`.s5proj` file, or unpacked as a directory, as described in the
[project file](../reference/project-file.md) reference. Projects are made
and edited in the [main window](main-window.md) or with the
[command-line tool](cli.md).

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

A phrase also has a **description**: free text for your own notes, kept
only in the project and never exported.

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
   level itself (**Target RMS**);
4. stores the 8 kHz copy in the project as 24-bit PCM, with the original
   file's name, rate, channels, length and SHA-256.

The original file is not kept in the project. To resample a phrase again,
import the original file again.

If no name is given, one is made from the file name: `good_morning.wav`
becomes `GOOD MORNING`, cut to 16 characters, with ` 2`, ` 3`, ...
added if the name is already taken.

A new phrase is not encoded: encode it with **Phrase > Re-encode**
(Ctrl+E), or by changing one of its settings.

## Importing PHROM images

The words of an existing Acorn PHROM image, such as Acorn's own PHROM A or
an image exported from another project, can be added to a project with
**File > Import PHROM Image...** (Ctrl+I) in the main window, or the
command-line tool's [`import-phrom`](cli.md#commands) command. A file may
hold one 16 KB PHROM or several concatenated, as the **All PHROMs in one
file** export writes them.

Each word becomes a **locked** phrase, after the last selected phrase (or
at the end) in the window, or at the end with the command-line tool, in
PHROM then word order. It has no source audio: its frames, read from the
image up to and including the stop code, are its content, and it is never
re-encoded. Its settings cannot be changed, and the Encode view shows only
its statistics. It is exported, measured and spoken like any other phrase,
so a project can mix imported words with new ones.

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
  project's [ASCII map](#project-settings) naming the phrase that word
  became, for each character the project does not already map;
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
nothing is added from it.

## Encoding

A phrase is encoded with its [preprocessing](preprocess.md) and
[encoder](encode.md) settings. The frames are saved in the project, so it
opens without re-encoding. Encoding the same audio with the same settings
always gives the same frames.

A phrase is in one of these states, shown in the
[phrase list](main-window.md#the-phrase-list) and on the Encode view:

| State | Meaning | What to do |
|---|---|---|
| **not encoded** | It has no frames yet, as a newly imported phrase | **Phrase > Re-encode** (Ctrl+E), or **Encode** on the Encode view |
| **stale** | Its frames were made from other audio or settings, or by another version of the encoder. They are still shown, played and kept, but cannot be exported | **Phrase > Re-encode** |
| up to date | Its frames match its settings | Nothing |
| **locked** | Its frames are its content and are never re-encoded | Nothing |

A phrase becomes stale when a project default it follows changes, or when
a new version of Speak5220 encodes differently. Changing a phrase's own
settings does not leave it stale: it is encoded again straight away.

### Phrase settings and project defaults

The settings come in two layers:

- **Project defaults**: the settings every phrase starts from, held once
  for the project. A new project takes them from
  [Preferences](preferences.md#new-projects), or the encoder's own
  defaults with the command-line tool.
- **The phrase's own settings**: the settings a phrase changes from the
  project defaults. Only those are stored with it; for the others it
  follows the defaults, also when they later change.

**Settings for**, on both the Preprocess and the Encode view, chooses which
layer the settings edit. The two views' choosers always match.

| Settings for | The form shows | A change | The ↺ buttons and the tick go back to | The reset button |
|---|---|---|---|---|
| **This phrase** | The phrase's settings: its own where it has them, the defaults elsewhere | Is applied 0.6 s after the last change: the phrase is encoded and measured in the background, and the new settings and frames are stored together as one step to undo (**Undo Change Encoder Settings**). A setting moved back to the project default is no longer the phrase's own | The project default | **Reset to Project Defaults**: removes all the phrase's own settings, and re-encodes it |
| **Project defaults** | The project defaults | Is applied 0.6 s after the last change, as one step to undo (**Undo Change Project Defaults**). Nothing is re-encoded: every phrase that follows a changed default becomes **stale**, until it is re-encoded | The encoder's own default | **Reset to Encoder Defaults**: puts every default back to the encoder's own |

The pause means that typing a number or holding an arrow key does not
encode every step. While a change is waiting, the Encode view's state says
*Settings changed: applied after a pause...*, and **Re-encode** applies it
at once. Selecting another phrase applies a change still waiting.

A new encoding that would take the project past its
[budget](#phroms-and-the-project-budget) is not stored: a message says
so, and the form shows the phrase's settings again.

For a locked phrase, or one with no audio, the **This phrase** settings
cannot be changed.

### The settings rows

Every setting on the Preprocess and Encode views, and under
[New Projects](preferences.md#new-projects) in Preferences, is a row of
the same kind:

- **The label**, in bold when the setting differs from its default (the
  project default for **This phrase**; the encoder's own for **Project
  defaults**). Hover over it for what the setting does and its default.
- **A check box**, for the settings that can be turned off: the noise
  gate, the bass cut, the de-esser and the resonance hold. The slider is
  greyed out while the setting is off. The noise gate's **Gate speed** is
  greyed out while the noise gate is off.
- **A slider**, with the value and its unit beside it (`24 codes`,
  `45 dB`, `32 ms`). Ratios read `off` at 1, and the treble tilt and the
  shortest voicing run read `off` at 0 and 1. A small tick under the
  slider marks the default, so a setting moved from it is easy to see. The
  slider covers the range where useful values lie; a value outside it,
  from the project file or Optimise, widens the slider to include it.
  Click a slider and use the arrow keys for single steps, Page Up and Page
  Down for steps of a tenth of its range.
- **↺**, which puts the setting back to its default. It is available only
  when the setting differs from it.

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
  shortened or re-encoded, and cannot be exported. The phrase list shows
  it in red.
- **A phrase not yet encoded** takes no space.
- **Word numbers** start at 127 in every PHROM.

The **project budget** is 64 PHROMs (1 MB). It is a limit chosen for
testing, not a hardware one: a BBC Micro can select only 16 PHROMs at a
time. No change takes a project past it:

- adding phrases that would need a 65th PHROM is refused;
- a settings change whose new encoding would need a 65th PHROM is not
  applied, and the phrase keeps its previous settings and encoding;
- renaming or reordering phrases that would need a 65th PHROM is refused.

The main window shows how many PHROMs the project fills, and the free space
in the last, in its [status bar](main-window.md#the-status-bar);
`speak5220-cli encode` reports the same. [Optimise](optimise.md) never
chooses settings that would take the project past its budget.

## Project settings

A project also has settings for its PHROMs as a whole, set in
[**File > Project Settings...**](project-settings.md):

- its **name**, the title of a BBC Micro disc;
- the **header strings** of its PHROMs, and of single PHROMs;
- its **ASCII map**: a phrase for each of the printable characters, which
  a program can speak by the character's code.

Its **export presets**, exports saved under a name, are saved and deleted
in the [Export dialog](export.md#presets).
