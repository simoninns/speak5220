# Project file

A Speak5220 project is one file, `NAME.s5proj`: a ZIP archive that any ZIP
tool can open. The same files can also be kept **unpacked**, as a
directory with the same layout. Speak5220 opens and saves either form: a
path ending in `.s5proj` is the ZIP form, any other path the directory
form.

```
project.yaml                    format, version, project settings, phrase order, export presets
phrases/<id>/source.wav         the phrase's audio: 8 kHz mono, 24-bit PCM
phrases/<id>/frames.yaml        the phrase's frames, and the inputs they were made from
```

`<id>` is the phrase's id, a lower-case UUID. A phrase made from frames
alone (a locked phrase with no source audio) has no `source.wav`, and a
phrase not yet encoded has no `frames.yaml`.

The metadata files are YAML 1.2. Keys are lower case with underscores.

!!! note
    **Strings are always written in double quotes**, so names such as `NO`,
    `YES`, `ON` and `OFF` stay names for any YAML reader. **Numbers** are
    written with a `.` as the decimal point, whatever the computer's
    language settings. When reading, a value of the wrong type is refused
    with a message naming the file and the key, for example
    `project.yaml: phrases[2].play_start must be a whole number`.

## project.yaml

```yaml
format: "speak5220-project"
version: 1
project:
  name: "My PHROM"
  header_strings: ["(C)2026 Simon Inns", "My PHROM", "1.00"]
  ascii_map:
    " ": "(TONE 1)"
    "1": "ONE,WON"
  default_encoder:
    target_rms: 24
    repeat_lambda: 70
    squelch_db: null
    pre_emphasis: "chirp"
    # ... every encoder option, each one required
  phroms:
    - number: 2
      header_strings: ["(C)2026 Simon Inns", "Numbers", "1.00"]
phrases:
  - id: "6f2c1f0e-3b7a-4c1d-9e2f-0a1b2c3d4e5f"
    name: "HELLO"
    description: "Greeting, recorded 2026-10-10"
    source:
      file: "hello.wav"
      rate: 44100
      channels: 2
      duration: 0.51
      sha256: "4efae645c896..."
    encoder:
      gate_db: 40
    locked: false
    play_start: 0
exports:
  - name: "phrom"
    format: "acorn-phrom"
    scope: "all"
    layout: "separate"
```

### Top level

| Key | Type | Meaning |
|---|---|---|
| `format` | String | Always `"speak5220-project"`. A file with any other value is not opened |
| `version` | Whole number | The format version, 1. A project from a newer version of Speak5220 is refused rather than misread; older versions are upgraded when opened |
| `project` | Mapping | The project settings, below. Required, for `default_encoder`; the other settings are optional and take their defaults |
| `phrases` | List | The phrases, in PHROM order: the order of this list is the order of the words in the PHROMs (datasheet §12.8) |
| `exports` | List | The export presets, below |

### project

| Key | Type | Default | Meaning |
|---|---|---|---|
| `name` | String | The file name, for a project made by `speak5220-cli new` | The project's name |
| `header_strings` | List of strings | `["(C)2026 speak5220", "Speak5220 PHROM", "1.00"]` | The strings in each PHROM's header. Each PHROM gets its number appended to the second string (`"My PHROM 2"`). Together, with a null after each, they must fit in 54 bytes, and they must be ASCII; the BBC Micro expects the first to start `(C)` |
| `ascii_map` | Mapping | Empty | The "ASCII associated" words: each key is one printable ASCII character, each value a phrase name. A PHROM includes only the entries whose phrase it holds |
| `default_encoder` | Mapping | Required | The options every phrase is encoded with, unless it sets its own. Every [encoder option](encoder-options.md) must be listed, an unset one as `null`, so a project never depends on the defaults: a later version of Speak5220 that changes one does not change the project's encodings. A project that does not list them all, such as one saved by a development version before this rule, is not opened, and the message names the missing options |
| `phroms` | List | Empty | Header strings for single PHROMs: `number` (from 1) and `header_strings`, which replace the project's strings in that PHROM |

### phrases

| Key | Type | Default | Meaning |
|---|---|---|---|
| `id` | String | Required | A lower-case UUID, which also names the phrase's directory. Anything else is refused |
| `name` | String | Required | The phrase name: the word's name in the PHROM. See the rules below |
| `description` | String | `""` | Free text (UTF-8), kept only in the project |
| `source` | Mapping | None | Where the audio came from, below. Every phrase that is not locked has one |
| `encoder` | Mapping | Empty | The [encoder options](encoder-options.md) this phrase sets for itself. Options it does not list follow `default_encoder`, also when that changes |
| `locked` | `true` or `false` | `false` | A locked phrase is never re-encoded: its frames are its content |
| `play_start` | Whole number | 0 | Where playback starts, in samples of the 8 kHz audio: from 0 to the audio's length. Set by clicking in a [waveform](../user-manual/waveforms.md#the-play-start) |

**Phrase names** follow the Acorn PHROM A word list:

- upper-case `A`–`Z`, digits, space, and `' ( ) , - .`;
- at most 16 characters;
- unique within the project.

