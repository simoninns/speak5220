# 8.1 Coded speech parameters

Measured with the synthetic vector sets [vectors `energy`, vectors `pitch`, vectors `k`]:

- **energy:** every index, voiced and unvoiced, with the K parameters near zero;
- **pitch:** every non-zero index;
- **K:** every index of every K, with the other K near zero, voiced; K1–K4 also unvoiced.

Each setting was held for six frames, so the filter settles. Every vector was captured twice through each path with the standard procedure.

**How the values were measured.** The model of [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model) was run on each vector with a table entry at its listed value, and at the values 1, 2 and 4 either side. An entry is **confirmed** when its listed value fits the captures strictly better than every other value tried (`investigation/captures/phase8/analysis/tables.csv`):

- **Energy:** 14 of 14 entries confirmed.
- **Pitch:** 63 of 63 confirmed. Each pitch period was also measured from the chip's output alone, and is the listed value plus one sample, for all 63 ([§8.6](excitation-interpolation.md#86-excitation)).
- **K:** 142 of 168 confirmed.
- **Chirp:** 26 of 52 confirmed ([§8.6](excitation-interpolation.md#86-excitation)).

An entry is left unconfirmed by the fit where a neighbouring value fits as well or slightly better. That happens because the model is not exact for voiced frames ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)): a nearby value can offset its one-code slips at a few samples. No neighbouring value made any vector exact, so the fit contradicts none of the listed values. The ROM reads below confirm every entry.

**Read from the ROM.** The energy, pitch and K tables were read directly from chip 1's parameter ROM through `PROM OUT` ([§8.10](prom-out.md#810-parameter-rom-output-prom-out)). All 248 entries equal Tables 8-4 to 8-6, including the 26 K entries the fit left unconfirmed [`investigation/captures/promout/chip1/`]. The chirp ROM is separate and does not appear on `PROM OUT`.

**Read from the die.** Both ROMs were also read from the die image.

- **The parameter ROM:** every readable bit of the 248 entries equals `PROM OUT`.
- **The chirp ROM:** a separate array of 52 words of 8 bits. All 52 equal Table 8-7, including the 26 chirp entries that the fit left unconfirmed. The die has no locations 52–63.

[`rom_chirp.csv`, `rom_parameters.csv` of the die analysis]

