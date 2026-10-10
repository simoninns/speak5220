# speak5220

speak5220 studies the Texas Instruments TMS5220 voice synthesis processor on real hardware. A purpose-built board drives the chip from a Raspberry Pi Pico 2, emulates its speech ROMs and records every output sample, so that the chip's behaviour can be measured rather than guessed.

From those measurements the project has produced:

- a **reference datasheet** that describes the chip exactly, sample for sample;
- a **reference emulator** written from that datasheet alone;
- a **speech encoder** that turns recorded or synthesised speech into TMS5220 data, also written from the datasheet alone;
- the **capture data and tools** that the datasheet's evidence comes from;
- an analysis of the **die image**.

## Sections

- [Technical](technical/index.md): the reference datasheet, the emulator, the encoder, the capture method and tools, the hardware, the die analysis, and transcriptions of TI's original documents.

The technical documentation is the basis for an application to encode and decode TMS5220 speech PROMs (and make new ones), and to generate Speak External data.

## Source

The repository is at [github.com/simoninns/speak5220](https://github.com/simoninns/speak5220).
