# Audio output stage

This describes the analogue path from the TMS5220's `SPEAKER` output (pin 8) to the line output and the speaker. The `SPEAKER` load is on the top sheet (`tms5220_usb.kicad_sch`). The filters are on the audio output sheet (`audio_output.kicad_sch`), and the speaker amplifier is on the Power Amp sheet (`poweramp.kicad_sch`). The filter circuit copies the speech path of the BBC Microcomputer Model B's sound circuit, as drawn in Acorn's sound schematic for the Model B.

The chain is:

1. **Current-to-voltage load and AC coupling.** A 1 K load (R2) and a high-pass filter at about 280 Hz (C1, R3).
2. **First low-pass filter.** A 2nd-order Butterworth filter at about 7 kHz with a gain of about 1.56 (U2A).
3. **Inverting stage.** A gain of −0.39 (U2D). In the BBC this was the mixer.
4. **Attenuator.** A resistive divider of about ÷5.7 (R10, R11).
5. **Second low-pass filter.** An identical 2nd-order Butterworth filter (U2C). It drives the line output `Audio_out` (J2).
6. **Speaker amplifier.** A volume control (RV1) and an LM386 amplifier (U6), driving the speaker connector (J3).

![Audio output stage schematic](img/schematic_audio_output.svg)

*Figure: the audio output sheet (`audio_output.kicad_sch`). `5220_in` is the `SPEAKER` net from the top sheet. `Audio_out` goes to J2 on the top sheet and to the Power Amp sheet.*

![Power amplifier schematic](img/schematic_poweramp.svg)

*Figure: the Power Amp sheet (`poweramp.kicad_sch`).*

From `SPEAKER` to `Audio_out`, the result is:

- a 4th-order low-pass filter, −3 dB at about 5.5 kHz and falling at 24 dB/octave;
- a 1st-order high-pass filter at about 280 Hz;
- a passband gain of about **0.138 (−17.2 dB)** relative to the voltage across R2, or about 0.13 V per mA of `SPEAKER` current.

```
 TMS5220
 SPEAKER ──┬── C1 100n ──┬── R4 10K ──┬── R5 10K ──┬──[U2A +, pin 3]    U2A: Sallen-Key LPF
 (pin 8)   │             │            │            │                    f0 ≈ 6.95 kHz, Q ≈ 0.71
          R2 1K         R3 4K7        C2 2n2 ──────┼── U2A out (pin 1)  gain 1 + R6/R7
           │             │            (to U2A out) C3 2n2               = 1 + 22K/39K = 1.56
          GND           GND                        GND
 (top sheet)

 U2A out ── R8 100K ──[U2D −, pin 13]── R9 39K feedback ── U2D out (pin 14)   U2D: inverting, ×−0.39

 U2D out ── R10 4K7 ──┬── R12 10K ──┬── R13 10K ──┬──[U2C +, pin 10]    U2C: Sallen-Key LPF
                      │             │             │                     f0 ≈ 6.95 kHz, Q ≈ 0.71
                     R11 1K         C4 2n2 ───────┼── U2C out (pin 8) ── Audio_out (J2)
                      │             (to U2C out)  C5 2n2                 gain 1 + R14/R15 = 1.56
                     GND                          GND

 Audio_out ── RV1 10K (volume) ── wiper ── C13 100n ──[U6 +, pin 3]   U6: LM386, gain 20
                                                       U6 out (pin 5) ── C15 47µ ── J3 speaker
                                                                    └── R24 10R ── C14 47n ── GND
```

