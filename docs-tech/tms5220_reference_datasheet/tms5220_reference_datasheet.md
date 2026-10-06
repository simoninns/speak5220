# TMS5220 Voice Synthesis Processor: Reference Datasheet

This datasheet describes the TMS5220 as measured on real hardware. It follows the section order of TI's *TMS 5220 Voice Synthesis Processor Data Manual* (transcribed in `../tms5220_datasheet/`, referred to below as "the original"), so that readers of the original can find things. The text is written fresh, and it cites the original rather than copying it.

The aim is that someone working from this document alone can author new phrases, author new speech PROMs, and emulate the chip exactly. `../speech_capture_plan.md` describes how the measurements are made.

**Status (2026-10-06):** Phase 1 and 2 sections are drafted: the system clock (§2, Appendix A), the DAC and the audio output (§8.2, §8.3), the serial sample output (§8.4), measured output characteristics (§9.5), and pin functions (§11.2). §4 and §6 hold early observations from the Phase 3 and Phase 4 VSM traces. The other sections are placeholders and are filled in by later phases.

## Conventions

- **Evidence.** Every measured statement cites one or more capture IDs in brackets, for example [p1-03]. `evidence/README.md` lists each capture with its settings and the SHA-256 hash of its raw data.
- **New and corrected content.** Content the original does not have is marked **New**. Content that disagrees with the original is marked **Correction** and says what the original states.
- **Voltages** are measured at the probe tip, referred to board ground (`VREF`, 0 V), with `VSS` = +5 V and `VDD` = −5 V. Probes are 10 MΩ passive ×10 probes unless stated. Edge times are 10–90 % unless stated, and edge positions are taken at the midpoint between the low and high levels.
- **Slots.** A *slot* is one ROMCLK period, 6.25 µs at the nominal clock. Each 125 µs sample period has 20 slots, numbered 0–19. Slot 0 is the one in which `T11` is high (§8.4).
- **Codes.** A *code* is the signed 10-bit sample value the chip sends on `I/O` (§8.4).

## 1. Introduction

The original's introduction (scope, key features, device operation) still applies. This datasheet adds the measured behaviour that the original leaves out or gets wrong. The change log at the end lists every addition and correction.

## 2. System clock

**Status: drafted (Phase 1).**

The original says the internal RC oscillator runs at 640 kHz for an 8 kHz sample rate, and that the clock is divided by four into two major phases. The measurements confirm the division ratios:

| Quantity | Relationship | At 640 kHz | Measured |
|---|---|---|---|
| Oscillator (`OSC`) | `fosc` | 640 kHz | Driven by the test board at 640.000 kHz (crystal-derived) |
| ROMCLK (pin 3) | `fosc` ÷ 4 | 160 kHz | 160.0005 kHz, period 6249.98 ns [p1-03] |
| Slot (bit time) | 1 ROMCLK period | 6.25 µs | 6.25 µs |
| Sample period | 20 slots = `fosc` ÷ 80 | 125 µs (8 kHz) | 124.9996 µs [p1-01, p1-02, p1-03] |
| Interpolation interval | 25 samples | 3.125 ms | Not yet measured (Phase 8) |
| Frame | 8 interpolation intervals = 200 samples | 25 ms (40 Hz) | Not yet measured (Phase 8) |

The measured periods differ from nominal by about 3 ppm. That is within the combined tolerance of the test board's crystal and the scope's timebase.

**Correction.** The original's 8 kHz column in Table A-1 gives the interpolation interval as 3.125 ms, but its §2 text says 3.25 ms. 25 samples of 125 µs is 3.125 ms.

ROMCLK is high for 50.6 % of each period (3162.7 ns of 6250 ns) [p1-03]. The original's Figure A-2 calls it PHI-2.

**Every `T11` and `I/O` transition follows a ROMCLK rising edge** [p1-03]. No transition lines up with a ROMCLK falling edge. §8.4 gives the delays.

