# Atari Star Wars (1983) arcade speech

- **Image:** 6323 bytes, SHA-256 `cdf1349cf198b3b925f27c7a00d9dc2745496606e5445864e914d10dcd20bd45`, bank A. Target chip: TMS5220.
- **Provenance:** The game sends these blocks to the TMS5220 with Speak External; it has no VSM. eRadiofan assembled them into a VSM image in TI's indexed format (ROMdata_StarWars.c, credited to nmikstas's disassembly of the sound ROM). Every block is checked against that disassembly. The image itself is not original, so only the Speak External path replays the game exactly. The sound ROM dump itself was not checked (MAME lists it as 136021-107.1jk, SHA-1 c38661b2b846fe93487eef09ca3cda19c44f08a0).
- **Captured:** 2 of 23 words. Each take directory follows `docs/technical/capture/data-layout.md`; its `capture.json` holds the metadata and checks. [`takes.csv`](takes.csv) lists every take, with the SHA-256 of its output (`talk_sha256`: the I/O codes from sample 0 to the end of talk, as 16-bit little-endian integers) and of its raw files.
- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent (Speak External), up to the stop code. ms is the talk time of the first take, from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with the first, sample for sample, after aligning them. For takes made with the standard procedure, it gives how many match the per-sample majority of all of them exactly, and the samples where no majority exists (`compare.json`).

| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Use the force Luke | 0x28030 |  |  |  | not captured |  |  |  |
| 2 | Remember | 0x2816A |  |  |  | not captured |  |  |  |
| 3 | I'm on the leader | 0x2820F |  |  |  | not captured |  |  |  |
| 4 | The force is strong with this one | 0x282CA |  |  |  | not captured |  |  |  |
| 5 | Red 5 standing by | 0x28404 |  |  |  | not captured |  |  |  |
| 6 | This is Red 5 I'm going in | 0x28545 |  |  |  | not captured |  |  |  |
| 7 | R2 try to increase the power | 0x28691 |  |  |  | not captured |  |  |  |
| 8 | You are all clear kid | 0x287D1 |  |  |  | not captured |  |  |  |
| 9 | Let go Luke | 0x288AE |  |  |  | not captured |  |  |  |
| 10 | Darth Vader breathing | 0x289A4 |  |  |  | not captured |  |  |  |
| 11 | Yahoo | 0x28A27 |  |  |  | not captured |  |  |  |
| 12 | I have you now | 0x28B06 | 45 | 2165 | 1137 | 51 external | 50 pass, 1 FAIL: start_slot | external: 0 of 50 match, 203 samples undecided | [samples](12_i_have_you_now/ext01/samples.wav) · [listen](12_i_have_you_now/ext01/listen.wav) · [frames](12_i_have_you_now/ext01/frames.csv) |
| 13 | Look at the size of that thing | 0x28C18 | 66 | 2914 | 1662 | 51 external | 50 pass, 1 FAIL: start_slot | external: 0 of 50 match, 32 samples undecided | [samples](13_look_at_the_size_of_that_thing/ext01/samples.wav) · [listen](13_look_at_the_size_of_that_thing/ext01/listen.wav) · [frames](13_look_at_the_size_of_that_thing/ext01/frames.csv) |
| 14 | Stay in attack formation | 0x28D88 |  |  |  | not captured |  |  |  |
| 15 | The force will be with you | 0x28EFF |  |  |  | not captured |  |  |  |
| 16 | Always | 0x28FFC |  |  |  | not captured |  |  |  |
| 17 | R2D2 sound effects | 0x29075 |  |  |  | not captured |  |  |  |
| 18 | Tie fighter sound effects | 0x29172 |  |  |  | not captured |  |  |  |
| 19 | I'm hit but not bad. R2 see what you can do with it | 0x29296 |  |  |  | not captured |  |  |  |
| 20 | I lost R2 | 0x2949B |  |  |  | not captured |  |  |  |
| 21 | Great shot kid.  That was one in a million | 0x2954B |  |  |  | not captured |  |  |  |
| 22 | I can't shake him | 0x29715 |  |  |  | not captured |  |  |  |
| 23 | Luke trust me | 0x297DE |  |  |  | not captured |  |  |  |
