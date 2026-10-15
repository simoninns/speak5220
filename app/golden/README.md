# Reference data

What the retired C emulator and Python encoder (`tms5220.c` and
`tms5220enc.py`, in the repository's history at commit `6abe9ed`)
produced, the C++ encoder's own level setting and frames where
it departs from the Python encoder, and what independent implementations
of the quality measures give, for the application's functional tests to
compare against. The build and tests read only
these files.

`app/scripts/golden/generate_golden.py` (`emulator/runs/`, which the
application's emulator reproduces exactly, run as `tms5220emu`),
`app/scripts/golden/encoder.py` (`encoder/`) and
`app/scripts/golden/measures.py` (`quality/measures/`) write them, run by
hand in the default Nix shell. `REFERENCE.tsv` in each directory records
the last commit that changed the references and the SHA-256 of each
reference source; `emulator/REFERENCE.tsv` records the C emulator's, as
committed. The Python encoder's own output (`packaging/`, the
emulator's corpus phrases, and the stages before level setting in the
encoder's intermediates) is fixed: the scripts keep it as committed.

```bash
nix develop --command python3 app/scripts/golden/generate_golden.py --check
```

| Path | Contents |
|---|---|
| `emulator/runs.tsv` | One row per run: name, mode (`script`, `vsm` or `external`), input, power-up phase, and whether the Python encoder's synthesiser matched the emulator. `generate_golden.py` runs the emulator for each row |
| `emulator/runs/NAME.codes` | The run's output code for every sample period from power-up, one per line |
| `emulator/runs/NAME.events` | The run's event log, in the format of `tms5220emu -e` |
| `emulator/scenarios/` | Scenario scripts (mirroring the emulator's self-tests) and the images they load |
| `emulator/phrases/` | Each phrase's packed bytes (`.bin`) and frame list (`.csv`): synthetic vectors, and the Python encoder's encodings of corpus phrases from public-domain and CC0 voices |
| `packaging/acorn.bin`, `ti_indexed.bin` | The Python encoder's Acorn PHROM and TI indexed images of the words in `words.tsv`, with their word tables (`*_table.tsv`) and the Acorn ASCII map (`acorn_ascii.tsv`) |
| `packaging/wordNN.csv`, `wordNN_external.bin` | Each word's frames and its Speak External stream |
| `encoder/option_sets.tsv` | The encoder option sets: the defaults and three fixed combinations that between them change every option the application has, as JSON, under the Python encoder's names (its other options stay at their defaults, where the application fixes them) |
| `encoder/frames/SET.tsv` | For each option set, every corpus file's frame count, near-tie frames (`-` for none) and packed frames (hex), from the C++ encoder |
| `encoder/inputs/NAME.wav` | WAV copies of the corpus subset (public-domain and CC0 voices only) |
| `encoder/intermediates/NAME.json` | For each subset file with the default options, the Python encoder's intermediate values up to the plan (from its `encodeDetailed()`), and in `encode` the C++ encoder's level setting and frames |
| `quality/measures/scores/SET.tsv` | For each option set, each subset file's reference encoding measured by independent implementations of the quality measures: the alignment lag, the frames the voice activity mask keeps, the mean log spectral distortion and its percentages of frames over 2 and 4 dB, REAPER's and Harvest's gross pitch error, F0 RMSE (cents) and voicing decision error, and ESTOI |
| `quality/measures/masks/SET.tsv` | Each subset file's voice activity mask, from py-webrtcvad, one character per 10 ms frame (`1` for speech) |
| `quality/measures/tracks/SET/NAME.tsv` | Each subset file's F0 tracks every 10 ms (0 where unvoiced), of the prepared original and output, from REAPER's command-line tool and from Harvest through pyworld |

The encoder's data is written by `app/scripts/golden/encoder.py`, with the
functional tests built (it runs the C++ encoder's by-hand writer,
`EncoderReferenceWriter`, on WAV copies of the corpus):

```bash
nix develop --command python3 app/scripts/golden/encoder.py --check
```

The frames and the level-setting intermediates are the C++ encoder's own,
regenerated with each intended change to it; the stages before level
setting are the Python encoder's, which `encoder.py` keeps as committed
(the Python encoder is retired). The C++ encoder's level setting, onsets
and fades and pitch anchor depart from the Python's: the pitch
track in the intermediates is the Python's, which the C++ tracker gives
without its anchor, and each frame's voicing is the Python's, without the
C++ encoder's onset rule, which only the unit tests check. A near-tie frame
is one whose coding changes when the input is disturbed at the level of
floating-point noise. The C++ encoder, which gives the same
frames on every platform, marks none.

The quality measures' data is written by `app/scripts/golden/measures.py`,
which speaks the frames with `tms5220emu` (the application's emulator,
built with the tests into `build/app/tests/support/`), REAPER's command-line tool (built, like the SPTK the application
uses, without fused multiply-add contraction, so its tracks are the same
on every platform) and the Python packages of the default Nix shell
(py-webrtcvad, pystoi, pyworld):

```bash
nix develop --command python3 app/scripts/golden/measures.py --check
```

`bbc/programs/NAME.bas` are BBC BASIC programs that use every keyword and the tokeniser's special cases; `NAME.bin` beside each is the program BBC BASIC II stored when it was typed in (the bytes SAVE writes), on MAME's emulated BBC Micro, by `app/scripts/golden/basic_programs.py`; `bbc/REFERENCE.tsv` records the SHA-256 of the BASIC and MOS ROMs. The tokeniser's functional test compares with them. They change only if the programs do:

```bash
nix develop --command python3 app/scripts/golden/basic_programs.py --check
```

`output-hashes/SUITE.sha256` is different: not reference data from the
retired implementations, but the application's own exports, recorded so a
change can be shown to leave them byte-identical. Each line is the SHA-256
and path (`FIXTURE/FILE`) of a file that the export presets of the
`general`, `phrom` or `bbc` fixture suite (`app/projects/`) write: PHROM
images, C headers, WAV files and BBC Micro discs. `app/scripts/output-hashes.sh` compares the
exports with it, and `--record` records it again:

```bash
nix develop .#app --command app/scripts/output-hashes.sh
```

The emulator's data never changes unless the reference datasheet does, and
the encoder's only for an intended improvement, with the quality data that
measures it (AGENTS.md §3.1). The output hashes are recorded again only for
an intended change to an exporter or to the fixtures' presets. Ask before
regenerating any of it.
