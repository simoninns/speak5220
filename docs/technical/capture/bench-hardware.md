# Bench hardware

The capture board differs from a BBC Micro speech system in a few ways that the measurements need. The board itself, its schematics and the firmware are described in [hardware](../hardware/index.md).

## Probe test points

Test points TP2–TP9 carry the PicoScope probes and their grounds, and TP10 carries `PROM OUT` to the logic input ([using the PicoScope](oscilloscope.md#wiring)).

## Pico-driven oscillator

The original BBC-style RC oscillator is removed. The TMS5220 is always clocked by the Pico. Pico GPIO22 (`OSC_PICO_3V3`) drives the spare 74AHCT125 gate U5D, whose ~OE is tied to GND. U5D's 0/5 V output drives a PNP level shifter (Q1 BC557: R1 10K to the base, R25 3.3K from base to emitter, emitter to +5 V, R26 1K from collector to −5 V). The collector drives `OSC` (U1 pin 6) between −5 V and +5 V. The clock is inverted, which doesn't matter.

The original datasheet (Figure A-1) shows only the RC option. `OSC` is not an ordinary logic input: it is the threshold node of the internal RC oscillator, whose timing network returns to −5 V. **A 0–5 V square wave does not clock the chip.** This was tested on the board: there was no ROMCLK with 640 kHz at 0–5 V, and none with 320 kHz at 0–5 V and `PROM OUT` strapped towards −5 V. The TI-99/4A note that `PROM OUT` at −5 V enables a 320 kHz external clock does not hold for this chip.

The working reference is the Atari Star Wars sound PCB (SP-225, sheet 16A). There, an original TMS5220 is clocked at 672 kHz from a 74LS161 through a 2N3906 PNP (collector 1K to −5 V) and two CD4069 inverters running from ±5 V, so `OSC` swings rail to rail. This board's circuit is that transistor stage without the CMOS buffer. **Verified on the board:** `OSC` swings between −5 V and +5 V at 640 kHz, ROMCLK on TP1 reads 160 kHz, and the TMS5220 speaks the Acorn PHROM words with no READY timeouts.

Pin 10 drives strongly, so it is not strapped. It is the parameter ROM's open-drain output, which pulls up towards +5 V, and it goes to the logic input D0 ([datasheet §8.10](../datasheet/prom-out.md)).

A clock at exactly 640 kHz, derived from the Pico's crystal, makes the sample rate exactly 8000 Hz. It also makes every run sample-aligned and repeatable, and lets the clock be swept to test timing edge cases. The measured clock (from `ROMCLK` on TP1, which should read 160 kHz) is recorded in every capture.

**The firmware generates this clock** (`tms5220ClockStart()` in `investigation/firmware/src/tms5220.c`). Without it the TMS5220 has no clock at all. 640 kHz does not divide evenly into the RP2350's default 150 MHz system clock, so the firmware sets the system clock to 144 MHz: the 12 MHz crystal × 120 gives a 1440 MHz VCO, ÷ 5 ÷ 2. That is within the chip's rating, and a PWM slice divides it by 225 to give exactly 640 kHz. To sweep the clock, pick a system clock that is a multiple of the wanted frequency, or accept a non-integer PWM period.

## Measurement configuration

The test board is built on a breadboard. For DAC measurements, the link from the `SPEAKER` / R2 node to C1 is removed, so TP2 sees R2 alone. R2 measures 996 Ω on a meter. Its tolerance does not matter, because the measured value is used. For the load test, a second 1 K was put in parallel with R2 (482 Ω measured). `investigation/tools/capture.py --board measure|measure-half|audio --load-ohms <R>` records the configuration in each `capture.json`. The audio output works again once the C1 link is put back (`--board audio`).

## Pull-down on GPIO22

R27 (10K, `OSC_PICO_3V3` to GND at U5D's input) gives U5D a defined low input while the Pico boots, before the firmware starts the clock. U5D's output is then low, so Q1 is on and `OSC` is held at +5 V instead of floating. R27 is fitted on the board and in the schematic.

## Possible additions

None of these is built.

1. **`T11` and `I/O` into the Pico.** This is not needed, because the PicoScope captures both. It would only matter for long unattended runs without the scope. They swing from −5 V to +5 V, so they need a clamped level shift to 0–3.3 V (a plain divider is not enough, because the Pico pins must never go negative). Options are a series resistor with Schottky clamps to GND and 3.3 V, or a small NPN stage per signal. GPIO27 and GPIO28 are free.
2. **A virtual-ground DAC measurement.** A transimpedance stage would hold `SPEAKER` at 0 V and give the DAC current with no load effect ([datasheet §8.3](../datasheet/dac-and-audio.md)). The spare LM324 section U2B could do this.
3. **Pico-switched TMS5220 power.** The firmware cannot power-cycle the TMS5220, and stopping its clock mid-speech has left it unable to complete a write until it was power-cycled by hand ([firmware protocol](../hardware/firmware-protocol.md)). Power cycles are a test condition for [repeatability](work/repeatability.md). A high-side switch on the ±5 V rails, driven from a spare GPIO (27 or 28), would make them automatic. It would also let the host recover a wedged chip without help. The datasheet's condition for a reliable power-up clear applies: VSS − VDD must rise to +10 V in under 2 ms, on both rails together.
4. **`TEST` and `I/O` control from the Pico** (designed, not built). A second 74AHCT125 (U7; AHCT, because AHC inputs need 3.85 V at 5 V) on the two free GPIOs. GPIO27 is `~TEST_EN` (10K pull-up to 3V3, so all outputs are off while the Pico boots) and drives the three gates' `~OE` together, so `I/O` can only be driven while `TEST` is asserted:
    - U7A: A at +5 V, output through a BAT85 (D3) and 1K (R39) to `TEST` (pin 20): about 4.7 V when enabled; when off, D3 leaves the pin floating at its own −4.3 V.
    - U7B: A from GPIO28 (`IO_DRV`), output through a BAT85 (D4) and 1K (R40) to `I/O` (pin 9): about 4.6 V for a 1.
    - U7C: A at GND, a 47K (R41) and a BAT85 (D5, anode at `I/O`) from `I/O` to its output: the 0 level (about 0.3 V) in test mode.
    - U7D: spare (a power switch for the chip, item 3, if leaving test mode turns out to need a power cycle).

    The diodes matter: AHCT outputs clamp to ground even when disabled, and in normal mode `TEST` sits at −4.3 V and `I/O` swings to −5 V. `OSC` stays on U5D and Q1, whose timing the start-slot calibration was measured with. The bits can be clocked by a PIO from the same PWM as `OSC` (80 `OSC` cycles per sample), with the phase to `T11` found once on the scope, so no `T11` input is needed. Planned firmware pin names: `PIN_TEST_EN_N` 27, `PIN_IO_DRV` 28. The purpose is to set every DAC code directly ([known gaps](../datasheet/gaps.md#the-test-mode)).
