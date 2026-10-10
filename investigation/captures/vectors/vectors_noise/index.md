# Phase 8 test vectors: noise

- **Image:** 873 bytes, SHA-256 `42ee054afacd26da4d138ad0682238844eb693551f8f40b9cbc4dba79987feae`, bank 0. Target chip: TMS5220.
- **Provenance:** tools/vectors.py set 'noise': Long unvoiced runs, loud and with neutral K, so that each sample's sign
shows the noise generator's output; and the same at other energies.
- **Captured:** 3 of 3 words. Each take directory follows `docs/technical/capture/data-layout.md`; its `capture.json` holds the metadata and checks. [`takes.csv`](takes.csv) lists every take, with the SHA-256 of its output (`talk_sha256`: the I/O codes from sample 0 to the end of talk, as 16-bit little-endian integers) and of its raw files.
- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent (Speak External), up to the stop code. ms is the talk time of the first take, from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with the first, sample for sample, after aligning them. For takes made with the standard procedure, it gives how many match the per-sample majority of all of them exactly, and the samples where no majority exists (`compare.json`).

| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |
|---|---|---|---|---|---|---|---|---|---|
| 1 | noise_e14 | 0x00000 | 41 | 1164 | 1037 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](1_noise_e14/vsm01/samples.wav) · [listen](1_noise_e14/vsm01/listen.wav) · [frames](1_noise_e14/vsm01/frames.csv) |
| 2 | noise_e10 | 0x00092 | 41 | 1164 | 1037 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](2_noise_e10/vsm01/samples.wav) · [listen](2_noise_e10/vsm01/listen.wav) · [frames](2_noise_e10/vsm01/frames.csv) |
| 3 | noise_e14_long | 0x00124 | 161 | 4644 | 4037 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](3_noise_e14_long/vsm01/samples.wav) · [listen](3_noise_e14_long/vsm01/listen.wav) · [frames](3_noise_e14_long/vsm01/frames.csv) |
