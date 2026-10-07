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

## Phase 5 captures (2026-10-06)

Board, chip and supplies: as Phase 4 (C1 link removed, R2 996 Ω, TMS5220NL `DEP 8423`). Firmware: the Phase 5 command firmware, which adds timed starts (`SPEAK … at_us`, `TIME`), `POLL` (status reads during speech; off by default) and `VSMDELAY` (`firmware/PROTOCOL.md`). It reports `d66eeef-dirty`, as do the host tools: both are `d66eeef` plus the uncommitted Phase 5 changes. The TMS5220 was power-cycled once, between p5-15 and p5-16, and the Pico was reflashed three times, always with the chip idle.

### The standard procedure: p5-01 to p5-08

One runner command, using the standard procedure (plan §12, Phase 5):

```
python tools/run.py acorn_phrom_a ACORN --takes 4 --path both --out captures/phase5 --zip
```

The runner located the frame grid, calibrated the start offset of each path (VSM −3.63 µs, Speak External +213.30 µs), then for each take sent the reset sequence and a Speak timed to the frame clock. Every check passed on every take, including `start_frame` and `start_slot`: each Speak was accepted 12255.5–12256.4 µs before its frame boundary, the target being 12256.1 µs.

| ID | Path | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|
| `p5-01` = `acorn_phrom_a/160_acorn/vsm01` | VSM Speak | `8dca11f595427286df40728aeb00c1a2ba978758218bfe4e4db55a7a470dab7d` | `df47b163f62eb6cdf107cde6a4c1e5f2f4b70da42635c9b75241785c6724693a` |
| `p5-02` = `…/vsm02` | VSM Speak | `7c1c7b218fd123bfbf97a6ec09fe0ea668e074fa7cd2c201a1fd9c9d3872de87` | `2df796bdeb244ecbf79202aa94a652765456e89519acbbf3f14ca0e73ed332e7` |
| `p5-03` = `…/vsm03` | VSM Speak | `31190d757065784b459ffbea421d2bea0ed142e101988d9235eca73f9bb596a5` | `f17e76d059ecbf0640ae225a9efe3818a232d66293b133ad5867faaa39d04f38` |
| `p5-04` = `…/vsm04` | VSM Speak | `4ed4583ad9758e4030aefd436576b9551216c8c0a1a12ca359fa8af6f43ea815` | `26b3df8a2dd4ff18e9f4c49430f8d14b7bd81b10b9c488af2a488d97c6f871b0` |
| `p5-05` = `…/ext01` | Speak External | `d7f0ccd3b4d0e0bc0d303580adc0fc3e01e82e7763f71261924b394fbd2e789a` | `d1bb4100d5b845f1c55b95cb9dc7bcd4391fad039897fbb4089c4442eb3eec7e` |
| `p5-06` = `…/ext02` | Speak External | `db4bb1027e04978a8e9a0fbe406e26bba711702845f647b919057a9975cbdde9` | `d004fb103337c1dbda64e24f8d582ce09656e79d484168193acd4e6af71664ae` |
| `p5-07` = `…/ext03` | Speak External | `4160d48f447a9b42fc365655b69bbafc2a3f70461d4bc1380e1b7cd9f4f17199` | `c140c9078a7b536e4ff9b9373d3440c7a5a61deee3ae97c33dcad73ed010dbb5` |
| `p5-08` = `…/ext04` | Speak External | `63bd4ac8686c37d6c4d2861434e02c1a50e6dff9118fdbc888d1f2fbc4ef932a` | `3d6ca5e5d10347680edd0868adb482b3f3d8fcf81fe2782f8478fab0328b89b6` |

To regenerate the derived files, the comparison and the index, run `process.py`, `compare.py` and `promindex.py` as for Phase 4, on `captures/phase5/…`.

**What the set shows:**

- **The same start in every take.** Speech starts at sample 306 (VSM) and 304 (Speak External) in every take, and Talk Status clears after 5499 and 5497 samples.
- **The same noise in every take of a path.** No two VSM takes differ by more than 3 codes, and no two Speak External takes by more than 2. Without the procedure, repeats differ by up to 35–40 codes (p4-01 to p4-04).
- **Upsets remain.** All four VSM takes differ from one another, each by a few hundred samples of ±1–3 codes. The departures start 2489–2881 samples after speech starts. The Speak External takes are the same, with departures from 22–752 and 4287–4705 samples.
- **VSM and Speak External differ.** Accepted at the same time before the frame, Speak External starts its output 2 samples earlier, and its noise differs from the VSM takes' (up to 35 codes).
- **DAC scale.** The fitted gain is 1.068–1.071, as in Phase 4. With the chip idle, a meter on TP2 read 0.642 V; the scope's idle level was 0.631 V.

