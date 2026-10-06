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

## Phase 3 captures (2026-10-06)

Board: as Phase 2, except that **the C1 link was connected**, so the audio stage also loaded TP2. `capture.json` originally recorded the default `measure` configuration; Phase 4's layer 3 check found the audio stage's history in TP2 (see "Phase 4 captures"), and the three scope captures' `board` entries were corrected to `audio` on 2026-10-06, with the original value kept as `recorded_as`. The DAC levels of these captures are therefore not reference data; their traces and `I/O` samples are unaffected. Firmware: the command firmware of Phase 3. It was built from the uncommitted working tree on top of `9fcb313`, so `capture.json` records it as `9fcb313-dirty`.

`tools/capture.py` drove each capture, with no manual steps:

1. Reset the chip with `--reset`: nine `0xFF`, then Reset.
2. Arm the scope.
3. Speak through the Pico.
4. Save the VSM trace (`raw/vsm_trace.bin`) beside the scope data.

All three captures used 496 ns sampling over 1.0 s, with probe D on `SCOPE_TRIG`.

| ID | Command | What it shows | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p3-01_speak_acorn` | `SPEAK 0x3CBB6` (ACORN, VSM) | The trace: 1197 events. The chip read 1152 bits, exactly the word up to the end of its stop code. One `INT`, five status reads. | `78d9f4c6e01ebb8f069105432b72697207cb2f3a9f96eb61cf3429d21a1fc217` | `cbb7053d64c9f21e343c4e9a5279e250ba2edea0684ed7ef2aedf6182ff4380b` |
| `p3-02_speakext_acorn` | `SPEAKEXT` of the same 144 bytes | The FIFO was topped up on `INT`: 19 interrupts and 145 writes. The word ends with BE set, because its stop code ends on a byte boundary. | `2aede503ad71024d5f929d776250aadb63f652ac135c3ef250a28a633099c661` | `fcfbdc14e83cd9e60d3da222ae05a4ba40f7f1ade1e2c110aa549e215044b441` |
| `p3-03_speak_acorn_repeat` | Repeat of p3-01 | A second VSM run, to compare with p3-01 and p3-02 | `2768489bf51e34cbec383c1dc6ae1fd2b52de5472e3501b61fc1bf9cb56071fc` | `590034554e7d36df419217688bc94dac3350f1d14afc40053b04d1959d26c2ab` |
| `p3-04_read_byte` | `CMD` Load Address ×5 (`0x3CBB6`), `0x10`, then `READ` | **A trace only, with no scope.** The chip read the byte 0x23 bit by bit, LSB first, then returned 0xC4 on the data bus: the byte bit-reversed. | — | `953787ff24f4d163fe3fcd9461d2686bfc7ed825e58a15938a4df050036cc183` |

Derived files in each capture directory: `vsm_trace.csv` from `tools/pico.py`, and `word0_samples.wav` and `word0_samples.csv` from `tools/decode.py`.

To regenerate them:

```
python tools/decode.py captures/phase3/p3-01_speak_acorn
python tools/pico.py trace-csv captures/phase3/p3-01_speak_acorn
```

**What the three captures show:** the word from the first sample that is not idle (−1) to the last.

- **Length.** The word is 5209, 5209 and 5207 samples long.
- **Codes.** p3-01 and p3-03 (VSM, VSM) differ in 1832 samples, by 3.0 codes rms. p3-03 and p3-02 (VSM, Speak External) differ in 2029 samples, by 3.1 codes rms.
- **Where they differ.** The two VSM runs agree for their first two frames. Speak External differs from sample 11 onwards.

So a repeat of one input path differs about as much as a change of path. Whether the paths are equivalent therefore has to wait until the start state is understood (Phase 5).

## Phase 4 captures (2026-10-06)

Board: as Phase 2, with the C1 link removed, so TP2 is loaded by R2 alone (996 Ω). The chip is a TMS5220NL marked `DEP 8423`; the supplies measured +4.995 V and −5.000 V (`tools/bench.json`). Firmware: the Phase 3 command firmware. It reports `9fcb313-dirty`, because it was built before the Phase 3 commit; its source differs from `9114160` only in a comment. Host tools: `9114160` plus the uncommitted Phase 4 tools.

One runner command made the whole set, with no manual steps:

```
python tools/run.py acorn_phrom_a ACORN --takes 2 --path both --out captures/phase4 --zip
```

It wrote `captures/phase4/acorn_phrom_a/`: `prom.json`, `index.md`, and `160_acorn/` with `compare.json` and four take directories in the plan §6 layout. Each take was captured at 496 ns for 0.88 s, with probe D on `SCOPE_TRIG`, and with no reset before it. Every check in `tools/process.py` passed on every take (`capture.json`, `processed.checks`).

| ID | Path | What it shows | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p4-01` = `acorn_phrom_a/160_acorn/vsm01` | VSM Speak | 27 frames, 1152 bits read, one frame read every 25.000 ms (±0.12 µs). The first frame read 19.69 ms after `SCOPE_TRIG` rose | `c8f9ec9e36b53b229d6737e4a3b23d8bbff63b0d6567bf846057d252337497db` | `6df198ec7ad1b4e3a67ebf4829ccfdf5dd3a409f1cbb2b7e09e6f149e21b2417` |
| `p4-02` = `…/vsm02` | VSM Speak | As p4-01, with the first frame read at 12.49 ms | `e96b29ee3e5251c181a0dea88e140039f0acd13d377d5675f6982544c1874637` | `365f9bdb30bf490740eccff89c9a48d2909380e79d5d284adfc6047fc4e912a5` |
| `p4-03` = `…/ext01` | Speak External | The same 144 bytes; the bytes on the bus match the stream | `25584991e89da9db0658eb43e0f44f52b527c6030e121008dedfb6c4acf42d8c` | `d4226f543a104160ef64626e04d910709162da05f9b8de68132807a6575511fd` |
| `p4-04` = `…/ext02` | Speak External | Repeat of p4-03 | `1e3b0e349bd4da595161ce3a8f1e18a5495f089163888e6166940476c09e4765` | `06a20f74a25e97ac2cb915330e4db18610d7afdce8e2834986ef65fe02e3ba2d` |

