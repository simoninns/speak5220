# Command-line options

Options of `speak5220-cli` and of the GUI, `speak5220`. Both take the
logging options.

## General

| Option | Executables | Meaning |
|---|---|---|
| `--help`, `-h` | both | Lists the options and exits |
| `--version`, `-v` | both | Prints the version and exits |

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
| 2 | The command line cannot be used: an unknown level, `--no-console-log` without `--log-file`, or a log file that cannot be opened |
