# Building speak5220

How to set up the development environment and build the application and its documentation. What the parts do is in the documentation: <https://simoninns.github.io/speak5220/>.

## Cloning

```bash
git clone https://github.com/simoninns/speak5220.git
```

## Development environment (Nix)

A Nix flake provides a development shell with the application's tools and libraries, Python 3 with the packages the scripts and the reference tools need, and MkDocs:

```bash
nix develop
```

The flake requires Nix with flakes enabled (`experimental-features = nix-command flakes`). Every command below is run from the repository root inside this shell, or prefixed with `nix develop -c`.

A smaller shell, `nix develop .#app`, has only the application's tools and libraries (Qt 6, the libraries it links, Google Test, and the format and lint tools). CI uses it. Both shells install the application's pre-commit hooks into `.git/hooks` when entered.

The flake supports Linux (x86-64 and ARM64) and macOS on Apple silicon. On macOS, `nix develop` leaves out Piper, which nixpkgs does not build there, and `nix build .#speak5220` installs `speak5220.app` and `bin/speak5220-cli`.

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
| `SPEAK5220_PACKAGE` | `OFF` | Windows and macOS only: installing also bundles Qt beside the programs, and `cpack` builds the MSI or DMG (see [Installers](#installers)) |
| `SPEAK5220_PACKAGE_VERSION` | `0.0.0` | With `SPEAK5220_PACKAGE`: the installer's version, three numbers |

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

### Installers

The release workflow (see [CI](#ci)) builds three installers. Each can also be built by hand.

**Flatpak (Linux).** The manifest is `app/packaging/flatpak/io.github.simoninns.speak5220.yml`, on the KDE runtime 6.11 from Flathub; it builds the libraries the runtime lacks. Build from the repository root with `flatpak-builder` (not in the Nix shell; install it from the distribution):

```bash
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
git rev-parse --short=7 HEAD > app/packaging/flatpak/version   # optional: the version it reports
flatpak-builder --user --install-deps-from=flathub --force-clean \
  --state-dir build/flatpak/state --repo build/flatpak/repo build/flatpak/app \
  app/packaging/flatpak/io.github.simoninns.speak5220.yml
flatpak build-bundle build/flatpak/repo build/flatpak/speak5220.flatpak io.github.simoninns.speak5220
```

**MSI (Windows) and DMG (macOS).** Configure a separate build with `SPEAK5220_PACKAGE=ON`, with the libraries linked statically, then run `cpack`. The MSI needs the [WiX](https://wixtoolset.org) 4 .NET tool and its UI extension:

```bash
dotnet tool install --global wix --version 4.0.4
wix extension add --global WixToolset.UI.wixext/4.0.4
```

Then, from a Visual Studio developer prompt on Windows:

```bash
cmake -S app -B build/package -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_DIR="$PWD/app" -DCMAKE_PREFIX_PATH="<Qt directory>" \
  -DSPEAK5220_BUILD_TESTS=OFF -DSPEAK5220_PACKAGE=ON -DSPEAK5220_PACKAGE_VERSION=1.0.0 \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build/package
cpack --config build/package/CPackConfig.cmake -B build/package/installer
```

On macOS, replace the triplet with `-DVCPKG_TARGET_TRIPLET=universal-osx "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3` (the overlay triplet in `app/packaging/vcpkg-triplets/`). The DMG's bundle is signed ad hoc and not notarised. Use Qt from the Qt installer or aqtinstall: a bundle made from the Nix shell's Qt is incomplete, because Nix keeps each Qt module in its own store path and macdeployqt looks in one.

The testing rules are in [`TESTING.md`](TESTING.md).

### Fixture projects

`app/projects/` holds small projects in the unpacked (directory) form, which the functional tests load: `general/` (one phrase, several phrases with Acorn-style names and an ASCII map, and a locked phrase with every kind of frame), `phrom/` (a project that fills exactly one PHROM, and one that spills into a second), `bbc/` (BBC Micro disc fixtures: one PHROM with two pages of phrases, and five PHROMs, on both sides of the disc) and `benchmark/` (two speech-like clips, for timing Optimise with `benchmark.sh`). The phrases with audio were added and encoded with `speak5220-cli` from short synthetic clips; the locked phrases' frames were written directly. When the encoder's output changes, the fixtures' encodings must be redone (`speak5220-cli encode --force` on each), or the functional tests report them as stale.

### Reference data

`app/golden/` holds the output of the retired C emulator and Python encoder, and of independent implementations of the quality measures, which the functional tests compare against. The Python encoder's output is fixed. The rest is written by three scripts run by hand in the default dev shell, with the application's tests built (they need Python with NumPy, SciPy, soundfile, py-webrtcvad, pystoi and pyworld, and REAPER's command-line tool). The emulator's runs and the quality measures are spoken by `tms5220emu`, the application's emulator behind the C emulator's command line, which the test build writes to `build/app/tests/support/`:

```bash
nix develop --command python3 app/scripts/golden/generate_golden.py --check   # emulator: run tms5220emu into a temporary directory and compare
nix develop --command python3 app/scripts/golden/generate_golden.py           # rewrite app/golden/emulator/runs/
nix develop --command python3 app/scripts/golden/encoder.py --check           # encoder: regenerate and compare (under a minute; needs the functional tests built)
nix develop --command python3 app/scripts/golden/encoder.py                   # rewrite app/golden/encoder/
nix develop --command python3 app/scripts/golden/measures.py --check          # quality measures: regenerate and compare (under a minute, on every core)
nix develop --command python3 app/scripts/golden/measures.py                  # rewrite app/golden/quality/measures/
```

`app/golden/bbc/` holds BBC BASIC II's own tokenisation of three test programs, made on MAME's emulated BBC Micro (below) by a fourth script:

```bash
nix develop --command python3 app/scripts/golden/basic_programs.py --check    # tokenise the programs again and compare
nix develop --command python3 app/scripts/golden/basic_programs.py            # rewrite app/golden/bbc/programs/*.bin
```

The emulator's reference data is never rewritten unless the reference datasheet changes, and the encoder's (with the quality data that measures it) only for an intended improvement; see [`TESTING.md`](TESTING.md).

Copies of copyrighted reference texts can be kept in `local/` at the repository root, which git ignores. They are never committed. So is the ROM set for MAME's BBC Micro Model B, in `local/mame/roms/`: `bbcb/` (`os12.rom`, `basic2.rom`, and `cm62024.bin`, the Acorn PHROM A), `saa5050/saa5050` and `bbc_acorn8271/dnfs120.rom`, with the checksums `mame -listroms bbcb` gives.

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
nix develop --command app/scripts/corpus-check.sh   # the whole speech corpus against the reference frames and quality measures, and Optimise on it
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

The speech corpus is not committed. `app/scripts/fetch-corpus.sh` restores it from the repository's history (commit `c537c1d`) into the gitignored `local/corpus/`, once, before the first corpus check. `corpus-check.sh` then writes WAV copies of it into `build/app/generated-corpus/` with `app/scripts/generate_corpus.py`, which needs Python with soundfile (the default shell). Its reports go to `build/app/corpus-check/`, the benchmark's timings to `build/app/project-output/benchmark/`, and the fixtures' exports to `build/app/project-output/SUITE/`.

The encoder-quality diagnostics (TESTING.md) need the default shell's Python (soundfile, NumPy, SciPy). `phrom-check.sh` reads Acorn PHROM A from `local/mame/roms/bbcb/cm62024.bin` (or `SPEAK5220_PHROM_IMAGE`), writes its words to `build/app/phrom-roundtrip/` and its report to `build/app/phrom-check/`; `option_sweep.py` writes to `build/app/option-sweep/`. `phoneme-check.sh` renders into the gitignored `local/phoneme-corpus/ENGINE/`: with Piper, which the default shell has on Linux only, after `python3 app/scripts/phoneme_corpus.py fetch` has downloaded the pinned voices into `local/piper/`; with `--engine say`, with macOS's own voices. Its report goes to `build/app/phoneme-check/`.

### Format and lint

Every commit is checked by pre-commit hooks ([git-hooks.nix](https://github.com/cachix/git-hooks.nix), declared in `flake.nix`): `clang-format`, `cpplint`, `clang-tidy`, the SPDX and header-guard checks (`app/scripts/check-spdx.sh`, `app/scripts/check-header-guards.sh`), `ruff`, `shellcheck`, `nixfmt`, `gersemi` and `actionlint`. The formatters run in check mode and print the command that fixes a failure. `clang-tidy` needs a configured and built `build/app` (for `compile_commands.json` and Qt's generated headers); without one, its hook fails and says how to create it. Never commit with `--no-verify`.

Two CMake targets run the same tools by hand:

```bash
cmake --build build/app --target format   # clang-format, ruff format, nixfmt, gersemi, in place
cmake --build build/app --target lint     # every read-only check, clang-tidy included
```

The style rules are in [`AGENTS.md`](AGENTS.md) §4. Their configuration is in `app/.clang-format`, `app/.clang-tidy` (with `app/tests/.clang-tidy` for the tests), `app/CPPLINT.cfg` and `app/.gersemirc`.

### Nix package

```bash
nix build .#speak5220    # builds the application and runs the unit lane
nix run .#speak5220      # starts the GUI
```

Nix builds from the files git tracks, so `git add` new files before running these.

### CI

`.github/workflows/app.yml` runs on every push to a branch and every pull request, on GitHub's hosted `ubuntu-latest` runner with Nix: `nix flake check` (every hook except `clang-tidy`, on every file), the build with both test lanes and `clang-tidy`, both lanes again under ASan and UBSan, and `nix build .#speak5220`.

`.github/workflows/app-platforms.yml` builds and runs both lanes on Windows (MSVC) and macOS. It runs only for a release or when started by hand from the repository's Actions tab, not on every push.

`.github/workflows/release.yml` runs for a tag `vMAJOR.MINOR.PATCH` (optionally with a suffix, such as `v1.0.0-rc1`, for a pre-release). It runs both of the above; once they pass, it builds the Flatpak, the MSI and the DMG (see [Installers](#installers)) and drafts a GitHub Release with them and their SHA-256 checksums. Publish the draft by hand.

## Emulator command line

`tms5220emu` is the application's emulator behind the retired C emulator's command line. The reference-data scripts (`app/scripts/golden/generate_golden.py`, `measures.py`) run it. It is built with the application's tests (above), into `build/app/tests/support/tms5220emu`; `TMS5220EMU` names another copy.

## Documentation

The site is built with MkDocs and the Material theme, from `docs/` and `mkdocs.yml`. Without Nix, `pip install -r docs/requirements.txt` installs the same versions.

```bash
mkdocs serve           # preview at http://127.0.0.1:8000/
mkdocs build --strict  # what CI runs; fails on broken links
```

Pushes to `main` that change `docs/` or `mkdocs.yml` build the site and publish it to GitHub Pages (`.github/workflows/docs.yml`). The repository's Pages source must be set to "GitHub Actions".
