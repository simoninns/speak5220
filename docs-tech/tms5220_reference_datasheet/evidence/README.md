# Evidence index

Every measured statement in the reference datasheet cites a capture by its ID. Each capture is in the repository under `captures/phase<N>/<ID>/`. The raw scope data is in `raw/scope_raw.npz.zip`, which holds the original `scope_raw.npz` unchanged. That file is never modified, and the SHA-256 hashes below are of the `.npz` inside the zip. `capture.json` records the settings and the host commit, and the derived files sit next to it.

To regenerate the analysis from a raw capture:

```
python tools/decode.py captures/phase1/<ID>
python tools/edge_timing.py captures/phase1/<ID> --plot
```

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
