# Appendix B: Known gaps

This appendix is the one list of what is not known about the chip. Each entry gives what is unknown, what is known around it, what it affects, and what would settle it. The section it belongs to says only that it is not known, and links here. Whatever is not listed here has been measured, or follows from what has.

Most of the gaps do not change the `I/O` codes of any capture in the data set. The ones that do are the first five under [Synthesis](#synthesis).

## Synthesis

### The lowest-order arithmetic

[§8.8](lattice-filter.md#the-lowest-order-arithmetic). The model's voiced output is one code off at about one sample per pitch period, at a fixed point in the period, mostly where the waveform is near zero [p8-04, p8-05]. On ordinary voiced data the model is about 6 dB further from either chip than the two chips are from each other. §8.8 lists what the error does (it is deterministic, period-doubled, energy-dependent and present in the first chirp of a strong resonance) and every form of the multiplier and adder that has been ruled out.

- **Known:** most of it is deterministic. About 87 % of the model's departures on PROM words start at samples where all of the chip's takes agree. Departures, deterministic and marginal alike, are enriched for coefficients whose second Booth digit is negative (*d*₁ < 0).
- **Affects:** bit-exactness on voiced data. The acceptance tests allow for it with a tolerance ([§8.8](lattice-filter.md#the-lowest-order-arithmetic), "Expected agreement"). It also sets what a pause of silence frames sounds like: the chip and the model ring through it with different limit cycles, and one can ring where the other is silent [p12-01, p12-02, p12-07, p12-08]. On the chip a neutral frame silenced every pause tested, so authoring need not wait for the rule ([§12.2](authoring.md#122-when-a-frame-is-heard)), but the model cannot show which pauses ring.
- **To settle:**
    - A trace of the multiplier's low bit slices, the adder's low end and the delay stack's low-order bits on the die images.
    - A candidate rule tested against the labelled departures [`investigation/captures/phase10/departures/`]. The sharpest test is the ring vector set [vectors `ring`], with the change made in the model.

### Overflow

[§8.8](lattice-filter.md#88-lattice-filter), "Word length and overflow". Every sum is modelled as wrapping at 14 bits, following [US 4,335,277](../sources/patent-us4335277/index.md). The die shows 14-bit rows. The wrap is still the best fit, not a proof: loud vectors match only about 60 % of their samples, because their one-code differences grow without limit [p8-12, vectors `loud`, vectors `random`]. It is settled together with the lowest-order arithmetic.

### Upsets and chip-specific departures

[§7](power-up.md#7-power-up-clear-and-the-state-between-utterances), [§8.8](lattice-filter.md#the-lowest-order-arithmetic). An upset is a ±1 at one of a few fixed samples per word, which a take leaves with a fixed chance of 2 % to about 50 %, and which the filter then carries on. A chip-specific departure is a ±1 at a sample where one chip leaves the model every take and the other does not.

- **Known:** both are the marginal part of the lowest-order arithmetic. They start where the model's value is one or two internal steps from an output boundary. Upsets did not change with the clock rate on chip 1, the status polls, the VSM data timing, a preceding word, a power cycle or the spacing between takes. Chip 4 has more of them, and they become rare there at 720 kHz, which fits charge leaking from dynamic nodes ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances), hypothesis).
- **Not known:** the physical cause.
- **Affects:** no model can match a chip bit for bit at these samples. Reference outputs are the per-sample majority of a word's takes.
- **To settle (bench changes):** the repeat test at a second temperature and a second `VDD`, which should move each marginal sample's chance if the hypothesis holds; decoupling; many power cycles, with the chip's power switched by the board's Pico. On the die, the sizes of the dynamic nodes on the multiplier and adder paths.

### A first frame of silence

[§7](power-up.md#7-power-up-clear-and-the-state-between-utterances). When an utterance starts with a silence frame, the rule the model uses is approximate: it differs from that vector at 431 of 1899 samples, by up to 34 codes [p8-08]. An utterance that starts with a voiced or unvoiced frame is not affected, and [§12.2](authoring.md#122-when-a-frame-is-heard) advises starting that way.

### The state at power-up

[§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model). After a power cycle the chip speaks without a Reset [p5-16], but the synthesis state it starts from (the stored indices, the filter and the noise register) has not been measured, nor what the power-up clear issues to the VSM or does to INT. It shows only in an utterance that starts with a repeat or silence frame, or with unvoiced frames and no voiced frame, after power-up. An emulator can start with every index, register and flag at 0, the filter idle and the noise register held at 0x1FFF.

### A Reset during speech

[§6](commands.md#6-description-of-commands), [§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model). Written during a VSM Speak, Reset stops the VSM reads at once, Talk Status is clear at the next status read, and the Reset's Load Address and Dummy Read follow [p8-22]. What the `I/O` output does afterwards was not captured, nor what happens to a frame being read. The model treats the Reset as the end of speech.

## Host interface

### Host interface timing

[§3](cpu-interface.md#3-cpu-interface). These have not been measured. None of them changes the output of any capture in the data set, so an emulator has to choose (the [reference emulator's choices](../emulator/choices.md#host-interface-timing)):

- READY latencies as whole slots, and how they depend on the slot in which the strobe falls (Table 3-1 gives only the times);
- when Talk Status sets after Speak (later than 22 µs), and when the Speak's Dummy Read and each command's VSM functions occur, apart from Load Address ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface));
- what releases READY after a second command written while one is held, and what a held command does when a Reset arrives. The test host gave up after 50 ms, and the chip then did not drive READY active again until it was reset [p8-23]. [US 4,335,277](../sources/patent-us4335277/index.md) holds the host only until the current instruction finishes, so READY may return once speech ends and the held command has run; a retest with a timeout longer than the word would show it;
- which reads return Read Byte data when it is not held (the original's §5 and §6 differ): the tests made one data read per Read Byte;
- whether INT goes active for the FIFO purges of Speak External and Reset, and whether Reset releases INT;
- a 17th byte written to a full FIFO (no stream so far wrote more than 16 bytes; the patent accepts a byte only while the FIFO's top level is empty, so it should hold READY until a frame has been taken), and when writes stop going to the FIFO after a Speak External ends;
- a Speak External written during a VSM Speak, or a Speak while another waits for its frame boundary;
- the exact length of the Read and Branch wait: more than 59 and at most 63 slots from the Branch function to the slot 15 that releases READY (two cases).

All of these are bench tests with the existing hardware.

### Speak External flags within a frame

[§5](io-structure.md#5-io-structure). When BL, BE and TS change within a frame of a Speak External stream, and when TS clears after a frame that runs out. Taking all of a frame's bits at its read time puts BE and the final INT within one frame of the chip, but not in the right slot; the VSM path's 446-slot parameter schedule ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)) fits better. The output is not affected. It needs status reads much closer together than the 1 ms of [p8-19, p8-20, p8-24].

## VSM

### A 4 KB VSM above 4 KB

How a real 4 KB VSM (VM71003A) answers addresses above its size is not known. The capture board served its image padded with zeros. It matters only for data that reads past the end of a 4 KB device.

## Internals with no effect on the output

### Slots and TI's bit times

[§8.4](serial-output.md#84-serial-sample-output-t11-and-io). The mapping from slots to the original's bit times T1–T20 is inferred from the pin name `T11` only.

### The Speak External two-sample lead

[§8.5](frame-timing.md#85-timing-of-a-frame). A Speak External runs two sample periods ahead of a Speak. How the chip does this is not known; the timing itself is measured and modelled. On the die it is a target of the die analysis.

### `PROM OUT`'s phase

[§8.10](prom-out.md#810-parameter-rom-output-prom-out). Which sample period of the 25-period cycle is PC 0 A was not measured against the frame read. A capture's alignment is found from its contents instead.

### The `TEST` mode

[§11.2](pins.md#pin-20-test). What the patent's internal signals C4 and C5 are. In test mode, which slots and which bit order the DAC takes from `I/O`; that needs `I/O` driven in step with `T11` from the board's Pico, which would also give a DAC table without speech.

## DAC and audio output

None of these affects `I/O`, the exact output.

### The DAC's absolute scale

[§8.2](dac-and-audio.md#82-da-conversion). After a power cycle the whole scale moved 7 % higher and stayed there, with the supplies within 6 mV of nominal [p4-01 to p4-04, p5-01 to p5-08]. A meter confirms the step at the pin, so it is in the chip or the load, not in the scope; which is not known. The absolute current figures rest on the scope's DC gain, accurate to a few per cent, and the fitted gain also differs between chips. A virtual-ground measurement (a transimpedance stage holding `SPEAKER` at 0 V) would give the current at 0 V, free of the load.

### Noise during speech

[§8.2](dac-and-audio.md#82-da-conversion). Samples of one code scatter by 1–2 mV during speech, against 0.5–0.8 mV at idle, independently of the code and of earlier codes. It is probably interference on the test board while the VSM is being read, not the DAC.

### The original's 1.8 kΩ application

[§8.3](dac-and-audio.md#83-audio-output). The original's 1.8 kΩ series load (2.7 V at 1.5 mA) has not been tested. Given the measured output resistance, expect noticeably less current into it.

## Chip variation

### Other chips and the TMS5220C

Two TMS5220NLs of one date code (`DEP 8423`), chips 1 and 4, have been compared, at one temperature and one supply ([§8.8](lattice-filter.md#the-lowest-order-arithmetic)). They share the voiced error, but each has its own ±1 departures, upset rate and DAC gain. Not known:

- how far two chips differ on the sets captured only on chip 1: the special frames, the loud and random vectors and the hand-designed new speech;
- whether other date codes, temperatures or supplies stay within the tolerance, which is observed, not proven ([§8.8](lattice-filter.md#the-lowest-order-arithmetic));
- whether other chips' energy, pitch and K tables equal chip 1's, which `PROM OUT` would show ([§8.10](prom-out.md#810-parameter-rom-output-prom-out));
- how the TMS5220C differs: none has been captured.

### Stopping the clock during speech

The chip has no clock of its own, so `OSC` stops whenever the board's Pico resets. Stops while the chip is idle are safe: up to 1 s they freeze its state, and after 10 s it still works, though its frame clock has moved ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). One stop during speech left the chip unable to complete a write until it was power-cycled. Stops during speech were not repeated, so whether that always happens is not known.

## Authoring

### The Acorn PHROM format and the BBC Micro

[§12.7](authoring.md#127-vsm-images), [§12.8](authoring.md#128-word-tables). Taken from the Acorn PHROM A alone, so not known:

- what header bytes 60–63 (0x00, 0x02, 0x00, 0x02) are for; a new image should copy them;
- whether the bit freed by shifting each name character left is used (for example, as an end-of-name flag), and so how a reader finds the first byte of a name;
- what an image should hold for an "ASCII associated" entry with no word of its own (every entry in the Acorn PHROM A has one);
- whether a BBC Micro's speech system reads a new image in this format. It has not been tried.

The project's encoder makes its own choices for them ([encoder choices](../encoder/choices.md#packaging-127129)).
