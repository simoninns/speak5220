# Speak5220 application

Speak5220 is a desktop application for creating TMS5220 speech data and
building PHROMs. It is in development: the pages here describe only what is
built so far.

So far the application has:

- a GUI, `speak5220`, whose [main window](user-manual/main-window.md) opens
  and saves projects, imports WAV files as phrases, orders, sorts, renames
  and describes them with undo and redo, and shows how many PHROMs they
  fill; it shows each phrase's original and encoded
  [waveforms](user-manual/waveforms.md) and plays them, in light or dark
  as the desktop is set;
- a command-line tool, `speak5220-cli`, that creates
  [projects](user-manual/projects.md), imports WAV files into them as
  phrases, encodes them and exports Acorn PHROM images, described in the
  [command-line tool](user-manual/cli.md) page and the
  [command-line options](reference/cli-options.md) reference;
- the project file, `.s5proj`, described in the
  [project file](reference/project-file.md) reference;
- an encoder that turns 8 kHz speech into TMS5220 frames, whose options are
  in the [encoder options](reference/encoder-options.md) reference;
- logging options shared by both executables.

Messages and what to do about them are in
[troubleshooting](user-manual/troubleshooting.md). Building it is described
in `BUILD.md` in the repository.
