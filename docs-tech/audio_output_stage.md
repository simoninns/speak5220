# TMS5220 Audio Output Stage

This describes the analogue output stage in `kicad/tms5220_usb/audio_output.kicad_sch`. The circuit copies the speech path of the BBC Microcomputer Model B sound circuit (see `docs-tech/schematics/acorn_bbc_model_b_sound_schematics.png`). It takes the TMS5220's `SPEAKER` output (pin 8), turns it into a voltage, band-limits it and scales it to line level for `Audio_out` (J1).

In short, the chain is:

1. A current-to-voltage load and an AC coupling high-pass filter at about 280 Hz.
2. A 2nd-order Butterworth low-pass filter at about 7 kHz, with a gain of about 1.56.
3. An inverting stage with a gain of −0.39. In the BBC this was the mixer.
4. A resistive attenuator of about ÷5.7.
5. A second, identical 2nd-order low-pass filter at about 7 kHz, with a gain of about 1.56.

The result is a 4th-order low-pass filter (−3 dB at about 5.6 kHz, falling at 24 dB/octave), a 1st-order high-pass filter at about 280 Hz, and an overall passband gain of about **0.14 (−17 dB)**.

```
 TMS5220                                                                                      
 SPEAKER ──┬── C1 100n ──┬── R3 10K ──┬── R4 10K ──┬──[U2A +]          U2A: Sallen-Key LPF     
 (pin 8)   │             │            │            │                   f0 ≈ 6.95 kHz, Q ≈ 0.71 
          R1 1K         R2 4K7        C3 2n2 ──────┼── U2A out (pin 1) gain 1 + 22K/39K = 1.56 
           │             │            (to output)  C2 2n2                                      
          GND           GND                        GND                                        
                                                                                               
 U2A out ── R7 100K ──[U2D −]── R10 39K feedback ── U2D out (pin 14)   U2D: inverting, ×−0.39  
                                                                                               
 U2D out ── R11 4K7 ──┬── R13 10K ──┬── R14 10K ──┬──[U2C +]           U2C: Sallen-Key LPF     
                      │             │             │                    f0 ≈ 6.95 kHz, Q ≈ 0.71 
                     R12 1K         C6 2n2 ───────┼── U2C out (pin 8) ── Audio_out (J1)         
                      │             (to output)   C7 2n2 *                                     
                     GND                          GND *                                        
                                                                                               
 * C7 and the R16 39K gain-setting resistor should go to GND. See "Schematic issue" below.     
```

## Power and housekeeping

- **Supplies:** The LM324 (U2) runs from ±5 V. Each rail feeds it through a 10 Ω resistor (R8 on +5 V, R9 on −5 V) with a 10 µF capacitor to ground (C4, C5). Each pair forms an RC supply filter with a corner at about 1.6 kHz. It keeps digital noise on the rails out of the audio. C5 is fitted with its negative terminal on the −5 V side, which is correct.
- **Unused section (U2B):** U2B is wired as a voltage follower with its non-inverting input grounded, so the spare amplifier sits at a fixed, quiet state. In the BBC this section did a job in the SN76489 sound-chip path. That path has been left out here.
- **DC levels:** Every op-amp input has a DC path to 0 V, so all stage outputs sit close to 0 V. The only DC offset is the LM324's own offset multiplied by the gains. The output has no coupling capacitor, so J1 is DC-coupled at about 0 V. In the BBC, the next stage was a 10 K volume pot (VR1) followed by an AC-coupled LM386 power amplifier.

## Stage by stage

### 1. Current-to-voltage conversion and AC coupling (R1, C1, R2)

The TMS5220 `SPEAKER` pin is a **current source**, not a voltage output. The datasheet (§8.3) gives its range as 0–1.5 mA in steps of about 5.9 µA. Large positive sample values give the least current and large negative values give the most. During silence the output idles at the sample value −1, which is about 750 µA.

R1 (1 K) to ground converts this current to a voltage of 0–1.5 V, centred on about 0.75 V. Because more current means a more negative sample, **the voltage at this point is inverted relative to the digital sample value**.

The datasheet says the output must be AC-coupled. C1 (100 nF) does this by removing the 0.75 V idle level. R2 (4K7) to ground provides the DC reference after the capacitor.

