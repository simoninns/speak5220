# Speak5220: high-level design

The living design specification of the Speak5220 application in `app/`. It
starts from §2 of the [application plan](../application-plan.md) and is kept
in step with the code (AGENTS.md §9.5). Each part says whether it is
**implemented** or still **planned**; the plan's phases say when planned
parts are built.

Section numbers written as "datasheet §N" refer to the reference datasheet
in [`docs/technical/datasheet/`](../../docs/technical/datasheet/index.md).

---

## 1. Status

| Area | Status |
|---|---|
| CMake build, in-source guard, compiler warnings, sanitizer option | Implemented |
| `s5core` library | Implemented as a skeleton: only `util/version` |
| `speak5220` GUI | Implemented as a skeleton: an empty `MainWindow` |
| `speak5220-cli` | Implemented as a skeleton: prints its version (`--version`, `--help`) |
| Test tree and lanes (TESTING.md) | Implemented, with one placeholder test per lane directory |
| Format, lint, pre-commit hooks, CI | Implemented |
| Nix dev shells and package | Implemented |
| Everything else in this document | Planned |

---

## 2. Repository layout

```
app/
  CMakeLists.txt                top level: in-source guard, C++20, options
  .clang-format  .clang-tidy  CPPLINT.cfg  .gersemirc
  cmake/                        CompilerWarnings.cmake, QualityTargets.cmake
  scripts/                      check-spdx.sh, check-header-guards.sh,
                                clang-format-check.sh, clang-tidy.sh,
                                format.sh, lint.sh
  src/
    core/                       library "s5core" (no Qt GUI dependency)
      util/                     version (implemented); Status/StatusOr,
                                hashing, logging (planned)
    gui/                        library "speak5220_gui" + the "speak5220" executable
      main_window/              MainWindow
    cli/                        the "speak5220-cli" executable
  tests/
    support/                    speak5220_test_support (shared helpers and mocks)
    unit/  functional/          speak5220_unit_tests, speak5220_functional_tests
    gui/support/                speak5220_gui_test_main (QApplication, offscreen)
    gui/unit/  gui/functional/  speak5220_gui_unit_tests, speak5220_gui_functional_tests
```

Planned additions, each created with its first code (plan §2.1):

- `src/core/`: `dsp/`, `audio/`, `lpc/`, `emulator/`, `encoder/`, `synth/`,
  `quality/`, `packaging/`, `bbc/`, `project/`;
- `src/gui/`: `phrase_list/`, `phrase_editor/`, `waveform/`,
  `encoder_panel/`, `export/`, `undo/`;
- `app/projects/` (fixture projects), `app/golden/` (reference data),
  `app/resources/` (icons, `.desktop`, metainfo), `app/packaging/`
  (Flatpak, WiX/CPack, macOS);
- more scripts in `app/scripts/`: `golden/`, `run-projects.sh`,
  `output-hashes.sh`, `corpus-check.sh`, `benchmark.sh`,
  `generate_corpus.py`.

`s5core` is a plain C++20 static library that depends on QtCore at most. All
DSP, emulation, encoding, packaging and file formats live there, so they can
be tested without a GUI and reused by the CLI. It contains no C sources and
no code copied from `investigation/`.

---

## 3. Build

**Implemented.**

- CMake ≥ 3.24 with Ninja. `app/CMakeLists.txt` refuses any build
  directory inside `app/`, so generated files never mix with the sources
  that the check scripts scan. The Nix package builds in `../build` for the
  same reason.
- C++20 without extensions. Every target links the interface library
  `speak5220_compile_options`, which carries `-Wall -Wextra -Wpedantic
  -Wconversion -Wshadow -Werror` (GCC/Clang) or `/W4 /WX` (MSVC), and,
  with `SPEAK5220_SANITIZE=ON`, `-fsanitize=address,undefined`.
- The version is set once, in `project(... VERSION ...)`. `s5core` receives
  it as `SPEAK5220_VERSION`, and `flake.nix` reads it for the package.
- `SPEAK5220_BUILD_TESTS` (default `ON`) adds `app/tests/`.
- Dependencies found so far: Qt 6 (Core, Widgets) and Google Test. The other
  libraries of §6 are in the Nix shell and package, and are found by CMake
  when the code that uses them is added.
- `format` and `lint` targets wrap `app/scripts/format.sh` and
  `app/scripts/lint.sh`.

**Nix** (`flake.nix`):

- `devShells.app`: the application's libraries, Google Test, Qt tools, the
  linters, and the pre-commit hooks (installed by its `shellHook`). It sets
  `QT_PLUGIN_PATH` so the GUI built in `build/app` runs.
