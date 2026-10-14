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

The flake supports Linux (x86-64 and ARM64) and macOS on Apple silicon. On macOS, `nix develop` leaves out KiCad and Piper, which nixpkgs does not build there (install KiCad separately), and `nix build .#speak5220` installs `speak5220.app` and `bin/speak5220-cli`.

## The Speak5220 application

The application is in `app/`, built with CMake and Ninja. Build directories go outside `app/` (conventionally `build/app`): configuring inside `app/` is refused.

```bash
cmake -S app -B build/app -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/app
ctest --test-dir build/app -L unit          # the fast, mocked lane
ctest --test-dir build/app -L functional    # file system, processes, reference data, GUI (offscreen)
```

The application links Qt 6.8 or later (Core, Widgets, Multimedia, and D-Bus on Linux), spdlog, libsndfile, libsamplerate, pocketfft (header-only), yaml-cpp and libzip (for the project file), and libfvad and SPTK 4 (for the quality measures; SPTK brings its own copies of REAPER and WORLD), all provided by the dev shell. nixpkgs has no libfvad, so `flake.nix` builds it. `app/cmake/Dependencies.cmake` finds them.

This builds:

| Output | What it is |
|---|---|
| `build/app/src/gui/speak5220` | The GUI: `speak5220 [PROJECT]` opens a project at start-up |
| `build/app/src/cli/speak5220-cli` | The command-line tool: `new`, `add`, `encode` and `export` on project files, and the logging options `--log-level`, `--log-file` and `--no-console-log` |
| `build/app/tests/...` | The test binaries; the `speak5220_tests` target builds them all |

CMake options:

| Option | Default | Effect |
|---|---|---|
| `SPEAK5220_BUILD_TESTS` | `ON` | Builds the Google Test suites (needs Google Test) |
| `SPEAK5220_SANITIZE` | `OFF` | Builds with AddressSanitizer and UBSan (GCC or Clang); use with `-DCMAKE_BUILD_TYPE=Debug` in a separate build directory |

Debug and sanitizer builds also range-check `operator[]` on standard containers (`_GLIBCXX_ASSERTIONS`). Every build compiles with `-ffp-contract=off` (GCC, Clang), so the encoder's floating point gives the same frames on every platform. SPTK is built the same way, by the flake and by its vcpkg port, so its pitch tracks match the reference data on ARM64 too.

### Windows and macOS

Outside Nix, Qt comes from the Qt installer or [aqtinstall](https://github.com/miurahr/aqtinstall), at the flake's version (6.11.2), and the other libraries from [vcpkg](https://vcpkg.io) in manifest mode: `app/vcpkg.json` lists them and pins vcpkg's baseline. vcpkg has no libfvad or SPTK, so `app/vcpkg-ports/` holds overlay ports for them, at the versions the flake uses, and `app/vcpkg-configuration.json` points vcpkg at them. On Windows, run these from a Visual Studio developer prompt (MSVC):

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

### Fixture projects

`app/projects/` holds small projects in the unpacked (directory) form, which the functional tests load: `general/` (one phrase, several phrases with Acorn-style names and an ASCII map, and a locked phrase with every kind of frame), `phrom/` (a project that fills exactly one PHROM, and one that spills into a second), `bbc/` (BBC Micro disc fixtures: one PHROM with two pages of phrases, and five PHROMs, on both sides of the disc) and `benchmark/` (two speech-like clips, for timing Optimise with `benchmark.sh`). The phrases with audio were added and encoded with `speak5220-cli` from short synthetic clips; the locked phrases' frames were written directly. When the encoder's output changes, the fixtures' encodings must be redone (`speak5220-cli encode --force` on each), or the functional tests report them as stale.

### Reference data

`app/golden/` holds the output of the retired reference emulator and Python encoder, and of independent implementations of the quality measures, which the functional tests compare against. The Python encoder's output is fixed. The rest is written by three scripts run by hand in the default dev shell (they need Python with NumPy, SciPy, soundfile, py-webrtcvad, pystoi and pyworld, REAPER's command-line tool, and a C compiler):

```bash
nix develop --command python3 app/scripts/golden/generate_golden.py --check   # emulator: run it into a temporary directory and compare
nix develop --command python3 app/scripts/golden/generate_golden.py           # rewrite app/golden/emulator/runs/
nix develop --command python3 app/scripts/golden/encoder.py --check           # encoder: regenerate and compare (under a minute; needs the functional tests built)
nix develop --command python3 app/scripts/golden/encoder.py                   # rewrite app/golden/encoder/
make -C investigation/emulator                                                # measures.py runs the built reference emulator
nix develop --command python3 app/scripts/golden/measures.py --check          # quality measures: regenerate and compare (under a minute, on every core)
nix develop --command python3 app/scripts/golden/measures.py                  # rewrite app/golden/quality/measures/
```

`app/golden/bbc/` holds BBC BASIC II's own tokenisation of three test programs, made on MAME's emulated BBC Micro (below) by a fourth script:

```bash
nix develop --command python3 app/scripts/golden/basic_programs.py --check    # tokenise the programs again and compare
nix develop --command python3 app/scripts/golden/basic_programs.py            # rewrite app/golden/bbc/programs/*.bin
```

