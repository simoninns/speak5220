# Acorn PHROM A (Kenneth Kendall voice)

- **Image:** 16384 bytes, SHA-256 `8c093401661f530032c5aca8fd80d91af58e596d082a51be58ce2ee063a89308`, bank F. Target chip: TMS5220.
- **Provenance:** TMS6100-Emulator (simoninns), https://github.com/simoninns/TMS6100-Emulator/tree/master/Firmware/tms6100: a dump of an Acorn PHROM A made with HxD. The word list was checked by decoding the LPC data at each address (romdata_acorn.h).
- **Captured:** 1 of 165 entries. Each take directory follows `docs/technical/capture/data-layout.md`; its `capture.json` holds the metadata and checks.
- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent (Speak External), up to the stop code. ms is the talk time of the first take, from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with the first, sample for sample, after aligning them. For takes made with the standard procedure, it gives how many match the per-sample majority of all of them exactly, and the samples where no majority exists (`compare.json`).

| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |
|---|---|---|---|---|---|---|---|---|---|
| 127 | -0.125 | 0x3C250 |  |  |  | not captured |  |  |  |
| 128 | -0.25 | 0x3C25F |  |  |  | not captured |  |  |  |
| 129 | (TONE 1) | 0x3C272 |  |  |  | not captured |  |  |  |
| 130 | (TONE2) | 0x3C2B0 |  |  |  | not captured |  |  |  |
| 131 | -D | 0x3C2E9 |  |  |  | not captured |  |  |  |
| 132 | -ED | 0x3C300 |  |  |  | not captured |  |  |  |
| 133 | -ING | 0x3C32B |  |  |  | not captured |  |  |  |
| 134 | -S | 0x3C361 |  |  |  | not captured |  |  |  |
| 135 | -TEE | 0x3C384 |  |  |  | not captured |  |  |  |
| 136 | -TH | 0x3C3DC |  |  |  | not captured |  |  |  |
| 137 | -T | 0x3C3F3 |  |  |  | not captured |  |  |  |
| 138 | - | 0x3C415 |  |  |  | not captured |  |  |  |
| 139 | ZERO (0) | 0x3C437 |  |  |  | not captured |  |  |  |
| 140 | HUNDRED (00) | 0x3C4C8 |  |  |  | not captured |  |  |  |
| 141 | THOUSAND (000) | 0x3C530 |  |  |  | not captured |  |  |  |
| 142 | ONE,WON | 0x3C5CA |  |  |  | not captured |  |  |  |
| 143 | TWO,TO,TOO | 0x3C633 |  |  |  | not captured |  |  |  |
| 144 | 2- (TWEN-) | 0x3C692 |  |  |  | not captured |  |  |  |
| 145 | THREE | 0x3C6D2 |  |  |  | not captured |  |  |  |
| 146 | 3- (THIR-) | 0x3C73F |  |  |  | not captured |  |  |  |
| 147 | FOUR,FOR,FORE | 0x3C788 |  |  |  | not captured |  |  |  |
| 148 | 4- (FOR-) | 0x3C7F7 |  |  |  | not captured |  |  |  |
| 149 | FIVE | 0x3C840 |  |  |  | not captured |  |  |  |
| 150 | 5- (FIF-) | 0x3C8B6 |  |  |  | not captured |  |  |  |
| 151 | SIX | 0x3C8E9 |  |  |  | not captured |  |  |  |
| 152 | 6- (SIX-) | 0x3C936 |  |  |  | not captured |  |  |  |
| 153 | SEVEN | 0x3C96E |  |  |  | not captured |  |  |  |
| 154 | 7- (SEVEN-) | 0x3C9CB |  |  |  | not captured |  |  |  |
| 155 | EIGHT,ATE | 0x3CA12 |  |  |  | not captured |  |  |  |
| 156 | 8- (EIGHT-) | 0x3CA46 |  |  |  | not captured |  |  |  |
| 157 | NINE | 0x3CA71 |  |  |  | not captured |  |  |  |
| 158 | 9- (NIN-) | 0x3CAFD |  |  |  | not captured |  |  |  |
| 159 | A | 0x3CB5D |  |  |  | not captured |  |  |  |
| 160 | ACORN | 0x3CBB6 | 27 | 1152 | 687 | 4 external, 4 vsm | pass | vsm: 0 of 4 match, 645 samples undecided; external: 0 of 4 match, 158 samples undecided | [samples](160_acorn/vsm01/samples.wav) · [listen](160_acorn/vsm01/listen.wav) · [frames](160_acorn/vsm01/frames.csv) |
| 161 | AFTER | 0x3CC4B |  |  |  | not captured |  |  |  |
| 162 | AGAIN | 0x3CCB3 |  |  |  | not captured |  |  |  |
| 163 | AMOUNT | 0x3CD46 |  |  |  | not captured |  |  |  |
| 164 | AN | 0x3CDBD |  |  |  | not captured |  |  |  |
| 165 | AND | 0x3CE12 |  |  |  | not captured |  |  |  |
| 166 | ANOTHER | 0x3CE8E |  |  |  | not captured |  |  |  |
| 167 | ANSWER | 0x3CEFD |  |  |  | not captured |  |  |  |
| 168 | ANY | 0x3CF72 |  |  |  | not captured |  |  |  |
| 169 | AVAILABLE | 0x3CFCF |  |  |  | not captured |  |  |  |
| 170 | B,BEE,BE | 0x3D065 |  |  |  | not captured |  |  |  |
| 171 | BAD | 0x3D0BD |  |  |  | not captured |  |  |  |
| 172 | BETWEEN | 0x3D13B |  |  |  | not captured |  |  |  |
| 173 | BOTH | 0x3D1B1 |  |  |  | not captured |  |  |  |
| 174 | BUTTON | 0x3D1F8 |  |  |  | not captured |  |  |  |
| 175 | C,SEE,SEA | 0x3D24D |  |  |  | not captured |  |  |  |
| 176 | CASSETTE | 0x3D2B1 |  |  |  | not captured |  |  |  |
| 177 | CHARACTER | 0x3D326 |  |  |  | not captured |  |  |  |
| 178 | COMPLETE | 0x3D39F |  |  |  | not captured |  |  |  |
| 179 | COMPUTER | 0x3D40B |  |  |  | not captured |  |  |  |
| 180 | CORRECT | 0x3D483 |  |  |  | not captured |  |  |  |
| 181 | D | 0x3D4F0 |  |  |  | not captured |  |  |  |
| 182 | DATA | 0x3D53B |  |  |  | not captured |  |  |  |
| 183 | DATE | 0x3D59F |  |  |  | not captured |  |  |  |
| 184 | DO | 0x3D5E6 |  |  |  | not captured |  |  |  |
| 185 | DOLLAR | 0x3D62A |  |  |  | not captured |  |  |  |
| 186 | DONT | 0x3D67E |  |  |  | not captured |  |  |  |
| 187 | DOWN | 0x3D6D7 |  |  |  | not captured |  |  |  |
| 188 | E | 0x3D75A |  |  |  | not captured |  |  |  |
| 189 | EACH | 0x3D78B |  |  |  | not captured |  |  |  |
| 190 | ELEVEN | 0x3D7D6 |  |  |  | not captured |  |  |  |
| 191 | ENGAGED | 0x3D852 |  |  |  | not captured |  |  |  |
| 192 | ENTER | 0x3D8FA |  |  |  | not captured |  |  |  |
| 193 | ERROR | 0x3D95C |  |  |  | not captured |  |  |  |
| 194 | ESCAPE | 0x3D9AA |  |  |  | not captured |  |  |  |
| 195 | F | 0x3DA1C |  |  |  | not captured |  |  |  |
| 196 | FEW | 0x3DA5D |  |  |  | not captured |  |  |  |
| 197 | FILE | 0x3DAAB |  |  |  | not captured |  |  |  |
| 198 | FIRST | 0x3DB1E |  |  |  | not captured |  |  |  |
| 199 | FOUND | 0x3DB96 |  |  |  | not captured |  |  |  |
| 200 | FROM | 0x3DC11 |  |  |  | not captured |  |  |  |
| 201 | G | 0x3DC6B |  |  |  | not captured |  |  |  |
| 202 | GOOD | 0x3DCC9 |  |  |  | not captured |  |  |  |
| 203 | H | 0x3DD09 |  |  |  | not captured |  |  |  |
| 204 | HAVE | 0x3DD60 |  |  |  | not captured |  |  |  |
| 205 | I,EYE | 0x3DDCC |  |  |  | not captured |  |  |  |
| 206 | ILLEGAL | 0x3DE3E |  |  |  | not captured |  |  |  |
| 207 | IN- | 0x3DEBD |  |  |  | not captured |  |  |  |
| 208 | INPUT | 0x3DEFC |  |  |  | not captured |  |  |  |
| 209 | IS | 0x3DF57 |  |  |  | not captured |  |  |  |
| 210 | J,JAY | 0x3DFA6 |  |  |  | not captured |  |  |  |
| 211 | K | 0x3E008 |  |  |  | not captured |  |  |  |
| 212 | KEY | 0x3E061 |  |  |  | not captured |  |  |  |
| 213 | L | 0x3E09F |  |  |  | not captured |  |  |  |
| 214 | LARGE | 0x3E0FC |  |  |  | not captured |  |  |  |
| 215 | LAST | 0x3E195 |  |  |  | not captured |  |  |  |
| 216 | LINE | 0x3E1F5 |  |  |  | not captured |  |  |  |
| 217 | M | 0x3E26F |  |  |  | not captured |  |  |  |
| 218 | MANY | 0x3E2CD |  |  |  | not captured |  |  |  |
| 219 | MINUS | 0x3E321 |  |  |  | not captured |  |  |  |
| 220 | MORE | 0x3E39D |  |  |  | not captured |  |  |  |
| 221 | MUST | 0x3E409 |  |  |  | not captured |  |  |  |
| 222 | N | 0x3E46F |  |  |  | not captured |  |  |  |
| 223 | NAME | 0x3E4C9 |  |  |  | not captured |  |  |  |
| 224 | NEGATIVE | 0x3E544 |  |  |  | not captured |  |  |  |
| 225 | NEW | 0x3E5DE |  |  |  | not captured |  |  |  |
| 226 | NO,KNOW | 0x3E63D |  |  |  | not captured |  |  |  |
| 227 | NOT,KNOT | 0x3E69D |  |  |  | not captured |  |  |  |
| 228 | NOW | 0x3E6F7 |  |  |  | not captured |  |  |  |
| 229 | NUMBER | 0x3E76A |  |  |  | not captured |  |  |  |
| 230 | 0 | 0x3E7E5 |  |  |  | not captured |  |  |  |
| 231 | O'CLOCK | 0x3E82D |  |  |  | not captured |  |  |  |
| 232 | OF | 0x3E892 |  |  |  | not captured |  |  |  |
| 233 | OFF | 0x3E8D9 |  |  |  | not captured |  |  |  |
| 234 | OLD | 0x3E923 |  |  |  | not captured |  |  |  |
| 235 | ON | 0x3E980 |  |  |  | not captured |  |  |  |
| 236 | ONLY | 0x3E9DE |  |  |  | not captured |  |  |  |
| 237 | OR | 0x3EA48 |  |  |  | not captured |  |  |  |
| 238 | P,PEA | 0x3EA90 |  |  |  | not captured |  |  |  |
| 239 | PARAMETER | 0x3EAC7 |  |  |  | not captured |  |  |  |
| 240 | PENCE | 0x3EB58 |  |  |  | not captured |  |  |  |
| 241 | PLEASE | 0x3EBBA |  |  |  | not captured |  |  |  |
| 242 | PLUS | 0x3EC45 |  |  |  | not captured |  |  |  |
| 243 | POINT | 0x3EC8B |  |  |  | not captured |  |  |  |
| 244 | POSITIVE | 0x3ECE6 |  |  |  | not captured |  |  |  |
| 245 | POUN- | 0x3ED64 |  |  |  | not captured |  |  |  |
| 246 | PRESS | 0x3EDCD |  |  |  | not captured |  |  |  |
| 247 | PROGRAM | 0x3EE3F |  |  |  | not captured |  |  |  |
| 248 | Q,QUEUE | 0x3EEC8 |  |  |  | not captured |  |  |  |
| 249 | R,ARE | 0x3EF1E |  |  |  | not captured |  |  |  |
| 250 | RED | 0x3EF59 |  |  |  | not captured |  |  |  |
| 251 | RESET | 0x3EFD1 |  |  |  | not captured |  |  |  |
| 252 | RETURN | 0x3F051 |  |  |  | not captured |  |  |  |
| 253 | RUN | 0x3F0E7 |  |  |  | not captured |  |  |  |
| 254 | RUNNING | 0x3F153 |  |  |  | not captured |  |  |  |
| 255 | S | 0x3F1E1 |  |  |  | not captured |  |  |  |
| 256 | SAME | 0x3F231 |  |  |  | not captured |  |  |  |
| 257 | SCORE | 0x3F29A |  |  |  | not captured |  |  |  |
| 258 | SECOND | 0x3F320 |  |  |  | not captured |  |  |  |
| 259 | SMALL | 0x3F39A |  |  |  | not captured |  |  |  |
| 260 | START | 0x3F41F |  |  |  | not captured |  |  |  |
| 261 | STOP | 0x3F49F |  |  |  | not captured |  |  |  |
| 262 | SWITCH | 0x3F4FB |  |  |  | not captured |  |  |  |
| 263 | T,TEA,TEE | 0x3F573 |  |  |  | not captured |  |  |  |
| 264 | TEN | 0x3F5CA |  |  |  | not captured |  |  |  |
| 265 | THANK | 0x3F62B |  |  |  | not captured |  |  |  |
| 266 | THAT | 0x3F684 |  |  |  | not captured |  |  |  |
| 267 | THE | 0x3F6DF |  |  |  | not captured |  |  |  |
| 268 | THEN | 0x3F724 |  |  |  | not captured |  |  |  |
| 269 | THIRD | 0x3F77E |  |  |  | not captured |  |  |  |
| 270 | THIS | 0x3F808 |  |  |  | not captured |  |  |  |
| 271 | TIME | 0x3F874 |  |  |  | not captured |  |  |  |
| 272 | TRY | 0x3F8E2 |  |  |  | not captured |  |  |  |
| 273 | TWELVE | 0x3F953 |  |  |  | not captured |  |  |  |
| 274 | TYPE | 0x3F9D2 |  |  |  | not captured |  |  |  |
| 275 | U,YOU | 0x3FA21 |  |  |  | not captured |  |  |  |
| 276 | UH | 0x3FA91 |  |  |  | not captured |  |  |  |
| 277 | UP | 0x3FAC0 |  |  |  | not captured |  |  |  |
| 278 | V | 0x3FAF7 |  |  |  | not captured |  |  |  |
| 279 | VERY | 0x3FB5C |  |  |  | not captured |  |  |  |
| 280 | W | 0x3FBB8 |  |  |  | not captured |  |  |  |
| 281 | WANT | 0x3FC1D |  |  |  | not captured |  |  |  |
| 282 | WAS | 0x3FC6C |  |  |  | not captured |  |  |  |
| 283 | WERE | 0x3FCC2 |  |  |  | not captured |  |  |  |
| 284 | WHAT | 0x3FD1A |  |  |  | not captured |  |  |  |
| 285 | WHICH | 0x3FD6E |  |  |  | not captured |  |  |  |
| 286 | X | 0x3FDB5 |  |  |  | not captured |  |  |  |
| 287 | Y,WHY | 0x3FE11 |  |  |  | not captured |  |  |  |
| 288 | YEAR | 0x3FE86 |  |  |  | not captured |  |  |  |
| 289 | YES | 0x3FEFC |  |  |  | not captured |  |  |  |
| 290 | YOUR | 0x3FF3D |  |  |  | not captured |  |  |  |
| 291 | Z | 0x3FF9B |  |  |  | not captured |  |  |  |
