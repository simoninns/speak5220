# 8.8 Lattice filter

**New.** The lowest-order arithmetic is not known, and the overflow rule is a best fit ([Appendix B](gaps.md#the-lowest-order-arithmetic)). The filter is a ten-stage lattice computed once per sample, as in the original's patents. With *u*₁₁ the excitation times the energy, and *b*₁–*b*₁₀ the backward values from the previous sample:

    u11 = E × x                        (x: the excitation, in units of 1/128)
    for i = 10 down to 1:  u_i = u_(i+1) − K_i × b_i
    for i = 10 down to 2:  b_i = b_(i−1) + K_(i−1) × u_(i−1)     (b_(i−1) from the previous sample)
    b_1 = u_1;  the output is u_1

**Multiplier (New).** Every product, the energy's included, goes through one array multiplier, described in [US 4,335,277](../sources/patent-us4335277/index.md). It multiplies a 14-bit two's-complement operand *M* (the excitation × 64, or a filter value) by a 10-bit coefficient *K* (or the energy), recoded into five radix-4 Booth digits *d*₀–*d*₄ in −2…+2:

    d_i = −2·k(2i+1) + k(2i) + k(2i−1)     for i = 0 … 4, with k(n) bit n of K and k(−1) = 0
    p_0 = d0 × M if d0 ≥ 0, else NOT(|d0| × M)    (bitwise inversion: −|d0|·M − 1, without the +1)
    p_i = d_i × M                                  for i = 1 … 4: exact, negative digits included
    acc = (p_0 >> 2) + p_1
    for i = 2, 3, 4:  acc = (acc >> 2) + p_i
    product = (acc >> 1) OR 1      (the patent: "P0 is always made a logical one")

*K* is taken as a 10-bit two's-complement number (the energy values, 0 to 114, are positive). `>>` is an arithmetic shift, rounding down. The partial sums are exact: nothing wraps inside the multiplier, only the sums of the lattice (below). Examples:

| *K* | *M* | Digits *d*₀…*d*₄ | Product |
|---|---|---|---|
| 0 | any | 0, 0, 0, 0, 0 | 1 |
| 33 | 192 | 1, 0, −2, 1, 0 | 13 |
| 114 | 4096 | −2, 1, −1, 2, 0 | 911 |
| −18 | 3 | −2, 0, −1, 0, 0 | −1 |
| −18 | −3 | −2, 0, −1, 0, 0 | 1 |
| 506 | 1000 | −2, −1, 0, 0, 2 | 989 |
| −501 | −777 | −1, −1, 1, 0, −2 | 761 |
| 409 | −8192 | 1, −2, 2, −2, 2 | −6543 |
| −256 | 8191 | 0, 0, 0, 0, −1 | −4095 |

This matched the chip better than every other form tried:

- the exact product, rounded down, towards zero or to nearest, with or without the forced 1;
- each partial product truncated;
- the +1 kept or dropped at each Booth stage;
- K negated before or after the product;
- (from the patent) stage 0 seeing only the twelve high bits of *M*, and the previous output fed back with its four or six low bits dropped, in the multiplier or in the adder. Each fitted the vectors as well as or worse than the form above, by up to 20 times [vectors `pitch`, `k`, `interp`, `energy`].

**From the die (provisional).** The die image agrees with this form ([the multiplier's low end](../die-analysis/multiplier-low-end.md)).

- **Stage 0 is built without an adder.** Its cells lack the half that adds the incoming partial sum, so the +1 that would complete its negative digit has no carry-in to enter.
- **Stages 1–3 are one cell, with identical bit-0 tie-offs.** Each stage's own +1 enters there. Stage 4 is a different cell.
- **Bits 1 and 2 of every stage are the ordinary cell.**

No reading of the bit-0 tie-offs fitted the vectors better than the form above.

The summer side agrees too ([the adder and the delay stack](../die-analysis/adder-and-delay-stack.md), provisional):

- **Each bit of the adder, the summer multiplexer, the Y latch and the multiplier multiplexer is one repeated cell,** the same at bit 0 as in the middle. So none of them has extra logic at its lowest bits, and nothing saturates at the top.
- **The multiplier's input wiring was traced.** It fits this form only if the array sees 2*M* with a 0 below its lowest bit, which gives exactly the same products as the form above.

Using it for the energy product too, as the patent describes, cut the mismatch on the energy and pitch vectors by a factor of 4 to 5. The forced 1 also explains the idle output: with the energy and all coefficients zero, *u*₁₁ = +1 and each stage subtracts 1, so *u*₁ settles at −9 and the code is −1 [vectors `energy`, vectors `pitch`].

**Word length and overflow.** The filter's values are 14-bit two's complement, −8192 to +8191. Every sum (*u*₁₁, each *u*ᵢ and each *b*ᵢ) wraps at 14 bits; nothing saturates. US 4,335,277 gives the adder, the delay stack and the multiplier input as 14 bits (D0–D13). On data that stays in the speech range the wrap never acts. On the loud and random vectors, wrapping every sum cut the model's total difference from the chip to a third (loud) and a half (random), against unbounded sums. These did worse:

- wrapping at 15 bits;
- saturating at 14 bits;
- wrapping only the *u* chain, or only the *b* chain.

[vectors `loud`, vectors `random`.] Those vectors differ from the chip in about 40 % of samples: their first one-code difference (below) grows without limit, as it does on the chip between takes ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). So the 14-bit wrap is the best fit, not a proof ([Appendix B](gaps.md#overflow)).

**Output.** The `I/O` code is *u*₁ shifted right by 4, with rounding down, as the top ten bits of the 14-bit value: YL13–YL4 ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)).

## The lowest-order arithmetic

The filter's lowest-order arithmetic is not known ([Appendix B](gaps.md#the-lowest-order-arithmetic)). With this multiplier the model's voiced output is one code off at about one sample per pitch period, at a fixed point in the period, mostly where the waveform is near zero [p8-04, p8-05]. No change to the tables, the multiplier forms above, the chirp or the timing removed it. What is known about it:

- **It is deterministic.** Voiced takes through the two paths, whose noise states differ, are identical. So the noise register plays no part [vectors `energy`, vectors `pitch`].
- **It lives in the silent part of each pitch period.** After the chirp ends (counter 21 and above), the excitation is zero, and the filter rings within a few LSBs of zero. There the chip settles into a different low-order limit cycle from the model. For example, the chip holds code −1 where the model alternates between 0 and −1 [vectors `energy` v_e14]. Unvoiced frames, whose excitation is never zero, match almost exactly through the same lattice.
- **It lasts through silence.** A silence frame keeps the K and stops the excitation, so the filter rings with nothing to excite it, for as long as the silence lasts. In a 12-frame pause, chip 1 stays between codes −2 and +1 to the end; the model starts between −1 and +2 and falls back to 0 and +1 [p12-01, p12-02]. Whether a pause rings at all can differ: in one pause the chip rings between −1 and 0 where the model is exactly 0, and with neutral K ([§12.2](authoring.md#122-when-a-frame-is-heard)) the chip is exactly 0 where the model rings between −1 and 0, every few samples [p12-07, p12-08]. In the other two pauses tested, both are exactly 0 with neutral K [p12-05, p12-06].
- **It is period-doubled.** At pitch index 10 (a period of 25 samples, one interpolation period) the chip's output repeats only every 50 samples. The model's repeats every 25. At periods of 24 and 26, both repeat every period [vectors `pitch`].
- **It is not a single event.** The first pitch period of a vector matches exactly. No single ±1 or ±2 change to any one operation in the samples before the first difference makes the rest match.
- **It depends on the energy.** Energies whose lowest Booth digit is negative (u₁₁ = −1 at zero excitation) show 2–30 times more differences than the others. A fixed u₁₁ at zero excitation fits worse, whatever its value.
- **It is not only in the tails.** With a strong resonance (K2 = 506), the first difference comes 6 samples into the very first chirp, while the coefficients are still arriving one per PC after the first jump [p9-01, vectors `ring`]. In steady state, with such a resonance, the chip's output is not periodic with the pitch period, while the model's is; the chip's is still deterministic (identical across takes and both paths) [vectors `k` k2_31].
- **Large coefficients hide it.** In the K sweeps, almost every non-neutral K6, and large negative K7–K10, leave far fewer differences than the neutral vector; positive K8–K10 leave more [vectors `k`].
- **Ruled out** (each fits as well or worse, on the ring, energy, pitch and K vectors):
    - a disturbance of each K once per interpolation period, at or around its PC (the K set to 0, halved, doubled, negated or with its low five bits cleared), seen by either chain or both;
    - a half-written K (some bits new, the rest old) in the first sample a new value is used;
    - *b*₁ taken from the Y latch with its 2, 4 or 6 low bits cleared, set or half-set, in the multiplier, the adder or both;
    - rounding towards zero, or sign-magnitude products, in one chain only;
    - every combination of stages negated by inversion without the +1, scored by how long each vector stays exact;
    - a free correction of ±2 or ±4 to the energy product at each chirp value: it could not make any energy vector exact, and the corrections it chose were inconsistent between energies;
    - the product's lowest bit forced to 0 instead of 1, in any combination of the energy, *u* and *b* products. This is how the Mega99 gate-level core (built from the schematics of US 4,335,277) reads the patent. It is 1.6 to 7 times worse, and its lattice otherwise has exactly the structure above;
    - a ±1 change to one product's lowest bit on alternate samples only, a state that would flip every sample;
    - the multiplier of US 4,700,323 (TI, 1984), which shifts each partial product before adding it, with exact or inverted negation and with or without the forced 1, for any one role or all of them.
- **Outside sources.** No published source settles this:
    - no netlist or trace of the filter of any TI LPC chip is public. The 2013 decaps of the TMS5220NL were used only for its ROMs and some control logic;
    - the software decoders and VHDL cores found do not claim a bit-exact multiplier, and share one lineage;
    - no bit-exact comparison of a real chip with an emulator has been published.

    The Mega99 core is the one independent reading of the patent, and it reproduces the model's lattice, not the chip. So what is missing is probably a detail of the TMS5220 that the TMC0280 and TMS5100 patents do not describe.
- **A second chip.** The ring, pitch and energy vectors were captured on chip 4 and compared with chip 1, sample for sample and with the model. Each vector was scored by its first departure from the model:

    | Set | Pairs (vector × path) | Both chips depart at the same sample, same sign | Only chip 1 departs there | Only chip 4 departs there | Both exact |
    |---|---|---|---|---|---|
    | pitch | 126 | 121 | 3 | 2 | 0 |
    | ring | 96 | 62 | 24 | 9 | 1 |
    | energy | 56 | 15 | 10 | 5 | 26 |

    - **The voiced one-code error is in the design.** On the pitch vectors both chips leave the model at the same sample, by the same ±1, and stay almost identical after it. Over the pitch set chips 1 and 4 differ from the model on 6887 and 6360 samples, but from each other on only 2589, mostly after the first departure. So the error is a property of the TMS5220, not of one part, and a better model of the arithmetic can still remove it.
    - **A second error is specific to each chip.** In about a third of the ring vectors (strong resonance, noise or a large K₁ or K₂) and some energy vectors, one chip departs from the model by ±1 at a sample where the other matches it. Each chip's departures repeat from take to take, and chip 1's were unchanged a day later and after it was removed and refitted. Once one chip departs, the resonant filter carries the difference on, so the two chips' outputs then differ by one or a few codes for the rest of the vector. **They do not depend on the clock.** Chip 4 was captured again with `OSC` at 480, 560 and 720 kHz (ring vectors and ten pitch vectors). 13 or 14 of its 15 or 16 own departures recurred at the same sample with the same value at every clock, and its pitch outputs at 560 kHz were bit-for-bit those at 640 kHz. So they are not a timing margin. Chips 1 and 4 carry the same marking and date code (TMS5220NL, DEP 8423), so a die revision is unlikely to explain them either. One mechanism fits a departure that is chip-specific, data-dependent, repeatable and independent of the clock: a marginal level within a phase, such as charge sharing on a precharged node, or a ratioed NMOS gate near its threshold. Either depends on device parameters that vary from chip to chip, not on time. A single model can match only one chip on the vectors where this happens.
    - **Chip 4 is less stable, and its upsets depend on the clock.** At 640 kHz its two takes of a vector agreed in 65 of 95 ring pairs, against 95 of 100 for chip 1. They agreed in 67 of 96 at 480 kHz, 66 of 93 at 560 kHz and 92 of 94 at 720 kHz. At 720 kHz the two remaining upsets were one sample each. The takes that disagree latched their Speak in the same slot, and they part at varied points within the vector, so these are upsets, not a different noise start. That upsets become rarer as the clock gets faster fits charge leaking from dynamic nodes during the clock phases ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). Its DAC also differs: the gain fitted to the DAC table of [§8.2](dac-and-audio.md#82-da-conversion) was about 1.24, against 1.16 for chip 1, with 3.5 mV rms residual.
    - Chips 2 and 3 were tried first and could not be used (see Conventions). Chip 1 worked normally in the same socket.
- **Some of it is marginal.** In the unvoiced K sweeps, the coefficients that leave the model early are those whose second Booth digit *d*₁ is negative. Every K1 with *d*₁ ≥ 0 matches the model in all 4,784 of its products. Chip 1 was refitted and these 384 takes were captured again two days after the originals. 340 were bit-for-bit identical, 6 moved because their Speak latched in a different slot, and every other change was in a vector with *d*₁ < 0: from the same state, the chip took a different ±1 than before. Chips 1 and 4 leave the model at the same samples on such a coefficient (ring vector k1_00, K1 = −501), so the effect belongs to the design. But its outcome is not purely digital, which fits a level effect in the negative-digit path of the multiplier's stage 1. No rule in the product's own operands reproduces it: the array's internal bits, the adder's carries, and stale low bits of *b*₁ were all tried [vectors `k`, `loc`; `investigation/captures/vectors_chip1_repeat/`].
- **A fit at scale finds no operand rule.** Every departure in the vector sets (chips 1 and 4) and in the PROM words was searched for the single product correction that keeps the model matched longest (`investigation/tools/fitsearch.py`). Single entries of the product table were then fitted to the vector sets, holding the PROM words out (`investigation/tools/tablefit.py`).
    - **Most departures fit ±1 or ±2 on one product,** but a median of 11 products tie for it.
    - **One entry improves the vectors a lot:** *K* = 10 (K6's neutral value) with *M* = −2 giving +1 instead of −1. With it, exact takes rise from 323 to 377. But it barely changes the PROM words (+0.1 %), and it fits in different chains on the two chips.
    - **Further entries improve the vectors and make the PROM words worse.**
    - **The multiplier variants tried at the same time all fit worse:** a stage 1 +1 conditioned on *d*₀, a stage 0 +1 conditioned on *d*₁, rounding at a shift, and a carry-save array.
    
    So no fixed function of a product's operands explains the error, and the model keeps the multiplier form above ([the lowest-order arithmetic](../die-analysis/lowest-order-arithmetic.md)).
- **Expected agreement.** Until the arithmetic is known, these are the measured distances between outputs of the same utterance, from `investigation/tools/variance.py` [`investigation/captures/phase9/variance/`]. Each is taken over the speech window and given as the median over pairs, and the model is the emulator (they agree on every take). The signal-to-difference ratio (SDR) is the chip's code power over the difference's power.

    | Pair | Set | Pairs | Exact | Samples equal | SDR (dB) | Largest difference (codes, 90th percentile) |
    |---|---|---|---|---|---|---|
    | Chip 1, two takes | pitch, ring, energy, K | 791 | 99 % | 100 % | identical | 0 |
    | Chip 1, two takes | Acorn words | 655 | 23 % | 93.4 % | 37.5 | 2 |
    | Chip 1, two days apart | unvoiced K sweeps | 380 | 90 % | 100 % | identical | 1 |
    | Chip 4 against chip 1 | pitch | 249 | 13 % | 98.8 % | 37.1 | 1 |
    | Chip 4 against chip 1 | ring | 189 | 3 % | 85.3 % | 22.1 | 6 |
    | Chip 4 against chip 1 | energy | 110 | 50 % | 99.9 % | 75.5 | 1 |
    | Model against chip 1 | pitch | 252 | 0 % | 95.7 % | 30.9 | 1 |
    | Model against chip 1 | ring | 192 | 2 % | 81.0 % | 21.9 | 24 |
    | Model against chip 1 | energy | 112 | 48 % | 99.8 % | 52.1 | 1 |
    | Model against chip 1 | K | 1,056 | 9 % | 98.6 % | 38.6 | 12 |
    | Model against chip 1 | Acorn words | 1,660 | 0 % | 79.4 % | 32.6 | 4 |
    | Chip 4 against chip 1 | Star Wars words | 89 | 0 % | 75.8 % | 35.6 | 5 |
    | Chip 4 against chip 1 | `vm61003_words` | 908 | 0 % | 79.4 % | 32.0 | 3 |
    | Model against chip 1 | Star Wars words | 115 | 0 % | 72.7 % | 32.5 | 46 |
    | Model against chip 1 | `vm61003_words` | 1,160 | 0 % | 78.1 % | 31.7 | 7 |
    | Model against chip 4 | pitch, ring, energy | 555 | 10 % | 95.6 % | 30.7 | 7 |

    - **The model is as close to chip 4 as to chip 1.** It is not fitted to one part.
    - **On PROM words, two chips agree no better than the model agrees with either** (with the three-sample pitch hold of [§8.6](excitation-interpolation.md#86-excitation)): on `vm61003_words` chip 4 is 32.0 dB from chip 1 and the model 31.7 dB; on the Star Wars words 35.6 and 32.5 dB. A chip agrees much better with itself (35–41 dB), because its upsets are few and its departures from the model repeat.
    - **On strongly resonant data, the model is as close to either chip as the two chips are to each other** (ring: 81–84 % against 85 %, 22–23 dB against 22 dB).
    - **On ordinary voiced data, the model is further from the chips than they are from each other**, by about 6 dB (pitch: 31 dB against 37 dB). This is the error of the lowest-order arithmetic. On real speech (the Acorn words) it is about 5 dB below the chip's agreement with itself: 32.5 dB against 37.5 dB.
    - **Every set has a typical difference of one code.** Larger ones come from a one-code difference growing in a resonant or overloaded filter, as they do between takes on the chip itself ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). The loud, random and decay vectors, not in the table, are dominated by that growth: their median SDR is 18–28 dB, and chip 1's own takes of them agree at a median of 45–99 dB.
    - **The acceptance tests of the capture work ([capture overview](../capture/index.md)) use these figures as a tolerance.** The emulator's median SDR against chip 1 may be at most 7 dB below the agreement of a second chip, or of chip 1's own takes, counting agreement above 40 dB as 40 dB. **This tolerance is observed, not proven.** It comes from two chips of one date code, at one temperature and supply. Nothing shows that it bounds the chip on other data, other parts or other conditions.
- **Also ruled out** (each fits as well or worse, on voiced energy, pitch and K vectors):
    - every completion of the Booth array's inverted partial products (0, +1 or +2 at each stage), with each of four final roundings;
    - sign-magnitude rounding of products;
    - subtraction by an inverted product without carry-in, with and without the forced 1;
    - undriven low bits read as ones (the excitation's six low bits, the Y latch's four low bits);
    - truncating the multiplier's 14-bit input by 1–3 bits;
    - a one-sample gap while a parameter is interpolated;
    - shifting when either chain sees a new K;
    - ±1 or ±2 on any of the near-zero K entries;
    - the patent's 1/4 at IP 6;
    - a refitted chirp, which halved the error on its training vectors and doubled it on others.
