# TI VM61004 (US male voice, military vocabulary)

- **Image:** 16384 bytes, SHA-256 `3258d15676a2705f7bedee5f87596043cc979b0cf6beb4dcdeecabe26b4f71c5`, bank 2. Target chip: TMS5220.
- **Provenance:** TMS6100 Multi-PHROM Emulator (eRadiofan), ROMdata_VM61004.c, padded with zeros to 16 KB; the result is MAME's dump. The word names are from its word list; the addresses are checked by following each word's stop code.
- **Captured:** 1 of 148 words. Each take directory follows `docs-tech/speech_capture_plan.md` §6; its `capture.json` holds the metadata and checks. [`takes.csv`](takes.csv) lists every take, with the SHA-256 of its output (`talk_sha256`: the I/O codes from sample 0 to the end of talk, as 16-bit little-endian integers) and of its raw files.
- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent (Speak External), up to the stop code. ms is the talk time of the first take, from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with the first, sample for sample, after aligning them. For takes made with the standard procedure, it gives how many match the per-sample majority of all of them exactly, and the samples where no majority exists (`compare.json`).

| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Alpha | 0x08000 |  |  |  | not captured |  |  |  |
| 2 | Bravo | 0x08064 |  |  |  | not captured |  |  |  |
| 3 | Charlie | 0x080BC |  |  |  | not captured |  |  |  |
| 4 | Delta | 0x0811E |  |  |  | not captured |  |  |  |
| 5 | Echo | 0x08171 |  |  |  | not captured |  |  |  |
| 6 | Foxtrot | 0x081B3 |  |  |  | not captured |  |  |  |
| 7 | Golf | 0x08248 |  |  |  | not captured |  |  |  |
| 8 | Hotel | 0x08280 |  |  |  | not captured |  |  |  |
| 9 | India | 0x082F6 |  |  |  | not captured |  |  |  |
| 10 | Juliet | 0x08356 |  |  |  | not captured |  |  |  |
| 11 | Kilo | 0x083BC |  |  |  | not captured |  |  |  |
| 12 | Lima | 0x08409 |  |  |  | not captured |  |  |  |
| 13 | Mike | 0x0844C |  |  |  | not captured |  |  |  |
| 14 | November | 0x0848E |  |  |  | not captured |  |  |  |
| 15 | Oscar | 0x0852A |  |  |  | not captured |  |  |  |
| 16 | Papa | 0x08587 |  |  |  | not captured |  |  |  |
| 17 | Quebec | 0x085C9 |  |  |  | not captured |  |  |  |
| 18 | Romeo | 0x08610 |  |  |  | not captured |  |  |  |
| 19 | Sierra | 0x08670 |  |  |  | not captured |  |  |  |
| 20 | Tango | 0x08706 |  |  |  | not captured |  |  |  |
| 21 | Uniform | 0x0877D |  |  |  | not captured |  |  |  |
| 22 | Victor | 0x087FB |  |  |  | not captured |  |  |  |
| 23 | Whisky | 0x0886E |  |  |  | not captured |  |  |  |
| 24 | Xray | 0x088C7 |  |  |  | not captured |  |  |  |
| 25 | Zulu | 0x0893B |  |  |  | not captured |  |  |  |
| 26 | Affirmative | 0x08984 |  |  |  | not captured |  |  |  |
| 27 | Negative | 0x08A38 |  |  |  | not captured |  |  |  |
| 28 | Mayday | 0x08ACF |  |  |  | not captured |  |  |  |
| 29 | Warning | 0x08B74 |  |  |  | not captured |  |  |  |
| 30 | Icing | 0x08C07 |  |  |  | not captured |  |  |  |
| 31 | Danger | 0x08C78 |  |  |  | not captured |  |  |  |
| 32 | Emergency | 0x08D0B |  |  |  | not captured |  |  |  |
| 33 | Roger | 0x08DD2 |  |  |  | not captured |  |  |  |
| 34 | Hertz | 0x08E50 |  |  |  | not captured |  |  |  |
| 35 | (Pause) | 0x08E8D |  |  |  | not captured |  |  |  |
| 36 | Security | 0x08E96 |  |  |  | not captured |  |  |  |
| 37 | Target | 0x08F13 |  |  |  | not captured |  |  |  |
| 38 | Vectors | 0x08F6E |  |  |  | not captured |  |  |  |
| 39 | Light | 0x08FE6 |  |  |  | not captured |  |  |  |
| 40 | Front | 0x0903D |  |  |  | not captured |  |  |  |
| 41 | Way | 0x09098 |  |  |  | not captured |  |  |  |
| 42 | Glide | 0x090DA |  |  |  | not captured |  |  |  |
| 43 | Open | 0x0915E |  |  |  | not captured |  |  |  |
| 44 | Lights | 0x091C2 |  |  |  | not captured |  |  |  |
| 45 | On | 0x09221 |  |  |  | not captured |  |  |  |
| 46 | Gun dish | 0x09287 |  |  |  | not captured |  |  |  |
| 47 | ArdNav | 0x0930A |  |  |  | not captured |  |  |  |
| 48 | Select | 0x093B9 |  |  |  | not captured |  |  |  |
| 49 | Filed | 0x09420 |  |  |  | not captured |  |  |  |
| 50 | MIG | 0x094A5 |  |  |  | not captured |  |  |  |
| 51 | Alert | 0x09511 |  |  |  | not captured |  |  |  |
| 52 | Zone | 0x09579 |  |  |  | not captured |  |  |  |
| 53 | Terminal | 0x095FD |  |  |  | not captured |  |  |  |
| 54 | Cancel | 0x0967E |  |  |  | not captured |  |  |  |
| 55 | Radios | 0x096EB |  |  |  | not captured |  |  |  |
| 56 | Speed | 0x0978E |  |  |  | not captured |  |  |  |
| 57 | Knots | 0x09800 |  |  |  | not captured |  |  |  |
| 58 | Expect | 0x09868 |  |  |  | not captured |  |  |  |
| 59 | Action | 0x098D6 |  |  |  | not captured |  |  |  |
| 60 | Radio | 0x0993C |  |  |  | not captured |  |  |  |
| 61 | Power | 0x099AA |  |  |  | not captured |  |  |  |
| 62 | Path | 0x09A07 |  |  |  | not captured |  |  |  |
| 63 | Information | 0x09A5F |  |  |  | not captured |  |  |  |
| 64 | True | 0x09B09 |  |  |  | not captured |  |  |  |
| 65 | Pressure | 0x09B52 |  |  |  | not captured |  |  |  |
| 66 | Check | 0x09BBF |  |  |  | not captured |  |  |  |
| 67 | Decrease | 0x09BF6 |  |  |  | not captured |  |  |  |
| 68 | Advise | 0x09C63 |  |  |  | not captured |  |  |  |
| 69 | You | 0x09CED |  |  |  | not captured |  |  |  |
| 70 | Half | 0x09D27 |  |  |  | not captured |  |  |  |
| 71 | Error | 0x09D7E | 17 | 726 | 437 | 51 vsm | 50 pass, 1 FAIL: start_slot | vsm: 0 of 50 match, 573 samples undecided | [samples](71_error/vsm01/samples.wav) · [listen](71_error/vsm01/listen.wav) · [frames](71_error/vsm01/frames.csv) |
| 72 | Call | 0x09DDA |  |  |  | not captured |  |  |  |
| 73 | Wrong | 0x09E31 |  |  |  | not captured |  |  |  |
| 74 | No | 0x09E91 |  |  |  | not captured |  |  |  |
| 75 | Immediately | 0x09EEA |  |  |  | not captured |  |  |  |
| 76 | Final | 0x09FA3 |  |  |  | not captured |  |  |  |
| 77 | Please | 0x0A00F |  |  |  | not captured |  |  |  |
| 78 | Start | 0x0A05A |  |  |  | not captured |  |  |  |
| 79 | Course | 0x0A0B6 |  |  |  | not captured |  |  |  |
| 80 | RADAR | 0x0A102 |  |  |  | not captured |  |  |  |
| 81 | Plan | 0x0A181 |  |  |  | not captured |  |  |  |
| 82 | Target | 0x0A1EE |  |  |  | not captured |  |  |  |
| 83 | Windows | 0x0A25B |  |  |  | not captured |  |  |  |
| 84 | Watch | 0x0A2F3 |  |  |  | not captured |  |  |  |
| 85 | Use (s) | 0x0A336 |  |  |  | not captured |  |  |  |
| 86 | Turn | 0x0A381 |  |  |  | not captured |  |  |  |
| 87 | Traffic | 0x0A3CE |  |  |  | not captured |  |  |  |
| 88 | Tech | 0x0A435 |  |  |  | not captured |  |  |  |
| 89 | Slow | 0x0A48A |  |  |  | not captured |  |  |  |
| 90 | Release | 0x0A4F9 |  |  |  | not captured |  |  |  |
| 91 | Key | 0x0A56D |  |  |  | not captured |  |  |  |
| 92 | Ignition | 0x0A5A8 |  |  |  | not captured |  |  |  |
| 93 | Degree | 0x0A639 |  |  |  | not captured |  |  |  |
| 94 | Grain | 0x0A6A4 |  |  |  | not captured |  |  |  |
| 95 | Repair | 0x0A6F9 |  |  |  | not captured |  |  |  |
| 96 | Cancel | 0x0A77C |  |  |  | not captured |  |  |  |
| 97 | Verify | 0x0A806 |  |  |  | not captured |  |  |  |
| 98 | Ready | 0x0A8A0 |  |  |  | not captured |  |  |  |
| 99 | Use (z) | 0x0A8FA |  |  |  | not captured |  |  |  |
| 100 | Out | 0x0A976 |  |  |  | not captured |  |  |  |
| 101 | Other | 0x0A9BF |  |  |  | not captured |  |  |  |
| 102 | Oil | 0x0AA25 |  |  |  | not captured |  |  |  |
| 103 | Off | 0x0AA78 |  |  |  | not captured |  |  |  |
| 104 | Mirror | 0x0AAC6 |  |  |  | not captured |  |  |  |
| 105 | Rate | 0x0AB26 |  |  |  | not captured |  |  |  |
| 106 | Mixture | 0x0AB93 |  |  |  | not captured |  |  |  |
| 107 | Much | 0x0AC16 |  |  |  | not captured |  |  |  |
| 108 | Rate | 0x0AC6E |  |  |  | not captured |  |  |  |
| 109 | Is | 0x0ACD0 |  |  |  | not captured |  |  |  |
| 110 | Caution | 0x0AD28 |  |  |  | not captured |  |  |  |
| 111 | Below | 0x0AD97 |  |  |  | not captured |  |  |  |
| 112 | Cylinder | 0x0AE13 |  |  |  | not captured |  |  |  |
| 113 | Contact | 0x0AE90 |  |  |  | not captured |  |  |  |
| 114 | And | 0x0AF0A |  |  |  | not captured |  |  |  |
| 115 | Fuel | 0x0AF51 |  |  |  | not captured |  |  |  |
| 116 | For | 0x0AFA8 |  |  |  | not captured |  |  |  |
| 117 | Sequence | 0x0AFE2 |  |  |  | not captured |  |  |  |
| 118 | Side | 0x0B068 |  |  |  | not captured |  |  |  |
| 119 | Slow | 0x0B0C3 |  |  |  | not captured |  |  |  |
| 120 | To | 0x0B119 |  |  |  | not captured |  |  |  |
| 121 | Rich | 0x0B148 |  |  |  | not captured |  |  |  |
| 122 | Pumps | 0x0B1B6 |  |  |  | not captured |  |  |  |
| 123 | Level | 0x0B20C |  |  |  | not captured |  |  |  |
| 124 | Lower | 0x0B273 |  |  |  | not captured |  |  |  |
| 125 | Rear | 0x0B2E5 |  |  |  | not captured |  |  |  |
| 126 | Hold | 0x0B364 |  |  |  | not captured |  |  |  |
| 127 | Long | 0x0B3C9 |  |  |  | not captured |  |  |  |
| 128 | Level | 0x0B435 |  |  |  | not captured |  |  |  |
| 129 | Left | 0x0B4AE |  |  |  | not captured |  |  |  |
| 130 | Right | 0x0B50F |  |  |  | not captured |  |  |  |
| 131 | Open | 0x0B56F |  |  |  | not captured |  |  |  |
| 132 | Stray | 0x0B5C9 |  |  |  | not captured |  |  |  |
| 133 | Closed | 0x0B659 |  |  |  | not captured |  |  |  |
| 134 | Evacuate | 0x0B6E2 |  |  |  | not captured |  |  |  |
| 135 | Failure | 0x0B79A |  |  |  | not captured |  |  |  |
| 136 | Service | 0x0B807 |  |  |  | not captured |  |  |  |
| 137 | Abort | 0x0B881 |  |  |  | not captured |  |  |  |
| 138 | Identify | 0x0B902 |  |  |  | not captured |  |  |  |
| 139 | Too low | 0x0B999 |  |  |  | not captured |  |  |  |
| 140 | Centre | 0x0BA2F |  |  |  | not captured |  |  |  |
| 141 | Area | 0x0BAA9 |  |  |  | not captured |  |  |  |
| 142 | Base | 0x0BB09 |  |  |  | not captured |  |  |  |
| 143 | Control | 0x0BB45 |  |  |  | not captured |  |  |  |
| 144 | Measured | 0x0BBBB |  |  |  | not captured |  |  |  |
| 145 | The (ee) | 0x0BC41 |  |  |  | not captured |  |  |  |
| 146 | The (uh) | 0x0BC74 |  |  |  | not captured |  |  |  |
| 147 | Vacuum | 0x0BC95 |  |  |  | not captured |  |  |  |
| 148 | Abeam | 0x0BD13 |  |  |  | not captured |  |  |  |