- `devShells.default`: the research shell (KiCad, Pico SDK, Python), plus
  everything in `devShells.app`.
- `packages.speak5220` (also `default`): built with `qt6.wrapQtAppsHook`;
  its check phase runs `ctest -L unit`. Its source is `app/` only.
- `packages.pocketfft`: pocketfft's header-only C++ version, which nixpkgs
  does not package.
- `checks.pre-commit`: every hook except `clang-tidy`, on every file, in the
  sandbox (`nix flake check`).

---

## 4. Quality gates

**Implemented.** The checks of plan §1.3:

| Check | Where it runs |
|---|---|
| `clang-format` (check mode) | Hook, `lint`, `nix flake check` |
| `cpplint` (`app/CPPLINT.cfg`) | Hook, `lint`, `nix flake check` |
| `clang-tidy` (`app/.clang-tidy`) | Hook (needs `build/app`), `lint`, CI build job |
| `check-spdx.sh` | Hook, `lint`, `nix flake check` |
| `check-header-guards.sh` | Hook, `lint`, `nix flake check` |
| `ruff check`, `ruff format --check` | Hook, `lint`, `nix flake check` |
| `shellcheck`, `nixfmt --check`, `gersemi --check`, `actionlint` | Hook, `lint`, `nix flake check` |

Design points:

- Each hook runs through a small wrapper that puts its tools on `PATH`, so
  the hooks also work from a git client started outside the dev shell.
- `clang-tidy` cannot run in the Nix sandbox, because it needs a configured
  build. It runs in the dev shell's hooks and in CI's build job instead. If
  `build/app/compile_commands.json` is missing, the hook fails with
  instructions; it never skips.
- Header guards are `SPEAK5220_` plus the path from `app/src/` (or from
  `app/` for headers outside `src/`). cpplint cannot add the prefix, so its
  guard check is disabled and `check-header-guards.sh` replaces it; the same
  pattern is exempted from clang-tidy's macro naming rule.
- The disabled clang-tidy and cpplint checks are listed, each with its
  reason, in `app/.clang-tidy` and `app/CPPLINT.cfg`.

**CI** (`.github/workflows/app.yml`), on every push and pull request, on
`ubuntu-latest` with Nix (decision D8): `nix flake check`; build, both lanes
and `clang-tidy`; both lanes under ASan/UBSan (Debug); `nix build
.#speak5220`. Protecting `main` so that this workflow must pass is a
repository setting on GitHub.

---

## 5. Tests

**Implemented** as laid out in [TESTING.md](../../TESTING.md).

- `speak5220_add_test_lane()` in `app/tests/CMakeLists.txt` globs a lane
  directory (`CONFIGURE_DEPENDS`), builds one binary from it, and labels its
  tests with the directory's name, `unit` or `functional`. Tests are
  discovered when CTest runs (`DISCOVERY_MODE PRE_TEST`).
- The aggregate target `speak5220_tests` builds every test binary.
- GUI test binaries use `speak5220_gui_test_main`, which selects the
  offscreen platform unless `QT_QPA_PLATFORM` is already set, then creates a
  `QApplication` and runs Google Test.
- The functional CLI test runs the built `speak5220-cli`, whose path is
  compiled in as `SPEAK5220_CLI_PATH`.

---

## 6. Main components

**Planned** (plan §2.2), except where marked.

| Component | Responsibility |
|---|---|
| `Version()`, `kApplicationName` | **Implemented** (`core/util/version`): the version and name shared by both executables |
| `MainWindow` | **Implemented** as an empty window (`gui/main_window`) |
| `AudioIO` | Read WAV (any rate, any channel count, mixed to mono); write WAV (16- or 24-bit) |
| `Resampler` | Sinc resampling to 8 kHz from any input rate (libsamplerate `SRC_SINC_BEST_QUALITY`) |
| `Fft` | A thin wrapper over pocketfft for the real FFTs the encoder and PESQ use |
| `Frame`, `FrameCodec` | Coded frames (silence, stop, repeat, unvoiced, voiced); pack and parse with the datasheet §8.1/§12.1 bit order |
| `Tms5220`, `Tms6100` | The chip and VSM emulators, re-implemented from `investigation/emulator/` (§7) |
| `Encoder` | Re-implementation of `encode()`/`encodeDetailed()` with the full `EncoderOptions` set |
| `Synth` | Runs `Tms5220` on a frame list through the VSM path or the Speak External path; returns codes and 16-bit samples |
| `Quality` | PESQ (P.862 narrowband, as P.862.1 MOS-LQO) plus cheap measures |
| `Optimiser` | Searches encoder options for the best PESQ, within the 64-PHROM project budget |
| `Packager` | Acorn PHROM image, TI indexed image, Speak External stream, raw `.bin`, C header |
| `BbcExport` | Tokenised BBC BASIC program plus data files on an Acorn DFS `.ssd` image |
| `Project` | Phrases, their audio, settings and cached encodings; the editing operations; load/save |