Driving `OSC` from an external clock is covered in Appendix A.

## 3. CPU interface

**Status: not yet written (Phase 8).** It will cover READY, `INT` and FIFO timing, measured with the protocol tests in plan §9.3.

## 4. Voice synthesis memory (VSM) interface

**Status: not yet written (Phase 8).** It will give the exact command and bit sequences from the VSM trace.

**New:** early observations from the Phase 3 traces, to be confirmed by the Phase 8 tests:

- **No read-ahead.** A Speak reads the VSM exactly up to the last bit of the stop code and no further. For ACORN that is 1152 bits, starting with a Dummy Read at the loaded address [p3-01, p3-03].
- **Reset uses the VSM.** Every Reset command issues a Load Address and then a Dummy Read to the VSM, as the original's §6.6 says. This includes each `0xFF` byte of the initialisation sequence, which decodes as Reset (`X111XXXX`) [p3-01].
- **One frame every 25 ms.** During a Speak, the chip reads each frame's bits in a burst, and the bursts start exactly 25.000 ms apart (within 0.12 µs, at the nominal clock) [p4-01, p4-02].
- **The start is not fixed.** The first frame read came 25.51, 19.69 and 12.49 ms after the host started its Load Address and Speak sequence (`SCOPE_TRIG` rising), in three Speaks of the same word with no reset between them. The time from that first read to Talk Status clearing was the same in all three, 674.87 ms, which is 27 frames of 25 ms [p3-01, p4-01, p4-02]. This suggests that a Speak waits for a frame boundary of a timer that keeps running between words. Phase 5 tests this.

## 5. I/O structure

**Status: not yet written (Phase 8).**

## 6. Description of commands

**Status: not yet written (Phase 8).**

**New:** early observations from Phase 3, to be confirmed by the Phase 8 tests:

- **Read Byte (`X001XXXX`) returns the VSM byte bit-reversed.** The chip reads the byte serially, LSB first, and the first bit read appears on D0, which is TI's most significant bit. With the byte at `0x3CBB6`, 0x23, the data register returned 0xC4 [p3-04]. The FIFO works the other way: Speak External shifts each written byte out LSB first (original §5). So a VSM byte read with Read Byte must be bit-reversed before it can be passed to Speak External.
- **Talk Status is not set at once.** A status read 22 µs after Speak (preceded by Load Address) still shows Talk Status clear [p3-01].
- **BE is left over outside Speak External.** After a VSM Speak, BE holds whatever the last Reset or Speak External left [p3-01, p3-02].
- **BE after a complete stream.** A Speak External stream whose stop code ends exactly on a byte boundary finishes with BE set. Its speech is nevertheless complete [p3-02].

## 7. Power-up clear

**Status: not yet written (Phase 5).** This includes what state survives a Speak or Reset, such as the noise generator and the start delay (plan §11).

## 8. Speech synthesis

### 8.1 Coded speech parameters

**Status: not yet written (Phase 8).**

### 8.2 D/A conversion

**Status: drafted (Phases 1 and 2).** Measured on one chip into a resistive load to 0 V (996 Ω and 482 Ω), DC-coupled, with nothing else on the pin.

**DAC update timing (New).** The DAC changes to each new sample at the start of **slot 1**, one slot after `T11` rises. Measured from the ROMCLK rising edge that starts slot 1, into 996 Ω, the step reaches 10 % after 0.66 µs, 50 % after 0.85 µs and 90 % after 1.29 µs [p2-03]. This is the average of 180 steps of at least 20 codes. The same word measured with the test board's audio stage also loading the pin gave the same times to within 30 ns [p1-03], so they are set by the chip. Another word stepped about 0.3 µs earlier (10 % at 0.34 µs, 90 % at 1.16 µs) [p1-01]. Why is not yet known: the steps in that word are larger, and the timing may depend on the step size or direction. The output is within 3 % of its final value 2.3 µs after slot 1 starts [p2-01]. It then holds that value for the rest of the sample period, apart from coupling spikes of about 1 % at the `I/O` edges in slots 2–11 (Figure 8-1).