### Experiments: p5-09 to p5-18

These are in `captures/phase5/experiments/`. The scope experiments keep, for each take, `capture.json`, `frames.csv`, `samples.wav` (the exact `I/O` codes) and `raw/vsm_trace.bin`; their raw scope data is kept locally, and `scope_raw_sha256.txt` lists its hashes. `scripts/` holds the experiment scripts, in their final form. The trace-only experiments need no scope.

| ID | Directory | What it does | What it shows |
|---|---|---|---|
| `p5-09` | `frame_grid/vsm_trace.bin` (SHA-256 `b34857cdfc20493c8598f1e18cfb9fb4d518324d5fb5ea097882d32ef38f64a3`) | 34 Speaks of B in one trace, 0.02–0.6 s apart, every other one after the reset sequence (`scripts/grid2.py`) | Every first frame read falls on one 25 ms grid, within ±0.08 µs, over 23 s. Speak to first read: 2.1–24.7 ms. Reset does not move the grid |
| `p5-10` | `start_threshold/threshold.json` | Timed Speaks with the Speak written 285–366 µs before a frame boundary (`scripts/edge.py`) | Written 323 µs or more before a boundary, the Speak starts on it; 317 µs or less, on the next |
| `p5-11` | `noise_period/` | Six VSM takes of ACORN, on frames n, n+600, n+1200, n+8191, n+8791 and n+9391, no reset (`scripts/e3.py`) | Takes 8191 frames apart differ by at most 3 codes; all other pairs by 33–40, starting in the voiced frames, 946 samples after speech starts |
| `p5-12` | `reset_timed/` | Reset, then a timed Speak, on frames n, n+800, n+1600, n+2400 and n+3200; before the fourth take only, an extra Speak of B after the reset (`scripts/e10.py`) | The four plain takes differ by at most 3 codes, although their frames differ; the take with the extra Speak differs from them by 32 |
| `p5-13` | `reset_free_start/` | Reset, then an untimed Speak, four times (`run.py --start free --reset`) | After alignment, the takes differ by 2.9–3.9 codes rms: the noise restarts with the Speak, not at the frame boundary |
| `p5-14` | `speak_slot/`, and `offset_sweep.json` | Three standard takes before the start was calibrated; then a sweep of the start offset over ±7 µs, six Speaks each (`scripts/slot.py`) | vsm01 and vsm02 are bit-identical, and bit-identical to p5-15's vsm01, from another clock session. vsm03's Speak was accepted one slot (6.35 µs) later, and its noise differs (up to 36 codes). The sweep shows the accepted time stepping by one slot, with a 1.5–2 µs band where it jitters between two |
| `p5-15` | `no_status_polls/` | Six timed takes as p5-12, with the firmware's status polls during speech turned off | vsm01–vsm03 are bit-identical, and so are vsm05 and vsm06; vsm04 differs from vsm01 by ±2 codes in 184 samples |
| `p5-16` | `power_cycle/` | After a power cycle of the TMS5220 (Pico kept running): a Speak with no reset, then four standard takes | The chip spoke at once, without a reset; its frame grid had moved by 262 µs. The takes differ from p5-15's vsm01 by at most 3 codes |
| `p5-17` | `clock_stops/clock_stops.json` | With the chip idle, `CLOCK 0` for 1 ms, 10 ms, 100 ms, 1 s and 10 s, then a Speak with no reset (`scripts/e6.py`) | Every Speak worked. After stops of up to 1 s the frame grid had moved by the stopped time, to within 6 µs; after 10 s it had moved 4.3 ms further |
| `p5-18` | `vsm_data_margin/vsm_delay.json` | The VSM emulation delays each data bit on `ADD8` by 0–10 µs after it sees `M0` rise (`VSMDELAY`; `scripts/delay.py`) | The chip reads one bit every 12.5 µs. It still read all 1152 bits of ACORN with a 9.5 µs delay; at 10 µs the word never ended |

## Phase 6 captures (2026-10-06)

