# Command-line tool

`speak5220-cli` is the application's command-line tool. It will run the
same encoding and export as the GUI, without a window. So far it has no
commands: run on its own, it prints its name and version.

```bash
speak5220-cli
speak5220-cli --version
speak5220-cli --help
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
speak5220-cli --log-level debug --log-file speak5220.log
speak5220 --log-level trace --log-file trace.log --no-console-log
```

!!! note
    Warnings and errors are always logged, whatever the level. On Windows
    the GUI has no console, so a log file is the only way to see its log.

A command line that cannot be used (an unknown level, or
`--no-console-log` without `--log-file`) prints a message and exits with
code 2. Every option is listed in the
[command-line options](../reference/cli-options.md) reference.