**The DAC and `I/O` show the same sample.** The value the DAC outputs from slot 1 of a sample period is the code sent on `I/O` in slots 2–11 of the **same** period [p1-01, p1-02].

**Input to the DAC.** The original's Table 4 describes the DAC input as bits Y<sub>L13</sub>–Y<sub>L4</sub> of the lattice output: the sign and seven low-order bits drive the DAC, and the two remaining high bits clip it to full on or full off. The `I/O` code (§8.4) carries all ten bits *before* that clipping. Codes up to +134, outside the DAC's −128 to +127 range, have been observed on `I/O` [p1-01].

**Clipping.** The DAC takes the code clipped to −128…+127. Codes +131 to +134 gave the same output as +127, to within 0.3 mV [p2-01, p2-02, p2-04, p2-05]. Codes below −111 did not occur in the speech measured, so clipping at the negative end has not been observed yet.

**Direction and zero.** A more positive code gives less current, as in the original's Table 4. Code +127 gives no current: across either load the pin sits at 0 V, to within ±2 mV [p2-01 to p2-06].

**Transfer function (Correction).** This chip delivers about 20 % less current than the original's Table 4. Its step is not uniform, and it jumps by about six steps between codes −1 and 0. Table 8-1 gives the main figures. `captures/phase2/dac_transfer*/dac_table.csv` gives the mean current for each code seen, at each load.

*Table 8-1. DAC output current. Means of 83,190 sample periods per load. Each step is the median of single-code steps that do not cross a multiple of 16.*

| Quantity | Original (Table 4) | Measured, 996 Ω load | Measured, 482 Ω load |
|---|---|---|---|
| One step (1 LSB) | 5.86 µA | 4.49 µA | 4.80 µA |
| Code +127 | 0 µA | 0 ± 2 µA | 0 ± 3 µA |
| Code 0 | 744 µA | 578 µA | 604 µA |
| Code −1 (idle) | 750 µA | 604 µA | 631 µA |
| Code −128 (full scale) | 1500 µA | about 1170 µA (extrapolated) | about 1230 µA (extrapolated) |

[p2-01, p2-02 at 996 Ω; p2-04, p2-05 at 482 Ω; scope zero from p2-06.] The currents are the TP2 voltage divided by the load resistance, measured with a meter. Their absolute accuracy depends on the scope's DC gain (a few per cent). The scope's own zero offset (−8.7 mV) was measured with the probe grounded [p2-06] and has been subtracted. The ratios between codes do not depend on any of this.

**Bit weights (New).** The DAC behaves as eight binary-weighted current sources, one per input bit, with input byte = code + 128 (code 0 = 0x80, code −1 = 0x7F). Fitting one weight per bit to every sample gives:

*Table 8-2. Bit weights, in LSB as defined in Table 8-1.*

| Input bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 (sign) |
|---|---|---|---|---|---|---|---|---|
| Ideal | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 |
| 996 Ω | 0.86 | 1.82 | 3.99 | 7.85 | 15.79 | 32.33 | 63.97 | 132.64 |
| 482 Ω | 0.87 | 1.82 | 3.94 | 7.78 | 15.74 | 32.09 | 62.89 | 130.96 |

The two loads agree to within 2 %, so the mismatch belongs to the DAC. It shows as steps of the wrong size wherever a carry crosses into a higher bit:

*Table 8-3. Measured step at the major carries, in LSB (ideal 1).*

| Codes | Input byte | 996 Ω | 482 Ω |
|---|---|---|---|
| −1 → 0 | 0x7F → 0x80 (bit 7) | **5.85** | **5.62** |
| −65 → −64, 63 → 64 | bit 6 | 0.03, 0.36 | 0.16, 0.32 |
| −33 → −32, 31 → 32 | bit 5 | 1.42, 1.35 | 1.38, 1.32 |
| −17 → −16, 15 → 16 | bit 4 | 0.91, 0.83 | 0.96, 0.86 |