**Where the values come from.** Every value is measured: through the vectors, through `PROM OUT` ([§8.10](prom-out.md#810-parameter-rom-output-prom-out)) and, for the chirp and parameter ROMs, read from the die image. The pitch period is one sample longer than the stored pitch value ([§8.6](excitation-interpolation.md#86-excitation)). The original lists only coded index ranges, not values. The energy table is also the one in [US 4,335,277](../sources/patent-us4335277/index.md) (its Table IV). That patent's pitch and K tables belong to an earlier chip and differ, though it stores them in the same form: 10-bit two's-complement values in a ROM addressed by the coded index.

*Table 8-4. Energy. The coefficient by which the excitation is multiplied ([§8.8](lattice-filter.md#88-lattice-filter)), as a 10-bit two's-complement value. Index 0 is silence and index 15 the stop code; both give the value 0. A value of 0 is multiplied like any other, so it gives u₁₁ = +1 ([§8.8](lattice-filter.md#88-lattice-filter)).*

| Index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Value | 0 (silence) | 1 | 2 | 3 | 4 | 6 | 8 | 11 | 16 | 23 | 33 | 47 | 63 | 85 | 114 | 0 (stop) |

*Table 8-5. Pitch. The value held in the pitch register ([§8.6](excitation-interpolation.md#86-excitation)), and the pitch period it gives, which is the value plus one sample. Index 0 means unvoiced.*

| Index | Value | Period (samples) | Index | Value | Period (samples) | Index | Value | Period (samples) | Index | Value | Period (samples) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | — (unvoiced) | — | 16 | 30 | 31 | 32 | 50 | 51 | 48 | 91 | 92 |
| 1 | 15 | 16 | 17 | 31 | 32 | 33 | 52 | 53 | 49 | 94 | 95 |
| 2 | 16 | 17 | 18 | 32 | 33 | 34 | 53 | 54 | 50 | 98 | 99 |
| 3 | 17 | 18 | 19 | 33 | 34 | 35 | 56 | 57 | 51 | 101 | 102 |
| 4 | 18 | 19 | 20 | 34 | 35 | 36 | 58 | 59 | 52 | 105 | 106 |
| 5 | 19 | 20 | 21 | 35 | 36 | 37 | 60 | 61 | 53 | 109 | 110 |
| 6 | 20 | 21 | 22 | 36 | 37 | 38 | 62 | 63 | 54 | 114 | 115 |
| 7 | 21 | 22 | 23 | 37 | 38 | 39 | 65 | 66 | 55 | 118 | 119 |
| 8 | 22 | 23 | 24 | 38 | 39 | 40 | 68 | 69 | 56 | 122 | 123 |
| 9 | 23 | 24 | 25 | 39 | 40 | 41 | 70 | 71 | 57 | 127 | 128 |
| 10 | 24 | 25 | 26 | 40 | 41 | 42 | 72 | 73 | 58 | 132 | 133 |
| 11 | 25 | 26 | 27 | 41 | 42 | 43 | 76 | 77 | 59 | 137 | 138 |
| 12 | 26 | 27 | 28 | 42 | 43 | 44 | 78 | 79 | 60 | 142 | 143 |
| 13 | 27 | 28 | 29 | 44 | 45 | 45 | 80 | 81 | 61 | 148 | 149 |
| 14 | 28 | 29 | 30 | 46 | 47 | 46 | 84 | 85 | 62 | 153 | 154 |
| 15 | 29 | 30 | 31 | 48 | 49 | 47 | 86 | 87 | 63 | 159 | 160 |

*Table 8-6. Reflection coefficients K1–K10. Each value is a 10-bit two's-complement number, the coefficient times 512 ([§8.8](lattice-filter.md#88-lattice-filter)).*

| Index | K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 | K9 | K10 |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | -501 | -328 | -441 | -328 | -328 | -256 | -308 | -256 | -256 | -205 |
| 1 | -498 | -303 | -387 | -273 | -282 | -212 | -260 | -161 | -176 | -132 |
| 2 | -497 | -274 | -333 | -217 | -235 | -168 | -212 | -66 | -96 | -59 |
| 3 | -495 | -244 | -279 | -161 | -189 | -123 | -164 | 29 | -15 | 14 |
| 4 | -493 | -211 | -225 | -106 | -142 | -79 | -117 | 124 | 65 | 87 |
| 5 | -491 | -175 | -171 | -50 | -96 | -35 | -69 | 219 | 146 | 160 |
| 6 | -488 | -138 | -117 | 5 | -50 | 10 | -21 | 314 | 226 | 234 |
| 7 | -482 | -99 | -63 | 61 | -3 | 54 | 27 | 409 | 307 | 307 |
| 8 | -478 | -59 | -9 | 116 | 43 | 98 | 75 |  |  |  |
| 9 | -474 | -18 | 45 | 172 | 90 | 143 | 122 |  |  |  |
| 10 | -469 | 24 | 98 | 228 | 136 | 187 | 170 |  |  |  |
| 11 | -464 | 64 | 152 | 283 | 182 | 232 | 218 |  |  |  |
| 12 | -459 | 105 | 206 | 339 | 229 | 276 | 266 |  |  |  |
| 13 | -452 | 143 | 260 | 394 | 275 | 320 | 314 |  |  |  |
| 14 | -445 | 180 | 314 | 450 | 322 | 365 | 361 |  |  |  |
| 15 | -437 | 215 | 368 | 506 | 368 | 409 | 409 |  |  |  |
| 16 | -412 | 248 |  |  |  |  |  |  |  |  |
| 17 | -380 | 278 |  |  |  |  |  |  |  |  |
| 18 | -339 | 306 |  |  |  |  |  |  |  |  |
| 19 | -288 | 331 |  |  |  |  |  |  |  |  |
| 20 | -227 | 354 |  |  |  |  |  |  |  |  |
| 21 | -158 | 374 |  |  |  |  |  |  |  |  |
| 22 | -81 | 392 |  |  |  |  |  |  |  |  |
| 23 | -1 | 408 |  |  |  |  |  |  |  |  |
| 24 | 80 | 422 |  |  |  |  |  |  |  |  |
| 25 | 157 | 435 |  |  |  |  |  |  |  |  |
| 26 | 226 | 445 |  |  |  |  |  |  |  |  |
| 27 | 287 | 455 |  |  |  |  |  |  |  |  |
| 28 | 337 | 463 |  |  |  |  |  |  |  |  |
| 29 | 379 | 470 |  |  |  |  |  |  |  |  |
| 30 | 411 | 476 |  |  |  |  |  |  |  |  |
| 31 | 436 | 506 |  |  |  |  |  |  |  |  |

*Table 8-7. Chirp. The voiced excitation at each value of the pitch counter ([§8.6](excitation-interpolation.md#86-excitation)), as an 8-bit two's-complement value in units of 1/128. The ROM has 52 locations, 0–51; locations 21–51 hold 0, and a counter above 51 reads location 51.*

| Address | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Value | 0 | 3 | 15 | 40 | 76 | 108 | 113 | 80 | 37 | 38 | 76 |
| **Address** | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 |
| Value | 68 | 26 | 50 | 59 | 19 | 55 | 26 | 37 | 31 | 29 | 0, to 51 |

Every value has been read from the die's chirp ROM (see above).

**Frame format (confirms the original; New detail).** Each parameter is read **most significant bit first**, from a stream whose bytes are read least significant bit first ([§6](commands.md#6-description-of-commands)). Fields, in order:

| Field | Bits | Present when |
|---|---|---|
| Energy | 4 | always; 0 = silence, 15 = stop code, and the frame ends there |
| Repeat | 1 | energy 1–14 |
| Pitch | 6 | energy 1–14; 0 = unvoiced |
| K1, K2 | 5, 5 | not a repeat frame |
| K3, K4 | 4, 4 | not a repeat frame |
| K5–K7 | 4, 4, 4 | not a repeat frame, and voiced |
| K8–K10 | 3, 3, 3 | not a repeat frame, and voiced |

So frames are 4 (silence, stop), 11 (repeat), 29 (unvoiced) or 50 (voiced) bits. The lengths were confirmed from every byte of the Acorn PHROM A ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)). The bit order within fields is confirmed by the model reproducing the vectors, which were encoded most significant bit first: the unvoiced and noise vectors bit for bit ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). Read least significant bit first, energy index 1 would be 8 and pitch 1 would be 32, which the output rules out [p8-01, vectors `pitch`].

**What a repeat frame changes.** It loads energy and pitch and keeps the previous K indices. A voiced repeat frame after an unvoiced frame keeps K5–K10 at zero, as the unvoiced frame left them; an unvoiced repeat frame zeroes them, as any unvoiced frame does [p8-10, vectors `special`]. A silence frame keeps the previous pitch and K indices: only the energy changes.
