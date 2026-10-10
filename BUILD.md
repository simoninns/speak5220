# Building speak5220

How to set up the development environment and build each part of the project. What the parts do is in the documentation: <https://simoninns.github.io/speak5220/>.

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

A Nix flake provides a development shell with KiCad, the Pico SDK and its toolchain, Python 3 with the analysis packages, and MkDocs:

```bash
nix develop
```

The flake requires Nix with flakes enabled (`experimental-features = nix-command flakes`). Every command below is run from the repository root inside this shell, or prefixed with `nix develop -c`.

## Hardware (KiCad)

Start KiCad with `kicad`, or use `kicad-cli` for command-line tasks such as ERC:

```bash
kicad-cli sch erc investigation/kicad/tms5220_usb/tms5220_usb.kicad_sch
```

The documentation shows the schematic sheets as SVG. To regenerate them after a change to the schematic:

```bash
kicad-cli sch export svg --exclude-drawing-sheet -o <dir> investigation/kicad/tms5220_usb/tms5220_usb.kicad_sch
```

Copy the files into `docs/technical/hardware/img/` as `schematic_top.svg`, `schematic_pico_interface.svg`, `schematic_audio_output.svg` and `schematic_poweramp.svg` (the "Power Amp" sheet's file is the one renamed to `schematic_poweramp.svg`).

## Firmware (Raspberry Pi Pico 2)

The dev shell provides the Pico SDK, the Arm GCC toolchain, CMake/Ninja, `picotool` and Raspberry Pi's OpenOCD fork (which supports the RP2350).

Build:

```bash
cmake -S investigation/firmware -B investigation/firmware/build -G Ninja
cmake --build investigation/firmware/build
```

Flash over USB (the running firmware reboots itself into BOOTSEL):

```bash
picotool load -f -x investigation/firmware/build/speak5220.uf2
```

**Reflash only while the TMS5220 is idle.** Flashing stops its clock, and stopping it mid-speech can leave the chip unable to complete a write until it is power-cycled. Afterwards, `python investigation/tools/pico.py reset` should print a status. The firmware protocol page of the documentation ("Stopping the clock") has the details.

Alternatively, flash through a Raspberry Pi Debug Probe:

```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" \
  -c "program investigation/firmware/build/speak5220.elf verify reset exit"
picotool reboot -f
```

The `picotool reboot -f` is needed: after an OpenOCD reset, core 1 (which runs the TMS6100 emulation) is put back into the bootrom shortly after it is launched. A clean reboot over USB (or a power cycle) avoids this.

`investigation/tools/pico.py` finds the Pico's USB serial port by its USB ID. The debug probe's UART is unused, because GPIO0/1 form part of the TMS5220 data bus. To check the board after flashing:

```bash
python investigation/tools/pico.py selftest    # upload, speak and trace, checking every result
```

## Reference emulator

```bash
make -C investigation/emulator          # builds tms5220emu and libtms5220.a
make -C investigation/emulator test     # builds and runs the self-test
```

## Encoder

The encoder is pure Python and needs no build. Its self-test:

```bash
python -m unittest discover -s investigation/encoder -p 'test_*.py'
```

## Capture tools

The Python tools in `investigation/tools/` need no build. Capturing also needs PicoScope 7 installed: it provides `libps3000a.so`, which is found next to the `picoscope` executable (set `PS3000A_LIB` to use a different copy). The dev shell sets `PICO_DRIVER_DEPS` to the driver's libusb and libstdc++ dependencies.

## Documentation

The site is built with MkDocs and the Material theme, from `docs/` and `mkdocs.yml`. Without Nix, `pip install -r docs/requirements.txt` installs the same versions.

```bash
mkdocs serve           # preview at http://127.0.0.1:8000/
mkdocs build --strict  # what CI runs; fails on broken links
```

Pushes to `main` that change `docs/` or `mkdocs.yml` build the site and publish it to GitHub Pages (`.github/workflows/docs.yml`). The repository's Pages source must be set to "GitHub Actions".
