# Reference data

What the retired reference emulator (`investigation/emulator/`) and encoder
(`investigation/encoder/tms5220enc.py`) produced, for the application's
functional tests to compare against (application plan §1.4). The build and
tests read only these files.

Written by `app/scripts/golden/generate_golden.py`, run by hand in the
default Nix shell. `REFERENCE.tsv` in each directory records the last commit
that changed the references and the SHA-256 of each reference source.

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

The emulator's data never changes unless the reference datasheet does
(AGENTS.md §3.1). Ask before regenerating any of it.