Board, chip, supplies and firmware: as Phase 5 (firmware `d66eeef-dirty`). The host tools are the Phase 6 ones (`processed` version 3, which judges the DAC check inside codes ±119). The PROM is the TI-99/4A speech VSMs, `ti99_speech` (`proms/README.md`), rebuilt with `tools/promimport.py` and loaded into banks 0 and 1. One runner command, using the standard procedure:

```
python tools/run.py ti99_speech MORE LINE --takes 2 --path both
```

The runner calibrated the start offsets (VSM −2.10 µs, Speak External +216.88 µs). Every check passed on every take; each Speak was accepted 12255.8–12256.2 µs before its frame boundary. The raw data was zipped afterwards, unchanged (`run.zipRaw`), and the set was moved to `captures/phase6/`.

| ID | Take | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|
| `p6-01` = `ti99_speech/0_more/vsm01` | MORE, VSM Speak | `c16d8c95e62ff4644fdd6c4704093d65cd7d23e60d0ee916db1b8f0d8ae9bb91` | `86d8e226ff9cf7fb6007baccb62bb3fe4e00f28d69469e8c5e75aeeeb7cd90a8` |
| `p6-02` = `…/0_more/vsm02` | MORE, VSM Speak | `69ba885f4d6de69f1b3f9d01c1e595cd3a6e88973c9a57772945d16949d01bab` | `883f5c8d6ca74034f9a523ffed5bae4b1851429e3330004919dfd0704c8793b0` |
| `p6-03` = `…/0_more/ext01` | MORE, Speak External | `dc91ce4190c831bd491288b65ba3912e4c59f8d08985628a69de8fd8caa96269` | `bbc5dcd8441f0d9089ac48006ec21fe351f56c03a1ea7ec5b56aa0c3a5dd9a6d` |
| `p6-04` = `…/0_more/ext02` | MORE, Speak External | `d252b6fb6cbe2cd3634e7fb4e1b0f186f33a84a816b6c9c36debf0117d743993` | `cd0f7b0ea2f56863db141a8296af7306213111902210068aae7887c97fbcefab` |
| `p6-05` = `…/167_line/vsm01` | LINE, VSM Speak | `6c1ef4d2324b1a29542b886a9adfa01dfda0d00fc20c1f388f335853a056bfe7` | `4c70df0b20f209281be2efb79d2a25627a096e45840bf892bd3d27a9c1f91086` |
| `p6-06` = `…/167_line/vsm02` | LINE, VSM Speak | `a2add170354da6a95099b8608ebb32c09049adcd6ef67a68807b177bdc3a7439` | `23eae154aad21bb07fbeb4438d8aca962fd7741bb67069e6970aba3c865d8974` |
| `p6-07` = `…/167_line/ext01` | LINE, Speak External | `5465d20e6afdc2947d178ba759cc139a278da8bc40854c8947631f27e6dbbe96` | `708e98a911ad6752a13996c1a70894a4f4f38a53e371679b7938fe5d7a98e519` |
| `p6-08` = `…/167_line/ext02` | LINE, Speak External | `d542637321cd24902a76b9c7029595aee97ecced74c9b9a44560820c015bc97f` | `264e59d179714739860468b1069b3f24b24ae5a87e6443ebb77cfe10336c503b` |

**What the set shows:**

- **The rails.** TI-99 speech, written for the TMS5200, is loud on the TMS5220: MORE's codes span −271 to +412, and LINE's the whole 10-bit range, −512 to +511. 514–723 samples per take lie outside the DAC's range. Codes above +127 give the same output as +127 to within 0.5 mV, and codes below −128 the same as −128 to within 2 mV. Fitted inside ±119 (gain 1.059–1.062, 1.9–2.2 mV rms), the Phase 2 model puts the rail samples 7.1–7.8 mV too high on average: about 6 mV at +127 and 10 mV at −128 (datasheet §8.2).
- **Reading across the VSMs.** LINE starts at 0x3FD5, in CD2325A, and ends at 0x4049, in CD2326A; the bits read match the image across the boundary (`vsm_image`).
- **Repeats.** The two LINE Speak External takes are bit-identical. The others differ within their path by up to 10 codes (MORE) and 15 codes (LINE, VSM), with departures starting as early as sample 3 of speech: larger and earlier than the Acorn word's 3 codes in Phase 5. This is an upset question for Phase 8.