The step between −1 and 0 is **about six times a normal step**: 26–27 µA, or 26 mV across 996 Ω [p2-01, p2-02, p2-04, p2-05]. The original's Table 4 implies a single 5.86 µA step. Every zero crossing of the waveform passes through this step, so the analogue output has a small crossover distortion that a linear DAC model does not reproduce.

**Large currents.** For codes below about −55, the current falls progressively short of the bit-weight model fitted to codes −50 and above. The shortfall is 2.4 LSB at code −110 into 996 Ω (pin at 1.08 V), and 1.5 LSB into 482 Ω (pin at 0.55 V). Part of it is in the DAC, and part depends on the pin voltage (§8.3).

**No memory between samples (New).** Each sample's output depends only on its own code. Regressing the residual from the per-code table on the levels of the four previous samples gives coefficients of 0.2 % or less, and removes less than 0.02 mV of residual [p2-01, p2-02, p2-04, p2-05]. Within one capture, the noise on a sample's mean is 0.5–0.8 mV while the chip is idle. During speech, samples of the same code scatter more (1–2 mV). That extra scatter does not depend on the code or on earlier codes, so it is probably interference on the test board while the VSM is being read, not DAC behaviour. Between captures a few minutes apart, the whole table moved by up to 0.35 %. Some hours later, after the chip had been power-cycled, the whole scale was 7 % higher (code −1 at 0.635 V against 0.592 V), with the supplies within 6 mV of nominal [p4-01 to p4-04]. The shape of the table still fitted to 2.2–2.3 mV rms. So the absolute current figures in this section can vary by at least 7 %; the cause (the chip, the load or the scope) is not known yet.

![TP2 against code into 996 ohm, its INL, and the residual against the previous sample](assets/fig_dac_transfer.png)

*Figure 8-2. DAC transfer into 996 Ω [p2-01, p2-02]. Top: mean TP2 voltage for each code, with the bit-weight model. Middle: deviation from a straight line through the model's end points, in LSB. The jump at code 0 is the bit-7 mismatch. The steps at ±32 and ±64 come from bits 5 and 6. Bottom: each sample's residual from the per-code table, plotted against the change from the previous sample, shows no dependence.*

**For emulator writers.** The `I/O` code (§8.4) is the exact output. To model the analogue output, take the code, clip it to −128…+127 and add 128, then sum the Table 8-2 weights for the bits that are **clear**. This is approximate: the fit also leaves a few µA of offset and the shortfall at large currents. More current means more bits clear: byte 0xFF gives no current and byte 0x00 gives full scale. Alternatively, use the per-code table. Expect another chip to have different bit weights.

### 8.3 Audio output

**Status: drafted (Phase 2).** Measured on one chip [p2-01 to p2-06].

**`SPEAKER` is a current output into a load returned to 0 V.** The pin pushes current into the load, so the load voltage is positive: 0 V at code +127 and about 0.6 V at idle (code −1) into 996 Ω. As the original says, the output must be AC-coupled to any amplifier, because the idle current puts a DC level on the load.

**The output is not an ideal current source (New).** Halving the load from 996 Ω to 482 Ω raises the current by 3–5 % (Figure 8-3). A current source with a finite output resistance would raise every code by the same fraction. The measured ratio is 1.049 at large currents and falls to 1.032 near code +60. Read as a Norton source, that is an output resistance of 10–16 kΩ in parallel with the pin, so the figure is approximate. Codes near zero current are not included: at those currents, the ±2 mV of offset left on the test board dominates the ratio.

![SPEAKER current per code at 996 and 482 ohm loads, and their ratio](assets/fig_dac_load.png)

