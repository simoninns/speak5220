# VSM reads and the two input paths

Captures p3-01 to p3-04 are the first with the Pico's trace of every VSM function: one word through a VSM Speak and through Speak External, and a Read Byte. Captures p4-01 to p4-04 repeat the word through both paths with the capture runner, which checks every take. The datasheet uses them in [§4](../vsm-interface.md#4-voice-synthesis-memory-vsm-interface), [§6](../commands.md#6-description-of-commands) and [§8.2](../dac-and-audio.md#82-da-conversion).

## First VSM traces: p3-01 to p3-04

Captured on 2026-10-06.

Board: as for the [DAC transfer captures](dac.md), except that **the C1 link was connected**, so the audio stage also loaded TP2. The three scope captures' `capture.json` give `board` as `audio`; the field `recorded_as` keeps the value first recorded, the default `measure` configuration, which was wrong. The layer 3 check of p4-01 to p4-04 found the audio stage's history in TP2 (below). The DAC levels of these captures are therefore not reference data; their traces and `I/O` samples are unaffected. Firmware: the command firmware, built from the uncommitted working tree on top of `9fcb313`, so `capture.json` records it as `9fcb313-dirty`.

`investigation/tools/capture.py` drove each capture, with no manual steps:

1. Reset the chip with `--reset`: nine `0xFF`, then Reset.
2. Arm the scope.
3. Speak through the Pico.
4. Save the VSM trace (`raw/vsm_trace.bin`) beside the scope data.

All three scope captures used 496 ns sampling over 1.0 s, with probe D on `SCOPE_TRIG`.

| ID | Command | What it shows | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p3-01_speak_acorn` | `SPEAK 0x3CBB6` (ACORN, VSM) | The trace: 1197 events. The chip read 1152 bits, exactly the word up to the end of its stop code. One `INT`, five status reads. | `78d9f4c6e01ebb8f069105432b72697207cb2f3a9f96eb61cf3429d21a1fc217` | `cbb7053d64c9f21e343c4e9a5279e250ba2edea0684ed7ef2aedf6182ff4380b` |
| `p3-02_speakext_acorn` | `SPEAKEXT` of the same 144 bytes | The FIFO was topped up on `INT`: 19 interrupts and 145 writes. The word ends with BE set, because its stop code ends on a byte boundary. | `2aede503ad71024d5f929d776250aadb63f652ac135c3ef250a28a633099c661` | `fcfbdc14e83cd9e60d3da222ae05a4ba40f7f1ade1e2c110aa549e215044b441` |
| `p3-03_speak_acorn_repeat` | Repeat of p3-01 | A second VSM run, to compare with p3-01 and p3-02 | `2768489bf51e34cbec383c1dc6ae1fd2b52de5472e3501b61fc1bf9cb56071fc` | `590034554e7d36df419217688bc94dac3350f1d14afc40053b04d1959d26c2ab` |
| `p3-04_read_byte` | `CMD` Load Address ×5 (`0x3CBB6`), `0x10`, then `READ` | **A trace only, with no scope.** The chip read the byte 0x23 bit by bit, LSB first, then returned 0xC4 on the data bus: the byte bit-reversed. | — | `953787ff24f4d163fe3fcd9461d2686bfc7ed825e58a15938a4df050036cc183` |

Derived files in each capture directory: `vsm_trace.csv` from `investigation/tools/pico.py`, and `word0_samples.wav` and `word0_samples.csv` from `investigation/tools/decode.py`. To regenerate them:

```
python investigation/tools/decode.py investigation/captures/phase3/p3-01_speak_acorn
python investigation/tools/pico.py trace-csv investigation/captures/phase3/p3-01_speak_acorn
```

**What the three captures show:** the word from the first sample that is not idle (−1) to the last.

- **Length.** The word is 5209, 5209 and 5207 samples long.
- **Codes.** p3-01 and p3-03 (VSM, VSM) differ in 1832 samples, by 3.0 codes rms. p3-03 and p3-02 (VSM, Speak External) differ in 2029 samples, by 3.1 codes rms.
- **Where they differ.** The two VSM runs agree for their first two frames. Speak External differs from sample 11 onwards.

So a repeat of one input path differs about as much as a change of path. Comparing the paths needs a controlled start state: see [Start timing, noise and repeats](start-and-noise.md).

## One word through both paths: p4-01 to p4-04

Captured on 2026-10-06.

Board: as for the DAC transfer captures, with the C1 link removed, so TP2 is loaded by R2 alone (996 Ω). The chip is a TMS5220NL marked `DEP 8423`; the supplies measured +4.995 V and −5.000 V (`investigation/tools/bench.json`). Firmware: the command firmware of p3-01 to p3-04. It reports `9fcb313-dirty`, because it was built before that firmware was committed; its source differs from `9114160` only in a comment. Host tools: `9114160` plus uncommitted runner tools.

One runner command made the whole set, with no manual steps:

```
python investigation/tools/run.py acorn_phrom_a ACORN --takes 2 --path both --out investigation/captures/phase4 --zip
```

It wrote `investigation/captures/phase4/acorn_phrom_a/`: `prom.json`, `index.md`, and `160_acorn/` with `compare.json` and four take directories in the layout of [data layout](../../capture/data-layout.md). Each take was captured at 496 ns for 0.88 s, with probe D on `SCOPE_TRIG`, and with no reset before it. Every check in `investigation/tools/process.py` passed on every take (`capture.json`, `processed.checks`).

| ID | Path | What it shows | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` |
|---|---|---|---|---|
| `p4-01` = `acorn_phrom_a/160_acorn/vsm01` | VSM Speak | 27 frames, 1152 bits read, one frame read every 25.000 ms (±0.12 µs). The first frame read 19.69 ms after `SCOPE_TRIG` rose | `c8f9ec9e36b53b229d6737e4a3b23d8bbff63b0d6567bf846057d252337497db` | `6df198ec7ad1b4e3a67ebf4829ccfdf5dd3a409f1cbb2b7e09e6f149e21b2417` |
| `p4-02` = `…/vsm02` | VSM Speak | As p4-01, with the first frame read at 12.49 ms | `e96b29ee3e5251c181a0dea88e140039f0acd13d377d5675f6982544c1874637` | `365f9bdb30bf490740eccff89c9a48d2909380e79d5d284adfc6047fc4e912a5` |
| `p4-03` = `…/ext01` | Speak External | The same 144 bytes; the bytes on the bus match the stream | `25584991e89da9db0658eb43e0f44f52b527c6030e121008dedfb6c4acf42d8c` | `d4226f543a104160ef64626e04d910709162da05f9b8de68132807a6575511fd` |
| `p4-04` = `…/ext02` | Speak External | Repeat of p4-03 | `1e3b0e349bd4da595161ce3a8f1e18a5495f089163888e6166940476c09e4765` | `06a20f74a25e97ac2cb915330e4db18610d7afdce8e2834986ef65fe02e3ba2d` |

