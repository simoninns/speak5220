# Phase 8 test vectors: k

- **Image:** 8592 bytes, SHA-256 `3531f147aa6f0d12a7f7d8a620a90cdd556a31d31665e60b0db4e958a1b256f2`, bank 0. Target chip: TMS5220.
- **Provenance:** tools/vectors.py set 'k': Every index of every K, the other K neutral: voiced, pitch index 32.
K1-K4 also unvoiced, since unvoiced frames use them too.
- **Captured:** 96 of 264 words. Each take directory follows `docs/technical/capture/data-layout.md`; its `capture.json` holds the metadata and checks. [`takes.csv`](takes.csv) lists every take, with the SHA-256 of its output (`talk_sha256`: the I/O codes from sample 0 to the end of talk, as 16-bit little-endian integers) and of its raw files.
- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent (Speak External), up to the stop code. ms is the talk time of the first take, from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with the first, sample for sample, after aligning them. For takes made with the standard procedure, it gives how many match the per-sample majority of all of them exactly, and the samples where no majority exists (`compare.json`).

| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |
|---|---|---|---|---|---|---|---|---|---|
| 1 | k1_00 | 0x00000 |  |  |  | not captured |  |  |  |
| 2 | k1_01 | 0x00026 |  |  |  | not captured |  |  |  |
| 3 | k1_02 | 0x0004C |  |  |  | not captured |  |  |  |
| 4 | k1_03 | 0x00072 |  |  |  | not captured |  |  |  |
| 5 | k1_04 | 0x00098 |  |  |  | not captured |  |  |  |
| 6 | k1_05 | 0x000BE |  |  |  | not captured |  |  |  |
| 7 | k1_06 | 0x000E4 |  |  |  | not captured |  |  |  |
| 8 | k1_07 | 0x0010A |  |  |  | not captured |  |  |  |
| 9 | k1_08 | 0x00130 |  |  |  | not captured |  |  |  |
| 10 | k1_09 | 0x00156 |  |  |  | not captured |  |  |  |
| 11 | k1_10 | 0x0017C |  |  |  | not captured |  |  |  |
| 12 | k1_11 | 0x001A2 |  |  |  | not captured |  |  |  |
| 13 | k1_12 | 0x001C8 |  |  |  | not captured |  |  |  |
| 14 | k1_13 | 0x001EE |  |  |  | not captured |  |  |  |
| 15 | k1_14 | 0x00214 |  |  |  | not captured |  |  |  |
| 16 | k1_15 | 0x0023A |  |  |  | not captured |  |  |  |
| 17 | k1_16 | 0x00260 |  |  |  | not captured |  |  |  |
| 18 | k1_17 | 0x00286 |  |  |  | not captured |  |  |  |
| 19 | k1_18 | 0x002AC |  |  |  | not captured |  |  |  |
| 20 | k1_19 | 0x002D2 |  |  |  | not captured |  |  |  |
| 21 | k1_20 | 0x002F8 |  |  |  | not captured |  |  |  |
| 22 | k1_21 | 0x0031E |  |  |  | not captured |  |  |  |
| 23 | k1_22 | 0x00344 |  |  |  | not captured |  |  |  |
| 24 | k1_23 | 0x0036A |  |  |  | not captured |  |  |  |
| 25 | k1_24 | 0x00390 |  |  |  | not captured |  |  |  |
| 26 | k1_25 | 0x003B6 |  |  |  | not captured |  |  |  |
| 27 | k1_26 | 0x003DC |  |  |  | not captured |  |  |  |
| 28 | k1_27 | 0x00402 |  |  |  | not captured |  |  |  |
| 29 | k1_28 | 0x00428 |  |  |  | not captured |  |  |  |
| 30 | k1_29 | 0x0044E |  |  |  | not captured |  |  |  |
| 31 | k1_30 | 0x00474 |  |  |  | not captured |  |  |  |
| 32 | k1_31 | 0x0049A |  |  |  | not captured |  |  |  |
| 33 | k2_00 | 0x004C0 |  |  |  | not captured |  |  |  |
| 34 | k2_01 | 0x004E6 |  |  |  | not captured |  |  |  |
| 35 | k2_02 | 0x0050C |  |  |  | not captured |  |  |  |
| 36 | k2_03 | 0x00532 |  |  |  | not captured |  |  |  |
| 37 | k2_04 | 0x00558 |  |  |  | not captured |  |  |  |
| 38 | k2_05 | 0x0057E |  |  |  | not captured |  |  |  |
| 39 | k2_06 | 0x005A4 |  |  |  | not captured |  |  |  |
| 40 | k2_07 | 0x005CA |  |  |  | not captured |  |  |  |
| 41 | k2_08 | 0x005F0 |  |  |  | not captured |  |  |  |
| 42 | k2_09 | 0x00616 |  |  |  | not captured |  |  |  |
| 43 | k2_10 | 0x0063C |  |  |  | not captured |  |  |  |
| 44 | k2_11 | 0x00662 |  |  |  | not captured |  |  |  |
| 45 | k2_12 | 0x00688 |  |  |  | not captured |  |  |  |
| 46 | k2_13 | 0x006AE |  |  |  | not captured |  |  |  |
| 47 | k2_14 | 0x006D4 |  |  |  | not captured |  |  |  |
| 48 | k2_15 | 0x006FA |  |  |  | not captured |  |  |  |
| 49 | k2_16 | 0x00720 |  |  |  | not captured |  |  |  |
| 50 | k2_17 | 0x00746 |  |  |  | not captured |  |  |  |
| 51 | k2_18 | 0x0076C |  |  |  | not captured |  |  |  |
| 52 | k2_19 | 0x00792 |  |  |  | not captured |  |  |  |
| 53 | k2_20 | 0x007B8 |  |  |  | not captured |  |  |  |
| 54 | k2_21 | 0x007DE |  |  |  | not captured |  |  |  |
| 55 | k2_22 | 0x00804 |  |  |  | not captured |  |  |  |
| 56 | k2_23 | 0x0082A |  |  |  | not captured |  |  |  |
| 57 | k2_24 | 0x00850 |  |  |  | not captured |  |  |  |
| 58 | k2_25 | 0x00876 |  |  |  | not captured |  |  |  |
| 59 | k2_26 | 0x0089C |  |  |  | not captured |  |  |  |
| 60 | k2_27 | 0x008C2 |  |  |  | not captured |  |  |  |
| 61 | k2_28 | 0x008E8 |  |  |  | not captured |  |  |  |
| 62 | k2_29 | 0x0090E |  |  |  | not captured |  |  |  |
| 63 | k2_30 | 0x00934 |  |  |  | not captured |  |  |  |
| 64 | k2_31 | 0x0095A |  |  |  | not captured |  |  |  |
| 65 | k3_00 | 0x00980 |  |  |  | not captured |  |  |  |
| 66 | k3_01 | 0x009A6 |  |  |  | not captured |  |  |  |
| 67 | k3_02 | 0x009CC |  |  |  | not captured |  |  |  |
| 68 | k3_03 | 0x009F2 |  |  |  | not captured |  |  |  |
| 69 | k3_04 | 0x00A18 |  |  |  | not captured |  |  |  |
| 70 | k3_05 | 0x00A3E |  |  |  | not captured |  |  |  |
| 71 | k3_06 | 0x00A64 |  |  |  | not captured |  |  |  |
| 72 | k3_07 | 0x00A8A |  |  |  | not captured |  |  |  |
| 73 | k3_08 | 0x00AB0 |  |  |  | not captured |  |  |  |
| 74 | k3_09 | 0x00AD6 |  |  |  | not captured |  |  |  |
| 75 | k3_10 | 0x00AFC |  |  |  | not captured |  |  |  |
| 76 | k3_11 | 0x00B22 |  |  |  | not captured |  |  |  |
| 77 | k3_12 | 0x00B48 |  |  |  | not captured |  |  |  |
| 78 | k3_13 | 0x00B6E |  |  |  | not captured |  |  |  |
| 79 | k3_14 | 0x00B94 |  |  |  | not captured |  |  |  |
| 80 | k3_15 | 0x00BBA |  |  |  | not captured |  |  |  |
| 81 | k4_00 | 0x00BE0 |  |  |  | not captured |  |  |  |
| 82 | k4_01 | 0x00C06 |  |  |  | not captured |  |  |  |
| 83 | k4_02 | 0x00C2C |  |  |  | not captured |  |  |  |
| 84 | k4_03 | 0x00C52 |  |  |  | not captured |  |  |  |
| 85 | k4_04 | 0x00C78 |  |  |  | not captured |  |  |  |
| 86 | k4_05 | 0x00C9E |  |  |  | not captured |  |  |  |
| 87 | k4_06 | 0x00CC4 |  |  |  | not captured |  |  |  |
| 88 | k4_07 | 0x00CEA |  |  |  | not captured |  |  |  |
| 89 | k4_08 | 0x00D10 |  |  |  | not captured |  |  |  |
| 90 | k4_09 | 0x00D36 |  |  |  | not captured |  |  |  |
| 91 | k4_10 | 0x00D5C |  |  |  | not captured |  |  |  |
| 92 | k4_11 | 0x00D82 |  |  |  | not captured |  |  |  |
| 93 | k4_12 | 0x00DA8 |  |  |  | not captured |  |  |  |
| 94 | k4_13 | 0x00DCE |  |  |  | not captured |  |  |  |
| 95 | k4_14 | 0x00DF4 |  |  |  | not captured |  |  |  |
| 96 | k4_15 | 0x00E1A |  |  |  | not captured |  |  |  |
| 97 | k5_00 | 0x00E40 |  |  |  | not captured |  |  |  |
| 98 | k5_01 | 0x00E66 |  |  |  | not captured |  |  |  |
| 99 | k5_02 | 0x00E8C |  |  |  | not captured |  |  |  |
| 100 | k5_03 | 0x00EB2 |  |  |  | not captured |  |  |  |
| 101 | k5_04 | 0x00ED8 |  |  |  | not captured |  |  |  |
| 102 | k5_05 | 0x00EFE |  |  |  | not captured |  |  |  |
| 103 | k5_06 | 0x00F24 |  |  |  | not captured |  |  |  |
| 104 | k5_07 | 0x00F4A |  |  |  | not captured |  |  |  |
| 105 | k5_08 | 0x00F70 |  |  |  | not captured |  |  |  |
| 106 | k5_09 | 0x00F96 |  |  |  | not captured |  |  |  |
| 107 | k5_10 | 0x00FBC |  |  |  | not captured |  |  |  |
| 108 | k5_11 | 0x00FE2 |  |  |  | not captured |  |  |  |
| 109 | k5_12 | 0x01008 |  |  |  | not captured |  |  |  |
| 110 | k5_13 | 0x0102E |  |  |  | not captured |  |  |  |
| 111 | k5_14 | 0x01054 |  |  |  | not captured |  |  |  |
| 112 | k5_15 | 0x0107A |  |  |  | not captured |  |  |  |
| 113 | k6_00 | 0x010A0 |  |  |  | not captured |  |  |  |
| 114 | k6_01 | 0x010C6 |  |  |  | not captured |  |  |  |
| 115 | k6_02 | 0x010EC |  |  |  | not captured |  |  |  |
| 116 | k6_03 | 0x01112 |  |  |  | not captured |  |  |  |
| 117 | k6_04 | 0x01138 |  |  |  | not captured |  |  |  |
| 118 | k6_05 | 0x0115E |  |  |  | not captured |  |  |  |
| 119 | k6_06 | 0x01184 |  |  |  | not captured |  |  |  |
| 120 | k6_07 | 0x011AA |  |  |  | not captured |  |  |  |
| 121 | k6_08 | 0x011D0 |  |  |  | not captured |  |  |  |
| 122 | k6_09 | 0x011F6 |  |  |  | not captured |  |  |  |
| 123 | k6_10 | 0x0121C |  |  |  | not captured |  |  |  |
| 124 | k6_11 | 0x01242 |  |  |  | not captured |  |  |  |
| 125 | k6_12 | 0x01268 |  |  |  | not captured |  |  |  |
| 126 | k6_13 | 0x0128E |  |  |  | not captured |  |  |  |
| 127 | k6_14 | 0x012B4 |  |  |  | not captured |  |  |  |
| 128 | k6_15 | 0x012DA |  |  |  | not captured |  |  |  |
| 129 | k7_00 | 0x01300 |  |  |  | not captured |  |  |  |
| 130 | k7_01 | 0x01326 |  |  |  | not captured |  |  |  |
| 131 | k7_02 | 0x0134C |  |  |  | not captured |  |  |  |
| 132 | k7_03 | 0x01372 |  |  |  | not captured |  |  |  |
| 133 | k7_04 | 0x01398 |  |  |  | not captured |  |  |  |
| 134 | k7_05 | 0x013BE |  |  |  | not captured |  |  |  |
| 135 | k7_06 | 0x013E4 |  |  |  | not captured |  |  |  |
| 136 | k7_07 | 0x0140A |  |  |  | not captured |  |  |  |
| 137 | k7_08 | 0x01430 |  |  |  | not captured |  |  |  |
| 138 | k7_09 | 0x01456 |  |  |  | not captured |  |  |  |
| 139 | k7_10 | 0x0147C |  |  |  | not captured |  |  |  |
| 140 | k7_11 | 0x014A2 |  |  |  | not captured |  |  |  |
| 141 | k7_12 | 0x014C8 |  |  |  | not captured |  |  |  |
| 142 | k7_13 | 0x014EE |  |  |  | not captured |  |  |  |
| 143 | k7_14 | 0x01514 |  |  |  | not captured |  |  |  |
| 144 | k7_15 | 0x0153A |  |  |  | not captured |  |  |  |
| 145 | k8_00 | 0x01560 |  |  |  | not captured |  |  |  |
| 146 | k8_01 | 0x01586 |  |  |  | not captured |  |  |  |
| 147 | k8_02 | 0x015AC |  |  |  | not captured |  |  |  |
| 148 | k8_03 | 0x015D2 |  |  |  | not captured |  |  |  |
| 149 | k8_04 | 0x015F8 |  |  |  | not captured |  |  |  |
| 150 | k8_05 | 0x0161E |  |  |  | not captured |  |  |  |
| 151 | k8_06 | 0x01644 |  |  |  | not captured |  |  |  |
| 152 | k8_07 | 0x0166A |  |  |  | not captured |  |  |  |
| 153 | k9_00 | 0x01690 |  |  |  | not captured |  |  |  |
| 154 | k9_01 | 0x016B6 |  |  |  | not captured |  |  |  |
| 155 | k9_02 | 0x016DC |  |  |  | not captured |  |  |  |
| 156 | k9_03 | 0x01702 |  |  |  | not captured |  |  |  |
| 157 | k9_04 | 0x01728 |  |  |  | not captured |  |  |  |
| 158 | k9_05 | 0x0174E |  |  |  | not captured |  |  |  |
| 159 | k9_06 | 0x01774 |  |  |  | not captured |  |  |  |
| 160 | k9_07 | 0x0179A |  |  |  | not captured |  |  |  |
| 161 | k10_00 | 0x017C0 |  |  |  | not captured |  |  |  |
| 162 | k10_01 | 0x017E6 |  |  |  | not captured |  |  |  |
| 163 | k10_02 | 0x0180C |  |  |  | not captured |  |  |  |
| 164 | k10_03 | 0x01832 |  |  |  | not captured |  |  |  |
| 165 | k10_04 | 0x01858 |  |  |  | not captured |  |  |  |
| 166 | k10_05 | 0x0187E |  |  |  | not captured |  |  |  |
| 167 | k10_06 | 0x018A4 |  |  |  | not captured |  |  |  |
| 168 | k10_07 | 0x018CA |  |  |  | not captured |  |  |  |
| 169 | uk1_00 | 0x018F0 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](169_uk1_00/vsm01/samples.wav) · [listen](169_uk1_00/vsm01/listen.wav) · [frames](169_uk1_00/vsm01/frames.csv) |
| 170 | uk1_01 | 0x01907 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](170_uk1_01/vsm01/samples.wav) · [listen](170_uk1_01/vsm01/listen.wav) · [frames](170_uk1_01/vsm01/frames.csv) |
| 171 | uk1_02 | 0x0191E | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](171_uk1_02/vsm01/samples.wav) · [listen](171_uk1_02/vsm01/listen.wav) · [frames](171_uk1_02/vsm01/frames.csv) |
| 172 | uk1_03 | 0x01935 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](172_uk1_03/vsm01/samples.wav) · [listen](172_uk1_03/vsm01/listen.wav) · [frames](172_uk1_03/vsm01/frames.csv) |
| 173 | uk1_04 | 0x0194C | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](173_uk1_04/vsm01/samples.wav) · [listen](173_uk1_04/vsm01/listen.wav) · [frames](173_uk1_04/vsm01/frames.csv) |
| 174 | uk1_05 | 0x01963 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](174_uk1_05/vsm01/samples.wav) · [listen](174_uk1_05/vsm01/listen.wav) · [frames](174_uk1_05/vsm01/frames.csv) |
| 175 | uk1_06 | 0x0197A | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 0 of 2 match, 2 samples undecided; external: 2 of 2 match | [samples](175_uk1_06/vsm01/samples.wav) · [listen](175_uk1_06/vsm01/listen.wav) · [frames](175_uk1_06/vsm01/frames.csv) |
| 176 | uk1_07 | 0x01991 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](176_uk1_07/vsm01/samples.wav) · [listen](176_uk1_07/vsm01/listen.wav) · [frames](176_uk1_07/vsm01/frames.csv) |
| 177 | uk1_08 | 0x019A8 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](177_uk1_08/vsm01/samples.wav) · [listen](177_uk1_08/vsm01/listen.wav) · [frames](177_uk1_08/vsm01/frames.csv) |
| 178 | uk1_09 | 0x019BF | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](178_uk1_09/vsm01/samples.wav) · [listen](178_uk1_09/vsm01/listen.wav) · [frames](178_uk1_09/vsm01/frames.csv) |
| 179 | uk1_10 | 0x019D6 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](179_uk1_10/vsm01/samples.wav) · [listen](179_uk1_10/vsm01/listen.wav) · [frames](179_uk1_10/vsm01/frames.csv) |
| 180 | uk1_11 | 0x019ED | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](180_uk1_11/vsm01/samples.wav) · [listen](180_uk1_11/vsm01/listen.wav) · [frames](180_uk1_11/vsm01/frames.csv) |
| 181 | uk1_12 | 0x01A04 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](181_uk1_12/vsm01/samples.wav) · [listen](181_uk1_12/vsm01/listen.wav) · [frames](181_uk1_12/vsm01/frames.csv) |
| 182 | uk1_13 | 0x01A1B | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](182_uk1_13/vsm01/samples.wav) · [listen](182_uk1_13/vsm01/listen.wav) · [frames](182_uk1_13/vsm01/frames.csv) |
| 183 | uk1_14 | 0x01A32 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](183_uk1_14/vsm01/samples.wav) · [listen](183_uk1_14/vsm01/listen.wav) · [frames](183_uk1_14/vsm01/frames.csv) |
| 184 | uk1_15 | 0x01A49 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](184_uk1_15/vsm01/samples.wav) · [listen](184_uk1_15/vsm01/listen.wav) · [frames](184_uk1_15/vsm01/frames.csv) |
| 185 | uk1_16 | 0x01A60 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](185_uk1_16/vsm01/samples.wav) · [listen](185_uk1_16/vsm01/listen.wav) · [frames](185_uk1_16/vsm01/frames.csv) |
| 186 | uk1_17 | 0x01A77 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](186_uk1_17/vsm01/samples.wav) · [listen](186_uk1_17/vsm01/listen.wav) · [frames](186_uk1_17/vsm01/frames.csv) |
| 187 | uk1_18 | 0x01A8E | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](187_uk1_18/vsm01/samples.wav) · [listen](187_uk1_18/vsm01/listen.wav) · [frames](187_uk1_18/vsm01/frames.csv) |
| 188 | uk1_19 | 0x01AA5 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](188_uk1_19/vsm01/samples.wav) · [listen](188_uk1_19/vsm01/listen.wav) · [frames](188_uk1_19/vsm01/frames.csv) |
| 189 | uk1_20 | 0x01ABC | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](189_uk1_20/vsm01/samples.wav) · [listen](189_uk1_20/vsm01/listen.wav) · [frames](189_uk1_20/vsm01/frames.csv) |
| 190 | uk1_21 | 0x01AD3 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](190_uk1_21/vsm01/samples.wav) · [listen](190_uk1_21/vsm01/listen.wav) · [frames](190_uk1_21/vsm01/frames.csv) |
| 191 | uk1_22 | 0x01AEA | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](191_uk1_22/vsm01/samples.wav) · [listen](191_uk1_22/vsm01/listen.wav) · [frames](191_uk1_22/vsm01/frames.csv) |
| 192 | uk1_23 | 0x01B01 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](192_uk1_23/vsm01/samples.wav) · [listen](192_uk1_23/vsm01/listen.wav) · [frames](192_uk1_23/vsm01/frames.csv) |
| 193 | uk1_24 | 0x01B18 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](193_uk1_24/vsm01/samples.wav) · [listen](193_uk1_24/vsm01/listen.wav) · [frames](193_uk1_24/vsm01/frames.csv) |
| 194 | uk1_25 | 0x01B2F | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](194_uk1_25/vsm01/samples.wav) · [listen](194_uk1_25/vsm01/listen.wav) · [frames](194_uk1_25/vsm01/frames.csv) |
| 195 | uk1_26 | 0x01B46 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](195_uk1_26/vsm01/samples.wav) · [listen](195_uk1_26/vsm01/listen.wav) · [frames](195_uk1_26/vsm01/frames.csv) |
| 196 | uk1_27 | 0x01B5D | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](196_uk1_27/vsm01/samples.wav) · [listen](196_uk1_27/vsm01/listen.wav) · [frames](196_uk1_27/vsm01/frames.csv) |
| 197 | uk1_28 | 0x01B74 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](197_uk1_28/vsm01/samples.wav) · [listen](197_uk1_28/vsm01/listen.wav) · [frames](197_uk1_28/vsm01/frames.csv) |
| 198 | uk1_29 | 0x01B8B | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](198_uk1_29/vsm01/samples.wav) · [listen](198_uk1_29/vsm01/listen.wav) · [frames](198_uk1_29/vsm01/frames.csv) |
| 199 | uk1_30 | 0x01BA2 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](199_uk1_30/vsm01/samples.wav) · [listen](199_uk1_30/vsm01/listen.wav) · [frames](199_uk1_30/vsm01/frames.csv) |
| 200 | uk1_31 | 0x01BB9 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](200_uk1_31/vsm01/samples.wav) · [listen](200_uk1_31/vsm01/listen.wav) · [frames](200_uk1_31/vsm01/frames.csv) |
| 201 | uk2_00 | 0x01BD0 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](201_uk2_00/vsm01/samples.wav) · [listen](201_uk2_00/vsm01/listen.wav) · [frames](201_uk2_00/vsm01/frames.csv) |
| 202 | uk2_01 | 0x01BE7 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](202_uk2_01/vsm01/samples.wav) · [listen](202_uk2_01/vsm01/listen.wav) · [frames](202_uk2_01/vsm01/frames.csv) |
| 203 | uk2_02 | 0x01BFE | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](203_uk2_02/vsm01/samples.wav) · [listen](203_uk2_02/vsm01/listen.wav) · [frames](203_uk2_02/vsm01/frames.csv) |
| 204 | uk2_03 | 0x01C15 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](204_uk2_03/vsm01/samples.wav) · [listen](204_uk2_03/vsm01/listen.wav) · [frames](204_uk2_03/vsm01/frames.csv) |
| 205 | uk2_04 | 0x01C2C | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](205_uk2_04/vsm01/samples.wav) · [listen](205_uk2_04/vsm01/listen.wav) · [frames](205_uk2_04/vsm01/frames.csv) |
| 206 | uk2_05 | 0x01C43 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](206_uk2_05/vsm01/samples.wav) · [listen](206_uk2_05/vsm01/listen.wav) · [frames](206_uk2_05/vsm01/frames.csv) |
| 207 | uk2_06 | 0x01C5A | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](207_uk2_06/vsm01/samples.wav) · [listen](207_uk2_06/vsm01/listen.wav) · [frames](207_uk2_06/vsm01/frames.csv) |
| 208 | uk2_07 | 0x01C71 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](208_uk2_07/vsm01/samples.wav) · [listen](208_uk2_07/vsm01/listen.wav) · [frames](208_uk2_07/vsm01/frames.csv) |
| 209 | uk2_08 | 0x01C88 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](209_uk2_08/vsm01/samples.wav) · [listen](209_uk2_08/vsm01/listen.wav) · [frames](209_uk2_08/vsm01/frames.csv) |
| 210 | uk2_09 | 0x01C9F | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](210_uk2_09/vsm01/samples.wav) · [listen](210_uk2_09/vsm01/listen.wav) · [frames](210_uk2_09/vsm01/frames.csv) |
| 211 | uk2_10 | 0x01CB6 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 1 of 2 match, 1 samples undecided | [samples](211_uk2_10/vsm01/samples.wav) · [listen](211_uk2_10/vsm01/listen.wav) · [frames](211_uk2_10/vsm01/frames.csv) |
| 212 | uk2_11 | 0x01CCD | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](212_uk2_11/vsm01/samples.wav) · [listen](212_uk2_11/vsm01/listen.wav) · [frames](212_uk2_11/vsm01/frames.csv) |
| 213 | uk2_12 | 0x01CE4 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](213_uk2_12/vsm01/samples.wav) · [listen](213_uk2_12/vsm01/listen.wav) · [frames](213_uk2_12/vsm01/frames.csv) |
| 214 | uk2_13 | 0x01CFB | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](214_uk2_13/vsm01/samples.wav) · [listen](214_uk2_13/vsm01/listen.wav) · [frames](214_uk2_13/vsm01/frames.csv) |
| 215 | uk2_14 | 0x01D12 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](215_uk2_14/vsm01/samples.wav) · [listen](215_uk2_14/vsm01/listen.wav) · [frames](215_uk2_14/vsm01/frames.csv) |
| 216 | uk2_15 | 0x01D29 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](216_uk2_15/vsm01/samples.wav) · [listen](216_uk2_15/vsm01/listen.wav) · [frames](216_uk2_15/vsm01/frames.csv) |
| 217 | uk2_16 | 0x01D40 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](217_uk2_16/vsm01/samples.wav) · [listen](217_uk2_16/vsm01/listen.wav) · [frames](217_uk2_16/vsm01/frames.csv) |
| 218 | uk2_17 | 0x01D57 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](218_uk2_17/vsm01/samples.wav) · [listen](218_uk2_17/vsm01/listen.wav) · [frames](218_uk2_17/vsm01/frames.csv) |
| 219 | uk2_18 | 0x01D6E | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](219_uk2_18/vsm01/samples.wav) · [listen](219_uk2_18/vsm01/listen.wav) · [frames](219_uk2_18/vsm01/frames.csv) |
| 220 | uk2_19 | 0x01D85 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](220_uk2_19/vsm01/samples.wav) · [listen](220_uk2_19/vsm01/listen.wav) · [frames](220_uk2_19/vsm01/frames.csv) |
| 221 | uk2_20 | 0x01D9C | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](221_uk2_20/vsm01/samples.wav) · [listen](221_uk2_20/vsm01/listen.wav) · [frames](221_uk2_20/vsm01/frames.csv) |
| 222 | uk2_21 | 0x01DB3 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](222_uk2_21/vsm01/samples.wav) · [listen](222_uk2_21/vsm01/listen.wav) · [frames](222_uk2_21/vsm01/frames.csv) |
| 223 | uk2_22 | 0x01DCA | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 1 of 2 match, 2 samples undecided | [samples](223_uk2_22/vsm01/samples.wav) · [listen](223_uk2_22/vsm01/listen.wav) · [frames](223_uk2_22/vsm01/frames.csv) |
| 224 | uk2_23 | 0x01DE1 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 1 of 2 match, 1 samples undecided | [samples](224_uk2_23/vsm01/samples.wav) · [listen](224_uk2_23/vsm01/listen.wav) · [frames](224_uk2_23/vsm01/frames.csv) |
| 225 | uk2_24 | 0x01DF8 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](225_uk2_24/vsm01/samples.wav) · [listen](225_uk2_24/vsm01/listen.wav) · [frames](225_uk2_24/vsm01/frames.csv) |
| 226 | uk2_25 | 0x01E0F | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](226_uk2_25/vsm01/samples.wav) · [listen](226_uk2_25/vsm01/listen.wav) · [frames](226_uk2_25/vsm01/frames.csv) |
| 227 | uk2_26 | 0x01E26 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](227_uk2_26/vsm01/samples.wav) · [listen](227_uk2_26/vsm01/listen.wav) · [frames](227_uk2_26/vsm01/frames.csv) |
| 228 | uk2_27 | 0x01E3D | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](228_uk2_27/vsm01/samples.wav) · [listen](228_uk2_27/vsm01/listen.wav) · [frames](228_uk2_27/vsm01/frames.csv) |
| 229 | uk2_28 | 0x01E54 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](229_uk2_28/vsm01/samples.wav) · [listen](229_uk2_28/vsm01/listen.wav) · [frames](229_uk2_28/vsm01/frames.csv) |
| 230 | uk2_29 | 0x01E6B | 7 | 178 | 187 | 3 external, 2 vsm | 4 pass, 1 FAIL: start_slot | vsm: 2 of 2 match; external: 2 of 2 match | [samples](230_uk2_29/vsm01/samples.wav) · [listen](230_uk2_29/vsm01/listen.wav) · [frames](230_uk2_29/vsm01/frames.csv) |
| 231 | uk2_30 | 0x01E82 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](231_uk2_30/vsm01/samples.wav) · [listen](231_uk2_30/vsm01/listen.wav) · [frames](231_uk2_30/vsm01/frames.csv) |
| 232 | uk2_31 | 0x01E99 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](232_uk2_31/vsm01/samples.wav) · [listen](232_uk2_31/vsm01/listen.wav) · [frames](232_uk2_31/vsm01/frames.csv) |
| 233 | uk3_00 | 0x01EB0 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](233_uk3_00/vsm01/samples.wav) · [listen](233_uk3_00/vsm01/listen.wav) · [frames](233_uk3_00/vsm01/frames.csv) |
| 234 | uk3_01 | 0x01EC7 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](234_uk3_01/vsm01/samples.wav) · [listen](234_uk3_01/vsm01/listen.wav) · [frames](234_uk3_01/vsm01/frames.csv) |
| 235 | uk3_02 | 0x01EDE | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](235_uk3_02/vsm01/samples.wav) · [listen](235_uk3_02/vsm01/listen.wav) · [frames](235_uk3_02/vsm01/frames.csv) |
| 236 | uk3_03 | 0x01EF5 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](236_uk3_03/vsm01/samples.wav) · [listen](236_uk3_03/vsm01/listen.wav) · [frames](236_uk3_03/vsm01/frames.csv) |
| 237 | uk3_04 | 0x01F0C | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](237_uk3_04/vsm01/samples.wav) · [listen](237_uk3_04/vsm01/listen.wav) · [frames](237_uk3_04/vsm01/frames.csv) |
| 238 | uk3_05 | 0x01F23 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](238_uk3_05/vsm01/samples.wav) · [listen](238_uk3_05/vsm01/listen.wav) · [frames](238_uk3_05/vsm01/frames.csv) |
| 239 | uk3_06 | 0x01F3A | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](239_uk3_06/vsm01/samples.wav) · [listen](239_uk3_06/vsm01/listen.wav) · [frames](239_uk3_06/vsm01/frames.csv) |
| 240 | uk3_07 | 0x01F51 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](240_uk3_07/vsm01/samples.wav) · [listen](240_uk3_07/vsm01/listen.wav) · [frames](240_uk3_07/vsm01/frames.csv) |
| 241 | uk3_08 | 0x01F68 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](241_uk3_08/vsm01/samples.wav) · [listen](241_uk3_08/vsm01/listen.wav) · [frames](241_uk3_08/vsm01/frames.csv) |
| 242 | uk3_09 | 0x01F7F | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](242_uk3_09/vsm01/samples.wav) · [listen](242_uk3_09/vsm01/listen.wav) · [frames](242_uk3_09/vsm01/frames.csv) |
| 243 | uk3_10 | 0x01F96 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](243_uk3_10/vsm01/samples.wav) · [listen](243_uk3_10/vsm01/listen.wav) · [frames](243_uk3_10/vsm01/frames.csv) |
| 244 | uk3_11 | 0x01FAD | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](244_uk3_11/vsm01/samples.wav) · [listen](244_uk3_11/vsm01/listen.wav) · [frames](244_uk3_11/vsm01/frames.csv) |
| 245 | uk3_12 | 0x01FC4 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](245_uk3_12/vsm01/samples.wav) · [listen](245_uk3_12/vsm01/listen.wav) · [frames](245_uk3_12/vsm01/frames.csv) |
| 246 | uk3_13 | 0x01FDB | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](246_uk3_13/vsm01/samples.wav) · [listen](246_uk3_13/vsm01/listen.wav) · [frames](246_uk3_13/vsm01/frames.csv) |
| 247 | uk3_14 | 0x01FF2 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](247_uk3_14/vsm01/samples.wav) · [listen](247_uk3_14/vsm01/listen.wav) · [frames](247_uk3_14/vsm01/frames.csv) |
| 248 | uk3_15 | 0x02009 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](248_uk3_15/vsm01/samples.wav) · [listen](248_uk3_15/vsm01/listen.wav) · [frames](248_uk3_15/vsm01/frames.csv) |
| 249 | uk4_00 | 0x02020 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](249_uk4_00/vsm01/samples.wav) · [listen](249_uk4_00/vsm01/listen.wav) · [frames](249_uk4_00/vsm01/frames.csv) |
| 250 | uk4_01 | 0x02037 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](250_uk4_01/vsm01/samples.wav) · [listen](250_uk4_01/vsm01/listen.wav) · [frames](250_uk4_01/vsm01/frames.csv) |
| 251 | uk4_02 | 0x0204E | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](251_uk4_02/vsm01/samples.wav) · [listen](251_uk4_02/vsm01/listen.wav) · [frames](251_uk4_02/vsm01/frames.csv) |
| 252 | uk4_03 | 0x02065 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](252_uk4_03/vsm01/samples.wav) · [listen](252_uk4_03/vsm01/listen.wav) · [frames](252_uk4_03/vsm01/frames.csv) |
| 253 | uk4_04 | 0x0207C | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](253_uk4_04/vsm01/samples.wav) · [listen](253_uk4_04/vsm01/listen.wav) · [frames](253_uk4_04/vsm01/frames.csv) |
| 254 | uk4_05 | 0x02093 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](254_uk4_05/vsm01/samples.wav) · [listen](254_uk4_05/vsm01/listen.wav) · [frames](254_uk4_05/vsm01/frames.csv) |
| 255 | uk4_06 | 0x020AA | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](255_uk4_06/vsm01/samples.wav) · [listen](255_uk4_06/vsm01/listen.wav) · [frames](255_uk4_06/vsm01/frames.csv) |
| 256 | uk4_07 | 0x020C1 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](256_uk4_07/vsm01/samples.wav) · [listen](256_uk4_07/vsm01/listen.wav) · [frames](256_uk4_07/vsm01/frames.csv) |
| 257 | uk4_08 | 0x020D8 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](257_uk4_08/vsm01/samples.wav) · [listen](257_uk4_08/vsm01/listen.wav) · [frames](257_uk4_08/vsm01/frames.csv) |
| 258 | uk4_09 | 0x020EF | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](258_uk4_09/vsm01/samples.wav) · [listen](258_uk4_09/vsm01/listen.wav) · [frames](258_uk4_09/vsm01/frames.csv) |
| 259 | uk4_10 | 0x02106 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](259_uk4_10/vsm01/samples.wav) · [listen](259_uk4_10/vsm01/listen.wav) · [frames](259_uk4_10/vsm01/frames.csv) |
| 260 | uk4_11 | 0x0211D | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](260_uk4_11/vsm01/samples.wav) · [listen](260_uk4_11/vsm01/listen.wav) · [frames](260_uk4_11/vsm01/frames.csv) |
| 261 | uk4_12 | 0x02134 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](261_uk4_12/vsm01/samples.wav) · [listen](261_uk4_12/vsm01/listen.wav) · [frames](261_uk4_12/vsm01/frames.csv) |
| 262 | uk4_13 | 0x0214B | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](262_uk4_13/vsm01/samples.wav) · [listen](262_uk4_13/vsm01/listen.wav) · [frames](262_uk4_13/vsm01/frames.csv) |
| 263 | uk4_14 | 0x02162 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](263_uk4_14/vsm01/samples.wav) · [listen](263_uk4_14/vsm01/listen.wav) · [frames](263_uk4_14/vsm01/frames.csv) |
| 264 | uk4_15 | 0x02179 | 7 | 178 | 187 | 2 external, 2 vsm | pass | vsm: 2 of 2 match; external: 2 of 2 match | [samples](264_uk4_15/vsm01/samples.wav) · [listen](264_uk4_15/vsm01/listen.wav) · [frames](264_uk4_15/vsm01/frames.csv) |