## Phase 7 captures (2026-10-06 to 2026-10-07)

Board, chip, supplies and firmware: as Phase 6 (firmware `d66eeef-dirty`). The host tools are the Phase 7 ones (`processed` version 4). The scope is held open for a whole run, and `hostCycles` sees a READY edge that shares a trace event with the end of a write.

**The reference set, corpus `acorn_phrom_a`.** These are every word of the Acorn PHROM A, five takes or more through each path, and the address sweep: one take from each of 16,144 start addresses, and five from every 64th. All were taken with the standard procedure, by:

```
python tools/run.py acorn_phrom_a all --takes 5 --path both --sweep --resume --zip-only
python tools/run.py acorn_phrom_a --sweep --sweep-step 64 --sweep-takes 5 --resume --zip-only
```

The set is kept outside the repository for now (plan §6), in `captures/corpus/acorn_phrom_a/`. Of it, only `prom.json`, `index.md`, `takes.csv` and `sweep.csv` are committed. `takes.csv` gives, for every take, the SHA-256 of its `scope_raw.npz` and `vsm_trace.bin`, and of its output (`talk_sha256`). `captures/phase7/scripts/corpus_stats.py` recomputes the figures the datasheet cites from the set.

**Takes copied from the set** into `captures/phase7/acorn_phrom_a/`:

| ID | Take | What it shows | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p7-07` = `sweep/addr_3c001/vsm01` | Start 0x3C001: a stop code first | Talk Status clears 0.09 ms after the stop code is read; no output | `c4bc12a56eb1a53083a50214371ad0be3d34194381e89fc3fa2219c06783e595` | `9ba54cf64ad16e4864285584a9d46afc2b2e48276d1659f8bb4b53fa0a4e82b7` |
| `p7-08` = `sweep/addr_3c000/vsm01` | Start 0x3C000: 3 frames | Talk Status clears 74.87 ms (3 × 25 ms − 0.13 ms) after the first read | `a2a603f57dfea28c461fb689cfdd00f54730ec4633cff678bb4758eda6b8e8bc` | `6e342fd3227321fcf3a99c0d9f62950315787a645be224138b810ec097003f47` |
| `p7-09` = `195_f/ext01` | F, Speak External | A take accepted in the target slot (12255.74 µs before the frame) | `3ce208da6cc2bc71682711c0fa051e59f6491e2d04832bd0b62d26c15cdb0055` | `eb23472e12dd2f6672ba06d772a670164780c8fe693081ca3e40525d33ef40d9` |
| `p7-10` = `195_f/ext04` | F, Speak External | Accepted one slot early (12261.95 µs before the frame): different noise, up to 63 codes from the other takes. Fails `start_slot` | `e53c849c22037b446adf0b821fac230aa8aa095d89ea863acbbc632ae8425dd4` | `a21c6953a638bd65e634c363ed2fff5b92d6fb8d2180729f9aa309fc0d9174e1` |
| `p7-11` = `sweep/addr_3f416/vsm01` | Start 0x3F416: loud | The DAC model 4–6 mV high for codes 64–119 in size; fails `layer2_vs_layer3` (3.06 mV rms), as did three retakes | `29ecec87d831ae6564cc8b80dd7b125c76701ed6934a357f5c79893d06e01254` | `2f61e87b0051f29a22877429c22975b496f3b4a972c7572d00b8e001fd82eedb` |

**The previous utterance**, `captures/phase7/previous_utterance/`. Sweep start 0x3F9D4 begins with a repeat frame, then loud voiced frames (the repeat frame's K values come from the previous utterance, Phase 8). It was spoken twelve times with the standard procedure. Before some takes, another utterance was spoken first, untimed:
- **X:** sweep start 0x3FD5E, loud.
- **Y:** the word B, 0x3D065.
- **–:** nothing, so the previous utterance was the take before.

The order was X, –, –, Y, –, –, X, –, Y, –, X, Y (`sequence.json`), by `captures/phase7/scripts/previous_utterance.py`. Six of the takes are kept. The raw data was zipped afterwards, unchanged.

| ID | Take | Spoken just before | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p7-01` = `vsm01` | 0x3F9D4 | X | `b43969ad793049313a45eff1eaba1fd2850d3710c3d6993cac392abf3ab5fe14` | `46eb8c5b676c7ba539ff6110bd09135fa947f9509ad4f170dfe49e75947a6b40` |
| `p7-02` = `vsm07` | 0x3F9D4 | X | `2965943d0540542eca36b6407329855b37bb2461890d4c48b6b76ead6abf1a69` | `ea9905963a2d160e4535500e1873bc3735c88fb9194c8bb790c28af4676e9d63` |
| `p7-03` = `vsm04` | 0x3F9D4 | Y | `399da3b984a15217806baf6d89c5d4090c5429ab058161c6424d9867e1c79635` | `88f392bb8060eeec21a0471652af928782ee9f576147d3c312978af4aae9d484` |
| `p7-04` = `vsm09` | 0x3F9D4 | Y | `e259247a50db68b446abd34edaf852492cd1ef66ac9d91427c0f92135555c042` | `e5c423e186fc725c2046e405e9ef960df6ead1f6f15e1312fc2a73262b142446` |
| `p7-05` = `vsm02` | 0x3F9D4 | – (after `vsm01`) | `ff0fda6306d84fe19828364ed2139bcfb5e50cda0d9f44f05981c304c8d04223` | `575f9fceeb9aad87cd74067d582b45be5997597dc7093e9f4362e93434f4a899` |
| `p7-06` = `vsm03` | 0x3F9D4 | – (after `vsm02`) | `ac2ffd611fda1b9d1d6cb41daab843850d869cb30f309f32c1bf17ee820ab7f4` | `99b59892de15101ced0f9be1a509880c4b6ce286dc8d31e65626a00a52cce256` |

