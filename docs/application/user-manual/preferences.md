# Preferences

**Edit > Preferences...** (Ctrl+, ; on macOS, **Speak5220 > Settings...**,
Cmd+,) sets what belongs to you rather than to a project. Nothing changes
until you press **OK**; **Cancel** leaves everything as it was, and
**Restore Defaults** puts every tab back as it was when Speak5220 was
first run. The choices are kept for next time.

## Appearance

| Setting | Choices | What it does |
|---|---|---|
| **Theme** | **Follow the desktop** (the default), **Light**, **Dark** | How the window looks, as **View > Theme** ([the main window](main-window.md#the-view-menu)) |
| **Waveforms** | **Dark, in either theme** (the default), **As the theme** | Draws the waveforms in colour on black whatever the theme, or in the window's own colours ([light and dark](waveforms.md#light-and-dark)) |

## Audio

| Setting | Choices | What it does |
|---|---|---|
| **Output device** | **System default** (the default), or one of the devices connected now | Where phrases are played. If the chosen device is later disconnected, phrases play on the system's default, and the list shows it as *not connected* until another is chosen |

## New Projects

The [preprocessing](preprocess.md#the-settings) and
[encoder](encode.md#the-settings) settings a new project starts from, in
the same form as the Preprocess and Encode views: the **Preset**, the five
preprocessing settings, and the encoder's three basic and seven advanced
settings. Each setting that differs from the encoder's default is in
bold, with its tick and ↺ button marking the encoder's default.

These are used by **File > New** only. A project keeps the settings it was
made with, so changing these does not change open or saved projects, and
`speak5220-cli new` always starts from the encoder's defaults (set others
with `--option`).

## Logging

| Setting | Choices | What it does |
|---|---|---|
| **Log level** | **Info** (the default), **Debug**, **Trace** | The most detailed messages logged, as `--log-level` ([logging](cli.md#logging)). Warnings and errors are always logged |
| **Log file** | A file, or empty for none (the default). **Browse...** chooses it | A file the log is also written to, replaced at each start, as `--log-file` |

The logging settings take effect the next time Speak5220 starts. A
`--log-level` or `--log-file` given on the command line overrides them
for that run. If the log file cannot be written, Speak5220 still starts,
logging to the console, and says why in the log.

## Remembered without asking

Speak5220 also remembers, without a setting in this dialog: the recent
projects (**File > Open Recent**), the tab last shown, whether the
[advanced encoder settings](encode.md#the-settings) are shown, and the
[listening options](encode.md#listening-options) of the **Audio** menu.