Lower case is converted to upper case, and leading and trailing spaces are
removed. Any other character is refused, with a message.

### source

| Key | Type | Meaning |
|---|---|---|
| `file` | String | The imported file's name, without its directory |
| `rate` | Whole number | Its sample rate before resampling (0: not known) |
| `channels` | Whole number | Its channel count before mixing (0: not known) |
| `duration` | Number | Its length in seconds |
| `sha256` | String | The SHA-256 of the imported file's bytes |

The original file is not kept in the project: only its 8 kHz copy, in
`source.wav`. Importing the original again is the way back if a phrase ever
needs resampling again.

### exports

| Key | Type | Meaning |
|---|---|---|
| `name` | String | The preset's name, unique in the project. The exported files are named after it |
| `format` | String | `"acorn-phrom"`: Acorn PHROM images (datasheet §12.8). Other formats are not available yet |
| `scope` | String | `"all"`: every phrase |
| `layout` | String | For PHROM images: `"separate"` (one 16 KB file per PHROM, `NAME_1.bin`, `NAME_2.bin`, ...) or `"concatenated"` (one file, `NAME.bin`) |

## frames.yaml

```yaml
encoder_version: "1.0.0"
settings_hash: "ec247184561a..."
frames:
  - {energy: 9, repeat: 0, pitch: 34, k: [12, 20, 8, 7, 9, 8, 7, 4, 3, 2]}
  - {energy: 9, repeat: 1, pitch: 35}
  - {energy: 4, repeat: 0, pitch: 0, k: [20, 18, 9, 6]}
  - {energy: 0}
  - {energy: 15}
quality:
  measures_version: 1
  lsd_db: 3.34
  lsd_over_2db: 82.4
  lsd_over_4db: 27.8
  gross_pitch_error: 5.6
  f0_rmse_cents: 60.1
  voicing_error: 9.9
  estoi: 0.76
  score: 0.70
```

| Key | Type | Meaning |
|---|---|---|
| `encoder_version` | String | The version of the encoder that made the frames |
| `settings_hash` | String | A SHA-256 of the audio, the encoder options in effect and the encoder version. When it no longer matches, the encoding is out of date and `speak5220-cli encode` redoes it. Empty for frames that were not encoded by Speak5220 |
| `frames` | List | One flow mapping per frame, ending in a stop frame |
| `quality` | Mapping | Optional: the quality measured with the encoding (below) |

Each frame (datasheet §8.1) has `energy` (0–15; 0 is a silence frame, 15
the stop frame), and, for the other kinds, `repeat` (0 or 1), `pitch`
(0–63; 0 is unvoiced) and `k`, the coefficient indices: four for an
unvoiced frame, ten for a voiced one, none for a repeat. Missing fields are
0. Every frame is checked against its field widths when the project is
opened.

The `quality` mapping holds the [quality rating](../user-manual/encoder.md#the-quality-rating)'s
figures, as the encoder panel and Optimise measured them against the
phrase's recording (squelched, if the phrase is):

| Key | Type | Meaning |
|---|---|---|
| `measures_version` | Integer | The version of the quality measures that gave the figures. Figures from another version are dropped when the project is opened, and measured again |
| `lsd_db` | Number | The mean log spectral distortion between the envelopes, in dB |
| `lsd_over_2db`, `lsd_over_4db` | Number | The percentages of speech frames whose distortion is over 2 dB and over 4 dB |
| `gross_pitch_error` | Number | The percentage of frames voiced in both whose pitch differs by more than 20% |
| `f0_rmse_cents` | Number | The RMS pitch difference over the other frames voiced in both, in cents |
| `voicing_error` | Number | The percentage of speech frames whose voicing differs |
| `estoi` | Number | ESTOI, 0 to 1; absent when the phrase had too little speech for it |
| `score` | Number | The overall score, 0 to 1 |

Projects saved before the quality measures had `pesq` and `anchor` keys
here; they are dropped when the project is opened, and the quality is
measured again. Other keys are kept.

For a phrase that is not locked, the frames are a cache: they are kept so a
project opens instantly, and are redone from the audio and options when
those change. The packed bytes are not stored, since they are rebuilt from
the frames.

## Keys and files from newer versions

Keys that this version does not know, in any mapping, are kept and written
back unchanged when the project is saved, with their quoting. So are files
in the project that it does not use. A project can therefore go back and
forth between versions without losing what a newer one added.

## Saving and opening

- **Saving is atomic.** The ZIP form is written to a temporary file beside
  the project, which replaces the project only when it is complete; the
  directory form is written to `NAME.saving` and then swapped in. If saving
  fails or is interrupted, the previous file is left as it was.
- **The previous version is kept** beside the project, as `NAME.s5proj~`
  (or `NAME~` for the directory form), replaced on each save.
- The ZIP form's entries have a fixed date (1 January 1980), so saving the
  same project twice gives the same file.
- A directory that holds files but no `project.yaml` is never replaced.
- **Opening is checked.** An archive or directory with a path that could
  lead outside the project (an absolute path, `..`, a drive letter, a
  backslash), a symbolic link, or more than 1 GB of files is refused.
