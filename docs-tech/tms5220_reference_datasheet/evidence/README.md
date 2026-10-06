# Evidence index

Every measured statement in the reference datasheet cites a capture by its ID. Each capture is in the repository under `captures/phase<N>/<ID>/`. The raw scope data is in `raw/scope_raw.npz.zip`, which holds the original `scope_raw.npz` unchanged. That file is never modified, and the SHA-256 hashes below are of the `.npz` inside the zip. `capture.json` records the settings and the host commit, and the derived files sit next to it.

To regenerate the analysis from a raw capture:

```
python tools/decode.py captures/phase1/<ID>
python tools/edge_timing.py captures/phase1/<ID> --plot
```

The Phase 2 DAC analysis pools several captures (see "Phase 2 captures" below).

## Phase 1 captures (2026-10-06)

Board: `kicad/tms5220_usb` with the Pico-driven 640 kHz `OSC` (plan §5). C1 is still fitted, so TP2 is loaded by the audio stage. Firmware: the demo loop at `6ffd2bc` (ACORN, B, B, C, COMPUTER from the Acorn PHROM, repeated). Probes: A ×1 on TP2, B ×10 on TP3 (`T11`), C ×10 on TP4 (`I/O`), D ×10 as listed.

| ID | Probe D | Trigger | Interval | Length | What it shows | SHA-256 of `scope_raw.npz` |
|---|---|---|---|---|---|---|
| `p1-01_speech_8ns` | TP5 `SCOPE_TRIG` | `SCOPE_TRIG` rising | 8 ns | 122 ms | `I/O` and DAC edge timing relative to `T11`; codes up to +134 | `08cc906a6ab03cb639d2f3efec08c30390916c9e32f901d0911e040418a5828a` |
| `p1-02_phrase_496ns` | TP5 `SCOPE_TRIG` | `SCOPE_TRIG` rising | 496 ns | 5.2 s | Eight whole words and a 1 s idle gap (8016 samples of −1); no `I/O` activity outside slots 2–11 in 41,599 sample periods | `0ccbbc6ac06b6a9b625e00843de8ba6e0b3528bcedf2011b9fe5589c06514573` |
| `p1-03_romclk_8ns` | TP1 `ROMCLK` | `T11` rising | 8 ns | 100 ms | ROMCLK frequency and duty; every `T11` and `I/O` edge relative to ROMCLK | `bd57607ab93b45a17d8ffcb20e59099dba83a04f15c4d5b43eb7cc419db7cf5d` |

Derived files in each capture directory:

- `edge_timing.json`, `edge_timing.png`: the output of `tools/edge_timing.py` (p1-01, p1-03).
- `word<N>_samples.wav`, `word<N>_samples.csv`: the output of `tools/decode.py` (p1-01, p1-02). p1-03 has no word files, because its probe D is on ROMCLK rather than `SCOPE_TRIG`.

## Phase 2 captures (2026-10-06)

Board: as Phase 1, with the measurement changes of plan §5. The C1 link is removed, so the audio stage no longer loads TP2, and R27 (10 kΩ, GPIO22 to GND) is fitted. TP2 is loaded by R2 alone (996 Ω, measured with a meter), or by R2 with a second 1 kΩ in parallel (482 Ω combined, measured). `capture.json` records this under `board`. Firmware: the same demo loop. Host tools: `d78e6ca` plus the uncommitted Phase 2 tools. Probes: as Phase 1, with D on TP5 `SCOPE_TRIG`, apart from p2-06.

| ID | TP2 load | Interval | Length | What it shows | SHA-256 of `scope_raw.npz` |
|---|---|---|---|---|---|
| `p2-01_phrase_496ns` | 996 Ω | 496 ns | 5.2 s | DAC transfer, with p2-02: 41,599 sample periods | `91f0d89239b393cb9e3193f31691ffa00b671035bc8a36827b11775c8ae3afca` |
| `p2-02_phrase_496ns` | 996 Ω | 496 ns | 5.2 s | Repeat of p2-01 | `f83ad011076baed4d4b2e405d9756f0ac9925fe785e11a22005dc9e89a32b19c` |
| `p2-03_speech_8ns` | 996 Ω | 8 ns | 122 ms | DAC step timing with the audio stage disconnected (180 steps) | `1c94cfff9a841e6ed89fe6ae4e3392bb6e33f185988dabb3277a6aa6fc739912` |
| `p2-04_phrase_half_load` | 482 Ω | 496 ns | 5.2 s | DAC transfer at the lower load, with p2-05 | `3cfc267df7d479c88dbf1d6c9fbcb967b4e1a08b453ecb02e48547fc42ff7584` |
| `p2-05_phrase_half_load` | 482 Ω | 496 ns | 5.2 s | Repeat of p2-04 | `81bdc1166451f4e0e80502b5100e48cbfe574e7e1356348272f3e345d53d8b4f` |
| `p2-06_probe_a_grounded` | 482 Ω | 496 ns | 2.2 s | Probe A's tip on a GND test point: the scope's zero offset on Ch A (−8.69 mV) | `a8b7773e390696a978f9825e72b8bc83e7b00d5e7c56003960b47b0fd68a2e47` |

Derived files:

- `p2-03_speech_8ns/edge_timing.json`, `edge_timing.png`: `tools/edge_timing.py`.
- `captures/phase2/dac_transfer/` (p2-01 + p2-02) and `dac_transfer_half_load/` (p2-04 + p2-05): `dac_transfer.json`, `dac_table.csv` and `dac_transfer.png` from `tools/dac_transfer.py`.
- `captures/phase2/dac_compare/`: `dac_compare.json` and `dac_compare.png` from `tools/dac_compare.py`.

To regenerate them:

```
python tools/edge_timing.py captures/phase2/p2-03_speech_8ns --plot
python tools/dac_transfer.py captures/phase2/p2-01_phrase_496ns captures/phase2/p2-02_phrase_496ns \
    --zero captures/phase2/p2-06_probe_a_grounded --out captures/phase2/dac_transfer --plot
python tools/dac_transfer.py captures/phase2/p2-04_phrase_half_load captures/phase2/p2-05_phrase_half_load \
    --zero captures/phase2/p2-06_probe_a_grounded --out captures/phase2/dac_transfer_half_load --plot
python tools/dac_compare.py captures/phase2/dac_transfer captures/phase2/dac_transfer_half_load \
    --out captures/phase2/dac_compare --plot
```

The phrase captures have no `word<N>` WAV and CSV files. `tools/decode.py` makes them if they are wanted.