Derived files in each take directory, all from `tools/process.py`: `vsm_trace.csv`, `frames.csv`, `samples.wav`, `samples.csv`, `dac.wav` and `listen.wav`. To regenerate them, and the comparison and index:

```
python tools/process.py captures/phase4/acorn_phrom_a/160_acorn/*/
python tools/compare.py captures/phase4/acorn_phrom_a/160_acorn
python tools/promindex.py captures/phase4/acorn_phrom_a
```

**What the set shows:**

- **Layer 2 against layer 3.** The DAC voltage follows the Phase 2 table of the `I/O` codes to 2.2–2.3 mV rms, and 99.9 % of the codes recovered from it are within ±2 of `I/O` (62–71 % exactly). The fitted gain is 1.070–1.071: the DAC's scale is 7 % above the Phase 2 table (the idle level, code −1, is 0.635 V against 0.592 V in p2-01). The supplies were within 6 mV of nominal, so the cause is not known yet.
- **The audio stage leaves a signature.** With the C1 link connected (a scratch run just before this set, and the Phase 3 captures), the same fit leaves 6.5–6.7 mV rms and a gain of 0.92–0.93. A first-order term for the audio stage's coupling network brings it down to 1.8–1.9 mV. This is how the Phase 3 board entries were found to be wrong.
- **The start varies by most of a frame.** Speech began 219–398 samples after `SCOPE_TRIG` rose. In the VSM takes the first frame was read 19.69 ms and 12.49 ms after it (p3-01: 25.51 ms), while the time from the first frame read to Talk Status clearing was the same, 674.87 ms, in all three.
- **Repeats differ as in Phase 3.** After alignment, each take differs from p4-01 by 3.1–3.5 codes rms. p4-02 first differs 946 samples after speech starts; both Speak External takes differ from sample 204. The frames parsed are identical in all four.
