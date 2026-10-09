# TI-99/4A Solid State Speech Synthesizer VSMs (US male voice)

- **Image:** 32768 bytes, SHA-256 `ae133ff9c238a3d26c68fe875925c5fdd132b0138df0bbda55798336289441d0`, bank 0. Target chip: TMS5200.
- **Provenance:** TMS6100 Multi-PHROM Emulator (eRadiofan), ROMdata_TI99.c, padded with zeros to 32 KB; its two halves are MAME's dumps of the two VSMs. The phrase names are the ones stored in the image's index.
- **Captured:** 1 of 339 words. Each take directory follows `docs-tech/speech_capture_plan.md` §6; its `capture.json` holds the metadata and checks. [`takes.csv`](takes.csv) lists every take, with the SHA-256 of its output (`talk_sha256`: the I/O codes from sample 0 to the end of talk, as 16-bit little-endian integers) and of its raw files.
- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent (Speak External), up to the stop code. ms is the talk time of the first take, from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with the first, sample for sample, after aligning them. For takes made with the standard procedure, it gives how many match the per-sample majority of all of them exactly, and the samples where no majority exists (`compare.json`).

| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |
|---|---|---|---|---|---|---|---|---|---|
| 0 | MORE | 0x04642 |  |  |  | not captured |  |  |  |
| 1 | -, NEGATIVE | 0x048DC |  |  |  | not captured |  |  |  |
| 2 | ., POINT | 0x050EC |  |  |  | not captured |  |  |  |
| 3 | 0, ZERO | 0x013C3 |  |  |  | not captured |  |  |  |
| 4 | 1, ONE, WON | 0x01409 |  |  |  | not captured |  |  |  |
| 5 | 2, TO, TOO, TWO | 0x0145C |  |  |  | not captured |  |  |  |
| 6 | 3, THREE | 0x0149A |  |  |  | not captured |  |  |  |
| 7 | 4, FOR, FOUR | 0x014E7 |  |  |  | not captured |  |  |  |
| 8 | 5, FIVE | 0x01531 |  |  |  | not captured |  |  |  |
| 9 | 6, SIX | 0x015A8 |  |  |  | not captured |  |  |  |
| 10 | 7, SEVEN | 0x015E8 |  |  |  | not captured |  |  |  |
| 11 | 8, EIGHT | 0x01637 |  |  |  | not captured |  |  |  |
| 12 | 9, NINE | 0x01664 |  |  |  | not captured |  |  |  |
| 13 | A | 0x016E4 |  |  |  | not captured |  |  |  |
| 14 | A1 | 0x01700 |  |  |  | not captured |  |  |  |
| 15 | ABOUT | 0x01714 |  |  |  | not captured |  |  |  |
| 16 | AFTER | 0x01769 |  |  |  | not captured |  |  |  |
| 17 | AGAIN | 0x017A5 |  |  |  | not captured |  |  |  |
| 18 | ALL | 0x01807 |  |  |  | not captured |  |  |  |
| 19 | AM | 0x01830 |  |  |  | not captured |  |  |  |
| 20 | AN | 0x01876 |  |  |  | not captured |  |  |  |
| 21 | AND | 0x018AC |  |  |  | not captured |  |  |  |
| 22 | ANSWER | 0x01913 |  |  |  | not captured |  |  |  |
| 23 | ANY | 0x01962 |  |  |  | not captured |  |  |  |
| 24 | ARE, R | 0x0556E |  |  |  | not captured |  |  |  |
| 25 | AS | 0x019A7 |  |  |  | not captured |  |  |  |
| 26 | ASSUME | 0x019E8 |  |  |  | not captured |  |  |  |
| 27 | AT | 0x01A25 |  |  |  | not captured |  |  |  |
| 28 | B, BE | 0x01A42 |  |  |  | not captured |  |  |  |
| 29 | BACK | 0x01A64 |  |  |  | not captured |  |  |  |
| 30 | BASE | 0x01A8F |  |  |  | not captured |  |  |  |
| 32 | BETWEEN | 0x01ADE |  |  |  | not captured |  |  |  |
| 33 | BLACK | 0x01B47 |  |  |  | not captured |  |  |  |
| 34 | BLUE | 0x01B8A |  |  |  | not captured |  |  |  |
| 35 | BOTH | 0x01BB6 |  |  |  | not captured |  |  |  |
| 36 | BOTTOM | 0x01BEA |  |  |  | not captured |  |  |  |
| 37 | BUT | 0x01C20 |  |  |  | not captured |  |  |  |
| 38 | BUY, BY, BYE | 0x01C48 |  |  |  | not captured |  |  |  |
| 41 | C, SEE | 0x01C86 |  |  |  | not captured |  |  |  |
| 42 | CAN | 0x01CD9 |  |  |  | not captured |  |  |  |
| 43 | CASSETTE | 0x01D10 |  |  |  | not captured |  |  |  |
| 44 | CENTER | 0x01D47 |  |  |  | not captured |  |  |  |
| 45 | CHECK | 0x01D82 |  |  |  | not captured |  |  |  |
| 46 | CHOICE | 0x01DA2 |  |  |  | not captured |  |  |  |
| 47 | CLEAR | 0x01DE6 |  |  |  | not captured |  |  |  |
| 48 | COLOR | 0x01E20 |  |  |  | not captured |  |  |  |
| 49 | COME | 0x01E54 |  |  |  | not captured |  |  |  |
| 50 | COMES | 0x01E87 |  |  |  | not captured |  |  |  |
| 51 | COMMA | 0x01EDE |  |  |  | not captured |  |  |  |
| 52 | COMMAND | 0x01F1A |  |  |  | not captured |  |  |  |
| 53 | COMPLETE | 0x01F71 |  |  |  | not captured |  |  |  |
| 54 | COMPLETED | 0x01FCD |  |  |  | not captured |  |  |  |
| 55 | COMPUTER | 0x02034 |  |  |  | not captured |  |  |  |
| 56 | CONNECTED | 0x0208B |  |  |  | not captured |  |  |  |
| 57 | CONSOLE | 0x020F3 |  |  |  | not captured |  |  |  |
| 58 | CORRECT | 0x0213C |  |  |  | not captured |  |  |  |
| 59 | COURSE | 0x02182 |  |  |  | not captured |  |  |  |
| 60 | CYAN | 0x021C0 |  |  |  | not captured |  |  |  |
| 61 | D | 0x02203 |  |  |  | not captured |  |  |  |
| 62 | DATA | 0x0223C |  |  |  | not captured |  |  |  |
| 63 | DECIDE | 0x02294 |  |  |  | not captured |  |  |  |
| 64 | DEVICE | 0x022FD |  |  |  | not captured |  |  |  |
| 65 | DID | 0x02366 |  |  |  | not captured |  |  |  |
| 66 | DIFFERENT | 0x023C4 |  |  |  | not captured |  |  |  |
| 67 | DISKETTE | 0x0242D |  |  |  | not captured |  |  |  |
| 68 | DO | 0x02480 |  |  |  | not captured |  |  |  |
| 69 | DOES | 0x024B3 |  |  |  | not captured |  |  |  |
| 70 | DOING | 0x024EA |  |  |  | not captured |  |  |  |
| 71 | DONE | 0x0253E |  |  |  | not captured |  |  |  |
| 72 | DOUBLE | 0x02599 |  |  |  | not captured |  |  |  |
| 73 | DOWN | 0x025D3 |  |  |  | not captured |  |  |  |
| 74 | DRAW | 0x02612 |  |  |  | not captured |  |  |  |
| 75 | DRAWING | 0x02668 |  |  |  | not captured |  |  |  |
| 76 | E | 0x026CB |  |  |  | not captured |  |  |  |
| 77 | EACH | 0x026F0 |  |  |  | not captured |  |  |  |
| 79 | EIGHTY | 0x02723 |  |  |  | not captured |  |  |  |
| 80 | ELEVEN | 0x02759 |  |  |  | not captured |  |  |  |
| 81 | ELSE | 0x027B6 |  |  |  | not captured |  |  |  |
| 82 | END | 0x027F5 |  |  |  | not captured |  |  |  |
| 83 | ENDS | 0x02866 |  |  |  | not captured |  |  |  |
| 84 | ENTER | 0x028AD |  |  |  | not captured |  |  |  |
| 85 | ERROR | 0x028EF |  |  |  | not captured |  |  |  |
| 86 | EXACTLY | 0x02937 |  |  |  | not captured |  |  |  |
| 87 | EYE, I | 0x03793 |  |  |  | not captured |  |  |  |
| 88 | F | 0x0299F |  |  |  | not captured |  |  |  |
| 89 | FIFTEEN | 0x029C2 |  |  |  | not captured |  |  |  |
| 90 | FIFTY | 0x02A1D |  |  |  | not captured |  |  |  |
| 91 | FIGURE | 0x02A60 |  |  |  | not captured |  |  |  |
| 92 | FIND | 0x02AD7 |  |  |  | not captured |  |  |  |
| 93 | FINE | 0x02B1E |  |  |  | not captured |  |  |  |
| 94 | FINISH | 0x02B5B |  |  |  | not captured |  |  |  |
| 95 | FINISHED | 0x02B94 |  |  |  | not captured |  |  |  |
| 96 | FIRST | 0x02BD7 |  |  |  | not captured |  |  |  |
| 97 | FIT | 0x02C14 |  |  |  | not captured |  |  |  |
| 100 | FORTY | 0x02C3E |  |  |  | not captured |  |  |  |
| 102 | FOURTEEN | 0x02C7F |  |  |  | not captured |  |  |  |
| 103 | FOURTH | 0x02D19 |  |  |  | not captured |  |  |  |
| 104 | FROM | 0x02D74 |  |  |  | not captured |  |  |  |
| 105 | FRONT | 0x02DBC |  |  |  | not captured |  |  |  |
| 106 | G | 0x02DEB |  |  |  | not captured |  |  |  |
| 107 | GAMES | 0x02E28 |  |  |  | not captured |  |  |  |
| 108 | GET | 0x02E8C |  |  |  | not captured |  |  |  |
| 109 | GETTING | 0x02EBA |  |  |  | not captured |  |  |  |
| 110 | GIVE | 0x02F38 |  |  |  | not captured |  |  |  |
| 111 | GIVES | 0x02F8D |  |  |  | not captured |  |  |  |
| 112 | GO | 0x02FFC |  |  |  | not captured |  |  |  |
| 113 | GOES | 0x03031 |  |  |  | not captured |  |  |  |
| 114 | GOING | 0x03079 |  |  |  | not captured |  |  |  |
| 115 | GOOD | 0x030D6 |  |  |  | not captured |  |  |  |
| 116 | GOOD WORK | 0x030FA |  |  |  | not captured |  |  |  |
| 117 | GOODBYE | 0x03148 |  |  |  | not captured |  |  |  |
| 118 | GOT | 0x031A0 |  |  |  | not captured |  |  |  |
| 119 | GRAY | 0x031D1 |  |  |  | not captured |  |  |  |
| 120 | GREEN | 0x0321D |  |  |  | not captured |  |  |  |
| 121 | GUESS | 0x0327E |  |  |  | not captured |  |  |  |
| 122 | H | 0x032C0 |  |  |  | not captured |  |  |  |
| 123 | HAD | 0x032EF |  |  |  | not captured |  |  |  |
| 124 | HAND | 0x03339 |  |  |  | not captured |  |  |  |
| 125 | HANDHELD UNIT | 0x0337F |  |  |  | not captured |  |  |  |
| 126 | HAS | 0x03405 |  |  |  | not captured |  |  |  |
| 127 | HAVE | 0x0344A |  |  |  | not captured |  |  |  |
| 128 | HEAD | 0x0348C |  |  |  | not captured |  |  |  |
| 129 | HEAR, HERE | 0x034E5 |  |  |  | not captured |  |  |  |
| 130 | HELLO | 0x0351A |  |  |  | not captured |  |  |  |
| 131 | HELP | 0x03571 |  |  |  | not captured |  |  |  |
| 133 | HIGHER | 0x035AE |  |  |  | not captured |  |  |  |
| 134 | HIT | 0x0360A |  |  |  | not captured |  |  |  |
| 135 | HOME | 0x0363E |  |  |  | not captured |  |  |  |
| 136 | HOW | 0x03689 |  |  |  | not captured |  |  |  |
| 137 | HUNDRED | 0x036EF |  |  |  | not captured |  |  |  |
| 138 | HURRY | 0x03757 |  |  |  | not captured |  |  |  |
| 140 | I WIN | 0x037CF |  |  |  | not captured |  |  |  |
| 141 | IF | 0x03850 |  |  |  | not captured |  |  |  |
| 142 | IN | 0x03872 |  |  |  | not captured |  |  |  |
| 143 | INCH | 0x038B5 |  |  |  | not captured |  |  |  |
| 144 | INCHES | 0x038FA |  |  |  | not captured |  |  |  |
| 145 | INSTRUCTION | 0x0394B |  |  |  | not captured |  |  |  |
| 146 | INSTRUCTIONS | 0x039BD |  |  |  | not captured |  |  |  |
| 147 | IS | 0x03A32 |  |  |  | not captured |  |  |  |
| 148 | IT | 0x03A7A |  |  |  | not captured |  |  |  |
| 149 | J | 0x03AAE |  |  |  | not captured |  |  |  |
| 150 | JOYSTICK | 0x03AED |  |  |  | not captured |  |  |  |
| 151 | JUST | 0x03B4C |  |  |  | not captured |  |  |  |
| 152 | K | 0x03B8A |  |  |  | not captured |  |  |  |
| 153 | KEY | 0x03BB9 |  |  |  | not captured |  |  |  |
| 154 | KEYBOARD | 0x03BE9 |  |  |  | not captured |  |  |  |
| 155 | KNOW, NO | 0x03C4F |  |  |  | not captured |  |  |  |
| 156 | L | 0x03C8F |  |  |  | not captured |  |  |  |
| 157 | LARGE | 0x03CD0 |  |  |  | not captured |  |  |  |
| 158 | LARGER | 0x03D19 |  |  |  | not captured |  |  |  |
| 159 | LARGEST | 0x03D67 |  |  |  | not captured |  |  |  |
| 160 | LAST | 0x03DDE |  |  |  | not captured |  |  |  |
| 161 | LEARN | 0x03E1E |  |  |  | not captured |  |  |  |
| 162 | LEFT | 0x03E78 |  |  |  | not captured |  |  |  |
| 163 | LESS | 0x03EB2 |  |  |  | not captured |  |  |  |
| 164 | LET | 0x03F08 |  |  |  | not captured |  |  |  |
| 165 | LIKE | 0x03F2F |  |  |  | not captured |  |  |  |
| 166 | LIKES | 0x03F6A |  |  |  | not captured |  |  |  |
| 167 | LINE | 0x03FD5 |  |  |  | not captured |  |  |  |
| 168 | LOAD | 0x0404B |  |  |  | not captured |  |  |  |
| 169 | LONG | 0x040D3 |  |  |  | not captured |  |  |  |
| 170 | LOOK | 0x0413D |  |  |  | not captured |  |  |  |
| 171 | LOOKS | 0x04191 |  |  |  | not captured |  |  |  |
| 172 | LOWER | 0x041E7 |  |  |  | not captured |  |  |  |
| 173 | M | 0x04233 |  |  |  | not captured |  |  |  |
| 174 | MADE | 0x04267 |  |  |  | not captured |  |  |  |
| 175 | MAGENTA | 0x042AE |  |  |  | not captured |  |  |  |
| 176 | MAKE | 0x0432E |  |  |  | not captured |  |  |  |
| 177 | ME | 0x0437D |  |  |  | not captured |  |  |  |
| 178 | MEAN | 0x043BD |  |  |  | not captured |  |  |  |
| 179 | MEMORY | 0x04405 |  |  |  | not captured |  |  |  |
| 180 | MESSAGE | 0x0446C |  |  |  | not captured |  |  |  |
| 181 | MESSAGES | 0x044D7 |  |  |  | not captured |  |  |  |
| 182 | MIDDLE | 0x04551 |  |  |  | not captured |  |  |  |
| 183 | MIGHT | 0x04593 |  |  |  | not captured |  |  |  |
| 184 | MODULE | 0x045DF |  |  |  | not captured |  |  |  |
| 185 | +, POSITIVE | 0x051B3 |  |  |  | not captured |  |  |  |
| 186 | MOST | 0x04693 |  |  |  | not captured |  |  |  |
| 187 | MOVE | 0x046DF |  |  |  | not captured |  |  |  |
| 188 | MUST | 0x0473D |  |  |  | not captured |  |  |  |
| 189 | N | 0x04786 |  |  |  | not captured |  |  |  |
| 190 | NAME | 0x047C0 |  |  |  | not captured |  |  |  |
| 191 | NEAR | 0x04833 |  |  |  | not captured |  |  |  |
| 192 | NEED | 0x04880 |  |  |  | not captured |  |  |  |
| 194 | NEXT | 0x04959 |  |  |  | not captured |  |  |  |
| 195 | NICE TRY | 0x049A5 |  |  |  | not captured |  |  |  |
| 197 | NINETY | 0x04A4E |  |  |  | not captured |  |  |  |
| 199 | NOT | 0x04AAB |  |  |  | not captured |  |  |  |
| 200 | NOW | 0x04ADA |  |  |  | not captured |  |  |  |
| 201 | NUMBER | 0x04B20 |  |  |  | not captured |  |  |  |
| 202 | O, OH | 0x04B7D |  |  |  | not captured |  |  |  |
| 203 | OF | 0x04BBA |  |  |  | not captured |  |  |  |
| 204 | OFF | 0x04C13 |  |  |  | not captured |  |  |  |
| 206 | ON | 0x04C41 |  |  |  | not captured |  |  |  |
| 208 | ONLY | 0x04C8B |  |  |  | not captured |  |  |  |
| 209 | OR | 0x04CDC |  |  |  | not captured |  |  |  |
| 210 | ORDER | 0x04D34 |  |  |  | not captured |  |  |  |
| 211 | OTHER | 0x04D8A |  |  |  | not captured |  |  |  |
| 212 | OUT | 0x04DD4 |  |  |  | not captured |  |  |  |
| 213 | OVER | 0x04E0A |  |  |  | not captured |  |  |  |
| 214 | P | 0x04E66 |  |  |  | not captured |  |  |  |
| 215 | PART | 0x04E9F |  |  |  | not captured |  |  |  |
| 216 | PARTNER | 0x04EE0 |  |  |  | not captured |  |  |  |
| 217 | PARTS | 0x04F31 |  |  |  | not captured |  |  |  |
| 218 | PERIOD | 0x04F81 |  |  |  | not captured |  |  |  |
| 219 | PLAY | 0x04FE5 |  |  |  | not captured |  |  |  |
| 220 | PLAYS | 0x0502D |  |  |  | not captured |  |  |  |
| 221 | PLEASE | 0x05093 |  |  |  | not captured |  |  |  |
| 223 | POSITION | 0x05148 |  |  |  | not captured |  |  |  |
| 225 | PRESS | 0x05231 |  |  |  | not captured |  |  |  |
| 226 | PRINT | 0x0526D |  |  |  | not captured |  |  |  |
| 227 | PRINTER | 0x052AA |  |  |  | not captured |  |  |  |
| 228 | PROBLEM | 0x052F9 |  |  |  | not captured |  |  |  |
| 229 | PROBLEMS | 0x05360 |  |  |  | not captured |  |  |  |
| 230 | PROGRAM | 0x053EE |  |  |  | not captured |  |  |  |
| 231 | PUT | 0x05477 |  |  |  | not captured |  |  |  |
| 232 | PUTTING | 0x054AA |  |  |  | not captured |  |  |  |
| 233 | Q | 0x05520 |  |  |  | not captured |  |  |  |
| 235 | RANDOMLY | 0x055A0 |  |  |  | not captured |  |  |  |
| 236 | READ | 0x05652 |  |  |  | not captured |  |  |  |
| 237 | READ1, RED | 0x057C1 |  |  |  | not captured |  |  |  |
| 238 | READY TO START | 0x056B3 |  |  |  | not captured |  |  |  |
| 239 | RECORDER | 0x05745 |  |  |  | not captured |  |  |  |
| 241 | REFER | 0x05801 |  |  |  | not captured |  |  |  |
| 242 | REMEMBER | 0x05861 |  |  |  | not captured |  |  |  |
| 243 | RETURN | 0x058CF |  |  |  | not captured |  |  |  |
| 244 | REWIND | 0x0593A |  |  |  | not captured |  |  |  |
| 245 | RIGHT, WRITE | 0x07C38 |  |  |  | not captured |  |  |  |
| 246 | ROUND | 0x059C2 |  |  |  | not captured |  |  |  |
| 247 | S | 0x05A5A |  |  |  | not captured |  |  |  |
| 248 | SAID | 0x05AA1 |  |  |  | not captured |  |  |  |
| 249 | SAVE | 0x05AEF |  |  |  | not captured |  |  |  |
| 250 | SAY | 0x05B65 |  |  |  | not captured |  |  |  |
| 251 | SAYS | 0x05BA2 |  |  |  | not captured |  |  |  |
| 252 | SCREEN | 0x05BFB |  |  |  | not captured |  |  |  |
| 253 | SECOND | 0x05C5B |  |  |  | not captured |  |  |  |
| 255 | SEES | 0x05CBF |  |  |  | not captured |  |  |  |
| 256 | SET | 0x05D1B |  |  |  | not captured |  |  |  |
| 258 | SEVENTY | 0x05D50 |  |  |  | not captured |  |  |  |
| 259 | SHAPE | 0x05DA5 |  |  |  | not captured |  |  |  |
| 260 | SHAPES | 0x05DDE |  |  |  | not captured |  |  |  |
| 261 | SHIFT | 0x05E27 |  |  |  | not captured |  |  |  |
| 262 | SHORT | 0x05E5C |  |  |  | not captured |  |  |  |
| 263 | SHORTER | 0x05EA5 |  |  |  | not captured |  |  |  |
| 264 | SHOULD | 0x05F24 |  |  |  | not captured |  |  |  |
| 265 | SIDE | 0x05F6D |  |  |  | not captured |  |  |  |
| 266 | SIDES | 0x05FC8 |  |  |  | not captured |  |  |  |
| 268 | SIXTY | 0x0601A |  |  |  | not captured |  |  |  |
| 269 | SMALL | 0x06070 |  |  |  | not captured |  |  |  |
| 270 | SMALLER | 0x060AE |  |  |  | not captured |  |  |  |
| 271 | SMALLEST | 0x060F1 |  |  |  | not captured |  |  |  |
| 272 | SO | 0x06153 |  |  |  | not captured |  |  |  |
| 273 | SOME, SUM | 0x06197 |  |  |  | not captured |  |  |  |
| 274 | SORRY | 0x061C6 |  |  |  | not captured |  |  |  |
| 275 | SPACE | 0x06226 |  |  |  | not captured |  |  |  |
| 276 | SPACES | 0x0625D |  |  |  | not captured |  |  |  |
| 277 | SPELL | 0x062CC |  |  |  | not captured |  |  |  |
| 278 | SQUARE | 0x06333 |  |  |  | not captured |  |  |  |
| 279 | START | 0x0637C |  |  |  | not captured |  |  |  |
| 280 | STEP | 0x063C5 |  |  |  | not captured |  |  |  |
| 281 | STOP | 0x063F7 |  |  |  | not captured |  |  |  |
| 283 | SUPPOSED | 0x06423 |  |  |  | not captured |  |  |  |
| 284 | SUPPOSED TO | 0x06489 |  |  |  | not captured |  |  |  |
| 285 | SURE | 0x064F4 |  |  |  | not captured |  |  |  |
| 286 | T | 0x06551 |  |  |  | not captured |  |  |  |
| 287 | TAKE | 0x0658B |  |  |  | not captured |  |  |  |
| 288 | TEEN | 0x065BF |  |  |  | not captured |  |  |  |
| 289 | TELL | 0x06603 |  |  |  | not captured |  |  |  |
| 290 | TEN | 0x0664E |  |  |  | not captured |  |  |  |
| 291 | TEXAS INSTRUMENTS | 0x06696 |  |  |  | not captured |  |  |  |
| 292 | THAN | 0x0675B |  |  |  | not captured |  |  |  |
| 293 | THAT | 0x067B6 |  |  |  | not captured |  |  |  |
| 294 | THAT IS INCORRECT | 0x06816 |  |  |  | not captured |  |  |  |
| 295 | THAT IS RIGHT | 0x068FE |  |  |  | not captured |  |  |  |
| 296 | THE | 0x06974 |  |  |  | not captured |  |  |  |
| 297 | THE1 | 0x069B6 |  |  |  | not captured |  |  |  |
| 298 | THEIR, THERE | 0x06A72 |  |  |  | not captured |  |  |  |
| 299 | THEN | 0x069E1 |  |  |  | not captured |  |  |  |
| 301 | THESE | 0x06ADE |  |  |  | not captured |  |  |  |
| 302 | THEY | 0x06B47 |  |  |  | not captured |  |  |  |
| 303 | THING | 0x06BA0 |  |  |  | not captured |  |  |  |
| 304 | THINGS | 0x06C0F |  |  |  | not captured |  |  |  |
| 305 | THINK | 0x06C73 |  |  |  | not captured |  |  |  |
| 306 | THIRD | 0x06CBC |  |  |  | not captured |  |  |  |
| 307 | THIRTEEN | 0x06D11 |  |  |  | not captured |  |  |  |
| 308 | THIRTY | 0x06DA2 |  |  |  | not captured |  |  |  |
| 309 | THIS | 0x06DDE |  |  |  | not captured |  |  |  |
| 311 | THREW, THROUGH | 0x06E26 |  |  |  | not captured |  |  |  |
| 313 | TIME | 0x06E69 |  |  |  | not captured |  |  |  |
| 315 | TOGETHER | 0x06EB0 |  |  |  | not captured |  |  |  |
| 316 | TONE | 0x06F1F |  |  |  | not captured |  |  |  |
| 318 | TOP | 0x06F8D |  |  |  | not captured |  |  |  |
| 319 | TRY | 0x06FBB |  |  |  | not captured |  |  |  |
| 320 | TRY AGAIN | 0x0700F |  |  |  | not captured |  |  |  |
| 321 | TURN | 0x07092 |  |  |  | not captured |  |  |  |
| 322 | TWELVE | 0x070CE |  |  |  | not captured |  |  |  |
| 323 | TWENTY | 0x07119 |  |  |  | not captured |  |  |  |
| 325 | TYPE | 0x07170 |  |  |  | not captured |  |  |  |
| 326 | U, YOU | 0x071BE |  |  |  | not captured |  |  |  |
| 327 | UHOH | 0x071F4 |  |  |  | not captured |  |  |  |
| 328 | UNDER | 0x07245 |  |  |  | not captured |  |  |  |
| 329 | UNDERSTAND | 0x0729D |  |  |  | not captured |  |  |  |
| 330 | UNTIL | 0x0732F |  |  |  | not captured |  |  |  |
| 331 | UP | 0x0739F |  |  |  | not captured |  |  |  |
| 332 | UPPER | 0x073C3 |  |  |  | not captured |  |  |  |
| 333 | USE | 0x07403 |  |  |  | not captured |  |  |  |
| 334 | V | 0x07449 |  |  |  | not captured |  |  |  |
| 335 | VARY | 0x07487 |  |  |  | not captured |  |  |  |
| 336 | VERY | 0x074DA |  |  |  | not captured |  |  |  |
| 337 | W | 0x07520 |  |  |  | not captured |  |  |  |
| 338 | WAIT, WEIGHT | 0x0759D |  |  |  | not captured |  |  |  |
| 339 | WANT | 0x075DF |  |  |  | not captured |  |  |  |
| 340 | WANTS | 0x07621 |  |  |  | not captured |  |  |  |
| 341 | WAY, WEIGH | 0x076B0 |  |  |  | not captured |  |  |  |
| 342 | WE | 0x0767D |  |  |  | not captured |  |  |  |
| 345 | WELL | 0x07717 | 19 | 546 | 487 | 50 vsm | pass | vsm: 0 of 50 match, 1627 samples undecided | [samples](345_well/vsm01/samples.wav) · [listen](345_well/vsm01/listen.wav) · [frames](345_well/vsm01/frames.csv) |
| 346 | WERE | 0x0775C |  |  |  | not captured |  |  |  |
| 347 | WHAT | 0x077BC |  |  |  | not captured |  |  |  |
| 348 | WHAT WAS THAT | 0x077E9 |  |  |  | not captured |  |  |  |
| 349 | WHEN | 0x07875 |  |  |  | not captured |  |  |  |
| 350 | WHERE | 0x078AB |  |  |  | not captured |  |  |  |
| 351 | WHICH | 0x078F4 |  |  |  | not captured |  |  |  |
| 352 | WHITE | 0x07924 |  |  |  | not captured |  |  |  |
| 353 | WHO | 0x07969 |  |  |  | not captured |  |  |  |
| 354 | WHY | 0x079B4 |  |  |  | not captured |  |  |  |
| 355 | WILL | 0x07A11 |  |  |  | not captured |  |  |  |
| 356 | WITH | 0x07A6B |  |  |  | not captured |  |  |  |
| 358 | WORD | 0x07AAB |  |  |  | not captured |  |  |  |
| 359 | WORDS | 0x07B0A |  |  |  | not captured |  |  |  |
| 360 | WORK | 0x07B75 |  |  |  | not captured |  |  |  |
| 361 | WORKING | 0x07BBC |  |  |  | not captured |  |  |  |
| 363 | X | 0x07C8D |  |  |  | not captured |  |  |  |
| 364 | Y | 0x07CB2 |  |  |  | not captured |  |  |  |
| 365 | YELLOW | 0x07CF8 |  |  |  | not captured |  |  |  |
| 366 | YES | 0x07D58 |  |  |  | not captured |  |  |  |
| 367 | YET | 0x07D99 |  |  |  | not captured |  |  |  |
| 369 | YOU WIN | 0x07DDB |  |  |  | not captured |  |  |  |
| 370 | YOUR | 0x07E4D |  |  |  | not captured |  |  |  |
| 371 | Z | 0x07E99 |  |  |  | not captured |  |  |  |
