# speak5220

![Speak5220 logo](images/speak5220-logo.svg){ width="480" }

Speak5220 is a desktop application for making speech for the Texas Instruments TMS5220 voice synthesis processor, the chip in the BBC Micro's speech system and many other machines of the early 1980s. You give it recordings of words and phrases; it encodes them into the data the chip speaks, lets you hear exactly how the chip will sound, and builds new speech ROMs (PHROMs) from them.

![The Speak5220 main window: the phrase list, an encoded phrase's waveform with its frame grid, and its quality rating](images/gui-main-window.webp)

The Speak5220 main window, showing an encoded phrase: its waveform with the frame grid, and its quality rating.

## What it does

- **Imports speech** from WAV files of any sample rate, or the words of existing Acorn PHROM images, such as Acorn's own PHROM A.
- **Prepares the recording** for the chip, with a noise gate, bass cut, treble tilt and de-esser, and a preset for the BBC Micro's speaker.
- **Encodes it** into TMS5220 frames, and plays the result through a built-in emulator of the chip, so what you hear is what the chip will say.
- **Rates the result** from *Poor* to *Good*, by how close it comes to the recording, and **optimises** the encoder's settings for the best quality or the smallest size.
- **Fills PHROMs**: phrases are packed into 16 KB PHROMs in order, with their names, header strings and ASCII map, and the window shows how much space is left.
- **Exports** Acorn PHROM images, C headers for your own firmware, WAV files, and BBC Micro disc images that speak the phrases without a PHROM.

It runs on Linux, Windows and macOS, and comes with a command-line tool, `speak5220-cli`, that does the same from scripts.

## Getting started

- [Installing](application/installing.md): the installers for each platform.
- [Making speech](application/making-speech.md): a walkthrough from a recording to a PHROM image.
- [The application](application/index.md): every view and option in detail, and the reference for its file formats.

## How it was made

Speak5220's emulator and encoder are not guesses at how the TMS5220 works. A purpose-built board drove a real chip from a Raspberry Pi Pico 2, emulated its speech ROMs and recorded every output sample, so that the chip's behaviour could be measured. From those measurements came:

- a **reference datasheet** that describes the chip exactly, sample for sample;
- a **reference emulator**, written from that datasheet alone, which the application uses to play the encoded speech;
- a **speech encoder**, also written from the datasheet alone, which is the application's encoder.

The capture data and tools, and an analysis of the die image, are no longer in the working tree; they are kept in the repository's history ([the research](technical/index.md#the-research)). The [Technical](technical/index.md) section holds the datasheet, how the emulator and encoder work, and transcriptions of TI's original documents.

## Source

The repository is at [github.com/simoninns/speak5220](https://github.com/simoninns/speak5220).