The emulator's reference data is never rewritten unless the reference datasheet changes, and the encoder's (with the quality data that measures it) only for an intended improvement; see [`TESTING.md`](TESTING.md).

Copies of copyrighted reference texts, such as the ITU-T P.862 Recommendation, can be kept in `local/` at the repository root, which git ignores. They are never committed. So is the ROM set for MAME's BBC Micro Model B, in `local/mame/roms/`: `bbcb/` (`os12.rom`, `basic2.rom`, and `cm62024.bin`, the Acorn PHROM A), `saa5050/saa5050` and `bbc_acorn8271/dnfs120.rom`, with the checksums `mame -listroms bbcb` gives.

The encoder's two fixed Butterworth filters are constants in `app/src/core/dsp/filter_constants.h`, designed by SciPy:

```bash
nix develop --command python3 app/scripts/generate_filter_constants.py --check   # compare with a fresh design
nix develop --command python3 app/scripts/generate_filter_constants.py           # rewrite the header (then clang-format it)
```

The application's icons (the PNG sizes, `.ico` and `.icns`) and the About box's logo in `app/resources/` are generated from the artwork in `assets/logo/`. After changing the artwork, regenerate them and commit the result:

```bash
nix shell nixpkgs#librsvg nixpkgs#imagemagick nixpkgs#libicns --command app/scripts/generate-icons.sh
```

The build puts the icon in the executable on Windows (`speak5220.rc`) and in the bundle on macOS (`speak5220.icns`). On Linux and the BSDs, `cmake --install` installs the `.desktop` file and the icon theme's sizes under `share/`.

### Corpus check and benchmark

Scripts check the encoder, the quality measures and the exports by hand, from the repository root, after building the functional tests (and `speak5220-cli`):

```bash
nix develop --command app/scripts/corpus-check.sh   # the whole phase 11 corpus against the reference frames and quality measures (plan §3.2, Phase 3), and Optimise on it (Phase 7)
nix develop .#app --command app/scripts/benchmark.sh   # encoding and quality-measure speed on one core; Optimise on one thread and on every core
nix develop .#app --command app/scripts/run-projects.sh   # encodes copies of the fixture projects and runs their export presets
nix develop .#app --command app/scripts/output-hashes.sh  # checks those exports against the recorded hashes in app/golden/output-hashes/
nix develop --command app/scripts/bbc-check.sh            # boots the BBC Micro discs on MAME's emulated BBC Micro and drives the player
nix develop .#app --command app/scripts/window-check.sh   # renders every window and dialog, light and dark, at 1x and 2x, to PNGs to look at
nix develop --command app/scripts/phrom-check.sh          # encodes Acorn PHROM A's own output and compares the encodings with its frames
nix develop --command python3 app/scripts/option_sweep.py --input build/app/generated-corpus --phrom   # compares the option sets of app/scripts/sweeps/
nix develop --command app/scripts/phoneme-check.sh --engine say   # renders and encodes the phoneme test set (Piper on Linux: leave out --engine)
```

`bbc-check.sh` needs MAME, which it takes from `PATH` or else runs with `nix shell nixpkgs#mame` (MAME is not in the dev shells: it is large and used only here), and the ROM set in `local/mame/roms/`. Its recordings, logs and summary go to `build/app/bbc-check/`.

`corpus-check.sh` first writes WAV copies of the corpus into `build/app/generated-corpus/` with `app/scripts/generate_corpus.py`, which needs Python with soundfile (the default shell). Its reports go to `build/app/corpus-check/`, the benchmark's timings to `build/app/project-output/benchmark/`, and the fixtures' exports to `build/app/project-output/SUITE/`.

The encoder-quality diagnostics (TESTING.md) need the default shell's Python (soundfile, NumPy, SciPy). `phrom-check.sh` writes the PHROM's words to `build/app/phrom-roundtrip/` and its report to `build/app/phrom-check/`; `option_sweep.py` writes to `build/app/option-sweep/`. `phoneme-check.sh` renders into the gitignored `local/phoneme-corpus/ENGINE/`: with Piper, which the default shell has on Linux only, after `python3 app/scripts/phoneme_corpus.py fetch` has downloaded the pinned voices into `local/piper/`; with `--engine say`, with macOS's own voices. Its report goes to `build/app/phoneme-check/`.

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

## Capture tools

The Python tools in `investigation/tools/` need no build. Capturing also needs PicoScope 7 installed: it provides `libps3000a.so`, which is found next to the `picoscope` executable (set `PS3000A_LIB` to use a different copy). On Linux, the dev shell sets `PICO_DRIVER_DEPS` to the driver's libusb and libstdc++ dependencies.

## Documentation

The site is built with MkDocs and the Material theme, from `docs/` and `mkdocs.yml`. Without Nix, `pip install -r docs/requirements.txt` installs the same versions.

```bash
mkdocs serve           # preview at http://127.0.0.1:8000/
mkdocs build --strict  # what CI runs; fails on broken links
```

Pushes to `main` that change `docs/` or `mkdocs.yml` build the site and publish it to GitHub Pages (`.github/workflows/docs.yml`). The repository's Pages source must be set to "GitHub Actions".
