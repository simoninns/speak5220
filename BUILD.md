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

A smaller shell, `nix develop .#app`, has only the application's tools and libraries (Qt 6, the libraries of the plan's §2.4, Google Test, and the format and lint tools). CI uses it. Both shells install the application's pre-commit hooks into `.git/hooks` when entered.

## The Speak5220 application

The application is in `app/`, built with CMake and Ninja. Build directories go outside `app/` (conventionally `build/app`): configuring inside `app/` is refused.

```bash
cmake -S app -B build/app -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/app
ctest --test-dir build/app -L unit          # the fast, mocked lane
ctest --test-dir build/app -L functional    # file system, processes, reference data, GUI (offscreen)
```

The application links Qt 6 (Core, Widgets), spdlog, libsndfile, libsamplerate and pocketfft (header-only), all provided by the dev shell. `app/cmake/Dependencies.cmake` finds them.

This builds:

| Output | What it is |
|---|---|
| `build/app/src/gui/speak5220` | The GUI (so far an empty main window) |
| `build/app/src/cli/speak5220-cli` | The command-line tool (so far prints its version, and takes the logging options `--log-level`, `--log-file` and `--no-console-log`) |
| `build/app/tests/...` | The test binaries; the `speak5220_tests` target builds them all |

CMake options:

| Option | Default | Effect |
|---|---|---|
| `SPEAK5220_BUILD_TESTS` | `ON` | Builds the Google Test suites (needs Google Test) |
| `SPEAK5220_SANITIZE` | `OFF` | Builds with AddressSanitizer and UBSan (GCC or Clang); use with `-DCMAKE_BUILD_TYPE=Debug` in a separate build directory |

Debug and sanitizer builds also range-check `operator[]` on standard containers (`_GLIBCXX_ASSERTIONS`). Every build compiles with `-ffp-contract=off` (GCC, Clang), so the encoder's floating point gives the same frames on every platform.

### Windows and macOS

Outside Nix, Qt comes from the Qt installer or [aqtinstall](https://github.com/miurahr/aqtinstall), at the flake's version (6.11.2), and the other libraries from [vcpkg](https://vcpkg.io) in manifest mode: `app/vcpkg.json` lists them and pins vcpkg's baseline. On Windows, run these from a Visual Studio developer prompt (MSVC):

```bash
cmake -S app -B build/app -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_DIR="$PWD/app" -DCMAKE_PREFIX_PATH="<Qt directory>"
cmake --build build/app
ctest --test-dir build/app -L unit
ctest --test-dir build/app -L functional
```

`.github/workflows/app-platforms.yml` does the same on GitHub's Windows and macOS runners.

The testing rules are in [`TESTING.md`](TESTING.md).

### Reference data

`app/golden/` holds the output of the retired reference emulator and encoder, which the functional tests compare against. It is written by two scripts run by hand in the default dev shell (they need Python with NumPy, SciPy and soundfile, and a C compiler):

```bash
nix develop --command python3 app/scripts/golden/generate_golden.py --check   # emulator and packaging: regenerate in a temporary directory and compare
nix develop --command python3 app/scripts/golden/generate_golden.py           # rewrite app/golden/emulator/ and packaging/
nix develop --command python3 app/scripts/golden/encoder.py --check           # encoder: regenerate and compare (a few minutes, on every core)
nix develop --command python3 app/scripts/golden/encoder.py                   # rewrite app/golden/encoder/
```

The emulator's reference data is never rewritten unless the reference datasheet changes, and the encoder's only for an intended improvement; see [`TESTING.md`](TESTING.md).

The encoder's two fixed Butterworth filters are constants in `app/src/core/dsp/filter_constants.h`, designed by SciPy:

```bash
nix develop --command python3 app/scripts/generate_filter_constants.py --check   # compare with a fresh design
nix develop --command python3 app/scripts/generate_filter_constants.py           # rewrite the header (then clang-format it)
```

### Corpus check and benchmark

Two scripts check the encoder by hand, from the repository root, after building the functional tests:

```bash
nix develop --command app/scripts/corpus-check.sh   # the whole phase 11 corpus against the reference frames (plan §3.2)
nix develop .#app --command app/scripts/benchmark.sh   # encoding speed on one core
```

`corpus-check.sh` first writes WAV copies of the corpus into `build/app/generated-corpus/` with `app/scripts/generate_corpus.py`, which needs Python with soundfile (the default shell). Its report goes to `build/app/corpus-check/`, the benchmark's timings to `build/app/project-output/benchmark/`.

### Format and lint

Every commit is checked by pre-commit hooks ([git-hooks.nix](https://github.com/cachix/git-hooks.nix), declared in `flake.nix`): `clang-format`, `cpplint`, `clang-tidy`, the SPDX and header-guard checks (`app/scripts/check-spdx.sh`, `app/scripts/check-header-guards.sh`), `ruff`, `shellcheck`, `nixfmt`, `gersemi` and `actionlint`. The formatters run in check mode and print the command that fixes a failure. `clang-tidy` needs a configured and built `build/app` (for `compile_commands.json` and Qt's generated headers); without one, its hook fails and says how to create it. Never commit with `--no-verify`.

Two CMake targets run the same tools by hand:

```bash
cmake --build build/app --target format   # clang-format, ruff format, nixfmt, gersemi, in place
cmake --build build/app --target lint     # every read-only check, clang-tidy included
```

The style rules are in the plan's §1.3 and in [`AGENTS.md`](AGENTS.md) §4. Their configuration is in `app/.clang-format`, `app/.clang-tidy` (with `app/tests/.clang-tidy` for the tests), `app/CPPLINT.cfg` and `app/.gersemirc`.

### Nix package

```bash
nix build .#speak5220    # builds the application and runs the unit lane
nix run .#speak5220      # starts the GUI
```

Nix builds from the files git tracks, so `git add` new files before running these.

### CI

`.github/workflows/app.yml` runs on every push and pull request, on GitHub's hosted `ubuntu-latest` runner with Nix: `nix flake check` (every hook except `clang-tidy`, on every file), the build with both test lanes and `clang-tidy`, both lanes again under ASan and UBSan, and `nix build .#speak5220`.

`.github/workflows/app-platforms.yml` builds and runs both lanes on Windows (MSVC) and macOS. It runs only for a release tag (`v*`) or when started by hand from the repository's Actions tab, not on every push.

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
