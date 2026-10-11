# Reference data

What the retired reference emulator (`investigation/emulator/`), encoder
(`investigation/encoder/tms5220enc.py`) and quality tools
(`investigation/tools/speechqual.py`, `investigation/encoder/evaluate.py`)
produced, for the application's functional tests to compare against
(application plan §1.4). The build and tests read only these files.

Written by `app/scripts/golden/generate_golden.py` (`emulator/`,
`packaging/`), `app/scripts/golden/encoder.py` (`encoder/`) and
`app/scripts/golden/quality.py` (`quality/`), run by hand in the default Nix
shell. `REFERENCE.tsv` in each directory records the last
commit that changed the references and the SHA-256 of each reference
source.

```bash
nix develop --command python3 app/scripts/golden/generate_golden.py --check
```

| Path | Contents |
|---|---|
| `emulator/runs.tsv` | One row per run: name, mode (`script`, `vsm` or `external`), input, power-up phase, and whether the encoder's synthesiser matched the emulator |
| `emulator/runs/NAME.codes` | The run's output code for every sample period from power-up, one per line |
| `emulator/runs/NAME.events` | The run's event log, in the format of `tms5220emu -e` |
| `emulator/scenarios/` | Scenario scripts (mirroring the emulator's self-tests) and the images they load |
| `emulator/phrases/` | Each phrase's packed bytes (`.bin`) and frame list (`.csv`): synthetic vectors and corpus phrases from public-domain and CC0 voices |
| `packaging/acorn.bin`, `ti_indexed.bin` | Acorn PHROM and TI indexed images of the words in `words.tsv`, with their word tables (`*_table.tsv`) and the Acorn ASCII map (`acorn_ascii.tsv`) |
| `packaging/wordNN.csv`, `wordNN_external.bin` | Each word's frames and its Speak External stream |
| `encoder/option_sets.tsv` | The encoder option sets: the defaults and three fixed combinations that between them change every option, as JSON |
| `encoder/frames/SET.tsv` | For each option set, every corpus file's frame count, near-tie frames (`-` for none) and packed frames (hex) |
| `encoder/inputs/NAME.wav` | WAV copies of the corpus subset (public-domain and CC0 voices only) |
| `encoder/intermediates/NAME.json` | `encodeDetailed()`'s intermediate values for each subset file, default options |
| `quality/scores/SET.tsv` | For each option set, every corpus file's reference encoding measured: the lag and length of the emulator's output, PESQ as `speechqual.py` measures it (P.862 narrowband as P.862.1 MOS-LQO), and `evaluate.py`'s level error and spectral distance (dB) |
| `quality/anchor.tsv` | Every corpus file's anchor score: the PESQ of `speechqual.py`'s floating-point LPC-10 vocoder (`--anchor`, noise seed 1) |

The encoder's data is written by `app/scripts/golden/encoder.py`:

```bash
nix develop --command python3 app/scripts/golden/encoder.py --check
```

A near-tie frame is one whose coding the Python encoder changes when the
input is disturbed at the level of floating-point noise (a relative 1e-9);
the C++ encoder may differ from the reference there, and only there (plan
§3.2). The script's docstring gives the disturbances.

The quality data is written by `app/scripts/golden/quality.py`, which runs
the built reference emulator (`make -C investigation/emulator` first):

```bash
nix develop --command python3 app/scripts/golden/quality.py --check
```

The emulator's data never changes unless the reference datasheet does, and
the encoder's only for an intended improvement, with the quality data that
measures it (AGENTS.md §3.1). Ask before
regenerating any of it.