**High-pass corner.** C1 sees R1 and R2 in series, so the corner is:

f = 1 / (2π · (1 K + 4K7) · 100 nF) ≈ **279 Hz**, 1st order (6 dB/octave)

This removes low-frequency energy below most of the speech band. A small speaker could not reproduce those frequencies anyway, and they would only use up headroom.

**Attenuation and source impedance.** In the passband, the R1/R2 network looks like a source of 4K7 / 5K7 ≈ **0.825 ×** the voltage across R1, with an output impedance of 1 K ∥ 4K7 ≈ **825 Ω**. That 825 Ω adds to R3 in the filter that follows, which is why that filter lands on exactly Butterworth.

### 2. First low-pass filter (U2A: R3, R4, C3, C2, R5, R6)

This is a unity-capacitor-ratio **Sallen-Key low-pass** filter with a non-inverting gain of K = 1 + R5/R6 = 1 + 22K/39K ≈ **1.564**.

- Ra = R3 + 825 Ω source = 10.825 K. Rb = R4 = 10 K.
- C3 = 2n2 is the feedback capacitor, which bootstraps from the output. C2 = 2n2 is the capacitor to ground.

f0 = 1 / (2π √(Ra·Rb·C3·C2)) ≈ **6.95 kHz**

Q = √(Ra·Rb·C3·C2) / (C2·(Ra + Rb) + Ra·C3·(1 − K)) ≈ **0.707**

That is a 2nd-order **Butterworth** response: flat to the corner, then falling at 12 dB/octave. Using equal capacitors with a gain of about 1.56 is a standard way to get Butterworth Q from a Sallen-Key filter. The BBC schematic labels this stage "7KHZ", which matches.

The passband gain from the voltage across R1 to the U2A output is 0.825 × 1.564 ≈ **1.29**. At the largest input swing of ±0.75 V, the U2A output swings about ±0.97 V, which is well within the LM324's limits on ±5 V.

### 3. Inverting stage (U2D: R7, R10)

U2D is an inverting amplifier with its non-inverting input at ground:

gain = −R10 / R7 = −39K / 100K = **−0.39**

In the BBC, this was the **summing mixer** for the computer's audio. R7 (100 K) carried the speech signal into the virtual-earth node at pin 13, alongside the SN76489 sound chip, the analogue input and other sources. Only the speech input is kept here, so the stage is a simple ×−0.39 attenuator. It also **inverts the signal again**, which cancels the inversion from the TMS5220's current output. The signal at `Audio_out` is therefore in phase with the digital sample value.

### 4. Attenuator (R11, R12)

R11 (4K7) and R12 (1 K) form a divider of 1K / 5K7 ≈ **0.175 (−15 dB)**, with a Thevenin output impedance of 4K7 ∥ 1K ≈ **825 Ω**. This lowers the level towards what the BBC's power amplifier expected. The values are chosen so the second filter sees the same 825 Ω source as the first one.

### 5. Second low-pass filter (U2C: R13, R14, C6, C7, R15, R16)

This is a component-for-component copy of stage 2:

- Ra = R13 + 825 Ω = 10.825 K. Rb = R14 = 10 K.
- C6 = 2n2 is the feedback capacitor and C7 = 2n2 the ground capacitor.
- K = 1 + R15/R16 = 1 + 22K/39K ≈ 1.564.

So it is also a **2nd-order Butterworth at about 6.95 kHz** with a gain of 1.564. The BBC schematic labels this stage "8KHZ", but with the values used it calculates to the same 6.95 kHz as the first stage. The label looks like a nominal figure. U2C pin 8 drives `Audio_out` directly.

## Overall response

**Passband gain.** Measured from the voltage across R1 to `Audio_out`:

0.825 × 1.564 × (−0.39) × 0.175 × 1.564 ≈ **−0.138 (−17.2 dB)**

Measured from the digital sample value, there is no net inversion. A full-scale input is 1.5 V peak-to-peak across R1, which gives about **200 mV peak-to-peak (about ±0.1 V)** at `Audio_out`. That is a modest consumer line level.

**Frequency response.** The two identical Butterworth sections in cascade make a 4th-order low-pass filter that is **not** Butterworth overall. It is −3 dB at about 5.6 kHz, −6 dB at 6.95 kHz, and falls at 24 dB/octave above that. Combined with the 279 Hz high-pass, the response relative to the passband is:

