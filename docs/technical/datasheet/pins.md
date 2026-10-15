# 11. Pin assignments and functions

## 11.2 Pin functions

| Pin | Name | Original | Measured function |
|---|---|---|---|
| 3 | ROMCLK | Clock to VSM | `OSC` ÷ 4, 50.6 % duty. Its rising edges start each slot, and all `T11` and `I/O` transitions follow them ([§2](clock.md#2-system-clock), [§8.4](serial-output.md#84-serial-sample-output-t11-and-io)) |
| 6 | `OSC` | Oscillator input | The threshold node of the internal RC oscillator. An external clock must swing from `VDD` to `VSS`. A 0–5 V square wave does not clock the chip (Appendix A) |
| 7 | `T11` | Sync | High for slot 0 of each sample period. It frames the `I/O` data and runs continuously ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)) |
| 8 | `SPEAKER` | Audio output | DAC current output into a load to 0 V. About 4.5 µA per code, with a step of about 26 µA between codes −1 and 0. The current depends on the load. It changes at the start of slot 1 ([§8.2](dac-and-audio.md#82-da-conversion), [§8.3](dac-and-audio.md#83-audio-output)) |
| 9 | `I/O` | Serial data out | The 10-bit output sample before clipping, LSB first, in slots 2–11. It runs continuously, and sends −1 when idle ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)). With `TEST` high it is not driven and becomes the DAC's input instead: held high it gives code −1, held low code 0 (pin 20) |
| 10 | `PROM OUT` | Testing use only | The parameter ROM's output: for each parameter in turn, the table value at its stored coded index, 10 bits LSB first, one bit per slot. It runs continuously and repeats every 25 sample periods. Open drain: it pulls up towards `VSS` and otherwise floats. Unconnected in every measurement except those of [§8.10](prom-out.md#810-parameter-rom-output-prom-out) |
| 20 | `TEST` | Testing use only | **New.** An active-high test input. With it high, the frame sequence does not advance and `I/O` becomes the DAC's input ([below](#pin-20-test)) |
| 1, 12–14, 19, 22, 24, 26 | D0–D7 | Data bus | TI numbers D0 as the most significant bit. Commands and FIFO data are written on it; status and Read Byte data are read from it. A byte from Read Byte arrives bit-reversed ([§6](commands.md#6-description-of-commands)) |
| 27, 28 | `WS`, `RS` | Write and read strobes | Active low. A write or status read completes 12–24 µs after the strobe, when READY goes active ([§3](cpu-interface.md#3-cpu-interface)) |
| 18 | `READY` | Transfer handshake | Active low. Inactive while a transfer is in progress; held inactive for a second command written while one is held during speech ([§3](cpu-interface.md#3-cpu-interface)) |
| 17 | `INT` | Interrupt | Active low. Set by Talk Status clearing and Buffer Low setting, not by Buffer Empty; released by a status read ([§3](cpu-interface.md#3-cpu-interface)) |
| 15, 16 | M0, M1 | VSM control | One Load Address, Read, Dummy Read or Read and Branch function per command or frame bit ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)) |
| 2, 21, 23, 25 | ADD1, ADD8, ADD4, ADD2 | VSM address and data | Carry Load Address nibbles out; ADD8 carries the data bits in, read about 10 µs after M0 rises ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)) |

### Pin 20, `TEST`

**New.** `TEST` is an active-high test input. [US 4,335,277](../sources/patent-us4335277/index.md) Fig. 8g shows a `TEST` pad with an on-chip load to `VDD`, NANDed with two internal signals, C4 and C5, whose source is not drawn, to make two test signals. TEST1 (Fig. 7a) forces the interpolation counter to step. TEST2 (Figs. 11c, 11d) disables the `I/O` output driver and connects the `I/O` pad into the serial chain that feeds the DAC, while blocking that chain's load from the Y latch: the DAC is then driven from outside, through `I/O`.

The die agrees that pin 20 is a protected input: a diffusion resistor, a short metal line and a contact into the control logic at (10,412, 1,709); a little further on, the image cannot be followed. The TMS5220NL and TMS5200NL die images were both used. What C4 and C5 are is not known ([Appendix B](gaps.md#the-test-mode)); on chip 1 both test signals act with `TEST` high alone.

It floats on the test board, where the chip runs normally. Left floating, it sits at −4.32 V on a meter (chip 1), near `VDD`, as the patent's load predicts. So that level is the normal state, and the test mode is asserted by pulling the pin to `VSS`.

**Bench trials (chip 1).** `TEST` through 1 kΩ to +5 V (+4.80 V on the pin), fitted with the power off. At power-up the status read 0xFF; after Reset, 0x7F, and writes worked. A Speak loaded its address, did its dummy read and set Talk Status, but no frame was ever read, and Talk Status stayed set: the frame sequence does not advance (TEST1). `T11` kept its 125 µs period and `PROM OUT` its field stream.

**`I/O` becomes an input to the DAC (TEST2).** The pin is not driven: it follows a 4.7 kΩ pull-up or pull-down in all 20 slots. With `I/O` held high, `SPEAKER` sat at 0.6153 V, which is code −1 (all ones). Moving the 4.7 kΩ to ground in the same power-up dropped it by 28.4 mV to 0.5869 V, which is code 0 (all zeros). That is the measured −1 → 0 step of the same day (28.6 mV, from 0.6102 to 0.5816 V in normal mode). How the bits are clocked in (which slots, which order) has not been measured ([Appendix B](gaps.md#the-test-mode)).

**Handling.** Changing `TEST` with the chip running, and power-cycling by hand, left the write logic hung (READY never returned) until a clean power cycle. Afterwards the chip passed the Pico self-test, and an ACORN take departed from the model at the same sample (218) as its reference takes of four days earlier, and by a similar count (1,606 of 5,899 samples, against 1,548–1,608).

The records of the bench trials were kept locally, outside the repository.
