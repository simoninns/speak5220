# DAC transfer and the rails

Captures p2-01 to p2-06 measure the DAC's transfer function at two loads and its step timing, with the audio stage disconnected. Captures p6-01 to p6-08 use loud TI-99/4A speech to reach both rails. The datasheet uses them in [§8.2](../dac-and-audio.md#82-da-conversion) and [§8.3](../dac-and-audio.md#83-audio-output).

## The DAC transfer function: p2-01 to p2-06

Captured on 2026-10-06.

Board: as for the [output-signal captures](output-signals.md), with the measurement changes of [bench hardware](../../capture/bench-hardware.md). The C1 link is removed, so the audio stage does not load TP2, and R27 (10 kΩ, GPIO22 to GND) is fitted. TP2 is loaded by R2 alone (996 Ω, measured with a meter), or by R2 with a second 1 kΩ in parallel (482 Ω combined, measured). `capture.json` records this under `board`. Firmware: the same demo loop. Host tools: `d78e6ca` plus uncommitted analysis tools. Probes: as for the output-signal captures, with D on TP5 `SCOPE_TRIG`, apart from p2-06.

| ID | TP2 load | Interval | Length | What it shows | SHA-256 of `scope_raw.npz` |
|---|---|---|---|---|---|
| `p2-01_phrase_496ns` | 996 Ω | 496 ns | 5.2 s | DAC transfer, with p2-02: 41,599 sample periods | `91f0d89239b393cb9e3193f31691ffa00b671035bc8a36827b11775c8ae3afca` |
| `p2-02_phrase_496ns` | 996 Ω | 496 ns | 5.2 s | Repeat of p2-01 | `f83ad011076baed4d4b2e405d9756f0ac9925fe785e11a22005dc9e89a32b19c` |
| `p2-03_speech_8ns` | 996 Ω | 8 ns | 122 ms | DAC step timing with the audio stage disconnected (180 steps) | `1c94cfff9a841e6ed89fe6ae4e3392bb6e33f185988dabb3277a6aa6fc739912` |
| `p2-04_phrase_half_load` | 482 Ω | 496 ns | 5.2 s | DAC transfer at the lower load, with p2-05 | `3cfc267df7d479c88dbf1d6c9fbcb967b4e1a08b453ecb02e48547fc42ff7584` |
| `p2-05_phrase_half_load` | 482 Ω | 496 ns | 5.2 s | Repeat of p2-04 | `81bdc1166451f4e0e80502b5100e48cbfe574e7e1356348272f3e345d53d8b4f` |
| `p2-06_probe_a_grounded` | 482 Ω | 496 ns | 2.2 s | Probe A's tip on a GND test point: the scope's zero offset on Ch A (−8.69 mV) | `a8b7773e390696a978f9825e72b8bc83e7b00d5e7c56003960b47b0fd68a2e47` |

### The step timing

![One sample period of p2-03: SPEAKER, T11, I/O and SCOPE_TRIG, with code −22](../img/evidence/p2-03_edge_timing.png)

*Figure: one sample period of p2-03, into 996 Ω with the audio stage disconnected, from `investigation/tools/edge_timing.py`. The code is −22. The DAC step comes at the start of slot 1.*

### The transfer at each load

![TP2 against code into 996 ohm, its INL, and the residual against the previous sample](../img/fig_dac_transfer.png)

*Figure: `investigation/captures/phase2/dac_transfer/` (p2-01 and p2-02, 996 Ω). Top: mean TP2 voltage for each code, with the bit-weight model. Middle: deviation from the end-point line, in LSB. Bottom: each sample's residual from the per-code table against the change from the previous sample. This is Figure 8-2 of the datasheet.*

![TP2 against code into 482 ohm, its INL, and the residual against the previous sample](../img/evidence/dac_transfer_half_load.png)

*Figure: `investigation/captures/phase2/dac_transfer_half_load/` (p2-04 and p2-05, 482 Ω), in the same form.*

![SPEAKER current per code at 996 and 482 ohm loads, and their ratio](../img/fig_dac_load.png)

