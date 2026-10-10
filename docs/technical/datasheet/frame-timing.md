# 8.5 Timing of a frame

**New.** The chip runs its synthesis on fixed counters, whether it is speaking or not. Like the original's patents, this section names them the interpolation period (IP, 0–7) and the parameter count (PC, 0–12). Each sample period is one step:

- **An interpolation period is 25 samples.** PCs 0 to 11 last two samples each, A then B, and PC 12 lasts one sample.
- **A frame is 8 interpolation periods, 200 samples,** in the order IP 1, 2, …, 7, 0.

Sample numbers below count from the sample period in which a frame's first bit is read ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)), sample 0. That sample is PC 12 of IP 7. These timings are those that make the model match the captures ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)).

| Sample | What happens |
|---|---|
| 0 | The frame's bits are read, in slot 17 onwards. The voiced/unvoiced and silence flags of the frame read before it are latched for the excitation and for inhibiting interpolation ([§8.7](excitation-interpolation.md#87-interpolation)) |
| 1–25 | IP 0 of the previous frame: each parameter reaches the previous frame's value ([§8.7](excitation-interpolation.md#87-interpolation)) |
| 25 | PC 12 of IP 0: the frame read at sample 0 becomes the interpolation target |
| 26–200 | IP 1 to IP 7: the parameters move towards the new target |
| 200 | The next frame is read |

**When the filter sees a new value.** In each interpolation period, the parameter of PC *n* is computed in that PC's A sample, sample 2*n* of the period (0–24). The filter uses it from:

- **energy (PC 0):** sample 2;
- **pitch (PC 1):** sample 5;
- **K*j* (PC *j*+1):** sample 2*j*+3, so K1 at sample 5 and K10 at sample 23.

**When the excitation changes between voiced and unvoiced.** The flag latched at sample 0 changes the excitation from sample 2 on [vectors `interp`, corpus `acorn_phrom_a`].

**The start of speech.** A Speak latched at least 52 slots before the slot-17 read of a frame-read sample ([§6](commands.md#6-description-of-commands), [§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) has its first frame read in that sample, and the chip is *talking* from the end of that sample. The Speak also sets a zeroing state. While it is in force, every target is 0, and the filter sees 0 for every parameter, written in each parameter's PC. The interpolator's registers move towards those zero targets as usual, so IP 0 (shift 0) leaves every register at 0 before the first frame becomes the target ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). The zeroing ends when the first frame becomes the target at sample 25. If the first frame is silence, it lasts until the second frame takes effect, approximately ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) [p8-08]. So the output stays at its idle value, −1, until the first frame's energy arrives in IP 0 of the next frame. The first sample that differs from idle comes 206–207 samples after the first frame read (Speak), and 2 samples earlier for Speak External [p5-01].

**The end of speech.** A stop frame (or a frame that runs out of FIFO data, [§5](io-structure.md#5-io-structure)) sets the energy target to 0. Talk Status clears 24.87 ms after the stop frame's read, in slot 16 of its sample 199 ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)). The synthesis treats sample 200 as the last sample of talking: from the end of that sample the chip is no longer talking, and the frame counter no longer loads frames. In the following interpolation period the interpolator writes zero to the filter for each parameter in its PC, one sample earlier than a normal update. The filter keeps running meanwhile, so its output decays for a few samples to −1 [p8-01, vectors `energy`]. Interpolation of an unvoiced frame to a stop frame is inhibited ([§8.7](excitation-interpolation.md#87-interpolation)), so the energy stays at its last value up to the end, and then falls to 0 in one step.

**Speak External (New).** A Speak External latched at the same time before a frame boundary as a Speak gives exactly the output of the Speak, two sample periods earlier: voiced vectors through the two paths match sample for sample once shifted by 2 [p5-01, vectors `energy`, `pitch`, `k`]. So for a Speak External, every timing in this section and [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model) applies with sample 0 two sample periods before the VSM read point: the frame is taken from the FIFO in sample 198 of the frame clock, and the interpolation counters run two samples ahead for that utterance. How the chip does this is not known ([Appendix B](gaps.md#the-speak-external-two-sample-lead)); nothing in the VSM path shows it, because a Speak External reads no VSM, and the VSM reads of a later Speak are back on the 25 ms grid ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)).
