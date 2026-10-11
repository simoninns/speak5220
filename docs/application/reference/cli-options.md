# Command-line options

Options of `speak5220-cli` and of the GUI, `speak5220`. Both take the
general and logging options; the commands and their options are
`speak5220-cli`'s alone.

## General

| Option | Executables | Meaning |
|---|---|---|
| `--help`, `-h` | both | Lists the options and exits |
| `--version`, `-v` | both | Prints the version and exits |

## The GUI's argument

`speak5220 [PROJECT]` opens `PROJECT`, a `.s5proj` file or a project
directory, at start-up. More than one is an unusable command line (exit
code 2).

## Commands

`speak5220-cli COMMAND PROJECT [ARGUMENTS] [OPTIONS]`. `PROJECT` is a
`.s5proj` file, or any other path for the unpacked (directory) form. With
no command, `speak5220-cli` prints its name and version.

| Command | Arguments | Options | What it does |
|---|---|---|---|
| `new` | `PROJECT` | `--name`, `--output` | Creates an empty project. Fails if `PROJECT` exists |
| `add` | `PROJECT FILE.wav...` | `--name` (one file only), `--normalise`, `--output` | Imports WAV files as phrases at the end of the list, and saves |
| `encode` | `PROJECT` | `--force`, `--output` | Encodes the phrases whose frames are out of date, and saves |
| `export` | `PROJECT` | `--output` (required), `--preset`, `--format`, `--layout` | Writes exported files into the `--output` directory |

## Command options

| Option | Value | Commands | Meaning |
|---|---|---|---|
| `--name` | text | `new`, `add` | `new`: the project's name (default: the file name without its extension). `add`: the phrase's name, normalised to upper case (default: made from the file name) |
| `--normalise` | none | `add` | Scales a file that clips or is very quiet (peak below −30 dBFS) to a peak of −1 dBFS. Other files are never changed |
| `--force` | none | `encode` | Encodes every phrase that is not locked, even if its frames are current |
| `--output` | a directory | all | `new`, `add`, `encode`: saves the project into this directory, under its own name, instead of in place. `export`: the directory for the exported files. Created if missing |
| `--preset` | a preset name | `export` | Runs this export preset of the project; may be given more than once. Without `--preset` or `--format`, every preset runs |
| `--format` | `acorn-phrom` | `export` | Exports in this format instead of running presets; the files are named after the project |
| `--layout` | `separate` or `concatenated` | `export --format acorn-phrom` | One 16 KB file per PHROM (`NAME_1.bin`, `NAME_2.bin`, ...; the default), or all PHROMs in one file (`NAME.bin`) |

## Logging

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

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | The command failed: a project or file that cannot be read or written, an invalid or taken name, a phrase that cannot be encoded or would exceed the budget, or an export that cannot run. The message is on stderr |
| 2 | The command line cannot be used: an unknown command or level, missing arguments, `--name` with more than one file, `export` without `--output`, `--preset` with `--format`, `--no-console-log` without `--log-file`, or a log file that cannot be opened |
