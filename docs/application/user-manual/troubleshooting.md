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
| `export format "c-header" is not one of "acorn-phrom", "c-header-phrom", "c-header-speak-external", "wav"` | The preset or `--format` names a format Speak5220 does not have; see [Export formats](../reference/export-formats.md) |
| `the layout "concatenated" does not apply to the format "wav"` | A preset holds an option of another format (`layout` belongs to `acorn-phrom`, `licence` to the C headers, `audio` to `wav`). Remove it |
| `export scope "selected" needs the phrases it exports` | A preset of scope `selected` must list its `phrases` |
| `the export names a phrase the project does not have: HELLO` | A preset's `phrases`, or `--phrase`, names a phrase that was renamed or removed. Edit the preset |
| `exported files cannot be named from "../out": use a name without / \ : * ? " < > \|` | The exported files are named after the preset (or the project file), so the name must be a plain file name |
| `the licence "MIT */" is not an SPDX identifier (letters, digits, spaces and ". + - ( )")` | A C header's licence is written in its comment, so only an SPDX identifier or expression is accepted |
| `HELLO needs 70000 bytes, more than a C header's stream holds (65535)` | A Speak External header gives each phrase's length as a 16-bit number. Shorten the phrase |
| `the project has no export presets: give --format` | `speak5220-cli export` without `--preset` or `--format` runs the project's presets, and there are none |

## The encoder and Optimise

| Message | Meaning |
|---|---|
| `cannot re-encode HELLO: the project would need 65 PHROMs, and its budget is 64` (Change Encoder Settings, Re-encode) | The new encoding is bigger and does not fit. The phrase keeps its settings and frames, and the form shows them again; with several phrases, none is changed. Choose settings that give fewer bytes, or make room |
| `none of the phrases can be optimised: they are locked or have no audio` (Optimise) | Optimise needs a phrase's audio to try settings on, and locked phrases are never re-encoded |
| `No better settings were found within the limits.` (Optimise) | No settings tried scored higher than the current ones while fitting the byte limit and the budget; or none fitted at all. Nothing changes. Raise the byte limit, or leave it off |
| `Quality not measured` (encoder panel) | The quality could not be measured: the encoding holds no sound, or the recording holds no speech the voice activity detector finds (with a squelch, check it does not mute everything) |
| `ESTOI too short` (encoder panel figures) | The phrase has under about 0.4 s of speech, too little for the intelligibility measure; the rating comes from the envelope and pitch alone |
| A message titled **Encode HELLO** | The encoder refused the phrase's audio or settings, for the reason given; the phrase keeps its frames |
| *Stale: made with other settings* on phrases whose settings did not change | They were encoded by an earlier version of the encoder, whose output differs: for example, before [quiet pauses](encoder.md#quiet-pauses). Their frames still work; re-encode them (**Phrase > Re-encode**) to get the current encoder's |
| A faint buzz or whine in the pauses of the encoded audio | The window plays the emulator, which is not exact at this level: a pause it plays with a whine can be silent on a real chip, and the other way round (see [quiet pauses](encoder.md#quiet-pauses)). On the chip, a pause buzzes only when it is short: two silence frames or fewer, which the encoder leaves alone |

## Importing audio

| Message | Meaning |
|---|---|
| `warning: hello.wav clips; --normalise would bring its peak to -1 dBFS` | The file reaches full scale. It was imported unchanged; add it again with `--normalise` to scale it down |
| `warning: hello.wav is very quiet; ...` | The file's peak is below −30 dBFS. It was imported unchanged; the encoder sets its own level, but `--normalise` brings it up first |
| `cannot open hello.wav: ...` / `hello.wav is not a WAV file` | The file is missing, or not a WAV file. Only WAV is read |

## Opening and saving projects

| Message | Meaning |
|---|---|
| `not a Speak5220 project: there is no project.yaml` | The file or directory is not a project |
| `project.yaml: phrases[2].play_start must be a whole number` | A value in the project file has the wrong type; the message names the file and the key. See the [project file](../reference/project-file.md) reference |
| `the project was saved by a newer version of Speak5220 (format version 2; this version reads up to 1)` | Open it with the newer version |
| `project.yaml: project.default_encoder does not list ...: ... cannot be opened` | The project was saved by a development version of Speak5220 that did not list every encoder option, or before an option existed. A project lists every option, so it is not opened. Adding the missing options to `default_encoder` in `project.yaml` (in an unpacked project) with the values it was made with makes it open: `squelch_db: null` and `resonance_hold_db: null` (no hold, as before it existed). See the [project file](../reference/project-file.md) and [encoder options](../reference/encoder-options.md) references |
| `... holds an unsafe path, "../evil.txt": refusing to open it` | The archive has a path that could write outside the project. It may have been made by another tool, or tampered with; it is not opened |
| `... is not a Speak5220 project (it has no project.yaml): not replacing it` | Saving the unpacked form never replaces a directory that holds other files |
| `phrases/<id>/source.wav is missing: phrase HELLO has no audio` | A file of the project has been deleted. The previous version, `NAME.s5proj~`, may still have it |
| `cannot open PATH: ...` / `cannot save PATH: ...` | The main window's form of the messages above, naming the project. A failed save leaves the project open with its changes, and the file as it was |

## The main window

| Message | Meaning |
|---|---|
| `PATH: ...` (Add Phrases) | A file could not be imported; the message after the path says why (as under [importing audio](#importing-audio)). The other files were added |
| `cannot sort the phrases: the project would need 65 PHROMs, ...` (Edit Phrases) | A sort, or a move (`cannot move the phrases: ...`), would take the project past its budget, so it was not made |
| `there is no audio output device` (Play) | The system has no output device to play on. Connect or enable one |
| `cannot play on DEVICE` (Play) | The output device would not start. Another program may hold it, or it may not take 16-bit audio |
| `Cannot speak the frames: ...` (in the encoded pane) | The emulator could not speak the phrase's frames, for the reason given; the frames may have been edited outside Speak5220. Encoding the phrase again replaces them |
