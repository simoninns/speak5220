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

The TMS5220 has no oscillator of its own on this board. The firmware runs the Pico 2 at 144 MHz and generates the TMS5220's 640 kHz clock (an 8 kHz sample rate) with PWM on GPIO22, buffered to 5 V by U5D.

The test firmware in `firmware/` makes the Pico 2 emulate the Acorn speech PHROM (TMS6100) on core 1, with the PHROM image held in SRAM. It then repeatedly asks the TMS5220 to say "Acorn" (word 160, VSM address `0x3CBB6`). The PHROM data comes from [TMS6100-Emulator](https://github.com/simoninns/TMS6100-Emulator). The firmware reports progress, status and VSM activity over USB serial, and the on-board LED lights while the TMS5220 is talking.

Build:

```bash
cmake -S firmware -B firmware/build -G Ninja
cmake --build firmware/build
```

Flash through a Raspberry Pi Debug Probe:

```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" \
  -c "program firmware/build/speak5220.elf verify reset exit"
picotool reboot -f
```

The `picotool reboot -f` is needed: after an OpenOCD reset, core 1 (which runs the TMS6100 emulation) is put back into the bootrom shortly after it is launched. A clean reboot over USB (or a power cycle) avoids this.

Building with `-DTMS6100_TRACE` makes core 1 record a timestamped trace of every change on GPIO8-19 (the VSM bus, strobes, READY and INT) in `tms6100TraceTime`/`tms6100TraceValue`, for debugging the bus interface.

Alternatively, hold BOOTSEL while plugging in the Pico 2 and copy `firmware/build/speak5220.uf2` to it, or run `picotool load -x firmware/build/speak5220.uf2`.

Monitor the output with `minicom -D /dev/ttyACM1` (the Pico's USB serial port; the debug probe's UART is unused, as GPIO0/1 form part of the TMS5220 data bus).

## Repository layout

- `kicad/tms5220_usb/` – KiCad project, including the project symbol library for the TMS5220 and TMS6100
- `firmware/` – Raspberry Pi Pico 2 test firmware (C, Pico SDK)
- `kicad/libraries/ki-lime-pi-pico/` – Ki-Lime Pi library submodule (Raspberry Pi Pico symbols, footprints and 3D models)
- `docs-tech/` – Datasheets and reference schematics