Classes depend on `I`-prefixed interfaces, injected through their
constructors. Everything that does I/O, wraps a third-party library, uses
threads or time, or is a large pipeline stage sits behind one (`IWavReader`,
`IResampler`, `IFft`, `IEncoder`, `ISynth`, `IQualityMetric`,
`IProjectStore`, `ITaskRunner`, `IAudioOutput`, `IClock`, …). TESTING.md
lists each interface, its real implementation and where it is mocked.

---

## 7. The emulator

**Planned** (plan §2.3). The C emulator is rewritten as C++ classes in
`s5core/emulator/`, stepped one slot (6.25 µs) at a time, with the same
defaults for the datasheet's gaps. The structure changes; the arithmetic and
order of evaluation do not.

| C reference | C++ |
|---|---|
| `struct tms5220` + `tms5220_*()` | class `Tms5220` (`Step()`, `Write()`, `Read()`, `Status()`, `IsTalking()`) |
| `tms5220_config` | `struct Tms5220Config`, one commented field per datasheet-gap choice |
| VSM callback | interface `IVsmBus` (`Load()`, `Read()`, `Dummy()`, `Branch()`), implemented by `Tms6100` |
| sample and event callbacks | `ISampleSink` and an event observer interface |
| `struct tms6100` | class `Tms6100` over a `std::vector<uint8_t>` or `std::span` |
| tables and arithmetic helpers | `constexpr` tables and functions in `lpc/` and `emulator/` |

`Synth` drives the classes through the VSM path (the default for playback)
or the Speak External path (datasheet §12.9), and outputs codes × 64 as
16-bit samples at 8 kHz, with the leading idle marked. It runs on a worker
thread and caches per phrase. Acceptance is identical codes sample for
sample, and identical events slot for slot, against the C emulator's golden
runs.

---

## 8. Third-party libraries

| Need | Library | Licence | Status |
|---|---|---|---|
| GUI, threads | Qt 6 (Widgets, Concurrent, Multimedia) | LGPLv3 / GPLv3 | Core and Widgets linked; the rest in the Nix shell |
| Audio playback | Qt Multimedia `QAudioSink` | LGPLv3 | In the Nix shell |
| WAV read/write | libsndfile | LGPL-2.1+ | In the Nix shell |
| Sinc resampling | libsamplerate | BSD-2 | In the Nix shell |
| FFT | pocketfft (C++ header-only) | BSD-3 | Packaged in `flake.nix` |
| Project container | libzip | BSD-3 | In the Nix shell |
| Project metadata | yaml-cpp | MIT | In the Nix shell |
| PESQ | The project's own code | GPLv3-or-later | Planned |
| Logging | spdlog | MIT | In the Nix shell |
| Tests | Google Test with Google Mock | BSD-3 | Linked by the test binaries |

---

## 9. GUI, CLI and the core

Both executables are thin front ends over `s5core`.

**Implemented:**

- The GUI's widgets are the static library `speak5220_gui`, so the GUI
  tests link them; the `speak5220` executable adds only `main()`.
  `MainWindow` sets the title "Speak5220" and an initial size.
- `speak5220-cli` uses `QCoreApplication` and `QCommandLineParser`. It has
  `--help` and `--version`, and with no arguments prints
  `speak5220-cli <version>`.

**Planned** (plan §2.5): everything that produces a result is in `s5core`,
so the GUI and CLI give byte-identical exports. Editing operations are core
functions behind `IProjectEditor`, each returning a `Status` and what is
needed to reverse it. Undo/redo lives in the GUI (`gui/undo/`, one
`QUndoCommand` per operation). The CLI calls the editing operations directly
and has no undo.

---

## 10. Logging

**Planned** (plan §2.6): spdlog, set up once by `util/logging`; `info`,
`debug` and `trace` levels, with warnings and errors always logged; console
(stderr) and file sinks (`_mt` variants, asynchronous file logger); a named
logger per module; options `--log-level`, `--log-file` and
`--no-console-log` in both executables. Logging never changes results.
