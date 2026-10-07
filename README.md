# speak5220
Texas Instruments TMS5220 voice synthesis processor

## Cloning

The repository uses a git submodule for the [Ki-Lime Pi](https://github.com/recursivenomad/ki-lime-pi-to-go) Raspberry Pi Pico KiCad library, so clone it recursively:

```bash
git clone --recursive https://github.com/simoninns/speak5220.git
```

If you have already cloned without `--recursive`, fetch the submodule with:

```bash
git submodule update --init
```

## Development environment (Nix)

A Nix flake provides a development shell with KiCad 10, Python 3 and git:

```bash
nix develop
```

From inside the shell, start KiCad with `kicad`, or use `kicad-cli` for command-line tasks such as ERC:

```bash
kicad-cli sch erc kicad/tms5220_usb/tms5220_usb.kicad_sch
```

The flake requires Nix with flakes enabled (`experimental-features = nix-command flakes`).

## Firmware (Raspberry Pi Pico 2)

The dev shell also provides the Pico SDK, the Arm GCC toolchain, CMake/Ninja, `picotool` and Raspberry Pi's OpenOCD fork (which supports the RP2350).

The TMS5220 has no oscillator of its own on this board. The firmware runs the Pico 2 at 144 MHz and generates the TMS5220's 640 kHz clock (an 8 kHz sample rate) with PWM on GPIO22. The clock is buffered to 5 V by U5D, then level-shifted by a PNP transistor (Q1) so that OSC swings between −5 V and +5 V, as on Atari's Star Wars sound board. A 0–5 V clock does not work.

The capture firmware in `firmware/` makes the Pico 2 emulate up to four speech PHROMs (TMS6100) on core 1, with their images held in SRAM. The Acorn PHROM A is built in, in bank `F`, and the host can upload other images. Core 1 also records a timestamped trace of every VSM access and every host interface cycle. The TMS5220 is driven entirely by commands from the host over USB serial: upload, Speak, Speak External, raw command writes, status reads and trace download. `firmware/PROTOCOL.md` documents the protocol, and `tools/pico.py` implements the host side:

```bash
python tools/pico.py demo        # say "Acorn BBC Computer" once
python tools/pico.py selftest    # upload, speak and trace, checking every result
```

The PHROM data comes from [TMS6100-Emulator](https://github.com/simoninns/TMS6100-Emulator). The on-board LED lights while the TMS5220 is talking.

With the PicoScope connected, one command captures a word into a complete capture directory, with the readable files and checks (`tools/README.md`). Every take uses the standard procedure: the chip is reset, then the Speak is timed to the chip's frame clock, so that repeats start from the same state.

```bash
python tools/run.py acorn_phrom_a ACORN --takes 2 --path both
```

Build:

```bash
cmake -S firmware -B firmware/build -G Ninja
cmake --build firmware/build
```

Flash over USB (the running firmware reboots itself into BOOTSEL):

```bash
picotool load -f -x firmware/build/speak5220.uf2
```

Reflash only while the TMS5220 is idle. Flashing stops its clock, and stopping it mid-speech has left the chip unable to complete a write until it was power-cycled. Afterwards, `python tools/pico.py reset` should print a status; `firmware/PROTOCOL.md` has the details.

Alternatively, flash through a Raspberry Pi Debug Probe:

```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" \
  -c "program firmware/build/speak5220.elf verify reset exit"
picotool reboot -f
```

The `picotool reboot -f` is needed: after an OpenOCD reset, core 1 (which runs the TMS6100 emulation) is put back into the bootrom shortly after it is launched. A clean reboot over USB (or a power cycle) avoids this.

The Pico's USB serial port is `/dev/ttyACM1` here (`tools/pico.py` finds it by its USB ID). The debug probe's UART is unused, because GPIO0/1 form part of the TMS5220 data bus.

## Repository layout

- `kicad/tms5220_usb/` – KiCad project, including the project symbol library for the TMS5220 and TMS6100
- `firmware/` – Raspberry Pi Pico 2 capture firmware (C, Pico SDK) and its host protocol (`PROTOCOL.md`)
- `kicad/libraries/ki-lime-pi-pico/` – Ki-Lime Pi library submodule (Raspberry Pi Pico symbols, footprints and 3D models)
- `tools/` – Python host tools: the capture runner, Pico control, PicoScope capture and analysis
- `proms/` – the speech PROM inventory (`proms/README.md`) and manifests: image hashes, provenance and word tables. `python tools/promimport.py` rebuilds the images
- `captures/` – evidence captures cited by the reference datasheet. The PROM reference sets in `captures/corpus/` are kept local, apart from each PROM's index (`index.md`, `takes.csv`, `sweep.csv`). The Phase 8 vector captures in `captures/vectors/` are kept local in the same way; the vector sets themselves are in `captures/phase8/vectors/`.
- `docs-tech/` – Datasheets and reference schematics