| Frequency | High-pass (C1) | Low-pass (both stages) | Total |
|---|---|---|---|
| 100 Hz | −9.4 dB | 0.0 dB | −9.4 dB |
| 200 Hz | −4.7 dB | 0.0 dB | −4.7 dB |
| 279 Hz | −3.0 dB | 0.0 dB | −3.0 dB |
| 500 Hz | −1.2 dB | 0.0 dB | −1.2 dB |
| 1 kHz | −0.3 dB | 0.0 dB | −0.3 dB |
| 2 kHz | −0.1 dB | −0.1 dB | −0.1 dB |
| 3 kHz | 0.0 dB | −0.3 dB | −0.3 dB |
| 4 kHz | 0.0 dB | −0.9 dB | −0.9 dB |
| 5.6 kHz | 0.0 dB | −3.0 dB | −3.0 dB |
| 6.95 kHz | 0.0 dB | −6.0 dB | −6.0 dB |
| 8 kHz | 0.0 dB | −8.8 dB | −8.8 dB |
| 12 kHz | 0.0 dB | −19.9 dB | −19.9 dB |
| 16 kHz | 0.0 dB | −29.3 dB | −29.3 dB |

## Why the filtering looks like this

The TMS5220 produces a new 8-bit sample every 125 µs, which is an **8 kHz sample rate** at the nominal 640 kHz oscillator. Each sample is held until the next one, so the raw `SPEAKER` output is a staircase. That staircase contains the wanted speech band up to 4 kHz plus images of it around 8 kHz, 16 kHz and so on. These images are heard as a harsh, buzzy, "digital" edge.

The two low-pass sections work as a **reconstruction (anti-imaging) filter**:

- They are almost flat through the useful speech band. The loss is less than 1 dB up to 4 kHz.
- They smooth the staircase by reducing the image energy: about 9 dB at 8 kHz, about 20 dB at 12 kHz and about 29 dB at 16 kHz.

This is a compromise rather than a brick wall. Images near 4–8 kHz are only partly removed, and the 4 kHz Nyquist limit is not enforced sharply. The result was good enough for the BBC's small internal speaker, cheap to build from one quad op-amp, and it shares the same LM324 that the mixer used.

The ~280 Hz high-pass rolls off the low bass, which LPC speech hardly uses and a small speaker can't reproduce. It also removes the DC idle level required by the datasheet.

One further effect: the hold behaviour of the TMS5220's D/A converter rolls off the top of the band slightly (about −2 dB at 3 kHz and −3.9 dB at 4 kHz for an 8 kHz rate). The filter does not correct for this, so the high frequencies are a little darker than the raw LPC output.

## Component cross-reference to the BBC Model B

| This design | BBC Model B | Function |
|---|---|---|
| R1 1K | R53 1K | TMS5220 current-to-voltage load |
| C1 100n | C21 100nF | AC coupling / high-pass |
| R2 4K7 | R31 4K7 | High-pass reference / divider |
| R3, R4 10K | R30, R1 10K | Filter 1 resistors |
| C3, C2 2n2 | C3, C1 2n2 | Filter 1 feedback / ground capacitors |
| R5 22K, R6 39K | R4 22K, R9 39K | Filter 1 gain (1.56) |
| R7 100K | R7 100K | Mixer input (speech) |
| R10 39K | R15 39K | Mixer feedback |
| R11 4K7, R12 1K | R28 4K7, R29 1K | Attenuator |
| R13, R14 10K | R27, R17 10K | Filter 2 resistors |
| C6, C7 2n2 | C11, C7 2n2 | Filter 2 feedback / ground capacitors |
| R15 22K, R16 39K | R15 22K, R24 39K | Filter 2 gain (1.56) |
| R8, R9 10R | R14, R18 10R | Supply filter resistors |
| C4, C5 10µF | C5, C9 10µF | Supply decoupling |
| U2 LM324 | IC17 LM324 | Quad op-amp (same pin usage) |

Not reproduced from the BBC: the other mixer inputs (R11, R5 and R12 for the analogue input and SN76489 paths), the U2B rectifier circuit (D2, C2, R8), and the VR1 volume pot with the LM386 speaker amplifier.
