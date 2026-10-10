# Lowest-order arithmetic

The one thing between the model and bit-exact agreement with the chip (acceptance test 1) is the TMS5220's lowest-order arithmetic: the model's voiced output is one code off at a few samples per pitch period, and no form of the multiplier or adder tried so far removes it ([datasheet §8.8](../datasheet/lattice-filter.md#the-lowest-order-arithmetic), [known gaps](../datasheet/gaps.md#the-lowest-order-arithmetic)). This page collects what any answer has to explain, what the data and the die show so far, and the analyses and bench tests that could still settle it. Numbers come from the emulator regression (`investigation/captures/phase9/regress/`), from the first-departure search, or from quick checks noted as such.

Conventions as in the datasheet: *K* is a 10-bit two's-complement coefficient, *M* the 14-bit multiplicand, *d*₀–*d*₄ its radix-4 Booth digits (−2…+2, *d*₀ least significant), *u*ᵢ and *b*ᵢ the lattice values, and a "product" is one of the 20 multiplications in a sample period (energy, the ten *u* products, the nine *b* products).

## 1. What a candidate must explain

Each of these is measured, on chip 1 unless said otherwise. A candidate that fails one of them is wrong, which makes most of them useful as fast filters.

1. **The first pitch period of every vector is exact,** and unvoiced frames with neutral K match 32,699 samples bit for bit [vectors `energy`, `noise`]. So the tables, the timing, the excitation and the multiplier's form for ordinary operands are right. The fault is sparse.
2. **It is deterministic and shared.** Two chips of the same date code leave the model at the same sample by the same ±1 on 121 of 126 pitch-vector pairs [vectors `pitch`, chip 4]. It is a property of the design, so a model can reproduce it.
3. **It is sparse, not systematic.** Every systematic change tried (a rounding, a dropped carry, a forced bit) makes the match worse by 2 to 40 times. Whatever the fault is, it changes a product only under a condition met at a few per cent of the products at most, and the filter's feedback carries each change on for tens of samples.
4. **It depends on the coefficient's bit pattern, not its size** ([§2.1](#21-the-unvoiced-k-sweeps-follow-the-booth-digits)). In the unvoiced K sweeps, values whose Booth digits have *d*₁ < 0 (most strongly with *d*₀ = 0) depart early and often; their neighbours one index away do not.
5. **It depends on the energy's lowest Booth digit.** Voiced energy vectors with *d*₀ < 0 (so *u*₁₁ = −1 at zero excitation) show 2–30 times more differences than those with *d*₀ ≥ 0 [vectors `energy`].
6. **It is worst near zero.** In voiced tails (chirp over, excitation 0, every value within a few LSBs of zero) the chip settles into a different limit cycle from the model: it holds −1 where the model alternates 0 and −1 [vectors `energy` v_e14]. At pitch index 10 (period 25, one interpolation period) the chip's tail repeats every 50 samples, the model's every 25 [vectors `pitch`].
7. **But it is not only near zero.** With K2 = 506 the first difference comes 6 samples into the first chirp, with large values everywhere [p9-01, vectors `ring`], and unvoiced sweeps with a large K1 or K2 depart while the values are in the hundreds ([§2.1](#21-the-unvoiced-k-sweeps-follow-the-booth-digits)).
8. **Large coefficients can hide it.** Almost every non-neutral K6, and large negative K7–K10, leave fewer differences than the neutral vector; positive K8–K10 leave more [vectors `k`].
9. **Chip-specific departures exist too,** ±1 at a sample where the other chip matches, repeatable on each chip and independent of the clock from 480 to 720 kHz [vectors `ring`, chip 4]. A model can match only one chip there. These are a second population and should be kept out of the fit for the design error (datasheet §8.8).

Everything already ruled out is listed in datasheet §8.8 (two "Ruled out" lists and the survey of variants) and in [§2.4](#24-ruled-out) below. It is long; check it before trying a variant.

## 2. Observations and results

### 2.1 The unvoiced K sweeps follow the Booth digits

Unvoiced frames with neutral K match almost exactly. The K sweeps also contain unvoiced vectors, `uk1`–`uk4`, each K1–K4 index with noise excitation [vectors `k`]. Of their 384 takes, 88 are exact and most of the rest differ in one sample at the end of speech. A few indices differ early and heavily, and their neighbours do not:

| Vector | *K* | *K* in binary | Booth digits *d*₀…*d*₄ | Samples differing | First difference |
|---|---|---|---|---|---|
| uk1_05 | −491 | `1000010101` | 1, 1, 1, 0, −2 | 0.2 % | 1403 (end of speech) |
| **uk1_06** | **−488** | `1000011000` | **0, −2**, 2, 0, −2 | **11.8 %** | **210** |
| uk1_07 | −482 | `1000011110` | −2, 0, 2, 0, −2 | 0.2 % | 1403 |
| uk1_00 | −501 | `1000001011` | −1, −1, 1, 0, −2 | 3.6 % | 225 |
| uk1_10 | −469 | `1000101011` | −1, −1, −1, 1, −2 | 1.3 % | 395 |
| uk2_22 | 392 | `0110001000` | **0, −2**, 1, −2, 2 | 3.0 % | 248 |
| uk2_23 | 408 | `0110011000` | **0, −2**, 2, −2, 2 | 3.4 % | 247 |
| uk2_00, uk4_00 | −328 | `1010111000` | **0, −2**, 0, −1, −1 | 1.7 %, 2.0 % | 214, 244 |
| uk2_10 | 24 | `0000011000` | **0, −2**, 2, 0, 0 | 1.5 % | 252 |
| uk2_30 | 476 | `0111011100` | **0, −1**, 2, −1, 2 | 1.6 % | 363 |
| uk2_03 | −244 | `1100001100` | **0, −1**, 1, 0, −1 | 0.9 % | 220 |
| uk3_11 | 152 | `0010011000` | **0, −2**, 2, −2, 1 | 0.5 % | 240 |

(Per index: the mismatched samples over all four takes, two per path, from `investigation/captures/phase9/regress/vectors_k.csv`; the digits from `Model.boothDigits`.)

The split is clean. K1–K4 have twelve values with *d*₀ = 0 and *d*₁ < 0, that is a low nibble of `1000` or `1100`: all twelve depart before the end of speech (first difference 210–494), nine of them at 0.5 % or more, and the strongest cases in the whole sweep are all of that form. The three light ones are 248 (0, −2, 0, 0, 1), 172 (0, −1, −1, −1, 1) and −452 (0, −1, 0, 1, −2). The twelve values with *d*₀ = 0 and *d*₁ ≥ 0 (low nibble `0000` or `0100`) never depart before the end of speech. A weaker group has *d*₀ = −1 with *d*₁ < 0 (uk1_00, uk1_10, uk1_15, uk2_19, uk4_11); *d*₀ = +1 with *d*₁ < 0 is mostly clean. The K1 row is the most sensitive, as expected: its products are the largest.

The first-departure search ([§2.2](#22-the-first-departure-search)) sharpens this on the K1 sweep. **Every K1 value that errs has *d*₁ < 0, and every K1 with *d*₁ ≥ 0 has no corrections at all in 4,784 products.** *d*₀ = 0 is not needed (−501 has *d*₀ = −1, −339 has *d*₀ = +1). The *d*₁ < 0 values with *d*₂ = 0 err little or not at all in K1 (379: none, −452: 2), but −328 (*d*₂ = 0) errs in K2 and K4.

So in the array, something goes wrong when stage 1 is a *negative* partial product, which is an inverted multiplicand plus a completing one, most strongly when stage 0 contributes nothing (*d*₀ = 0: no cell of stage 0 is selected, its partial sum is "empty"). That is a hardware condition, not an arithmetic one, and it is exactly the kind of detail US 4,335,277 glosses over: its stage 0 is built from different cells (A-1, A-2) from stages 1–4 (B-1, B-2, B-3, B-C), and the text says only that stage 0 "receives only −2, −1 and +1 outputs". The die shows why stage 0 differs: it has no adder ([multiplier low end](multiplier-low-end.md#stage-0-has-no-adder)).

The voiced sweeps do not contradict this, but they are dominated by the tail regime (constraint 6), so they are a poorer test of it. The signature gives a **fast filter**: a candidate arithmetic must produce its errors on these K values and not on their neighbours. The unvoiced takes are also the cleanest data there is: no tails, a known excitation, and a large K in one place.

### 2.2 The first-departure search

**The method.** The model is exact up to the first departure of every take, so the operands of all 20 products in the samples just before it are known exactly. For each departure, the search finds the single product corrections (±1, ±2, in the departure sample and the ones before it) that keep the chip matched for as long as possible afterwards, applies the best one, and continues to the next departure. Each step yields a triple (*K*, *M*, correction). The triples are a data set from which to read a rule: tabulate the corrections against the low bits of *M*, against the two bits shifted out at each Booth stage, against the sign and low bits of each partial sum, and against the operands of the adder the product feeds.

Two things have to be handled:

- **Degeneracy.** With neutral K, a ±1 on any *u* product between *u*₂ and *u*₆ gives the same *u*₁ and nearly the same *b* chain, so the search cannot tell them apart (6 equivalent corrections per departure on uk2_22). It can always tell the *sample* and the *sign*. Vectors with two or three large coefficients ([§5.2](#52-localisation-vectors)) are meant to localise the product, so that an error in one product reaches the output by a different route from an error in the next. The unvoiced K1 sweep is already localised: the only large product is K1's.
- **The two chips.** Running it on chip 1 and chip 4 takes of the same vector and keeping only the corrections both need drops the chip-specific departures (constraint 9).

**A pilot** (a subclass of `investigation/tools/model.py`'s `Model` with a hook in `lattice()`, a few seconds a take) shows that the method works. On uk1_00, a single −1 on the K1 × *b*₁ product (K = −501, *M* = −639, model product 625) two samples before the first visible difference keeps the chip matched for the following 57 samples; on uk1_06, +1 on the same product (K = −488, *M* = −75, model product 71) for 23 samples. Two samples before is expected: the output is *u*₁ ≫ 4, so a one-LSB change in the state is usually invisible until the filter has moved it across a code boundary.

**At scale.** `investigation/tools/fitsearch.py` records each take's filter inputs once from `model.py`. The energy, the excitation and the coefficients do not depend on the filter's state, so from then on only the filter is replayed. The products come from a table of the multiplier for every *K* and *M*, and each departure's 320 candidate corrections (20 products, ±1 and ±2, in the departure's sample and the 3 before it) are replayed together as one numpy batch. The replay equals `model.py` on all 1,069 takes of the K set. The whole data set runs in under 6 minutes on 16 cores. Results (`investigation/local/fitsearch/`, kept local):

| Source | Runs | Departures searched | Explained by one correction | Products tied (median) | Tied on one product only |
|---|---|---|---|---|---|
| Vectors (chips 1 and 4, and chip 1's repeat) | 4,338 takes | 18,903 | 15,077 | 11 | 504 |
| PROM words (the takes' majority) | 2,271 words | 5,635 | 3,365 | 12 | 254 |

**The search cannot place the error any closer than "*u*₁ off by ±2 in one of a few samples".**

- Over the uk1 sweep (128 takes, 282 useful departures), a correction to K1 × *b*₁ alone always does as well as the best correction to any product. A ±2 correction (which the forced-odd products can produce) always does as well as a ±1, but then each departure has 3–6 equally good candidates in neighbouring samples.
- Any error at *u*₂…*u*₁₁, the energy product included, reaches *u*₁ identically, and two levels of the *u* chain can be told apart only through a product pair K_i, K_(i+1) that are both large. So the localisation vectors (`loc` set, `investigation/captures/vectors/vectors_loc/`) separate less than hoped: unvoiced frames hold K5–K10 at zero. All 133 of their useful departures include K1 × *b*₁ among the tied candidates.
- Over everything, a departure is almost always explained by ±1 or ±2 on one product, but a median of 11–12 products tie. Most of the 758 departures that point at one product come from the tails of three voiced vectors: `energy` v_e13, `pitch` p19 and `ring` n_e13, repeated each period, on both paths and both chips. There, K1 × *b*₁ with *K* = −1 and *M* = 1 needs +1 where the model gives −1. The same operands occur about 9,500 times elsewhere, and there the chip gives −1. The label follows the run and the state, not the operands.

### 2.3 A fixed function of the operands

**No rule in the product's own operands fits the K1 sweep.** The features searched were the array's shifted-out bits, the low bits of each partial sum, the carries at each stage, the bits of *M* and of each partial product, the bits and carries of the *u*₁ adder, and the longest carry-propagation run in stage 1. Every candidate rule was required to cover the tied candidates at 85 % of departures. The best pairs still fire on about 20,000 of 167,000 operations where the chip agrees with the model.

**A table fit.** `investigation/tools/tablefit.py` asks whether the error is a fixed function of the operands. It changes single entries of the product table, picked from the corrections tied at each run's first departure, and scores each one by replaying every run that uses that entry. It fits the vector sets and holds the PROM words out.

- **One table entry stands out.** Changing *K* = 10, *M* = −2 from −1 to +1 is the only large single change. *K* = 10 is K6's neutral value (index 6), which constraint 8 pointed at. On its own, over all 6,609 runs:
  - exact runs rise from 323 to 377;
  - samples differing fall by 5.4 %;
  - the sum of first-departure indices rises by 26 %.

  On the PROM words it changes almost nothing (first departures +0.1 %), because there K6 is rarely 10 while the filter is near zero.
- **That entry depends on the chip and the chain.** Over chip 1's vector takes it fits best in both chains: exact takes rise from 259 to 313. Over chip 4's it fits best in the *u* chain only (K6 × *b*₆): 64 to 102 exact. In the *b* chain (K6 × *u*₆) it makes chip 4 worse. A design rule would be the same on both chips. The other entries near it in *M* fit worse: *M* = −1 and −3 make no difference, and −4 → +1 is much worse.
- **More entries overfit.** Fifteen rounds of four entries each added 55 entries:
  - exact vector takes rose from 323 to 396;
  - on the held-out PROM words, the first-departure sum fell 1.1 % and samples differing rose 0.2 %;
  - after the first round the entries are scattered over *K* and *M*, and some make a product even, which the forced 1 forbids.

  The fit is learning the vectors' limit cycles, not a rule.

### 2.4 Ruled out

Each of these fits worse than the present form.

**Carry placements, on the worst unvoiced takes and three clean ones (a quick check).** Five placements of the completing +1 of a negative partial product: the +1 entering before the preceding shift (so that it is usually lost), entering one alignment later, applied to the final shift, and with stage 0's own completion restored. All are 10 to 40 times worse than the present form on the clean takes, and no better on the bad ones. This agrees with constraint 3: a change that acts on every negative digit acts far too often. The present multiplier is right for almost every operand; the fault needs a *conjunction* of conditions (the coefficient's digits and something about *M* or the partial sums).

**On the unvoiced sweeps:**

- all 256 per-stage placements of a negative digit's completing +1: at the stage, into the previous stage's dropped bits, lost, or one alignment later. The present form is best; the only tie gives identical products;
- the forced P0 applied as an OR at an earlier stage and bit (20 placements);
- K1 × *b*₁ seeing *b*₁'s low 2 or 4 bits from another value (*b*₂, *u*₂, *b*₃, *u*₃, the excitation, *u*₁₁, zeros or ones). All are 4–18 times worse, so *b*₁ reaches the multiplier whole.

**By replaying every run** (the present form has 323 exact runs; the conditioned +1s come closest, at 289–319, and lose on every other measure; rounding and the carry-save array fall to 30–70):

- the +1 of stage 1 dropped when *d*₀ = 0, when *d*₀ ≤ 0, or when *d*₀ < 0;
- stage 0's +1 restored when *d*₁ < 0 or when *d*₁ ≥ 0, alone or with the stage 1 drop;
- rounding (adding half the shifted-out weight) at the shift into stage 3, at every shift, or only where the next digit is 0;
- a carry-save array, with sum and carry words shifted separately, with and without stage 0's +1, and with the final shift before or after the carry-propagate add;
- *K* = −1, *M* = 1 → +1 for every product, and *K* = −1, 0 < *M* < 512 → +1.

**From the die:** every tie-off of bit 0 in each stage, passing stage 0's +1 on to stage 1, an unselected stage 0 that puts out all ones ([multiplier low end](multiplier-low-end.md#tie-offs-scored-in-the-model)), and a 1 or *M*'s sign on the extra MR line below the multiplicand's lowest bit ([adder and delay stack](adder-and-delay-stack.md#mr-wiring-scored-in-the-model)).

### 2.5 A marginal operation

- **It is not chip 1's alone.** On ring vector k1_00 (K1 = −501), chips 1 and 4 need the same corrections at the same samples through the first 200 samples, at large amplitudes.
- **The *d*₁ < 0 departures drift from day to day.** Chip 1 was refitted and its 384 unvoiced-sweep takes were captured again two days after the originals (`investigation/captures/vectors_chip1_repeat/`, kept local). 340 were bit-for-bit identical. Six moved because their Speak latched in another slot. Every other change was in a vector whose coefficient has *d*₁ < 0 (uk1_00, 06, 10, 30; uk2_00, 22, 23, 30; uk3_11; uk4_00). There the chip took a different ±1 a few samples after the first departure, from the same state, and in uk1_06 and uk1_10 the first departure itself moved (188 → 184, 498 → 312).

The operation behind the [§2.1](#21-the-unvoiced-k-sweeps-follow-the-booth-digits) signature gives the same result nearly always, and the same on two chips over the first 200 samples of k1_00. But it is marginal: it can change with refit or temperature. That fits a level effect (charge sharing, or a ratioed gate near its threshold) in the negative-digit path of stage 1 better than a digital rule. It would also explain why no function of the product's own operands fits: the outcome would depend on what the node held before. If so, the unvoiced signature and constraint 9's chip-specific departures may be one mechanism. A model can then match a chip only statistically on these products. Whether the voiced tail error (constraints 5 and 6) is the same mechanism is open. It is the larger part of the mismatch, and nothing here tests it directly.

### 2.6 The corpus departures and the upsets

`investigation/tools/departures.py corpus repeat` labels every clean departure in the PROM words, with the model's state at each (`investigation/captures/phase10/departures/`; `events.csv` is kept local). Over every PROM: 15,851 deterministic departures, 387 upset starts and 56,999 controls, far more cases, and more varied, than the vectors.

- **Every departure starts at the edge of an output step.** Where the chip is one code below the model, the model's *u*₁ is 1 above a multiple of 16 in 74 % of cases. Where it is one code above, *u*₁ is 1 below in 68 %. So the chip's *u*₁ differs from the model's by a few units of the lowest bit, never more, and is seen only where that crosses a step.
- **Both kinds follow *d*₁ < 0,** independently for each K once the step position is allowed for (logistic fit, `summary.json`). For deterministic departures K8 (z = +42), K2, K3, K6, K1 and K4 matter most; for upsets K9, K7, K8 and K10. K7 marks an upset rather than a deterministic departure: at deterministic departures, once the other features are allowed for, its *d*₁ < 0 is rarer than at the controls (z = −10).
- **An upset is the same error at a marginal sample.** Each upset starts as a single ±1, towards the nearer output step, at a few fixed samples per word, and at each with a fixed chance (2 % to about 50 % over 49 takes, `departures.py repeat`). Where the model also departs there, the takes that leave usually give the model's value.

A candidate rule should therefore: give the corpus's deterministic departures with the right sign; leave the controls alone; and put the upset samples nearest the margin, in the order of their chance. The repeat test gives that chance for the six words in `investigation/captures/phase10/repeat/`.

### 2.7 Reading

The vectors cannot be fitted by any function of a product's own operands that also holds on the PROM words. The one entry that helps a lot behaves differently on the two chips and in the two chains. This agrees with the drift of [§2.5](#25-a-marginal-operation): the error belongs to the design (the chips leave the model at the same samples), but which way an operation goes depends on the state and on the part, as a level effect would. A rule read from the departures has gone as far as these data allow. What could still move it is listed under open work: the temperature and supply test ([§5.5](#55-temperature-and-supply)), a delayered die image ([die analysis](index.md#ways-of-working)), and vectors that let the chip itself label an operation ([§4.6](#46-the-previous-contents-and-labelling-an-operation)).

## 3. What the die shows

The die image has been read as far as a top-metal image allows ([die analysis](index.md)):

- **The array is found.** Bits 1 and 2 of every stage are the ordinary cell. Bit 0's inputs from below are tied off, and no reading of the ties improves the model ([multiplier low end](multiplier-low-end.md#bit-0-inputs-tied-off)).
- **Stage 0 has no adder,** which explains its missing +1 ([multiplier low end](multiplier-low-end.md#stage-0-has-no-adder)).
- **Stages 1–3 are one cell,** so the *d*₁ < 0 signature points at stage 1's input from stage 0 ([multiplier low end](multiplier-low-end.md#where-this-leaves-t1)).
- **The summer side is one cell per bit.** The adder, the summer multiplexer and the Y latch are the same at bit 0 as in the middle, and the Y latch is 14 bits wide ([adder and delay stack](adder-and-delay-stack.md)).
- **The traced multiplier input fits the model only with a constant 0 below its lowest bit.** That bit comes from a separate cell, which makes it a second candidate for a marginal node ([adder and delay stack](adder-and-delay-stack.md#mr-wiring-scored-in-the-model)).

The remaining candidates need levels and timing, which a top-metal image cannot show. The die is the only route that can also explain the chip-specific departures (a node with a weak pull-up, a long diffusion line), so the die work waits for a delayered image ([open work](index.md#open-work)).

## 4. Open work: analysis

None of these needs bench time. A rule is accepted when, stated in terms of the operands, it reproduces every triple of the first-departure search and, put into `model.py`, makes the unvoiced sweeps exact and raises the voiced sets above 99 %; on the corpus it must also meet the conditions of [§2.6](#26-the-corpus-departures-and-the-upsets).

### 4.1 A systematic family of array wirings

Instead of hand-picked variants, enumerate a parametrised family of the patent's array and score each member first on the ten worst and ten cleanest unvoiced takes (about a second a take), then on the ring set. Parameters worth including:

- for each stage, where the completing +1 of a negative digit enters: at the stage, one alignment early, one alignment late, or never;
- for each stage, whether the two bits shifted out are dropped, rounded into the kept sum, or kept as a carry into the next stage;
- for stage 0, what an unselected stage leaves on its lines: zero, all ones (a precharged bus that nothing discharged), or the previous product's low bits (a bus that was not precharged);
- the sign extension width when the partial sum shifts (whether a partial sum of −2*M* for *M* near −8192 wraps);
- the final rounding, with and without the forced 1.

The [§2.1](#21-the-unvoiced-k-sweeps-follow-the-booth-digits) signature prunes hard: any member that errs on *d*₀ = 0, *d*₁ ≥ 0 values, or on every negative digit, is out at once. Parts of this family are already scored ([§2.4](#24-ruled-out)): every per-stage placement of the +1, rounding at the shifts, and an unselected stage 0 left at all ones. The previous product's low bits on an unselected stage 0 is not among them. The all-ones case was the one the signature pointed at: a stage-0 bus left at all ones is −1, and −1 shifted right stays −1, so it is not a tiny error at a low alignment but a full −1 at the final one, *provided* the stage-1 partial product's own completion interacts with it.

### 4.2 The patents' stage-0/stage-1 interface

US 4,335,277 FIGS. 10a–10c and the text at "Array multiplier 401" describe the cells. Questions to answer from them, with US 4,209,844 (the filter) and the Booth reference the patent cites:

- what the A-1 and A-2 cells of stage 0 do when no digit is selected, and what the B-C cell is (a carry cell?);
- where each stage's completing +1 is wired, and whether stage 0's is simply absent (the model's reading, which the die supports) or wired into stage 1;
- what the two bits shifted out of each row do: dropped, or fed to the next row's lowest cell;
- how the inverted multiplicand (inverters 430) and the "true logic to the B type blocks" are combined for a −1 and a −2 digit.

The project's [copy of the patent](../sources/patent-us4335277/index.md) has the text; the figures need the PDF. A reading that explains [§2.1](#21-the-unvoiced-k-sweeps-follow-the-booth-digits) is a hypothesis to put straight into [§4.1](#41-a-systematic-family-of-array-wirings)'s family.

### 4.3 The chip-specific departures

Each chip's own departures recur from take to take and do not depend on the clock, which fits a level margin on one node, data-dependent (datasheet §8.8). The same first-departure search, run on the takes where only one chip departs, gives the operands at those samples. If chip 4's departures all share a bit pattern in some operand (say, *M* with a run of ones in bits 4–7 at a stage where the digit is −2), that pattern names the node, and it may be the same node as the design error's. Cheap, and the data (ring, pitch and energy on both chips) is already captured.

### 4.4 The four unvoiced end-of-speech samples

Four of 68 unvoiced energy and noise takes differ from the model in one sample, where speech ends ([datasheet §7](../datasheet/power-up.md)). At that sample the energy has just been zeroed, so *u*₁₁ = booth(0, ±4096), and each K product sees a value that was large a sample earlier. It is one of the few places where a product with a zero coefficient meets a large multiplicand. Worth ten minutes: which value does the chip give, and which single correction explains it?

### 4.5 Loud and random data: wrap inside the array

Separate from the voiced error, but cheap. The loud and random vectors match only 49–54 % of samples because one-code differences grow without limit there, and the 14-bit wrap of the sums is "the best fit, not a proof" (datasheet §8.8). The model's partial sums inside the multiplier are exact; the patent's array is 14 cells wide with the MSB kept in the leftmost column, so a partial sum of (±2*M*) + (shifted sum) can exceed 14 bits for |*M*| near 8192. Wrapping each partial sum at 14 bits (or 15, if the row has a guard cell) costs one option in `model.py` and may lift the loud and random scores, which would also say how wide the rows are. The die shows 14 bit rows in A and no 15th ([floorplan](floorplan.md#bit-rows-and-word-length)).

### 4.6 The previous contents, and labelling an operation

- **Features from earlier operations.** A search with features from the operations before K1 × *b*₁ in the chip's time order, to look for the "previous contents" dependence that a level effect would have ([§2.5](#25-a-marginal-operation)).
- **Vectors that label an operation.** Vectors that hold the filter in one tail state for a long time and vary only one product's operands, so that the chip itself labels an operation.

## 5. Open work: bench tests

All are vector sets in the style of `investigation/tools/vectors.py`, captured with the standard procedure through both paths. Each is a few dozen vectors, an hour or two of bench time. Everything below is worth capturing on both chips, because the comparison is what separates the design error from the chip-specific ones (constraint 9). Captures that count for the reference set are made on chip 1, with `investigation/tools/bench.json` set for it.

### 5.1 The unvoiced K sweeps on chip 4

`uk1`–`uk4` from the existing `k` set, on chip 4. If chip 4 departs at the same samples as chip 1 on uk1_06, uk1_00, uk2_22, uk2_23 and uk4_00, the [§2.1](#21-the-unvoiced-k-sweeps-follow-the-booth-digits) signature is the design's, and the search can use both chips' takes. If not, it is chip 1's own, and §2.1 is about the marginal node instead. Either answer is useful.

### 5.2 Localisation vectors

Unvoiced frames (so no tails, and K5–K10 zero), with two of K1–K4 large at once, so that a product error in one stage reaches the output by a different route from one in the next:

- K1 = −488 (the worst value) with K2 at each of its four extreme-ish values (−328, 392, 408, 506), and the same with K2 neutral as the control;
- K2 = 392 with K1 at −501, −469, 436 and neutral;
- K1 = −488 with K3 = 152 and with K4 = −328.

Also the **energy's digits**: uk1_06 at energies 4, 8, 16 (*d*₀ = 0), 1, 33, 85 (*d*₀ = +1), 11, 47 (*d*₀ = −1) and 6, 114 (*d*₀ = −2), to see whether the energy product's digit pattern interacts with K1's (constraint 5 says the energy's *d*₀ matters in voiced tails; this asks whether it matters with noise too).

These are captured on chip 1 as the `loc` set (`investigation/captures/vectors/vectors_loc/`). Because unvoiced frames hold K5–K10 at zero, they separate the products less than hoped ([§2.2](#22-the-first-departure-search)).

### 5.3 Long free decay at a known state

The ring set gives 140 samples of free ringing per pitch period, but the chirp restarts it and the tail is near zero within a few tens of samples. A **decay** set gives much longer records in the zero-excitation regime with large values: a loud voiced frame (energy 14) with a resonant K set (K2 = 506, K1 at several values), followed by silence frames (energy 0). The silence frame keeps the K indices and zeroes only the energy, so the filter rings freely for 200 samples per silence frame at exactly known coefficients, from a large amplitude down to the limit cycle. Six to eight silence frames give 1,200–1,600 samples of decay per vector; a dozen vectors cover the K1 sweep at two resonances. The whole trajectory is a constraint on the arithmetic, and the amplitude passes through every scale, so it separates "wrong near zero" from "wrong at large values" (constraints 6 and 7) in one record. A *voiced* frame to a silence frame is not inhibited, so the energy ramps down over IP 1–7 before reaching 0; an **unvoiced** loud frame to silence *is* inhibited ([datasheet §8.7](../datasheet/excitation-interpolation.md)) and cuts the excitation in one step, which is cleaner. Include both.

Decay vectors of both kinds (`dv_…`, `du_…`) are part of the `loc` set on chip 1. No separate analysis of their decay records is reported.

### 5.4 The parity test for period doubling

Constraint 6: at a pitch period of 25 samples the chip's tail repeats every 50, the model's every 25. Two mechanisms would do that: a state in the arithmetic with period two samples (a 0/−1 alternation whose phase the chirp does not reset), or something that differs between the A and B samples of a parameter count, which flips its phase against the chirp every period only when the period is odd. The second is testable:

- pitch indices giving periods 24, 25, 26 (and 27, 49, 51) with neutral K at a loud energy, as in the `pitch` set, but each **preceded by one frame at another pitch**, chosen so that the chirp's phase against the frame clock at the end of the interpolation differs by exactly one sample between two otherwise identical vectors. The model predicts the chirp phase exactly, so the pairs can be designed from it;
- if the chip's tails in such a pair differ in a way the model (which is blind to A/B) cannot reproduce, the arithmetic depends on the sample's position in the PC, and the first-departure search must add that feature. If the pairs match the model's prediction equally well, it is a period-two state and the search should look for it in the low bits.

Speak External, which runs two samples earlier than Speak, does not change the parity, which is why the two paths have never shown this. This test is worth making only if the search shows a feature that depends on the sample's position.

### 5.5 Temperature and supply

Heating or cooling the chip, and `VDD` at the rails' limits, have not been tried. They do not bear on the design error directly, but they separate the populations: chip-specific departures and upsets should move or vanish with temperature and `VDD`; the design error should not move at all. A single run of the ring set hot and at low `VDD` on one chip would confirm the classification that everything above relies on. It is the decisive test for the drift of [§2.5](#25-a-marginal-operation): if the *d*₁ < 0 departures move with temperature or supply and the voiced tails do not, there are two mechanisms, and only the second can be modelled exactly.

### 5.6 More parts

- **Chip 3** may work after a power cycle (it wedged after loading a wrong address). A third chip of the same date code strengthens constraint 2 and gives a third set of chip-specific departures for [§4.3](#43-the-chip-specific-departures).
- **A TMS5220C** (the later revision, with the rate-control command) is cheap and common. If it shows the same error, the error survived a die revision, which argues for a feature of the architecture rather than a layout accident. If it does not, a comparison of the two dies' multipliers is the shortest route to the answer (a top-metal image of the TMS5220CNL exists: [preparation](preparation.md#source-and-licence)).
- **A TMS5110A** (the patent-era filter, different interface and PWM output) would show whether the TMC0280 family has the error at all. It needs an adapter board and a different capture path, so it is a project of its own; it is only worth it if everything else fails.

## 6. When a rule is found

A rule goes into `model.py` as the default, datasheet §8.8 and [§8.9](../datasheet/reference-algorithm.md) are rewritten from it, the emulator is updated from the datasheet alone, and the regression (`investigation/tools/regress.py`) reruns the whole data set. That is when acceptance test 1 can be judged. Search tools such as the pilot subclass stay outside `model.py`: `investigation/tools/model.py` is the measuring tool, and the emulator must not learn from it.
