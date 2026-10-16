# Command-line tool

`speak5220-cli` is the application's command-line tool. It creates
projects, imports WAV files and PHROM images into them, encodes them and
exports them,
with the same results as the GUI gives. It can also measure a project's
encodings and speak a frames file. Run on its own, it
prints its name and version.

```bash
speak5220-cli
speak5220-cli --version
speak5220-cli --help
speak5220-cli --help-all   # also Qt's own options
```

The installers ([installing](../installing.md)) put it
here:

| Installer | Running `speak5220-cli` |
|---|---|
| Linux (Flatpak) | `flatpak run --command=speak5220-cli io.github.simoninns.speak5220 ...`; it reads and writes files in your home directory only |
| Windows (MSI) | `"C:\Program Files\Speak5220\bin\speak5220-cli.exe" ...` |
| macOS (DMG) | `/Applications/speak5220.app/Contents/MacOS/speak5220-cli ...` |

## Commands

`speak5220-cli COMMAND PROJECT [ARGUMENTS] [OPTIONS]`. `PROJECT` is a
`.s5proj` file, or any other path for the unpacked (directory) form;
`synth` takes a frames file instead. With no command, `speak5220-cli`
prints its name and version.

| Command | Arguments | Options | What it does |
|---|---|---|---|
| `new` | `PROJECT` | `--name`, `--option`, `--output` | Creates an empty project. Fails if `PROJECT` exists |
| `add` | `PROJECT FILE.wav...` | `--name`, `--description` (one file only), `--normalise`, `--output` | Imports WAV files as phrases at the end of the list, and saves |
| `import-phrom` | `PROJECT IMAGE...` | `--output` | Adds the words of each Acorn PHROM image (16 KB, or several 16 KB PHROMs concatenated) to the end of the list as locked phrases with no source audio, in image then word order, and saves ([Importing PHROM images](phrases.md#importing-phrom-images)) |
| `encode` | `PROJECT` | `--force`, `--output` | Encodes the phrases whose frames are out of date, and saves |
| `export` | `PROJECT` | `--output` (required), `--preset`, or `--format` with `--phrase`, `--layout`, `--licence`, `--audio` | Writes exported files into the `--output` directory. Every phrase exported must be encoded with its current settings, except for the original or preprocessed audio |
| `measure` | `PROJECT` | `--output` (required) | Measures every phrase's encoding and writes `measures.tsv` into the `--output` directory ([below](#measures)). Every phrase must be encoded |
| `synth` | `FRAMES.yaml` | `--output` (required) | Speaks a frames file (a phrase's `frames.yaml`, or any YAML file with a `frames` list in that form; [Project file](../reference/project-file.md)) through the emulated chip, as a PHROM word is spoken, and writes the output to the WAV file `--output`: 8 kHz, 16-bit, the chip's I/O codes × 64, before the DAC clips them. A stop frame is added if there is none |

### What each command does

```bash
speak5220-cli new words.s5proj --name "My PHROM"
speak5220-cli new loud.s5proj --option target_rms=30 --option bass_cut_hz=200
speak5220-cli add words.s5proj hello.wav goodbye.wav
speak5220-cli add words.s5proj seven.wav --name SEVEN --normalise
speak5220-cli import-phrom words.s5proj phrom_a.bin
speak5220-cli encode words.s5proj
speak5220-cli export words.s5proj --output out --format acorn-phrom
speak5220-cli export words.s5proj --output out --format c-header-speak-external --phrase HELLO --licence CC0-1.0
speak5220-cli export words.s5proj --output out --preset phrom
speak5220-cli measure words.s5proj --output measures
speak5220-cli synth word.frames.yaml --output word.wav
```

- **`new PROJECT`** creates an empty project. `--name` sets its name (by
  default, the file name). `--option KEY=VALUE`, which may be repeated,
  sets one of the project's [default settings](phrases.md#phrase-settings-and-project-defaults), [preprocessing](preprocess.md#at-a-glance) or [encoder](encode.md#at-a-glance), under the
  name and in the form the project file uses. An existing file or
  directory is never replaced.
- **`add PROJECT FILE.wav...`** imports WAV files as new phrases at the end
  of the list (see [importing audio](phrases.md#importing-audio)). Each
  phrase is named from its file name, or by `--name` when one file is
  given; `--description` gives that one phrase a description. `--normalise` normalises a file that clips or is very quiet; without
  it, such a file is imported unchanged with a warning.
- **`import-phrom PROJECT IMAGE...`** adds the words of Acorn PHROM images
  at the end of the list, as locked phrases with no source audio (see
  [importing PHROM images](phrases.md#importing-phrom-images)). A file may
  hold one 16 KB PHROM or several concatenated; the words are added in file
  order, then PHROM order, then word order, and each is reported with the
  name it was given. A file that is not a whole number of 16 KB PHROMs, or
  that cannot be read as one, adds nothing, and the project is not saved.
- **`encode PROJECT`** encodes every phrase whose frames are out of date, in
  list order, and reports the PHROMs the project fills. `--force` encodes
  every phrase that is not locked. A phrase whose encoding would take the
  project past its [budget](phrases.md#phroms-and-the-project-budget) keeps
  its previous encoding, and is reported; the others are still encoded and
  saved.
- **`export PROJECT --output DIR`** runs the project's
  [export presets](export.md#presets) (saved from the Export dialog, or
  written in the project file) into `DIR`, or only those named by
  `--preset` (which may be repeated). Instead of the presets, `--format`
  exports in one format, with files named after the project file:
  `acorn-phrom` (with `--layout separate`, one file per PHROM, the
  default, or `--layout concatenated`), `c-header-phrom` or
  `c-header-speak-external` (with `--licence`, the header's SPDX licence),
  `wav` (with `--audio encoded`, the default, `--audio original` or
  `--audio preprocessed`), or
  `bbc-dsd` (a [BBC Micro disc](bbc-disc.md)).
  `--phrase NAME`, which may be repeated, exports only those phrases. Every
  phrase exported must be encoded with its current settings first, except
  for the original or preprocessed audio. Existing files of the same names are replaced.
- **`measure PROJECT --output DIR`** measures every phrase's encoding and
  writes `DIR/measures.tsv`, one row per phrase: its size, its quality
  score and the measures behind it, as the [encoder panel](encode.md#the-encoder-panel)
  shows them, and the loudness of the chip's output: its level where it
  sounds, its peak, the samples the DAC clips, and its level through a
  BBC Micro's speech path and A-weighted. Every phrase must be encoded
  first. The columns are listed in the
  [measures](#measures) below.
- **`synth FRAMES.yaml --output FILE.wav`** speaks a frames file (a
  phrase's `frames.yaml`, or any file with a `frames` list in that form)
  through the emulated chip, as a PHROM word is spoken, and writes the
  chip's output as an 8 kHz WAV file, before the DAC clips it.

`new`, `add`, `import-phrom` and `encode` save the project in place, keeping the previous
version as `PROJECT~`. With `--output DIR` they write the project into
`DIR` instead, under the same name, and leave the original as it was:

```bash
speak5220-cli encode app/projects/general/one-phrase --output build/encoded
```

## Command options

| Option | Value | Commands | Meaning |
|---|---|---|---|
| `--name` | text | `new`, `add` | `new`: the project's name (default: the file name without its extension). `add`: the phrase's name, normalised to upper case (default: made from the file name) |
| `--option` | `KEY=VALUE` | `new` | Sets one of the project's default options, of either stage, as the project file writes it: `repeat_lambda=300`, `noise_gate_db=none` ([Preprocess](preprocess.md#at-a-glance) and [Encode](encode.md#at-a-glance) views). May be given more than once |
| `--normalise` | none | `add` | Scales a file that clips or is very quiet (peak below −30 dBFS) to a peak of −1 dBFS. Other files are never changed |
| `--force` | none | `encode` | Encodes every phrase that is not locked, even if its frames are current |
| `--description` | text | `add` | The phrase's description, a note kept in the project only (default: none) |
| `--output` | a directory, or for `synth` a file | all | `new`, `add`, `import-phrom`, `encode`: saves the project into this directory, under its own name, instead of in place. `export`, `measure`: the directory for the files written. Created if missing. `synth`: the WAV file to write |
| `--preset` | a preset name | `export` | Runs this export preset of the project; may be given more than once. Without `--preset` or `--format`, every preset runs |
| `--format` | `acorn-phrom`, `c-header-phrom`, `c-header-speak-external`, `wav` or `bbc-dsd` | `export` | Exports in this format instead of running presets; the files are named after the project file ([Export formats](../reference/export-formats.md)) |
| `--phrase` | a phrase name | `export --format` | Exports only this phrase; may be given more than once. Default: every phrase |
| `--layout` | `separate` or `concatenated` | `export --format acorn-phrom` | One 16 KB file per PHROM (`NAME_1.bin`, `NAME_2.bin`, ...; the default), or all PHROMs in one file (`NAME.bin`) |
| `--licence` | an SPDX licence identifier | `export --format c-header-phrom` or `c-header-speak-external` | The licence written in the header (default `GPL-3.0-or-later`) |
| `--audio` | `encoded`, `original` or `preprocessed` | `export --format wav` | The chip's output (the default), the phrase's 8 kHz source, or that source with the phrase's [preprocessing](preprocess.md) |

## Measures

`measure` writes one tab-separated row per phrase, with a header row. The
quality figures are those of the encoder panel, taken against the phrase's
preprocessed source ([the quality rating](encode.md#the-quality-rating)); they are
blank for a phrase without source audio or with too little speech to
measure. The loudness figures are of the chip's output.

| Column | Meaning |
|---|---|
| `name`, `source` | The phrase's name and its source file's name |
| `frames`, `bytes` | The frame count, and the bytes the frames pack to |
| `score` | The overall quality score, 0 to 1 |
| `envelope_score`, `pitch_score`, `intelligibility_score` | Its three components, 0 to 1 (the last blank when ESTOI cannot be measured) |
| `lsd_db`, `lsd_over_2db`, `lsd_over_4db` | The envelope distortion in dB, and the percentage of frames over 2 and 4 dB |
| `gross_pitch_error`, `f0_rmse_cents`, `voicing_error` | The gross pitch error (%), the F0 RMS error in cents, the voicing decision error (%) |
| `estoi`, `speech_frames` | ESTOI, and the 10 ms frames the measures count as speech |
| `sounding_rms` | The RMS, in codes, of the output's level at each voiced, unvoiced and repeat frame: what `target_rms` aims at |
| `peak` | The largest code magnitude |
| `clipped_percent` | The percentage of samples beyond ±127, which the DAC clips |
| `wrapped_samples` | Samples in which a lattice sum wrapped (datasheet §8.8) |
| `bbc_rms` | The output's RMS in codes after a first-order 279 Hz high-pass: the level through a BBC Micro's speech path |
| `a_weighted_rms` | The output's A-weighted RMS in codes: a rough loudness |

## General options

| Option | Executables | Meaning |
|---|---|---|
| `--help`, `-h` | both | Lists the options and exits |
| `--help-all` | both | Lists the options together with Qt's generic options (such as `-platform` for the GUI) and exits |
| `--version`, `-v` | both | Prints the version and exits |

## The GUI's options

`speak5220 [PROJECT]` opens `PROJECT`, a `.s5proj` file or a project
directory, at start-up. More than one is an unusable command line (exit
code 2).

## Logging

Both `speak5220-cli` and the GUI, `speak5220`, write a log. By default it
goes to the console (stderr) at the `info` level: normal progress messages,
warnings and errors.

- **More detail.** `--log-level debug` adds configuration details and
  decisions; `--log-level trace` adds per-frame and per-sample detail, which
  is a lot of output.
- **A log file.** `--log-file PATH` also writes every message to `PATH`,
  with a timestamp and the thread on each line. An existing file is
  replaced.
- **File only.** `--no-console-log` turns the console output off, which is
  useful for `trace` runs. It needs `--log-file`.

```bash
speak5220-cli --log-level debug --log-file speak5220.log encode words.s5proj
speak5220 --log-level trace --log-file trace.log --no-console-log
```

!!! note
    Warnings and errors are always logged, whatever the level. On Windows
    the GUI has no console, so a log file is the only way to see its log.

| Option | Value | Default | Meaning |
|---|---|---|---|
| `--log-level` | `info`, `debug` or `trace` | `info` | The most detailed messages logged. Warnings and errors are logged at every level |
| `--log-file` | a path | none | Also writes every message to this file, replacing it |
| `--no-console-log` | none | off | Turns console (stderr) output off. Needs `--log-file` |

What each level adds:

| Level | Messages |
|---|---|
| `info` | Normal progress, stage changes, validation summaries |
| `debug` | Everything at `info`, plus configuration details, decisions and resolved paths |
| `trace` | Everything at `debug`, plus per-frame, per-sample and per-slot state |

Line formats:

| Destination | Format |
|---|---|
| Console | `[level] [module] message`, coloured on a terminal |
| File | `YYYY-MM-DD HH:MM:SS.mmm [level] [thread N] [module] message` |

## Exit codes

The messages, and what to do about them, are in
[troubleshooting](troubleshooting.md).


| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | The command failed: a project or file that cannot be read or written, an invalid or taken name, an unknown option or value for `--option`, a phrase that cannot be encoded or would exceed the budget, an export that cannot run, a phrase `measure` finds unencoded, a frames file `synth` cannot read, or a file `import-phrom` cannot read as Acorn PHROM images. The message is on stderr |
| 2 | The command line cannot be used: an unknown command or level, missing arguments, `--name` or `--description` with more than one file, `export`, `measure` or `synth` without `--output`, `--option` without `=`, `--preset` with `--format`, `--phrase`, `--layout`, `--licence` or `--audio` without `--format`, `--no-console-log` without `--log-file`, or a log file that cannot be opened |
