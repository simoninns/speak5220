# Excitation and interpolation

## 8.6 Excitation

**New.** The original does not describe the excitation.

**Voiced: the chirp.** A pitch counter counts samples from 0. When it is equal to or greater than the pitch value it returns to 0, so **the period is the pitch value plus one sample** (Table 8-5). This was measured at every pitch index [vectors `pitch`] and in the PROM corpus (the Acorn tones, 25 and 35 samples for values 24 and 34). It agrees with [US 4,335,277](../sources/patent-us4335277/index.md), where the counter is zeroed once its contents equal or exceed the pitch register. The patent's chirp ROM has 52 locations: addresses above 51 are held at 51, which stores zero, until the counter returns to 0. Each sample, the counter's value addresses the chirp (Table 8-7): values 0 to 20 give the chirp, and 21 or more give 0. The counter advances only while the chip is talking ([§8.5](frame-timing.md#85-timing-of-a-frame)). When the change to the current target is inhibited ([§8.7](#87-interpolation)), the counter is held at 0 from the end of the frame-read sample that precedes the jump (sample 200 of the target's frame, [§8.5](frame-timing.md#85-timing-of-a-frame)) to the end of the first three samples of IP 0 (PC 0 A and B, and PC 1 A): those three samples read chirp address 0, the next reads address 1. So each inhibited change restarts the chirp with the jump. The third sample is measured: a voiced frame after silence that followed voiced speech starts its chirp one sample later than a two-sample hold gives, after any number of silence frames (1 to 6 tested), at any pitch before or after, through both paths. Against a two-sample hold, the three-sample hold cuts the model's error power on the PROM words by up to 13 times, and makes no PROM or vector set worse. After unvoiced speech the hold's length makes no difference, because the counter is already at 0 (pitch 0) until the new pitch reaches the filter in sample 5 [vectors `onset`; corpus, every PROM].

**Unvoiced: the noise register.** The excitation is +64 or −64, half of full scale, with the sign chosen by a 13-bit linear-feedback shift register:

- **Each step:** shift the register left by one, and bring in, as the new bit 0, the exclusive OR of bits 12, 3, 2 and 0.
- **The sign:** each sample, if bit 0 is 1 the excitation is −64, otherwise +64. The register then steps 20 times, once per slot.

This register has the maximal period, 8191 steps. Its state at the start of speech, and how it runs between utterances, are in [§7](power-up.md#7-power-up-clear-and-the-state-between-utterances). It matched the chip bit for bit over 32,699 samples [p8-03, vectors `noise`]. The patent (US 4,335,277) describes this excitation as a random sign on a steady 0.5. The excitation's top bit is the random sign and the next bit is forced to 1, so the value is ±64 in the units used here. The choice between chirp and noise follows the previous frame's voicing flag. That is why the excitation changes a frame after the flag ([§8.5](frame-timing.md#85-timing-of-a-frame)).

## 8.7 Interpolation

**New.** Each parameter (energy, pitch, K1–K10) has a register holding its current value, and a target from the frame (Tables 8-4 to 8-6). In each interpolation period, in its PC, the register moves towards the target:

    value = value + ((target − value) >> shift)

The shift is 3 in IP 1, 2 and 3, 2 in IP 4 and 5, 1 in IP 6 and 7, and 0 in IP 0, where the value reaches the target. `>>` is an arithmetic shift, rounding down. Rounding towards zero instead made the model's total difference from the PROM words 44 times larger [corpus `acorn_phrom_a`]. US 4,335,277 describes the same thing: the step is computed bit-serially, and the bits below the ten that are kept are zeroed, so no fraction carries over. The patent's Table I contradicts itself at IP 6: its divisor column gives 2 (shift 1), but its fraction column (0.717) needs 4. Shift 2 at IP 6 made the model worse on every vector set [vectors `pitch`, `k`, `interp`, `energy`].

**Inhibited interpolation.** The decision is made when a frame becomes the target (sample 25), from the flags latched at the frame read before it ([§8.5](frame-timing.md#85-timing-of-a-frame)) and the new frame's coded indices. Here *silence* means energy index 0 or 15 (a stop frame counts as silence in all three rules), and *unvoiced* means pitch index 0; a silence or stop frame carries no pitch, so it keeps the previous pitch index and with it the previous voicing. In these cases the registers do not change in IP 1 to 7, and jump to the target in IP 0:

- the new frame is unvoiced and the old one voiced, or the other way round;
- the old frame was silence (or the start of speech) and the new one is not;
- the old frame was unvoiced and the new one is silence or a stop frame.

In the last case the energy does not fade: an unvoiced sound followed by a stop code ends at full level [p8-01, vectors `interp`].

**K5–K10 of unvoiced frames.** An unvoiced frame (or an unvoiced repeat frame) sets K5–K10 to zero: in their PCs, their registers are set to 0 directly (not interpolated towards 0) and the filter sees 0. Every frame load ends the zeroing, and a voiced or unvoiced frame then sets it again according to its own voicing ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)). So a silence frame ends it: the targets of K5–K10 return to the table values at their stored indices, which an unvoiced frame never changes. A voiced repeat frame keeps the zeroing if the last non-repeat frame was unvoiced ([§8.1](coded-parameters.md#81-coded-speech-parameters)). In US 4,335,277 this zeroing also acts on the value written back to the register. While interpolation is inhibited, this waits for IP 0 like any other change, so for the first frame after a voiced one the old K5–K10 stay in use [corpus `acorn_phrom_a`, vectors `interp`].

**Measured.** Jumps of each parameter, from one end of its table to the other and back, were captured for every parameter, along with each transition between voiced, unvoiced and silent frames [vectors `interp`]. The model with these rules matches 92 % of their samples; the rest are one-code differences except where noted in [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model).
