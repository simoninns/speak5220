# 8.4 Serial sample output: `T11` and `I/O`

**New.** The original lists pin 7 (`T11`) only as "Sync" and pin 9 (`I/O`) only as "Serial data out". Together they give the exact digital value of every output sample, with no analogue error. Both run continuously while the chip is clocked, whether it is speaking or idle [p1-02].

**Framing.** `T11` is high for one slot (slot 0) in every sample period and low for the other 19. Its rising edge marks the start of the sample period. Its high time is 6.23 µs, which is one ROMCLK period less the difference between its slow fall and fast rise [p1-01, p1-03].

**Sample format.** `I/O` carries one code per sample period:

| Slot | `I/O` |
|---|---|
| 0, 1 | Low |
| 2 … 11 | Bits 0 … 9 of the code: LSB first, high = 1 |
| 12 … 19 | Low |

The code is a 10-bit two's-complement number (−512 to +511), equal to Y<sub>L13</sub>–Y<sub>L4</sub> of the original's Table 4. In 41,599 consecutive sample periods, `I/O` was never high outside slots 2–11 [p1-02].

**Idle value.** When the chip is not speaking, the code is −1. That is all ones in slots 2–11, which looks like a 50 % duty square wave at 8 kHz on a scope. A 1 s idle gap gave 8016 consecutive samples of −1 [p1-02]. At the end of a word, the output decays to 0 and keeps changing for about 15 samples after Talk Status clears, then returns to −1 [p5-01]. One capture shows it resting at 0 for longer between back-to-back words [p1-02]; that capture was made with the firmware's demo loop, and no capture made with the standard procedure shows it.

**Timing.** Slot *k* starts at a ROMCLK rising edge. Each output changes a fixed delay after that edge [p1-03]:

| Edge | Delay after the ROMCLK rising edge that starts the slot | Spread (1σ) |
|---|---|---|
| `T11` rising (start of slot 0) | 490 ns | 8 ns |
| `T11` falling (start of slot 1), to mid-level | 465 ns | 2 ns |
| `I/O` rising (start of a 1 bit) | 215 ns | 0.8 ns |
| `I/O` falling (end of a 1 bit), to mid-level | 450 ns | 1.9 ns |
| DAC, 50 % of the step (start of slot 1), into 996 Ω | 0.85 µs (0.62 µs in one word, [§8.2](dac-and-audio.md#82-da-conversion)) | — |

These delays were measured with ×10 probes on all three pins. For decoding, sample `I/O` in the middle of each slot. Taking slot *k* as 6.25·*k* µs to 6.25·(*k*+1) µs after the `T11` rising edge puts each sampling point at least 2.6 µs from any `I/O` edge.

![One sample period: SPEAKER (TP2), T11, I/O and ROMCLK, with grey lines every 6.25 µs from the T11 rising edge](img/fig_sample_period.png)

*Figure 8-1. One sample period [p1-03]. The code is −36 (1111011100 in binary), sent LSB first in slots 2–11: slots 2–3 low, 4–6 high, 7 low, 8–11 high. The DAC step at slot 1 is visible on SPEAKER. The small spikes on SPEAKER and the ripple on the low level of `T11` are coupling from the other signals.*

**Relation to TI's bit-time names (inference).** The original's Figure A-2 numbers the 20 bit times of a sample period T1–T20. The pin name suggests that `T11` is high during bit time T11. If so, slot *k* is bit time T((*k* + 10) mod 20 + 1): slot 0 is T11, slot 1 (DAC update) is T12, and the code is sent in T13–T20, then T1 and T2. This mapping comes from the pin name only. No measurement has confirmed it ([Appendix B](gaps.md#slots-and-tis-bit-times)). TI's own numbering is not fixed: [US 4,335,277](../sources/patent-us4335277/index.md) says its T17 is T9 in US 4,209,844. That patent starts each parameter count at T17, and this chip reads each frame from slot 17. That may tie slots to TI's bit times, but it is a hint, not a measurement.

**For emulator writers.** In each sample period:

1. Slot 0: `T11` goes high.
2. Slot 1: `T11` goes low, and the DAC changes to this period's sample.
3. Slots 2–11: the same sample goes out on `I/O`, LSB first.

The internal timing of the sample computation is not visible on these pins.
