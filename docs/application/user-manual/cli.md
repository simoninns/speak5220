# Command-line tool

`speak5220-cli` is the application's command-line tool. It creates
projects, imports WAV files into them, encodes them and exports PHROM
images, with the same results as the GUI will give. Run on its own, it
prints its name and version.

```bash
speak5220-cli
speak5220-cli --version
speak5220-cli --help
```

## Commands

A command is followed by the project: a `.s5proj` file, or any other path
for the unpacked (directory) form.

```bash
speak5220-cli new words.s5proj --name "My PHROM"
speak5220-cli add words.s5proj hello.wav goodbye.wav
speak5220-cli add words.s5proj seven.wav --name SEVEN --normalise
speak5220-cli encode words.s5proj
speak5220-cli export words.s5proj --output out --format acorn-phrom
```

- **`new PROJECT`** creates an empty project. `--name` sets its name (by
  default, the file name). An existing file or directory is never replaced.
- **`add PROJECT FILE.wav...`** imports WAV files as new phrases at the end
  of the list (see [importing audio](projects.md#importing-audio)). Each
  phrase is named from its file name, or by `--name` when one file is
  given. `--normalise` normalises a file that clips or is very quiet; without
  it, such a file is imported unchanged with a warning.
- **`encode PROJECT`** encodes every phrase whose frames are out of date, in
  list order, and reports the PHROMs the project fills. `--force` encodes
  every phrase that is not locked. A phrase whose encoding would take the
  project past its [budget](projects.md#phroms-and-the-project-budget) keeps
  its previous encoding, and is reported; the others are still encoded and
  saved.
- **`export PROJECT --output DIR`** runs the project's export presets into
  `DIR`, or only those named by `--preset` (which may be repeated). Instead
  of the presets, `--format acorn-phrom` exports PHROM images named after
  the project, with `--layout separate` (one file per PHROM, the default) or
  `--layout concatenated`. Every phrase must be encoded with its current
  settings first.

`new`, `add` and `encode` save the project in place, keeping the previous
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
that cannot be used (an unknown command or level, missing arguments, or
`--no-console-log` without `--log-file`) prints a message and exits with
code 2. Every option is listed in the
[command-line options](../reference/cli-options.md) reference, and the
messages in [troubleshooting](troubleshooting.md).
