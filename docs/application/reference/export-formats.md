# Export formats

The files each export format writes, and their layout. `NAME` is the
name the export is run under: the preset's name, the project file's name
with `--format`, or the **Name** field of the
[Export dialog](../user-manual/export.md). A phrase's **C identifier** is
its name with every run of other characters than letters and digits made
one `_`, none leading or trailing, `W_` before a leading digit, and `_2`,
`_3`, ... after one that is already taken (`ONE,WON` gives `ONE_WON`,
`-ING` gives `ING`, `2- (TWEN-)` gives `W_2_TWEN`). Phrases are always in
the project's list order.

| Format key | Files |
|---|---|
| `acorn-phrom` | `NAME_1.bin`, `NAME_2.bin`, ... (layout `separate`), or `NAME.bin` (layout `concatenated`) |
| `c-header-phrom` | `NAME.h` |
| `c-header-speak-external` | `NAME.h` |
| `wav` | `NAME_IDENTIFIER.wav` per phrase |

## Acorn PHROM images

Each image is 16 384 bytes, in the layout of
[datasheet §12.8](../../technical/datasheet/authoring.md#128-word-tables):
a bit-reversed 64-byte header holding the header strings, a 14-bit
pointer for each word number from 32, 0xFF after the last pointer, then
each word's name (backwards, each character shifted left and bit-reversed)
before its packed frames. Word numbers 32 to 126 are the ASCII entries:
an entry of the project's ASCII map points to its phrase if the phrase is
in that PHROM, and every other entry to a one-byte null word. Phrases are
words from 127, filling PHROM 1 in list order, then PHROM 2, as the
[project budget](../user-manual/projects.md#phroms-and-the-project-budget)
places them. Each PHROM has the project's header strings with its number
appended to the second, unless the project sets that PHROM's own.

With the `concatenated` layout, the images follow one another in one
file, PHROM 1 first.

## C header, PHROM images

The header's names start with `NAME`'s C identifier, in upper case for
constants and in lower case for variables and types. Exported as `n`, a
header of two PHROMs holding `HELLO` and `SEVEN` reads as follows (the
offsets and lengths are an example):

```c
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * TMS5220 speech data from the Speak5220 project "Words":
 * 2 Acorn PHROM images of 16384 bytes.
 * This file is generated: export it again rather than edit it.
 *
 * Phrase names and their C identifiers:
 *   HELLO  N_WORD_HELLO
 *   SEVEN  N_WORD_SEVEN
 */

#ifndef N_H_
#define N_H_

#include <stdint.h>

#define N_PHROMS 2
#define N_PHROM_SIZE 16384
#define N_WORDS 2

static const uint8_t n_phrom_1[N_PHROM_SIZE] = {
    0x00, 0xFF, ...
};

static const uint8_t n_phrom_2[N_PHROM_SIZE] = { ... };

enum n_word {
  N_WORD_HELLO,
  N_WORD_SEVEN,
};

/* A word's PHROM (from 1), its word number, and the offset and
   length of its data in the PHROM. */
struct n_entry {
  uint8_t phrom;
  uint16_t number;
  uint16_t offset;
  uint16_t length;
  const char *name;
};

static const struct n_entry n_words[N_WORDS] = {
  {1, 127, 0x0106, 812, "HELLO"},  /* N_WORD_HELLO */
  {2, 127, 0x0106, 640, "SEVEN"},  /* N_WORD_SEVEN */
};

#endif  /* N_H_ */
```

- **The arrays** hold the images byte for byte, as the `acorn-phrom`
  format writes them, 12 bytes to a line.
- **The table** has one entry per phrase, in the order of the enum, so
  `n_words[N_WORD_SEVEN]` describes `SEVEN`. `offset` is where the
  phrase's packed frames start in its PHROM (after its name), and
  `length` their bytes, the stop code's byte included.
- **The licence** on the SPDX line is the preset's `licence`.
- **The project's name** is written as it is, except that `*/` and line
  breaks are changed so that they cannot end the comment.

## C header, Speak External streams

Each phrase's frames packed as a Speak External stream
([datasheet §12.9](../../technical/datasheet/authoring.md#129-speak-external-streams)):
in the order the chip's FIFO takes them, so they are written to the chip
as they are, with silence frames added before the stop code if needed to
make the 9 bytes the chip needs before it speaks. Exported as `n`, for
`HELLO` and `SEVEN`:

```c
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * ... as above, "2 Speak External streams, each written to the chip's
 * FIFO byte by byte as it is."
 */

#ifndef N_H_
#define N_H_

#include <stdint.h>

#define N_WORDS 2

#define N_LENGTH_HELLO 806
static const uint8_t n_word_hello[N_LENGTH_HELLO] = { ... };

#define N_LENGTH_SEVEN 634
static const uint8_t n_word_seven[N_LENGTH_SEVEN] = { ... };

enum n_word {
  N_WORD_HELLO,
  N_WORD_SEVEN,
};

struct n_entry {
  const char *name;
  const uint8_t *data;
  uint16_t length;
};

static const struct n_entry n_words[N_WORDS] = {
  {"HELLO", n_word_hello, N_LENGTH_HELLO},
  {"SEVEN", n_word_seven, N_LENGTH_SEVEN},
};

#endif  /* N_H_ */
```

A stream's length is a `uint16_t`, so a phrase of more than 65 535 bytes
is refused.

**Names cannot clash.** The constants for phrases all start `N_WORD_` and
`N_LENGTH_`, their arrays `n_word_`, and nothing else in either header
does: whatever the phrases are called, the names stay distinct.

## WAV audio

One mono WAV file per phrase, at 8000 Hz:

| `audio` | Samples | Format |
|---|---|---|
| `encoded` (the default) | The chip's output through the VSM path (a PHROM word spoken with Speak), from the sample in which the first frame is read until the output has been idle for 25 samples and a frame more. Each output code is scaled by 64 and clipped to the DAC's range (−128 to +127, the SPEAKER pin), so full scale is a quarter of 16-bit full scale | 16-bit PCM |
| `original` | The phrase's audio as the project holds it: its source, resampled to 8 kHz and mixed to mono when it was imported | 24-bit PCM |
