# speak5220

![Speak5220 logo](images/speak5220-logo.svg){ width="480" }

speak5220 studies the Texas Instruments TMS5220 voice synthesis processor on real hardware. A purpose-built board drives the chip from a Raspberry Pi Pico 2, emulates its speech ROMs and records every output sample, so that the chip's behaviour can be measured rather than guessed.

From those measurements the project has produced:

- a **reference datasheet** that describes the chip exactly, sample for sample;
- a **reference emulator** written from that datasheet alone;
- a **speech encoder** that turns recorded or synthesised speech into TMS5220 data, also written from the datasheet alone;
- the **capture data and tools** that the datasheet's evidence comes from;
- an analysis of the **die image**.

![The Speak5220 main window: the phrase list, an encoded phrase's waveform with its frame grid, and its quality rating](images/gui-main-window.png)

The Speak5220 application's main window, showing an encoded phrase: its waveform with the frame grid, and its quality rating.

## Sections

- [Application](application/index.md): Speak5220, the desktop application built on this work, which encodes speech for the TMS5220 and builds PHROMs, with its installers, user manual and reference.
- [Technical](technical/index.md): the reference datasheet, the emulator, the encoder, the capture method and tools, the hardware, the die analysis, and transcriptions of TI's original documents.

The technical documentation is the basis of the application: its emulator and encoder are written from the reference datasheet.

## Source

The repository is at [github.com/simoninns/speak5220](https://github.com/simoninns/speak5220).