All figures on this page are calculated from the component values on the schematic. The filter figures come from a nodal analysis of the netlist with ideal op-amps. The signal levels use the DAC currents measured in the [reference datasheet (§8.2)](../datasheet/dac-and-audio.md#82-da-conversion), not the original data manual's design values.

## Power and housekeeping

- **Supplies.** The LM324 (U2) runs from ±5 V. Each rail feeds it through a 10 Ω resistor (R16 on +5 V, R17 on −5 V) with a 10 µF capacitor to ground (C6 on V+, C7 on V−). Each pair forms an RC supply filter with a corner at about 1.6 kHz, which keeps digital noise on the rails out of the audio. C7 has its positive terminal on ground and its negative terminal on the −5 V side, which is correct.
- **Unused section (U2B).** U2B is wired as a voltage follower with its non-inverting input (pin 5) grounded, so the spare amplifier sits at a fixed, quiet state. In the BBC this section did a job in the SN76489 sound-chip path, which has been left out here.
- **DC levels.** Every op-amp input has a DC path to 0 V, so all stage outputs sit close to 0 V. The only DC offset is the LM324's own offset multiplied by the gains. `Audio_out` has no coupling capacitor, so J2 is DC-coupled at about 0 V. The speaker amplifier is AC-coupled at its input (C13) and its output (C15).
- **Speaker amplifier supply.** U6 (LM386) runs from +5 V alone (pin 6), with ground on pin 4.

## Stage by stage

### 1. Current-to-voltage conversion and AC coupling (R2, C1, R3)

The TMS5220 `SPEAKER` pin is a **current output**, not a voltage output. The pin pushes current into a load returned to 0 V. Large positive sample values give the least current, and large negative values give the most. The [reference datasheet (§8.2, §8.3)](../datasheet/dac-and-audio.md) measures it into about 1 K:

| Code | Current | Voltage across R2 |
|---|---|---|
| +127 | 0 µA | 0 V |
| −1 (idle, during silence) | 604 µA | 0.60 V |
| −128 (full scale) | about 1160 µA | about 1.16 V |
| One step | 4.49 µA | 4.5 mV |

R2 (1 K, on the top sheet) to ground converts the current to this voltage. Because more current means a more negative sample, **the voltage at this point is inverted relative to the digital sample value**. TP2 (`PROBE_A`) is on this node.

The output must be AC-coupled. C1 (100 nF) does this by removing the 0.6 V idle level. R3 (4K7) to ground provides the DC reference after the capacitor.

**The chip's output resistance.** The output is not an ideal current source: §8.3 reads it as about 10–16 kΩ in parallel with the pin. In parallel with R2, that lowers the voltage, and with it the gain of the whole chain, by 0.4–0.7 dB. The figures below are for an ideal current source.

**High-pass corner.** C1 sees R2 and R3 in series, so the corner is:

f = 1 / (2π · (1 K + 4K7) · 100 nF) ≈ **279 Hz**, 1st order (6 dB/octave)

This removes low-frequency energy below most of the speech band. A small speaker could not reproduce those frequencies anyway, and they would only use up headroom.

**Attenuation and source impedance.** In the passband, the R2/R3 network looks like a source of 4K7 / 5K7 ≈ **0.825 ×** the voltage across R2, with an output impedance of 1 K ∥ 4K7 ≈ **825 Ω**. That 825 Ω adds to R4 in the filter that follows, which is why that filter lands on exactly Butterworth.

### 2. First low-pass filter (U2A: R4, R5, C2, C3, R6, R7)

This is a unity-capacitor-ratio **Sallen-Key low-pass** filter with a non-inverting gain of K = 1 + R6/R7 = 1 + 22K/39K ≈ **1.564**.

- Ra = R4 + 825 Ω source = 10.825 K. Rb = R5 = 10 K.
- C2 = 2n2 is the feedback capacitor, from the R4/R5 junction to the U2A output. C3 = 2n2 is the capacitor from the non-inverting input to ground.

f0 = 1 / (2π √(Ra·Rb·C2·C3)) ≈ **6.95 kHz**

Q = √(Ra·Rb·C2·C3) / (C3·(Ra + Rb) + Ra·C2·(1 − K)) ≈ **0.707**

That is a 2nd-order **Butterworth** response: flat to the corner, then falling at 12 dB/octave. Using equal capacitors with a gain of about 1.56 is a standard way to get a Butterworth Q from a Sallen-Key filter. The schematic, like the BBC's, labels this stage "7KHz", which matches.

The passband gain from the voltage across R2 to the U2A output is 0.825 × 1.564 ≈ **1.29**. The full code range is 1.16 V peak-to-peak across R2, so the U2A output swings about 1.5 V peak-to-peak. That is well within the LM324's limits on ±5 V.

### 3. Inverting stage (U2D: R8, R9)

U2D is an inverting amplifier with its non-inverting input (pin 12) at ground:

gain = −R9 / R8 = −39K / 100K = **−0.39**

In the BBC, this was the **summing mixer** for the computer's audio. The speech signal came into the virtual-earth node at pin 13 through a 100 K resistor, alongside the SN76489 sound chip, the analogue input and other sources. Only the speech input is kept here, so the stage is a simple ×−0.39 attenuator. It also **inverts the signal again**, which cancels the inversion from the TMS5220's current output. The signal at `Audio_out` is therefore in phase with the digital sample value.

### 4. Attenuator (R10, R11)

R10 (4K7) and R11 (1 K) form a divider of 1K / 5K7 ≈ **0.175 (−15 dB)**, with a Thevenin output impedance of 4K7 ∥ 1K ≈ **825 Ω**. This lowers the level towards what the BBC's power amplifier expected. The values are chosen so that the second filter sees the same 825 Ω source as the first one.

### 5. Second low-pass filter (U2C: R12, R13, C4, C5, R14, R15)

This is a component-for-component copy of stage 2:

- Ra = R12 + 825 Ω = 10.825 K. Rb = R13 = 10 K.
- C4 = 2n2 is the feedback capacitor, and C5 = 2n2 the capacitor to ground.
- K = 1 + R14/R15 = 1 + 22K/39K ≈ 1.564.

So it is also a **2nd-order Butterworth at about 6.95 kHz** with a gain of 1.564. The schematic, like the BBC's, labels this stage "8KHz". With the values used it calculates to the same 6.95 kHz as the first stage, so the label looks like a nominal figure. U2C pin 8 drives `Audio_out` directly. It goes to J2 and to the top of RV1.

### 6. Speaker amplifier (Power Amp sheet: RV1, C13, U6, C15, R24, C14)

- **Volume.** RV1 (10 K) is a volume control across `Audio_out`. Its 10 K is an easy load for the LM324.
- **Input coupling.** The wiper feeds U6's non-inverting input (pin 3) through C13 (100 nF). The inverting input (pin 2) is grounded. The LM386's input resistance is about 50 kΩ (its data sheet's typical value), so C13 makes a high-pass corner at about **32 Hz**, well below the 279 Hz of stage 1.
- **Gain.** The gain pins (1 and 8) and the bypass pin (7) are left open, so U6 runs at its internal gain of **20 (26 dB)**.
- **Output coupling.** The output (pin 5) drives the speaker (J3) through C15 (47 µF). C15 and the speaker form a second high-pass filter. The schematic does not give the speaker's impedance:

    | Speaker | Corner |
    |---|---|
    | 4 Ω | about 850 Hz |
    | 8 Ω | about 420 Hz |
    | 16 Ω | about 210 Hz |

    With an 8 Ω speaker, this corner, not the 279 Hz of stage 1, sets the bass roll-off at the speaker.
- **Zobel network.** R24 (10 Ω) and C14 (47 nF) to ground keep the LM386 stable into the speaker's inductance. This is the usual LM386 arrangement.

With RV1 at maximum, a full-scale signal (about 0.15 V peak-to-peak at `Audio_out`, see below) becomes about 3.1 V peak-to-peak at U6's output. That is close to what an LM386 can swing from a single 5 V supply, so the loudest data may clip at full volume.

## Overall response

**Passband gain.** From the voltage across R2 to `Audio_out`, with C1 treated as a short circuit:

0.825 × 1.564 × (−0.39) × 0.175 × 1.564 ≈ **−0.138 (−17.2 dB)**

The low-pass and high-pass skirts overlap, so the response never quite reaches this figure: it peaks at −0.2 dB relative to it, around 1.5–2 kHz.

From the digital sample value, there is no net inversion. A full-scale input is about 1.16 V peak-to-peak across R2, which gives about **160 mV peak-to-peak (about ±0.08 V)** at `Audio_out`. That is a modest line level. With the original data manual's 1.5 mA full scale it would be about 200 mV, but this chip measures 1.16 mA (§8.2). Through the speaker amplifier, see stage 6.

**Frequency response.** The two identical Butterworth sections in cascade make a 4th-order low-pass filter that is **not** Butterworth overall. It is −3 dB at about 5.5 kHz and −6 dB at about 6.95 kHz, and falls at 24 dB/octave above that. Together with the 279 Hz high-pass, the response at `Audio_out` relative to the passband gain is as follows. The total is the full circuit; the separate columns are for each filter alone.

| Frequency | High-pass (C1) | Low-pass (both stages) | Total |
|---|---|---|---|
| 100 Hz | −9.4 dB | 0.0 dB | −9.4 dB |
| 200 Hz | −4.7 dB | 0.0 dB | −4.7 dB |
| 279 Hz | −3.0 dB | 0.0 dB | −3.0 dB |
| 500 Hz | −1.2 dB | 0.0 dB | −1.2 dB |
| 1 kHz | −0.3 dB | 0.0 dB | −0.4 dB |
| 2 kHz | −0.1 dB | 0.0 dB | −0.2 dB |
| 3 kHz | 0.0 dB | −0.3 dB | −0.4 dB |
| 4 kHz | 0.0 dB | −0.9 dB | −1.0 dB |
| 5.5 kHz | 0.0 dB | −2.9 dB | −3.0 dB |
| 6.95 kHz | 0.0 dB | −6.0 dB | −6.1 dB |
| 8 kHz | 0.0 dB | −8.8 dB | −8.9 dB |
| 12 kHz | 0.0 dB | −19.9 dB | −19.9 dB |
| 16 kHz | 0.0 dB | −29.3 dB | −29.3 dB |

The full circuit differs from an ideal 1st-order high-pass at 279 Hz cascaded with two ideal Butterworth sections at 6.95 kHz by at most 0.1 dB between 30 Hz and 20 kHz. `investigation/tools/process.py` uses that ideal cascade (`listenAudio`) to make the "BBC" listening files. The application's BBC Micro [output filter](../../application/user-manual/waveforms.md#output-filter) uses the same cascade.

## Why the filtering looks like this

The TMS5220 produces a new sample every 125 µs, which is an **8 kHz sample rate** at the nominal 640 kHz oscillator. Each sample is held until the next one, so the raw `SPEAKER` output is a staircase. That staircase contains the wanted speech band up to 4 kHz, plus images of it around 8 kHz, 16 kHz and so on. These images are heard as a harsh, buzzy, "digital" edge.

The two low-pass sections work as a **reconstruction (anti-imaging) filter**:

- They are almost flat through the useful speech band. The loss is about 1 dB at 4 kHz.
- They smooth the staircase by reducing the image energy: about 9 dB at 8 kHz, about 20 dB at 12 kHz and about 29 dB at 16 kHz.

This is a compromise rather than a brick wall. Images near 4–8 kHz are only partly removed, and the 4 kHz Nyquist limit is not enforced sharply. The result was good enough for the BBC's small internal speaker, and cheap to build from one quad op-amp shared with the mixer.

The ~280 Hz high-pass rolls off the low bass. LPC speech hardly uses it, and a small speaker can't reproduce it. It also removes the DC idle level, which must not reach an amplifier.

One further effect: holding each sample for a full period rolls off the top of the band slightly (sin x / x: about −2.1 dB at 3 kHz and −3.9 dB at 4 kHz, at an 8 kHz rate). The filter does not correct for this, so the high frequencies are a little darker than the raw LPC output.

## Component cross-reference to the BBC Model B

| This design | BBC Model B | Function |
|---|---|---|
| R2 1K (top sheet) | R53 1K | TMS5220 current-to-voltage load |
| C1 100n | C21 100nF | AC coupling / high-pass |
| R3 4K7 | R31 4K7 | High-pass reference / divider |
| R4, R5 10K | R30, R1 10K | Filter 1 resistors |
| C2, C3 2n2 | C3, C1 2n2 | Filter 1 feedback / ground capacitors |
| R6 22K, R7 39K | R4 22K, R9 39K | Filter 1 gain (1.56) |
| R8 100K | R7 100K | Mixer input (speech) |
| R9 39K | R15 39K | Mixer feedback |
| R10 4K7, R11 1K | R28 4K7, R29 1K | Attenuator |
| R12, R13 10K | R27, R17 10K | Filter 2 resistors |
| C4, C5 2n2 | C11, C7 2n2 | Filter 2 feedback / ground capacitors |
| R14 22K, R15 39K | R15 22K, R24 39K | Filter 2 gain (1.56) |
| R16, R17 10R | R14, R18 10R | Supply filter resistors |
| C6, C7 10µF | C5, C9 10µF | Supply decoupling |
| U2 LM324 | IC17 LM324 | Quad op-amp (same pin usage) |
| RV1 10K | VR1 10K | Volume control |
| U6 LM386 | LM386 | Speaker amplifier |

The BBC designators are as read from Acorn's schematic. Two BBC designators appear twice in the BBC column (R15 for both the mixer feedback and a filter 2 gain resistor), and the schematic is not in the repository, so that column could not be checked.

Not reproduced from the BBC: the other mixer inputs (R11, R5 and R12, for the analogue input and SN76489 paths) and the U2B rectifier circuit (D2, C2, R8).

## On the bench

For DAC measurements, the breadboard link between the `SPEAKER`/R2 node and C1 is removed, so TP2 sees R2 alone ([bench hardware](../capture/bench-hardware.md)). The schematic shows C1 connected directly. With the link fitted, C1 and the filter load the pin, and the voltage on TP2 depends on earlier samples.
