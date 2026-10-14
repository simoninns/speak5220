# Troubleshooting

Messages Speak5220 gives, what they mean, and what to do. `speak5220-cli`
prints them on stderr, prefixed `speak5220-cli:`.

## Phrase names

| Message | Meaning |
|---|---|
| `a phrase name cannot contain "!": use A-Z, 0-9, space and ' ( ) , - .` | Names use the Acorn PHROM's characters only (see [phrases and their names](projects.md#phrases-and-their-names)). Lower case is accepted and converted |
| `"..." is 17 characters long; a phrase name has at most 16` | Shorten the name |
| `a phrase name cannot be empty` | Give a name with at least one character other than spaces |
| `there is already a phrase called HELLO` | Names are unique within a project; choose another, or rename the existing phrase |

## The project budget

| Message | Meaning |
|---|---|
| `cannot add 1 phrase: the project would need 65 PHROMs, and its budget is 64` | The project already fills its 64 PHROMs; remove or shorten phrases first (see [the project budget](projects.md#phroms-and-the-project-budget)) |
| `the new settings for HELLO were not applied: ... It keeps its previous settings and encoding.` | Re-encoding the phrase with the new options would need a 65th PHROM. Choose options that give fewer bytes, or make room |
| `HELLO: cannot re-encode HELLO: the project would need 65 PHROMs, ...` | From `speak5220-cli encode`: the phrase's new encoding does not fit. The phrase keeps its previous encoding (or stays unencoded); the other phrases are still encoded |
| `HELLO needs 17000 bytes, more than an empty PHROM holds (16128): shorten or re-encode it` | A single phrase must fit in one PHROM. Shorten its audio, or encode it with options that give fewer bytes |

## Encoding and export

| Message | Meaning |
|---|---|
| `HELLO is not encoded with its current settings: re-encode it first` (Export), `... run "speak5220-cli encode" first` (`speak5220-cli export`) | Export uses the stored frames, so they must be up to date. Only a WAV export of the original audio needs no encoding |
| `HELLO has not been encoded yet, so it cannot be exported` | As above, for a phrase with no frames at all |
| `HELLO has no source audio, so its original cannot be exported` | A WAV export of the original audio needs the phrase's audio, and a locked phrase made from frames alone (such as a word from a PHROM image) has none |
| `HELLO is locked: its frames are never re-encoded` | Locked phrases keep their frames; their encoder settings cannot be changed |
| `export format "c-header" is not one of "acorn-phrom", "c-header-phrom", "c-header-speak-external", "wav", "bbc-dsd"` | The preset or `--format` names a format Speak5220 does not have; see [Export formats](../reference/export-formats.md) |
| `the layout "concatenated" does not apply to the format "wav"` | A preset holds an option of another format (`layout` belongs to `acorn-phrom`, `licence` to the C headers, `audio` to `wav`). Remove it |
| `export scope "selected" needs the phrases it exports` | A preset of scope `selected` must list its `phrases` |
| `the export names a phrase the project does not have: HELLO` | A preset's `phrases`, or `--phrase`, names a phrase that was renamed or removed. Edit the preset |
| `exported files cannot be named from "../out": use a name without / \ : * ? " < > \|` | The exported files are named after the preset (or the project file), so the name must be a plain file name |
| `the licence "MIT */" is not an SPDX identifier (letters, digits, spaces and ". + - ( )")` | A C header's licence is written in its comment, so only an SPDX identifier or expression is accepted |
| `HELLO needs 70000 bytes, more than a C header's stream holds (65535)` | A Speak External header gives each phrase's length as a 16-bit number. Shorten the phrase |
| `the project has no export presets: give --format` | `speak5220-cli export` without `--preset` or `--format` runs the project's presets, and there are none |
| `these phrases fill 9 PHROMs, and a BBC Micro disc holds at most 8: export fewer phrases` | A [BBC Micro disc](bbc-disc.md) holds 8 PHROMs. Export only some of the phrases (**Selected phrases**, or `--phrase`) |

## The BBC Micro disc

What the [disc's player](bbc-disc.md) says on the BBC Micro:

| Message | Meaning |
|---|---|
| `This BBC Micro has no speech system.` | The operating system found no speech chip. The player needs the Acorn speech system (the TMS5220; no PHROM is needed) |
| `Not enough memory: PAGE must be &2554 or lower.` | Another filing system or ROM has raised PAGE, leaving too little memory beside the player's 16 KB buffer. Turn off ROMs that claim workspace, or use Acorn DFS |
| `None: the disc has no INDEX files.` | The disc holds no PHROMs: it was not written by the BBC Micro disc export, or its files were deleted |

## The encoder and Optimise

| Message | Meaning |
|---|---|
| `cannot re-encode HELLO: the project would need 65 PHROMs, and its budget is 64` (Change Encoder Settings, Re-encode) | The new encoding is bigger and does not fit. The phrase keeps its settings and frames, and the form shows them again; with several phrases, none is changed. Choose settings that give fewer bytes, or make room |
| `none of the phrases can be optimised: they are locked or have no audio` (Optimise) | Optimise needs a phrase's audio to try settings on, and locked phrases are never re-encoded |
| `No better settings were found within the limits.` (Optimise) | No settings tried scored higher than the current ones while fitting the byte limit and the budget; or none fitted at all. Nothing changes. Raise the byte limit, or leave it off |
| `Quality not measured` (encoder panel) | The quality could not be measured: the encoding holds no sound, or the recording holds no speech the voice activity detector finds (with a noise gate, check it does not gate everything) |
| `ESTOI too short` (encoder panel figures) | The phrase has under about 0.4 s of speech, too little for the intelligibility measure; the rating comes from the envelope and pitch alone |
| A message titled **Encode HELLO** | The encoder refused the phrase's audio or settings, for the reason given; the phrase keeps its frames |
| *Stale: made with other settings* on phrases whose settings did not change | They were encoded by an earlier version of the encoder, whose output differs: for example, before [quiet pauses](encoder.md#quiet-pauses). Their frames still work; re-encode them (**Phrase > Re-encode**) to get the current encoder's |
| A faint buzz or whine in the pauses of the encoded audio | The window plays the emulator, which is not exact at this level: a pause it plays with a whine can be silent on a real chip, and the other way round (see [quiet pauses](encoder.md#quiet-pauses)). On the chip, a pause buzzes only when it is short: two silence frames or fewer, which the encoder leaves alone |

## Importing audio

| Message | Meaning |
|---|---|
| `warning: hello.wav clips; --normalise would bring its peak to -1 dBFS` | The file reaches full scale. It was imported unchanged; add it again with `--normalise` to scale it down |
| `warning: hello.wav is very quiet; ...` | The file's peak is below −30 dBFS. It was imported unchanged; the encoder sets its own level, but `--normalise` brings it up first |
| `hello.wav does not exist or is not a file` | Check the path: the file may have been moved, or the path names a folder |
| `cannot read hello.wav as a WAV file: Format not recognised.` | The file is not a WAV file (only WAV is read), or is empty. The reason after the colon comes from libsndfile, the library that reads WAV files |
| `cannot read hello.wav as a WAV file: Error in WAV file. No 'data' chunk marker.` (or `Malformed 'fmt ' chunk.`, and others) | The file starts as a WAV file but is damaged or cut short, or uses an encoding libsndfile does not know. Export it again from the program that made it, as 16-bit or 24-bit PCM |
| `hello.wav is not a WAV file` / `hello.wav: unsupported WAV sample format` | libsndfile read the file, but it is another format, or its samples are of a kind Speak5220 does not take |
| `hello.wav holds no samples` / `hello.wav is truncated: ...` | The file has a header but no audio, or less than its header says. A file cut short is imported as far as its samples go when they can be read |

## Importing PHROM images

| Message | Meaning |
|---|---|
| `the PHROM image is empty` | The file holds nothing |
| `a PHROM image is a multiple of 16384 bytes (16 KB per PHROM), not N` | An Acorn PHROM image is 16 KB, and a file of several holds them one after another. The file is something else, or was cut short |
| `not an Acorn PHROM image: its header does not start with 0x00, 0xFF` | The file is 16 KB but not an Acorn PHROM (datasheet §12.8); a TI-layout image, for example, is not read |
| `the PHROM header counts do not fit: ...` / `the PHROM pointer table (P pointers) does not fit in 16 KB` | The header's counts of entries are impossible: the image is damaged |
| `PHROM word N points into the header or the pointer table (offset X)` / `PHROM word N (offset X) has no stop code within the bank` | A word's pointer, or its data, is damaged. No word of that image is imported |
| `PHROM B of the image: ...` | In a file of several images, the one numbered B (from 1) is unreadable, for the reason after the colon |
| `PATH: cannot be read: ...` / `PATH: too large for PHROM images (N bytes)` (Import PHROM Images) | The main window could not read the file, or the file is larger than a project's 64 PHROMs |

## Opening and saving projects

| Message | Meaning |
|---|---|
| `not a Speak5220 project: there is no project.yaml` | The file or directory is not a project |
| `PATH is not a usable project archive: Not a zip archive` / `...: Possibly truncated or corrupted zip archive` | A `.s5proj` file is a ZIP archive, and this one is not, or was cut short (for example by a full disk or an interrupted copy). The previous version, `NAME.s5proj~`, may still open |
| `project.yaml is not valid YAML: yaml-cpp: error at line 2, column 1: ...` | The project file was edited by hand and its syntax broken at the place given. Fix it there (an unpacked project), or use the previous version |
| `phrases/<id>/source.wav: not a usable WAV file: ...` | A phrase's stored audio is damaged |
| `phrases/<id>/frames.yaml: frames[0] must be a mapping` | A phrase's stored frames are damaged; the message names the file and the entry |
| `project.yaml: phrases[2].play_start must be a whole number` | A value in the project file has the wrong type; the message names the file and the key. See the [project file](../reference/project-file.md) reference |
| `the project was saved by a newer version of Speak5220 (format version 2; this version reads up to 1)` | Open it with the newer version |
| `project.yaml: project.default_encoder does not list ...: ... cannot be opened` | The project was saved by a development version of Speak5220 that did not list every encoder option, or before an option existed. A project lists every option, so it is not opened. Adding the missing options to `default_encoder` in `project.yaml` (in an unpacked project) with the values it was made with makes it open: `resonance_hold_db: null` (no hold, as before it existed). See the [project file](../reference/project-file.md) and [encoder options](../reference/encoder-options.md) references |
| `project.yaml: project.default_preprocess does not list noise_gate_db, noise_gate_ms: ... cannot be opened` | The project was saved by a development version of Speak5220 that had a squelch (`squelch_db`) instead of the [noise gate](encoder.md#the-noise-gate). In `project.yaml` (in an unpacked project), replace `squelch_db: ...` under `default_preprocess` with `noise_gate_db: null` and `noise_gate_ms: 100`, and remove any `squelch_db` a phrase's `preprocess` sets. A project that used the squelch can turn the noise gate on at the same level instead, but it gates more softly, so those phrases encode differently |
| `project.yaml: project.default_preprocess does not list de_ess_db: ... cannot be opened` | The project was saved by a development version of Speak5220 from before the [de-esser](encoder.md#the-de-esser). In `project.yaml` (in an unpacked project), add `de_ess_db: null` under `default_preprocess` (off, as before it existed). Its encodings are then redone, giving the same frames |
| `... holds an unsafe path, "../evil.txt": refusing to open it` | The archive has a path that could write outside the project. It may have been made by another tool, or tampered with; it is not opened |
| `... is not a Speak5220 project (it has no project.yaml): not replacing it` | Saving the unpacked form never replaces a directory that holds other files |
| `phrases/<id>/source.wav is missing: phrase HELLO has no audio` | A file of the project has been deleted. The previous version, `NAME.s5proj~`, may still have it |
| `cannot open PATH: ...` / `cannot save PATH: ...` | The main window's form of the messages above, naming the project. A failed save leaves the project open with its changes, and the file as it was |

## The main window

| Message | Meaning |
|---|---|
| A message titled **Add Phrases** | A file could not be imported; each line names the file and says why (as under [importing audio](#importing-audio)). The other files were added |
| A message titled **Import PHROM Images** | An image could not be read; each line names the file and says why (as under [importing PHROM images](#importing-phrom-images)). The words of the other images were added |
| `cannot sort the phrases: the project would need 65 PHROMs, ...` (Edit Phrases) | A sort, or a move (`cannot move the phrases: ...`), would take the project past its budget, so it was not made |
| `there is no audio output device: connect one, or check the system's sound settings` (Play) | The system has no output device to play on. Connect or enable one |
| `cannot play on DEVICE: it may be in use or disconnected; choose another in Preferences` (Play) | The output device would not start. Another program may hold it, or it may not take 16-bit audio. Choose another under **Edit > Preferences > Audio** |
| `the audio device DEVICE stopped while playing: check that it is still connected, or choose another in Preferences` (Play) | The device failed during playback, for example because it was unplugged. Playback stops; playing again tries the device again, or the system's default if it has gone |
| `the log file chosen in Preferences cannot be used: ...` (in the console log) | The log file under **Edit > Preferences > Logging** cannot be written (its folder may be gone, or read-only). Speak5220 started, logging to the console only. Choose another file |
| `Cannot speak the frames: ...` (on the Encode tab's waveform) | The emulator could not speak the phrase's frames, for the reason given; the frames may have been edited outside Speak5220. Encoding the phrase again replaces them |