*Figure 8-3. Current per code at two loads [p2-01, p2-02, p2-04, p2-05; zero from p2-06]. Top: current. Bottom: the ratio of the two currents. Codes seen fewer than 20 times are grey. The line is the mean over codes above a quarter of full scale.*

**Consequences.**
- **Quoting a current means quoting the load.** At 996 Ω, full scale extrapolates to about 1.17 mA. At 482 Ω it extrapolates to about 1.23 mA. A virtual-ground (transimpedance) measurement would give the current at 0 V, which has not been made yet.
- **A lower-value load gives a more linear voltage.** The extra shortfall at large currents (§8.2) grows with the pin voltage.
- **The original's 1.8 kΩ application.** The original says that 1.8 kΩ in series gives 2.7 V at 1.5 mA. That has not been tested here. Given the output resistance above, expect noticeably less current into that load.
- **AC coupling on the pin changes what the pin delivers.** The test board's own coupling network (C1 and 4.7 kΩ to ground, after a 1 kΩ load) made the voltage on the pin depend on earlier samples. That network was disconnected for these measurements (plan §5).

### 8.4 Serial sample output: `T11` and `I/O` (New)

**Status: drafted (Phase 1).**

The original lists pin 7 (`T11`) only as "Sync" and pin 9 (`I/O`) only as "Serial data out". Together they give the exact digital value of every output sample, with no analogue error. Both run continuously while the chip is clocked, whether it is speaking or idle [p1-02].

**Framing.** `T11` is high for one slot (slot 0) in every sample period and low for the other 19. Its rising edge marks the start of the sample period. Its high time is 6.23 µs, which is one ROMCLK period less the difference between its slow fall and fast rise [p1-01, p1-03].

**Sample format.** `I/O` carries one code per sample period:

| Slot | `I/O` |
|---|---|
| 0, 1 | Low |
| 2 … 11 | Bits 0 … 9 of the code: LSB first, high = 1 |
| 12 … 19 | Low |

The code is a 10-bit two's-complement number (−512 to +511), equal to Y<sub>L13</sub>–Y<sub>L4</sub> of the original's Table 4. In 41,599 consecutive sample periods, `I/O` was never high outside slots 2–11 [p1-02].

**Idle value.** When the chip is not speaking, the code is −1. That is all ones in slots 2–11, which looks like a 50 % duty square wave at 8 kHz on a scope. A 1 s idle gap gave 8016 consecutive samples of −1 [p1-02]. Between the end of one word and the next Speak, the code can rest at 0 rather than −1 [p1-02]. This is examined in Phase 5.

**Timing.** Slot *k* starts at a ROMCLK rising edge. Each output changes a fixed delay after that edge [p1-03]:

| Edge | Delay after the ROMCLK rising edge that starts the slot | Spread (1σ) |
|---|---|---|
| `T11` rising (start of slot 0) | 490 ns | 8 ns |
| `T11` falling (start of slot 1), to mid-level | 465 ns | 2 ns |
| `I/O` rising (start of a 1 bit) | 215 ns | 0.8 ns |
| `I/O` falling (end of a 1 bit), to mid-level | 450 ns | 1.9 ns |
| DAC, 50 % of the step (start of slot 1), into 996 Ω | 0.85 µs (0.62 µs in one word, §8.2) | — |

These delays were measured with ×10 probes on all three pins. For decoding, sample `I/O` in the middle of each slot. Taking slot *k* as 6.25·*k* µs to 6.25·(*k*+1) µs after the `T11` rising edge puts each sampling point at least 2.6 µs from any `I/O` edge.

![One sample period: SPEAKER (TP2), T11, I/O and ROMCLK, with grey lines every 6.25 µs from the T11 rising edge](assets/fig_sample_period.png)

*Figure 8-1. One sample period [p1-03]. The code is −36 (1111011100 in binary), sent LSB first in slots 2–11: slots 2–3 low, 4–6 high, 7 low, 8–11 high. The DAC step at slot 1 is visible on SPEAKER. The small spikes on SPEAKER and the ripple on the low level of `T11` are coupling from the other signals.*