*Figure: `investigation/captures/phase2/dac_compare/`, the two loads compared. Top: current per code. Bottom: the ratio of the two currents; codes seen fewer than 20 times are grey. This is Figure 8-3 of the datasheet.*

### Derived files

- `p2-03_speech_8ns/edge_timing.json`, `edge_timing.png`: `investigation/tools/edge_timing.py`.
- `investigation/captures/phase2/dac_transfer/` (p2-01 + p2-02) and `dac_transfer_half_load/` (p2-04 + p2-05): `dac_transfer.json`, `dac_table.csv` and `dac_transfer.png` from `investigation/tools/dac_transfer.py`.
- `investigation/captures/phase2/dac_compare/`: `dac_compare.json` and `dac_compare.png` from `investigation/tools/dac_compare.py`.

To regenerate them:

```
python investigation/tools/edge_timing.py investigation/captures/phase2/p2-03_speech_8ns --plot
python investigation/tools/dac_transfer.py investigation/captures/phase2/p2-01_phrase_496ns investigation/captures/phase2/p2-02_phrase_496ns \
    --zero investigation/captures/phase2/p2-06_probe_a_grounded --out investigation/captures/phase2/dac_transfer --plot
python investigation/tools/dac_transfer.py investigation/captures/phase2/p2-04_phrase_half_load investigation/captures/phase2/p2-05_phrase_half_load \
    --zero investigation/captures/phase2/p2-06_probe_a_grounded --out investigation/captures/phase2/dac_transfer_half_load --plot
python investigation/tools/dac_compare.py investigation/captures/phase2/dac_transfer investigation/captures/phase2/dac_transfer_half_load \
    --out investigation/captures/phase2/dac_compare --plot
```

The phrase captures have no `word<N>` WAV and CSV files. `investigation/tools/decode.py` makes them if they are wanted.

## Loud speech and the DAC rails

Captures p6-01 to p6-08, made on 2026-10-06.

Board, chip, supplies and firmware: as for the [standard-procedure captures](start-and-noise.md) (firmware `d66eeef-dirty`). The host tools are those with `processed` version 3, which judges the DAC check inside codes ±119. The PROM is the TI-99/4A speech VSMs, `ti99_speech` ([PROM corpus](../../capture/prom-corpus.md)), rebuilt with `investigation/tools/promimport.py` and loaded into banks 0 and 1. One runner command, using the standard procedure:

```
python investigation/tools/run.py ti99_speech MORE LINE --takes 2 --path both
```

The runner calibrated the start offsets (VSM −2.10 µs, Speak External +216.88 µs). Every check passed on every take; each Speak was accepted 12255.8–12256.2 µs before its frame boundary. The raw data was zipped afterwards, unchanged (`run.zipRaw`), and the set was moved to `investigation/captures/phase6/`.

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

- **The rails.** TI-99 speech, written for the TMS5200, is loud on the TMS5220: MORE's codes span −271 to +412, and LINE's the whole 10-bit range, −512 to +511. 514–723 samples per take lie outside the DAC's range. Codes above +127 give the same output as +127 to within 0.5 mV, and codes below −128 the same as −128 to within 2 mV. Fitted inside ±119 (gain 1.059–1.062, 1.9–2.2 mV rms), the bit-weight model of p2-01 to p2-06 puts the rail samples 7.1–7.8 mV too high on average: about 6 mV at +127 and 10 mV at −128 ([§8.2](../dac-and-audio.md#82-da-conversion)).
- **Reading across the VSMs.** LINE starts at 0x3FD5, in CD2325A, and ends at 0x4049, in CD2326A; the bits read match the image across the boundary (`vsm_image`).
- **Repeats.** The two LINE Speak External takes are bit-identical. The others differ within their path by up to 10 codes (MORE) and 15 codes (LINE, VSM), with departures starting as early as sample 3 of speech: larger and earlier than the Acorn word's 3 codes in [p5-01 to p5-08](start-and-noise.md#the-standard-procedure-p5-01-to-p5-08). These are upsets ([§7](../power-up.md#7-power-up-clear-and-the-state-between-utterances)).
