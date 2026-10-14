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

## Commands

A command is followed by the project: a `.s5proj` file, or any other path
for the unpacked (directory) form.

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
  sets one of the project's [default options](encoder.md), under the
  name and in the form the project file uses. An existing file or
  directory is never replaced.
- **`add PROJECT FILE.wav...`** imports WAV files as new phrases at the end
  of the list (see [importing audio](projects.md#importing-audio)). Each
  phrase is named from its file name, or by `--name` when one file is
  given; `--description` gives that one phrase a description. `--normalise` normalises a file that clips or is very quiet; without
  it, such a file is imported unchanged with a warning.
- **`import-phrom PROJECT IMAGE...`** adds the words of Acorn PHROM images
  at the end of the list, as locked phrases with no source audio (see
  [importing PHROM images](projects.md#importing-phrom-images)). A file may
  hold one 16 KB PHROM or several concatenated; the words are added in file
  order, then PHROM order, then word order, and each is reported with the
  name it was given. A file that is not a whole number of 16 KB PHROMs, or
  that cannot be read as one, adds nothing, and the project is not saved.
- **`encode PROJECT`** encodes every phrase whose frames are out of date, in
  list order, and reports the PHROMs the project fills. `--force` encodes
  every phrase that is not locked. A phrase whose encoding would take the
  project past its [budget](projects.md#phroms-and-the-project-budget) keeps
  its previous encoding, and is reported; the others are still encoded and
  saved.
- **`export PROJECT --output DIR`** runs the project's
  [export presets](export.md#presets) into `DIR`, or only those named by
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
  score and the measures behind it, as the [encoder panel](encoder.md)
  shows them, and the loudness of the chip's output: its level where it
  sounds, its peak, the samples the DAC clips, and its level through a
  BBC Micro's speech path and A-weighted. Every phrase must be encoded
  first. The columns are listed in the
  [reference](../reference/cli-options.md#measures).
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

A command that fails prints a message and exits with code 1. A command line
that cannot be used (an unknown command or level, missing arguments,
`--option` without `=`, or `--no-console-log` without `--log-file`) prints
a message and exits with code 2. Every option is listed in the
[command-line options](../reference/cli-options.md) reference, and the
messages in [troubleshooting](troubleshooting.md).
