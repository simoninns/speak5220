# Silence and pauses

Eight takes of two utterances, spoken through Speak External, measure what the chip outputs during the silence frames of a pause, and how a neutral voiced frame quietens it ([§12.2](../authoring.md#122-when-a-frame-is-heard), [§8.8](../lattice-filter.md#the-lowest-order-arithmetic)). All were captured on 2026-10-11 on chip 1 at 640 kHz, with the board in `measure` (TP2 loaded by R2 alone; the load was not measured for these takes).

## The utterance

The utterance is `arctic_a0001` of the phase 11 corpus ([new speech data](../../capture/work/new-speech-data.md)), 3.4 s and 135 frames, encoded by the application's encoder with a 30 dB squelch, which makes its pauses silence frames. It has a 12-frame pause after unvoiced speech (frames 59–70), a 9-frame pause (102–110) and three of one or two frames. The three streams are in `investigation/captures/phase12/evidence/streams/`:

| Stream | Bytes | What it holds | SHA-256 |
|---|---|---|---|
| `plain.bin` | 468 | The encoder's frames with plain silence frames (quiet pauses off) | `5a3ead2981b7706466bcbc8260d611e7d05816c73ec3c304a02bdce00ab0c979` |
| `neutral_first.bin` | 502 | The same, with the first silence frame of every pause of two frames or more replaced by the neutral frame | `a75282bfbecdc113f64a3d6460eadc2d96f8fa1bb4608a0e11ecea6b01636575` |
| `neutral_second.bin` | 479 | The same, with the second silence frame of every pause of three frames or more replaced by the neutral frame: the encoder with quiet pauses on | `b00ab88a999780b38538abe78b813a3b6ede34e1c21151918b68a275fb40944e` |

A second utterance, `arctic_a0026` (2.7 s, 107 frames), encoded with a 24 dB squelch, has one long pause, between "me" and "that" (frames 33–41). It was spoken in two versions, after the model had rung through the pause with quiet pauses on and been silent with them off:

| Stream | Bytes | What it holds | SHA-256 |
|---|---|---|---|
| `a0026_plain.bin` | 465 | The encoder's frames with plain silence frames (quiet pauses off) | `49de83b0c3d3906562de617fa76a2d7bf745a3a54aadbc82a4e15086245fce0c` |
| `a0026_quiet.bin` | 471 | The encoder with quiet pauses on: the neutral frame in frame 34 | `b5d10a35f8165b38f3b4a038b53f5a43cf4049dcc99aab7dd546627a4e39eb01` |

The neutral frame is voiced, at energy index 1, at the pitch of the last voiced frame, with every K at its Table 8-6 entry nearest zero ([§12.2](../authoring.md#122-when-a-frame-is-heard)). No repeat frame follows a pause in either utterance, so no other frame changes.

## Procedure

Each stream was spoken twice, with the chip reset before each take, and decoded:

    python investigation/tools/capture.py <out> --speakext <stream> --reset --seconds 3.8   # 3.2 for arctic_a0026
    python investigation/tools/decode.py <out>

`investigation/captures/phase12/pauses.py` reads the decoded takes and the streams, and prints each pause's code range per frame and its level at TP2 against the loudest frame of the take. Its output is `investigation/captures/phase12/pauses.txt`. The level is the AC rms of the TP2 voltage over the pause's silence frames after its first frame, and after the neutral frame where there is one.

## The takes

Copied with their raw data (zipped, unchanged) into `investigation/captures/phase12/evidence/<id>/`; `investigation/captures/phase12/evidence/SHA256SUMS` lists the hashes of the copied files.

| ID | Stream | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` | What it shows |
|---|---|---|---|---|
| p12-01 | `plain.bin` | `34a66a5053b6890cf6a5541e9179991724d87741afbef1dfb8111c7685abe33d` | `f9d8cfc31f12b53ea2ede0fe44eadb5f9a1985aadf50cd4ff9fb031111892dc4` | Codes −2 to +1 through the 12-frame pause, 12.4 mV rms, 23.1 dB below the loudest frame; −1 to 0 through the 9-frame pause, 22.8 dB below |
| p12-02 | `plain.bin` | `782d675528f22437037dd033cb8b3b0143f8844a8bf45874ca6a591ddfbb7139` | `0c81dd9ad95dd9c0602e163f3278f9ecb017cdd101858040107e4f67477e023a` | The same: 23.5 and 23.0 dB below |
| p12-03 | `neutral_first.bin` | `95d248bdbf72f39b8aa53ec742641a2d26edd62aec6de087ba4d228b4d1cc36e` | `5cdb91e3cd30d0796b2f3396c9b26a902ff4923da32a62d451749c8e3aeb0bbb` | Code 0 after the neutral frame in every pause: 0.75 and 0.82 mV rms, 47.5 and 46.6 dB below |
| p12-04 | `neutral_first.bin` | `73c8ba21dea811f1fa40f6aa45d8ddcba46f27fd53fd76f7d86e8dce4309a7d6` | `a0db9caa901e2c24f0bd4933deeadc8fdf724f008204d1024eb01cb208f1ac10` | The same: 47.8 and 46.8 dB below |
| p12-05 | `neutral_second.bin` | `9164be03dce956071415b6cd14c7eb35fdda5876c56ab1c96923c1a8143e4885` | `ea68b49e5437391fa9a691964646160e94930d74e56bb3aa82687470a655853b` | Code 0 after the neutral frame: 47.5 and 47.1 dB below. The neutral frame itself is within a code of zero; the first frame of the pause, where the sound before fades, is as in p12-01 |
| p12-06 | `neutral_second.bin` | `78b745c603c5cd4e43a221925e34a3a222a39326eb74c239c2c2da041fead0f5` | `284d0c83f693bba7866d82809cd7bcbed3116cc83863de21798d843316135952` | The same: 47.7 and 47.4 dB below |
| p12-07 | `a0026_plain.bin` | `6c44473f9231e711ebd3a52643a2e09a161195e626b62bcca7965acb4336e565` | `3565bfbe2931ce2e2a4b3068780399508e5f5ab7489e7bebe22436a5165b29e5` | Codes −1 to 0 through the pause, 11.1 mV rms, 23.3 dB below the loudest frame, where the model is exactly 0 |
| p12-08 | `a0026_quiet.bin` | `4114de27e2cefc06bcfdf0bd8cd3e94bbfbb035e72d89640ddaf2fec5d14b15e` | `1cac0a553f60425ad90be76f88440e36ef9cb07c9ceab9e8c47e958ec5e2e080` | Code 0 after the neutral frame, 0.73 mV rms, 47.0 dB below, where the model rings between −1 and 0 every two or three samples |

Each `arctic_a0026` stream was also captured a second time, and the repeat takes behaved the same through the pause: ringing between −1 and 0 without the neutral frame (11.0 mV rms, 22.8 dB below), and code 0 after it (0.71 mV rms, 46.7 dB below). Only p12-07 and p12-08 are kept, to limit the size of the evidence.

The two-frame pauses are left alone in `neutral_second.bin`: in p12-05 and p12-06 the one at frames 94–95 still rings (24 to 25 dB below), as in p12-01. The two-frame pause at frames 113–114 settles at codes 0 and +1 in every take, with or without a neutral frame.

## Agreement

Around the quietened pauses, the takes of `neutral_second.bin` differ from those of `plain.bin` by no more than two takes of the same stream differ from each other: at most 11 codes in the unvoiced frames before the 12-frame pause (13 between the two `plain.bin` takes) and 2 in its first frame (2 and 3). The unvoiced frames differ from take to take because the noise generator does ([§8.6](../excitation-interpolation.md#86-excitation)).

Against the reference emulator, the `plain.bin` takes are identical at 52–53 % of their samples, and the `neutral_second.bin` takes at 62 %: the samples the neutral frame silences are among those where the model's low-order arithmetic differs from the chip's ([§8.8](../lattice-filter.md#the-lowest-order-arithmetic)). In the 12-frame pause the model starts between codes −1 and +2 and falls back to 0 and +1, where the chip stays between −2 and +1; after the neutral frame both are exactly 0.

In `arctic_a0026` the model gets the pause the other way round: exactly 0 with plain silence frames, where the chip rings between −1 and 0 (p12-07), and ringing between −1 and 0 after the neutral frame, where the chip is exactly 0 (p12-08). The neutral frame quietened the pause on the chip in all three pauses tested; the model cannot be used to tell which pauses ring.