**Relation to TI's bit-time names (inference).** The original's Figure A-2 numbers the 20 bit times of a sample period T1–T20. The pin name suggests that `T11` is high during bit time T11. If so, slot *k* is bit time T((*k* + 10) mod 20 + 1): slot 0 is T11, slot 1 (DAC update) is T12, and the code is sent in T13–T20, then T1 and T2. This mapping comes from the pin name only. No measurement has confirmed it.

**For emulator writers.** In each sample period:
1. Slot 0: `T11` goes high.
2. Slot 1: `T11` goes low, and the DAC changes to this period's sample.
3. Slots 2–11: the same sample goes out on `I/O`, LSB first.

The internal timing of the sample computation is not visible on these pins.

## 9. Electrical characteristics

The original's ratings and DC characteristics (§9.1–§9.4) are not repeated here.

### 9.5 Measured output signals (New)

**Status: drafted (Phase 1).** Measured on one chip, at room temperature, with `VSS` = +5 V and `VDD` = −5 V. Each output drives only a 10 MΩ ×10 probe, apart from ROMCLK, which also drives the VSM interface on the test board [p1-01, p1-03].

| Pin | Low level | High level | Rise (10–90 %) | Fall (90–10 %) |
|---|---|---|---|---|
| ROMCLK (3) | −2.7 V | +4.6 V | 36 ns | 173 ns |
| `T11` (7) | −3.0 V | +4.5 V | 73 ns | 480–510 ns |
| `I/O` (9) | −2.3 V | +4.5 V | 114 ns | 670–700 ns |

All three outputs swing from near `VSS` down to well below 0 V. Their falls are 5–7 times slower than their rises, which fits a strong pull-up to `VSS` and a weaker pull-down. Any circuit that takes `T11` or `I/O` into 0–3.3 V logic must clamp the negative swing.

## 11. Pin assignments and functions

### 11.2 Pin functions

**Status: drafted (Phase 1) for the pins below. The CPU and VSM pins are covered in Phase 8.**

| Pin | Name | Original | Measured function |
|---|---|---|---|
| 3 | ROMCLK | Clock to VSM | `OSC` ÷ 4, 50.6 % duty. Its rising edges start each slot, and all `T11` and `I/O` transitions follow them (§2, §8.4) |
| 6 | `OSC` | Oscillator input | The threshold node of the internal RC oscillator. An external clock must swing from `VDD` to `VSS`. A 0–5 V square wave does not clock the chip (Appendix A) |
| 7 | `T11` | Sync | High for slot 0 of each sample period. It frames the `I/O` data and runs continuously (§8.4) |
| 8 | `SPEAKER` | Audio output | DAC current output into a load to 0 V. About 4.5 µA per code, with a step of about 26 µA between codes −1 and 0. The current depends on the load. It changes at the start of slot 1 (§8.2, §8.3) |
| 9 | `I/O` | Serial data out | The 10-bit output sample before clipping, LSB first, in slots 2–11. It runs continuously, and sends −1 when idle (§8.4) |
| 10 | `PROM OUT` | Testing use only | An output that drives strongly. It was left unconnected for all measurements. Function unknown |
| 20 | `TEST` | Testing use only | Not explored (plan §11) |

## Appendix A: System clock

### A.1 Driving `OSC` from an external clock (New)

**Status: drafted (Phase 1).**

The original shows only the RC option (Figure A-1): a resistor and capacitor network on `OSC`, trimmed by measuring ROMCLK. It does not describe an external clock.

`OSC` is not an ordinary logic input. It is the threshold node of the internal RC oscillator, whose timing network returns to `VDD` (−5 V). On the test board:

