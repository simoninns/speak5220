# Start timing, noise and repeats

Captures p5-01 to p5-18, made on 2026-10-06. p5-01 to p5-08 are the first takes made with the standard procedure, which times each Speak to the frame clock. p5-09 to p5-18 are experiments on the frame clock, the noise generator, clock stops and the VSM data margin. The datasheet uses them in [§4](../vsm-interface.md#4-voice-synthesis-memory-vsm-interface), [§6](../commands.md#6-description-of-commands), [§7](../power-up.md#7-power-up-clear-and-the-state-between-utterances), [§8.2](../dac-and-audio.md#82-da-conversion) and [§8.4](../serial-output.md#84-serial-sample-output-t11-and-io).

## Set-up

Board, chip and supplies: as for [p4-01 to p4-04](speech-paths.md#one-word-through-both-paths-p4-01-to-p4-04) (C1 link removed, R2 996 Ω, TMS5220NL `DEP 8423`). Firmware: the command firmware with timed starts (`SPEAK … at_us`, `TIME`), `POLL` (status reads during speech; off by default) and `VSMDELAY` ([firmware protocol](../../hardware/firmware-protocol.md)). It reports `d66eeef-dirty`, as do the host tools: both are `d66eeef` plus uncommitted changes. The TMS5220 was power-cycled once, between p5-15 and p5-16, and the Pico was reflashed three times, always with the chip idle.

## The standard procedure: p5-01 to p5-08

One runner command, using the standard procedure ([repeatability](../../capture/work/repeatability.md)):

```
python investigation/tools/run.py acorn_phrom_a ACORN --takes 4 --path both --out investigation/captures/phase5 --zip
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

To regenerate the derived files, the comparison and the index, run `process.py`, `compare.py` and `promindex.py` as for [p4-01 to p4-04](speech-paths.md#one-word-through-both-paths-p4-01-to-p4-04), on `investigation/captures/phase5/…`.

**What the set shows:**

- **The same start in every take.** Speech starts at sample 306 (VSM) and 304 (Speak External) in every take, and Talk Status clears after 5499 and 5497 samples.
- **The same noise in every take of a path.** No two VSM takes differ by more than 3 codes, and no two Speak External takes by more than 2. Without the procedure, repeats differ by up to 35–40 codes (p4-01 to p4-04).
- **Upsets remain.** All four VSM takes differ from one another, each by a few hundred samples of ±1–3 codes. The departures start 2489–2881 samples after speech starts. The Speak External takes are the same, with departures from 22–752 and 4287–4705 samples.
- **VSM and Speak External differ.** Accepted at the same time before the frame, Speak External starts its output 2 samples earlier, and its noise differs from the VSM takes' (up to 35 codes).
- **DAC scale.** The fitted gain is 1.068–1.071, as in p4-01 to p4-04. With the chip idle, a meter on TP2 read 0.642 V; the scope's idle level was 0.631 V.

## Experiments: p5-09 to p5-18

These are in `investigation/captures/phase5/experiments/`. The scope experiments keep, for each take, `capture.json`, `frames.csv`, `samples.wav` (the exact `I/O` codes) and `raw/vsm_trace.bin`; their raw scope data is kept locally, and `scope_raw_sha256.txt` lists its hashes. `scripts/` holds the experiment scripts, in their final form. The trace-only experiments need no scope.

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
| `p5-18` | `vsm_data_margin/vsm_delay.json` | The VSM emulation delays each data bit on `ADD8` by 0–10 µs after it sees `M0` rise (`VSMDELAY`; `scripts/delay.py`) | Within a parameter, the chip reads one bit every 12.5 µs ([§4](../vsm-interface.md#4-voice-synthesis-memory-vsm-interface)). It still read all 1152 bits of ACORN with a 9.5 µs delay; at 10 µs the word never ended |
