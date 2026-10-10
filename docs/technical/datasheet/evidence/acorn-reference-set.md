# The Acorn PHROM A reference set

Captures p7-01 to p7-11, made on 2026-10-06 and 2026-10-07, and the reference set [corpus `acorn_phrom_a`] they come from or sit beside. The datasheet uses them in [§4](../vsm-interface.md#4-voice-synthesis-memory-vsm-interface), [§7](../power-up.md#7-power-up-clear-and-the-state-between-utterances) and [§8.2](../dac-and-audio.md#82-da-conversion).

## Set-up

Board, chip, supplies and firmware: as for the [loud-speech captures](dac.md#loud-speech-and-the-dac-rails) (firmware `d66eeef-dirty`). The host tools are those with `processed` version 4. The scope is held open for a whole run, and `hostCycles` sees a READY edge that shares a trace event with the end of a write.

## The reference set, corpus `acorn_phrom_a`

These are every word of the Acorn PHROM A, five takes or more through each path, and the address sweep: one take from each of 16,144 start addresses, and five from every 64th. All were taken with the standard procedure, by:

```
python investigation/tools/run.py acorn_phrom_a all --takes 5 --path both --sweep --resume --zip-only
python investigation/tools/run.py acorn_phrom_a --sweep --sweep-step 64 --sweep-takes 5 --resume --zip-only
```

The set is kept outside the repository until it is published ([data layout](../../capture/data-layout.md)), in `investigation/captures/corpus/acorn_phrom_a/`. Of it, only `prom.json`, `index.md`, `takes.csv` and `sweep.csv` are committed. `takes.csv` gives, for every take, the SHA-256 of its `scope_raw.npz` and `vsm_trace.bin`, and of its output (`talk_sha256`). `investigation/captures/phase7/scripts/corpus_stats.py` recomputes the figures the datasheet cites from the set.

**Takes copied from the set** into `investigation/captures/phase7/acorn_phrom_a/`:

| ID | Take | What it shows | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p7-07` = `sweep/addr_3c001/vsm01` | Start 0x3C001: a stop code first | Talk Status clears 0.09 ms after the stop code is read; no output | `c4bc12a56eb1a53083a50214371ad0be3d34194381e89fc3fa2219c06783e595` | `9ba54cf64ad16e4864285584a9d46afc2b2e48276d1659f8bb4b53fa0a4e82b7` |
| `p7-08` = `sweep/addr_3c000/vsm01` | Start 0x3C000: 3 frames | Talk Status clears 74.87 ms (3 × 25 ms − 0.13 ms) after the first read | `a2a603f57dfea28c461fb689cfdd00f54730ec4633cff678bb4758eda6b8e8bc` | `6e342fd3227321fcf3a99c0d9f62950315787a645be224138b810ec097003f47` |
| `p7-09` = `195_f/ext01` | F, Speak External | A take accepted in the target slot (12255.74 µs before the frame) | `3ce208da6cc2bc71682711c0fa051e59f6491e2d04832bd0b62d26c15cdb0055` | `eb23472e12dd2f6672ba06d772a670164780c8fe693081ca3e40525d33ef40d9` |
| `p7-10` = `195_f/ext04` | F, Speak External | Accepted one slot early (12261.95 µs before the frame): different noise, up to 63 codes from the other takes. Fails `start_slot` | `e53c849c22037b446adf0b821fac230aa8aa095d89ea863acbbc632ae8425dd4` | `a21c6953a638bd65e634c363ed2fff5b92d6fb8d2180729f9aa309fc0d9174e1` |
| `p7-11` = `sweep/addr_3f416/vsm01` | Start 0x3F416: loud | The DAC model 4–6 mV high for codes 64–119 in size; fails `layer2_vs_layer3` (3.06 mV rms), as did three retakes | `29ecec87d831ae6564cc8b80dd7b125c76701ed6934a357f5c79893d06e01254` | `2f61e87b0051f29a22877429c22975b496f3b4a972c7572d00b8e001fd82eedb` |

## The previous utterance: p7-01 to p7-06

In `investigation/captures/phase7/previous_utterance/`. Sweep start 0x3F9D4 begins with a repeat frame, then loud voiced frames (the repeat frame's K values come from the previous utterance, [§7](../power-up.md#7-power-up-clear-and-the-state-between-utterances)). It was spoken twelve times with the standard procedure. Before some takes, another utterance was spoken first, untimed:

- **X:** sweep start 0x3FD5E, loud.
- **Y:** the word B, 0x3D065.
- **–:** nothing, so the previous utterance was the take before.

The order was X, –, –, Y, –, –, X, –, Y, –, X, Y (`sequence.json`), by `investigation/captures/phase7/scripts/previous_utterance.py`. Six of the takes are kept. The raw data was zipped afterwards, unchanged.

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
