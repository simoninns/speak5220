# Speak5220 application

Speak5220 is a desktop application for creating TMS5220 speech data and
building PHROMs. It turns recordings into the frames the chip speaks,
shows and plays every stage, rates how close the result is, searches the
settings for a better one, and exports PHROM images, C headers, WAV files
and a BBC Micro disc.

## Start here

- [Installing](installing.md): the installers for Linux, Windows and
  macOS.
- [Making speech](making-speech.md): a walkthrough from a recording to a
  PHROM image, step by step.

## The application in detail

A phrase is made in three stages, each with its own view in the main
window:

| Stage | View | What it does |
|---|---|---|
| 1 | [Original](user-manual/original.md) | The recording as imported |
| 2 | [Preprocess](user-manual/preprocess.md) | Prepares the recording for the chip: noise gate, bass cut, treble tilt, de-esser |
| 3 | [Encode](user-manual/encode.md) | Encodes it into TMS5220 frames, rates the result, and sets the encoder's settings |

The other pages:

- [The main window](user-manual/main-window.md): the menus, the phrase
  list, undo and the status bar.
- [Phrases and PHROMs](user-manual/phrases.md): importing recordings and
  PHROM images, phrase names, how settings apply to a phrase or the whole
  project, and how phrases fill PHROMs.
- [Waveforms and playback](user-manual/waveforms.md): the controls the
  three views share.
- [Optimise](user-manual/optimise.md): searching the settings for the
  best quality.
- [Export](user-manual/export.md), with its presets, and
  [BBC Micro disc](user-manual/bbc-disc.md).
- [Project Settings](user-manual/project-settings.md): the project's name,
  its PHROMs' header strings and its ASCII map.
- [Preferences](user-manual/preferences.md): the theme, the audio device,
  the settings of new projects, and logging.
- [Command-line tool](user-manual/cli.md): `speak5220-cli`, which creates,
  encodes, exports and measures projects from scripts, and every
  command-line option.
- [Troubleshooting](user-manual/troubleshooting.md): messages and what to
  do about them.

## Reference

For those who read or write Speak5220's files directly, or want to know
what the encoder does beyond its settings:

- [Project file](reference/project-file.md): the `.s5proj` format.
- [Export formats](reference/export-formats.md): the byte layout of every
  export.
- [Encoder behaviour](reference/encoder-behaviour.md): what the encoder
  always does, and how its defaults were chosen.

Building Speak5220 from source is described in `BUILD.md` in the
repository.
