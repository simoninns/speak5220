# 2. System clock

The original says the internal RC oscillator runs at 640 kHz for an 8 kHz sample rate, and that the clock is divided by four into two major phases. The measurements confirm the division ratios:

| Quantity | Relationship | At 640 kHz | Measured |
|---|---|---|---|
| Oscillator (`OSC`) | `fosc` | 640 kHz | Driven by the test board at 640.000 kHz (crystal-derived) |
| ROMCLK (pin 3) | `fosc` ÷ 4 | 160 kHz | 160.0005 kHz, period 6249.98 ns [p1-03] |
| Slot (bit time) | 1 ROMCLK period | 6.25 µs | 6.25 µs |
| Sample period | 20 slots = `fosc` ÷ 80 | 125 µs (8 kHz) | 124.9996 µs [p1-01, p1-02, p1-03] |
| Interpolation interval | 25 samples | 3.125 ms | 25 samples ([§8.7](excitation-interpolation.md#87-interpolation)) [p8-04, vectors `interp`] |
| Frame | 8 interpolation intervals = 200 samples | 25 ms (40 Hz) | 200 samples: frames are read 25.000 ms apart ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)) |

The measured periods differ from nominal by about 3 ppm. That is within the combined tolerance of the test board's crystal and the scope's timebase.

**Correction.** The original's 8 kHz column in Table A-1 gives the interpolation interval as 3.125 ms, but its §2 text says 3.25 ms. 25 samples of 125 µs is 3.125 ms.

ROMCLK is high for 50.6 % of each period (3162.7 ns of 6250 ns) [p1-03]. The original's Figure A-2 calls it PHI-2.

**Every `T11` and `I/O` transition follows a ROMCLK rising edge** [p1-03]. No transition lines up with a ROMCLK falling edge. [§8.4](serial-output.md#84-serial-sample-output-t11-and-io) gives the delays.

Driving `OSC` from an external clock is covered in [Appendix A](#appendix-a-system-clock).

## Appendix A: System clock

### A.1 Driving `OSC` from an external clock

**New.** The original shows only the RC option (Figure A-1): a resistor and capacitor network on `OSC`, trimmed by measuring ROMCLK. It does not describe an external clock.

`OSC` is not an ordinary logic input. It is the threshold node of the internal RC oscillator, whose timing network returns to `VDD` (−5 V). On the test board (the bench tests of [bench hardware](../capture/bench-hardware.md)):

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