**What it shows.** Every take passed every check, each accepted within 0.15 µs of the others. The first differing sample, counted from the start of speech:
- **Same predecessor:** X and X at sample 258 (p7-01, p7-02); Y and Y at 6802 (p7-03, p7-04); – and – at 4473 (p7-05, p7-06).
- **Different predecessors:** any pair, at sample 0–2.

After their first difference, the takes diverge to the full 10-bit range.

## Phase 8 captures (2026-10-07)

**The vector sets.** `tools/vectors.py` builds eight sets of synthetic test vectors (plan §9.1), each written as a synthetic PROM in `captures/phase8/vectors/` (`<set>.json`, the image `<set>.bin`, and every vector's frames in `<set>.csv`):

| Set | Vectors | What they exercise |
|---|---|---|
| `energy` | 28 | every energy index, voiced and unvoiced, K near zero |
| `pitch` | 63 | every non-zero pitch index |
| `k` | 264 | every index of every K, voiced; K1–K4 also unvoiced |
| `noise` | 3 | 1 s and 4 s of unvoiced frames |
| `interp` | 25 | jumps of each parameter; transitions between voiced, unvoiced and silent frames |
| `special` | 13 | repeat, silence and stop frames in unusual places; the shortest utterances |
| `random` | 60 | random frames |
| `loud` | 24 | loud voiced frames with random K |

Each vector was captured twice through each path with the standard procedure, by `run.py` (1993 takes, about 2 GB, kept local in `captures/vectors/`):

    python tools/vectors.py build all
    python tools/run.py captures/phase8/vectors/<set>.json all --takes 2 --path both --out captures/vectors --zip-only --resume

`captures/vectors/vectors_<set>/index.md` and `takes.csv` are committed, with every take's checks and hashes. `tools/vecfit.py` compares every take with the model (`captures/phase8/analysis/vectors.csv`) and measures the tables (`tables.csv`, `pitch_periods.csv`, `summary.json`).

**Evidence takes**, copied with their raw data (zipped, unchanged) into `captures/phase8/evidence/<id>/`. `captures/phase8/evidence/SHA256SUMS` lists the hashes of the copied files; `captures/phase8/scripts/evidence_table.py` rebuilds this table, including the model comparison, which uses each take's predecessor. p8-13 to p8-18 come from `captures/phase8/scripts/experiments.py` (`latch_offset` and `previous`).

| ID | Take (under `captures/vectors/`) | Vector | Path | Model | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` | What it shows |
|---|---|---|---|---|---|---|---|
| p8-01 | `vectors_energy/24_u_e10/vsm01` | u_e10 | vsm | exact | `70583d98c67b17e1b5dacdccbd57127f76ed24d9d0656e796cfcbabc74465bf8` | `fb3c4b817ac23e659cfc2ca7f057e6a7cd753fe1c3d26165b9f24f1fc5a6b862` | Unvoiced, neutral K, energy index 10: the model matches bit for bit, including the noise from the state 1958 steps after 0x1FFF |
| p8-02 | `vectors_energy/24_u_e10/ext01` | u_e10 | external | exact | `1e7cc4a113d956ad76280086c5c11c23d9b5727b4be084733f541535e8c68d55` | `966a7516723bd85f2b9c4679dc6ce87d3bb1fce7217467eac356fcb809f659de` | The same through Speak External: bit for bit, from the state 1923 steps after 0x1FFF |
| p8-03 | `vectors_noise/2_noise_e10/vsm01` | noise_e10 | vsm | exact | `be01448cb7293bf8e7a43237d5765af8952887178e5167eef35b0bb6aafe25d9` | `892937541fddd91dec45796727f404bd8be8cb74d0a6be242823c5cb111c4e6e` | 1.1 s of unvoiced frames: the noise register matches over 8699 samples |
| p8-04 | `vectors_energy/10_v_e10/vsm01` | v_e10 | vsm | 41 of 1899 differ (max 1) | `2e56ef959bef2a677b5a28b114bf88b06d728a86c6aa44aea005ff5f8f535082` | `789d0c09fa90a8bef68946e09761a89636e068d4bfa7e4b7869963a13294f163` | Voiced, neutral K: within one code; one slip per pitch period |
| p8-05 | `vectors_pitch/63_p63/vsm01` | p63 | vsm | 45 of 1899 differ (max 1) | `c74cb43b7b00496da3c1206b583673b1d76569f5f1b72a388224b318de3c2305` | `e9847fafd37f6346200085373c687a99eb4eda353841ee7c4a9b20296c83699f` | Pitch index 63: period 160 samples (159 + 1); the chirp and its zero tail |
| p8-06 | `vectors_k/16_k1_15/vsm01` | k1_15 | vsm | 26 of 1899 differ (max 2) | `c3de637b300afac8e1478fec3a3c695058823e52c8321e2fd06d702fa5c39195` | `e141feacfafabf6017eec79622e46c111ec010549ee911723688f96cf2befb33` | A K1 sweep point |
| p8-07 | `vectors_interp/13_v_u_v/vsm01` | v_u_v | vsm | 49 of 2499 differ (max 10) | `5b978f570db5d45f831cfdd718110b82975e138c370d797cc6467e25654d6a12` | `1d23386c70cbc6d3081c432d88528f5dddacfcb0b1d416a4f812b6694a053895` | Voiced, unvoiced, voiced: inhibited interpolation and the excitation switch |
| p8-08 | `vectors_interp/19_s_v/vsm01` | s_v | vsm | 431 of 1899 differ (max 34) | `60cf2607031ac3007b51b4aa906ff5a7b775fd6aa034d074a81f80ea813a5d58` | `ce53a347162de0f7850203bb5ae49b83a0e0b070bdf474b2b2d85967911a6cb7` | Silence first: the zeroing lasts until the second frame, then the previous utterance's K values appear |
| p8-09 | `vectors_special/7_repeat_first/vsm01` | repeat_first | vsm | 189 of 1699 differ (max 1) | `828c05565444d3b240571af972904844f30a6b34dc157f123de805e2d64ca73c` | `b65427329fbce24cd2ce00fa42af6cb1fc4d53bb814b0847a19cf35226008d8a` | A repeat frame first: K values from the previous utterance (within 1 code with the predecessor, 520 without) |
| p8-10 | `vectors_special/8_repeat_after_unvoiced/vsm01` | repeat_after_unvoiced | vsm | 315 of 1699 differ (max 2) | `e2f42050ca9f8bea1b8bc84fa52744de87e7a0e88bcd4ca68aa20e575be1873c` | `c08190c86eac86c5abd3848e4d5e2a4efbacca6dc8b97f6117a693181b93395c` | A voiced repeat frame after unvoiced frames keeps K5–K10 at zero |
| p8-11 | `vectors_special/3_one_voiced/ext01` | one_voiced | external | chip never spoke (stream under 9 bytes) | `54f7bbfba09051e621c70c14e4888242475297f0123916d6fe5efa989e2ee59b` | `5499227dccb02146d3b341192accfb0a145b31aeec04fcca10d42869391e597b` | A 7-byte Speak External stream: the chip never starts |
| p8-12 | `vectors_random/1_r00/vsm01` | r00 | vsm | 1725 of 2699 differ (max 814) | `dfac39e53a34c062d79eaafe74c129566b3ab54a9f9c75bb5644c20902689f13` | `7f181d4c24021d8284c9f6631de4e7f6e69dcc0c606642d90238f9dddc5c681d` | Random frames: loud data, where one-code differences grow |
| p8-13 | `experiments/latch_offset/vsm01` | u_e14, latch_offset 0 | vsm | 1 of 1899 differ (max 1) | `f53449c5a940b959735e89e4e11884add99c16a1df14834a1b759eb7b6cc51d7` | `3577a1f29d116e29432225251dc621ff8204accc34150277442c79f1a10e8b8e` | Latch at the standard time: noise state 1958 steps (one end-of-speech sample off) |
| p8-14 | `experiments/latch_offset/vsm02` | u_e14, latch_offset 1 | vsm | exact | `1e4ae2ef8364a5df7dbc30a17f9cbc254a4aaa08b58c83634064e04e129210c6` | `feffe6324ce50ba691da22d4bb0b8fea5a70a3fa65659f2f01fd2a3125db4030` | Latch one slot later: noise state one step earlier, bit for bit |
| p8-15 | `experiments/latch_offset/vsm03` | u_e14, latch_offset -1 | vsm | 1 of 1899 differ (max 1) | `c286f037437b8870db9b989f71efbdd77089458778d5fce48a4c3e54635c53a0` | `b7a70b926df08027f194a7e8abae697364f97e204ec7908d538dfcecdd862d4e` | Latch one slot earlier: noise state one step later (one end-of-speech sample off) |
| p8-16 | `experiments/latch_offset/ext02` | u_e14, latch_offset 1 | external | exact | `9527fe2177291814edf94db193ded0eeb7749320d9c3ce046e0a9e0dd2d0165c` | `876ceaa25395ef842fba68e3395f2393889c6280670cc648cf945a522443ca13` | Speak External, latch one slot later: one step earlier, bit for bit |
| p8-17 | `experiments/previous_utterance/vsm02` | loud03, previous_utterance - | vsm | 472 of 1499 differ (max 16) | `de94051caaf1e178e5df8b2d83b38a7d5cafbabef123a39037083264ff74b488` | `8ff1dddc00789305cf5eebb25cb5959fb114f3d9348deb5dbfe896475d237b52` | A loud vector after a take of itself: one of five upset variants |
| p8-18 | `experiments/previous_utterance/vsm05` | loud03, previous_utterance loud10 | vsm | 562 of 1499 differ (max 17) | `514fb4c369da15e6409e9c8fc56fd1c9328869b171dd1051f0f5c24559b8a0ba` | `33c5f66cfed920d92d68ea4b6aaa1d9da0a65a83503e4f8db7d9345ef8ef8440` | The same after another vector: the start is the same; it is another upset variant |

**Host interface tests**, `captures/phase8/protocol/<test>/`, by `tools/protocol.py`. No scope: each step's raw VSM trace (`raw/<step>.bin`, the firmware's events, unchanged) records every host cycle with READY and `INT`, and every VSM function. `steps.json` gives each step's notes, `result.json` the decoded cycles, INT edges and VSM functions (`python tools/protocol.py <test> --analyse`). `captures/phase8/protocol/SHA256SUMS` lists the raw files' hashes.

| ID | Test | What it does |
|---|---|---|
| `p8-19` | `fifo_start` | Speak External with 8, 9, 12 and 16 bytes, one at a time with a status read after each, then left to run dry |
| `p8-20` | `starve` | Speak External with 9, 10, 13, 14, 15 and 16 bytes at once, then status reads every 1 ms |
| `p8-21` | `external_start` | 60 Speak Externals with the 9th byte written at a random time |
| `p8-22` | `commands_during_speech` | Load Address, Read Byte, Read and Branch, Speak, Reset and NOP written 200 ms into ACORN |
| `p8-23` | `held_command` | Two Load Address writes during speech, traced to the end of the word |
| `p8-24` | `reset_in_external` | `0x70` and `0xFF` written during Speak External |
| `p8-25` | `vsm_commands` | Load Address with 1 to 6 nibbles, repeated Read Bytes, Read and Branch, and Speak after Read and Branch |

The `load_address` step of `p8-22` left a command held when the test host gave up on READY; the next step needed retried resets (recorded in its notes).