- A 0 to +5 V square wave at 640 kHz did not clock the chip, so there was no ROMCLK.
- A 0 to +5 V square wave at 320 kHz with `PROM OUT` pulled towards −5 V also did not clock the chip. (A TI-99/4A note says this setup selects an external clock at half rate. That was not true for this chip.)
- **A square wave swinging from −5 V to +5 V at 640 kHz clocks the chip correctly.** ROMCLK is 160 kHz, and the chip speaks normally.

A working drive circuit is a single PNP stage:

- A 0/5 V logic signal goes through a 10 kΩ resistor to the base of a BC557 (or 2N3906).
- 3.3 kΩ from base to emitter.
- Emitter to +5 V.
- Collector to `OSC`, with 1 kΩ from the collector to −5 V.

This stage inverts the clock, which does not matter. It is the same arrangement used on the Atari Star Wars sound board, which clocks a TMS5220 at 672 kHz through a 2N3906 and CMOS inverters running from ±5 V.

The test board makes its 640 kHz from an RP2350 running at 144 MHz, with a PWM divider of 225. That gives exactly 8000 samples per second, with crystal accuracy, which makes captures repeatable and sample-aligned.

### A.2 Checking the clock

Measure ROMCLK (pin 3), as the original's trimming procedure does. It should be `fosc` ÷ 4 (160 kHz for 8 kHz sampling). `T11` gives the sample rate directly (`fosc` ÷ 80).

## Change log

Additions and corrections relative to the original, with their evidence.

| Date | Section | Change | Evidence |
|---|---|---|---|
| 2026-10-06 | §2 | Measured ROMCLK, slot and sample-period timing. ROMCLK rising edges start each slot | p1-01, p1-02, p1-03 |
| 2026-10-06 | §2 | Correction: the 8 kHz interpolation interval is 3.125 ms (§2 of the original says 3.25 ms) | Arithmetic, the original's Table A-1 |
| 2026-10-06 | §8.2 | DAC update timing (start of slot 1). `I/O` carries the code before clipping | p1-01, p1-03 |
| 2026-10-06 | §8.4 | New section: the `T11` and `I/O` serial sample format and its timing | p1-01, p1-02, p1-03 |
| 2026-10-06 | §9.5 | Measured output levels and edge times for ROMCLK, `T11` and `I/O` | p1-01, p1-03 |
| 2026-10-06 | §11.2 | Measured functions of pins 3, 6, 7, 8 and 9. Status of 10 and 20 | As above, plus the bench notes in plan §5 |
| 2026-10-06 | App. A | New: external clock drive requirements and circuit | Plan §5 bench tests |
| 2026-10-06 | §8.2 | DAC step timing into an unloaded 996 Ω. Clipping of codes above +127 confirmed | p2-03, p2-01, p2-02, p2-04, p2-05 |
| 2026-10-06 | §8.2 | Correction: the current is about 20 % below the original's Table 4, and the step from −1 to 0 is about 6 LSB, not 1. New: bit weights, carry errors, no memory between samples | p2-01, p2-02, p2-04, p2-05, p2-06 |
| 2026-10-06 | §8.3 | New: the `SPEAKER` current depends on the load (output resistance about 10–16 kΩ) | p2-01, p2-02, p2-04, p2-05, p2-06 |
| 2026-10-06 | §11.2 | Pin 8: measured current scale | As §8.2 |
| 2026-10-06 | §4 | New (early): no VSM read-ahead past the stop code; every Reset command issues a Load Address and a Dummy Read | p3-01, p3-03 |
| 2026-10-06 | §4 | New (early): frames are read exactly 25 ms apart; the delay from Speak to the first frame read varies by most of a frame | p3-01, p4-01, p4-02 |
| 2026-10-06 | §8.2 | The absolute DAC scale was 7 % higher on a later run; the table's shape was unchanged | p4-01 to p4-04 |
| 2026-10-06 | §6 | New (early): Read Byte returns the VSM byte bit-reversed. Talk Status is set some time after Speak. How BE behaves outside Speak External, and at the end of a stream | p3-01, p3-02, p3-04 |