Derived files in each take directory, all from `investigation/tools/process.py`: `vsm_trace.csv`, `frames.csv`, `samples.wav`, `samples.csv`, `dac.wav` and `listen.wav`. To regenerate them, and the comparison and index:

```
python investigation/tools/process.py investigation/captures/phase4/acorn_phrom_a/160_acorn/*/
python investigation/tools/compare.py investigation/captures/phase4/acorn_phrom_a/160_acorn
python investigation/tools/promindex.py investigation/captures/phase4/acorn_phrom_a
```

**What the set shows:**

- **Layer 2 against layer 3.** The DAC voltage follows the DAC table of p2-01 and p2-02 for the `I/O` codes to 2.2–2.3 mV rms, and 99.9 % of the codes recovered from it are within ±2 of `I/O` (62–71 % exactly). The fitted gain is 1.070–1.071: the DAC's scale is 7 % above that table (the idle level, code −1, is 0.635 V against 0.592 V in p2-01). The supplies were within 6 mV of nominal, so the cause is not known ([Appendix B](../gaps.md#the-dacs-absolute-scale)).
- **The audio stage leaves a signature.** With the C1 link connected (a scratch run just before this set, and p3-01 to p3-03), the same fit leaves 6.5–6.7 mV rms and a gain of 0.92–0.93. A first-order term for the audio stage's coupling network brings it down to 1.8–1.9 mV. This is how the board entries of p3-01 to p3-03 were found to be wrong.
- **The start varies by most of a frame.** Speech began 219–398 samples after `SCOPE_TRIG` rose. In the VSM takes the first frame was read 19.69 ms and 12.49 ms after it (p3-01: 25.51 ms), while the time from the first frame read to Talk Status clearing was the same, 674.87 ms, in all three.
- **Repeats differ as in p3-01 to p3-03.** After alignment, each take differs from p4-01 by 3.1–3.5 codes rms. p4-02 first differs 946 samples after speech starts; both Speak External takes differ from sample 204. The frames parsed are identical in all four.
