# Observables and capture layers

This page lists what can be observed on the chip, summarises what the first measurements of the outputs showed, and describes the three layers recorded for every utterance.

## What can be observed on the chip

| Signal | Pin | What it gives us | Wired on the board |
|---|---|---|---|
| `SPEAKER` | 8 | Analogue DAC output (a current pushed into the load: 0 to about 1.16 mA into 1 K, 4.49 µA per step, new value every 125 µs; the original data manual's design values are 0–1.5 mA and 5.86 µA, [§8.2](../datasheet/dac-and-audio.md#82-da-conversion)) | Yes, into the R2 1K load (top sheet) and the filter chain. Test point TP2 |
| `ROMCLK` | 3 | `OSC` ÷ 4. It sets the exact sample rate (`fosc` / 80) | Yes, to the VSM interface |
| `ADD1/2/4/8`, `M0`, `M1` | — | Every VSM command and every data bit the chip reads | Yes, the Pico sees all of them (GPIO8–13) |
| `T11` | 7 | Sync output, once per sample period | Test point TP3 only |
| `I/O` | 9 | Serial data out | Test point TP4 only |
| `PROM OUT` | 10 | The parameter ROM's output: each parameter's table value at the stored coded index, serially ([datasheet §8.10](../datasheet/prom-out.md)) | Yes: TP10, to the PicoScope's logic input D0 |
| `TEST` | 20 | "Testing use only" | No. Leave it alone (see [§11.2](../datasheet/pins.md#pin-20-test)) |
| `READY`, `INT`, status register | — | Command handshake, end-of-speech and FIFO events | Yes |

`T11` and `I/O` matter most. The original datasheet calls them only "sync" and "serial data out". Community reverse engineering of the TMS52xx family suggests that `I/O` carries the **digital sample value serially, framed by `T11`**. The bench confirms this (below), so the exact output word of the lattice filter can be captured with no analogue error at all.

## First observations of the outputs

Both `T11` and `I/O` swing from −5 V to +5 V.

- **`T11`:** an 8 kHz pulse train with 5% duty. That is one 6.25 µs bit time (one ROMCLK period) out of the 20 in each 125 µs sample period. It is the per-sample frame marker.
- **`I/O` when idle:** a 50% duty square wave. This fits a 10-bit sample value of −1 (all ones) sent serially in 10 of the 20 bit times, with the line low for the other 10.
- **`I/O` during speech:** the pattern changes on every sample.

The first automated PicoScope captures used probe A ×1 on ±1 V with a −0.5 V offset, and probes B–D ×10 on ±1 V (±10 V at the tip), in block mode at 10.4 MS/s and at 2 MS/s. They gave:

- **`T11` period:** 124.9996 µs, with 0.03 µs jitter.
- **`I/O` format:** a 10-bit two's-complement sample, **LSB first, in bit slots 2–11**. Slot *k* runs from 6.25·*k* to 6.25·(*k*+1) µs after the `T11` rising edge. Slots 0, 1 and 12–19 are always low, and the line high is a 1. Edges fall on slot boundaries. The sample is the one the DAC outputs in the same period. The idle value is −1.
- **DAC timing:** the DAC changes value 6.25 µs after the `T11` rising edge, that is, on its falling edge.
- **Averaging:** the settled part of each DAC step (`T11` + 12 µs to + 128 µs) averages down from 11 mV of noise to 0.33 mV.
- **The audio stage loads TP2.** It is not a pure 1 K load: C1 and R3 4K7 make the TP2 voltage depend on recent samples. A first-order model with a pole at 294 Hz (the C1 corner is 279 Hz) cuts the residual from 8.6 mV to 1.3 mV rms. For DAC characterisation, C1 is removed so TP2 sees only the 1 K ([bench hardware](bench-hardware.md#measurement-configuration)). Alternatively, the network can be modelled in post-processing.
- **Repeats are not bit-identical without a controlled start.** Repeats of "B" match exactly for the first ~1170 samples, then differ by ±1–2. Words with unvoiced sounds ("C", "ACORN") differ by up to ±26 from the first unvoiced frame onwards. This is consistent with a free-running noise generator that Speak does not reset. The delay from Speak to the first sample also varies: 6 and 25 samples were seen, and 25 samples is one interpolation period. Both are explained in [repeatability](work/repeatability.md).

### Clock-edge alignment

Every `T11` and `I/O` transition follows a ROMCLK **rising** edge. `I/O` rises 215 ns after it and `T11` rises 490 ns after it, each with a spread of 8 ns or less. ROMCLK measures 160.0005 kHz at 50.6 % duty. The DAC steps at the start of slot 1. `I/O` carries the 10-bit code before the DAC clips it (codes up to +134 were seen). The reference datasheet gives the full results, with the captures cited: [§2 and Appendix A](../datasheet/clock.md), [§8.2](../datasheet/dac-and-audio.md), [§8.4](../datasheet/serial-output.md), [§9.5](../datasheet/electrical.md) and [§11.2](../datasheet/pins.md). See also [bench bring-up](work/bench-bring-up.md).

### DAC transfer function

Measured with the audio stage disconnected, into 996 Ω and 482 Ω ([measurement hardware](work/measurement-hardware.md)):

- **Clipping and history.** The DAC takes the code clipped to −128…+127. Each sample's output depends only on its own code: the earlier samples explain less than 0.2 % of it.
- **Current.** The current is about 20 % below the original's Table 4: 4.49 µA per step and 604 µA at idle, into 996 Ω.
- **Carry errors.** The DAC is binary-weighted with mismatched bits. The step from −1 to 0 is about 6 LSB (26 µA), not 1. The bit-5 carries are 1.4 LSB and the bit-6 carries 0.0–0.4 LSB.
- **Load dependence.** The current depends on the load: it is 3–5 % more into 482 Ω than into 996 Ω, which is about 10–16 kΩ of output resistance. It also falls short at large currents.
- **Step timing.** The DAC step timing is set by the chip: 50 % at 0.85 µs after the slot-1 ROMCLK edge.

The reference datasheet's [§8.2 and §8.3](../datasheet/dac-and-audio.md) give the details.

## Capture strategy: three layers per utterance

Each capture records three things, so that any difference from an emulator can be traced to its cause:

1. **Input trace (digital, exact).** The Pico's VSM emulation logs every Load-Address, Read and Read-and-Branch, and the bits it returned. This is the exact bit stream the chip parsed. The firmware records this on core 1 as a compact, timestamped event log, together with every host interface cycle. The host downloads it after each capture as `raw/vsm_trace.bin` ([firmware protocol](../hardware/firmware-protocol.md)).
2. **Output samples (digital, exact).** `T11` and `I/O` are captured with the PicoScope and decoded in software into one integer per 125 µs sample. This is the gold standard for emulator comparison.
3. **Output waveform (analogue, measured).** The PicoScope measures the voltage across R2 (TP2), which is the raw DAC output **before** the filter chain. This checks the DAC transfer function and the hold behaviour, and it is the fallback if layer 2 cannot be used.

**All reference captures are taken directly at the TMS5220, before the audio stage.** Layer 3 measures pin 8 across R2 at TP2, DC-coupled and upstream of the audio stage's AC coupling capacitor, so the only thing between the DAC and the probe is the 1 K load. If R2's tolerance or the probe loading were to matter, R2 could be replaced with a 0.1% resistor of the same value, or pin 8 fed into a dedicated transimpedance stage used only for measurement. In practice R2's measured value is used ([bench hardware](bench-hardware.md#measurement-configuration)).

The filtered `Audio_out` signal is useful for listening tests, but it is not reference data. The 280 Hz high-pass and the 4th-order low-pass (see [audio output stage](../hardware/audio-output-stage.md)) change the waveform. Any filtered recording can be reproduced from layer 2 or layer 3 in software.
