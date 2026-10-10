# Speak5220: application implementation plan

A cross-platform Qt 6 / C++ desktop application for creating and editing
TMS5220 LPC speech data and building new PHROMs.

The application is **self-contained and written entirely in C++ with Qt**.
No code from `investigation/` is compiled into it, linked to it or called by
it. The project's existing tools are re-implemented as C++ classes:

- the **reference encoder** (`investigation/encoder/tms5220enc.py`) becomes
  an `Encoder` class and its helpers;
- the **reference emulator** (`investigation/emulator/`, C) becomes
  `Tms5220` and `Tms6100` classes, used for playback;
- the **PESQ** measurement used in `investigation/tools/speechqual.py`
  becomes an `IQualityMetric` implementation;
- the **Acorn PHROM, TI indexed and Speak External** packaging from
  §12.8–§12.9 of the reference datasheet (`acornPhrom()`, `tiIndexed()`,
  `speakExternalStream()` in the Python encoder) becomes the packaging
  classes.

This applies to the project's own code. Third-party libraries (Qt,
libsndfile, libsamplerate, pocketfft, libzip, yaml-cpp, spdlog) are used freely, provided
their licences are GPLv3-compatible (§2.4).

The original Python and C are the **references** while the C++ is being
written. They are run only offline, in the Nix shell, to produce golden test
data that the C++ must match. Once the C++ is accepted, they are retired
(§1.4).

The application is named **Speak5220**, after the repository. Its
executable is `speak5220`, and its source is in `app/`. This document is
the plan only. Section
numbers (§N) refer to the reference datasheet in `docs/technical/datasheet/`.

---

## 1. Ground rules

### 1.1 Licensing and headers

All source files (C++ sources and headers, CMake, Nix, shell, the Python
golden-data scripts, CI YAML, Qt `.ui`/`.qrc`) start with an SPDX header.

C++ files use the block template from AGENTS.md §4.2:

```cpp
/*
 * File:        frame_codec.cc
 * Module:      lpc
 * Purpose:     Packs and parses TMS5220 frames in the chip's bit order
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */
```

Other source files carry the two SPDX lines in their own comment syntax:

```cmake
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
```

- Headers are for **source code files only**. Documentation (Markdown, this
  plan included), images, test audio and golden data carry none.
- A small script, `app/scripts/check-spdx.sh`, runs in the pre-commit hooks
  and in CI. It fails the build if:
  - any source file (by extension: `.cc`, `.h`, `.cmake`, `CMakeLists.txt`,
    `.nix`, `.sh`, `.py`, `.ui`, `.qrc`, workflow `.yml`) under `app/`, or
    `flake.nix`, lacks both SPDX lines;
  - a C++ file's `File:` line does not match its filename exactly.
- `investigation/` is outside the application and is not changed by this
  plan.
- Third-party dependencies must be GPLv3-compatible (§2.4). PESQ is the
  project's own GPLv3 code (D6).

### 1.2 Build

- CMake (≥ 3.24) with Ninja, **out-of-source builds only**. The top-level
  `CMakeLists.txt` refuses an in-source build.
- **Nix first.** The existing `flake.nix` gains:
  - the application's dependencies in the default dev shell, or a separate
    `devShells.app` if the KiCad/Pico shell gets too heavy;
  - `packages.<system>.speak5220`, so `nix build .#speak5220` and
    `nix run .#speak5220` work on this NixOS machine.
- Local workflow:

  ```bash
  nix develop
  cmake -S app -B build/app -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build build/app
  ctest --test-dir build/app --output-on-failure
  ```

- In Phase 11, GitHub Actions builds a Flatpak (Linux), an MSI (Windows)
  and a DMG (macOS). CMake stays the single build description. Nix supplies
  the dependencies on Linux. On Windows and macOS, CI installs Qt with
  `aqtinstall` and the other libraries with vcpkg (manifest mode, pinned
  baseline).

### 1.3 Coding style and linting

All C++ in `app/` follows the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
[AGENTS.md](../AGENTS.md) §4 holds the full coding standard, and
[TESTING.md](../TESTING.md) the testing rules. This section covers how the
style is enforced.
Every commit is linted, and a commit that fails any check is rejected.

**What the guide means in practice for this project:**

| Area | Rule |
|---|---|
| Files | `.cc`/`.h`, lower case with underscores (`waveform_view.cc`); header guards `SPEAK5220_CORE_LPC_FRAME_CODEC_H_` (path from `app/src/`) |
| Names | Types and functions `PascalCase` (`FrameCodec::Pack()`); interfaces prefixed `I` (`IVsmBus`); variables `snake_case`; class members end in `_`; constants and enumerators `kName`; namespaces `speak5220::core`, `speak5220::app` |
| Formatting | 80 columns, 2-space indent, include order as in the guide; enforced by `clang-format` |
| Exceptions | Not used. Errors are returned as values: a small `Status`/`StatusOr<T>` type in `s5core` (modelled on Abseil's), or `std::optional` where there is nothing to report. Exceptions from third-party code are caught at the boundary. Qt does not use exceptions, so this fits |
| Other | No non-const references as output parameters; `explicit` single-argument constructors; no `using namespace`; RTTI only where Qt needs it (`qobject_cast`); C++20, no compiler extensions |

**Agreed exceptions to the guide** (each recorded in `.clang-tidy` with a
comment):

- **Qt API names.** Overrides and Qt-mandated members keep Qt's names:
  `paintEvent()`, `rowCount()`, `data()`, `mousePressEvent()`, and
  `Q_PROPERTY` accessors. Signals and slots that the application defines
  itself follow the guide (`PlaybackPositionChanged`).
- **Qt macros and ownership.** `Q_OBJECT`, `signals:`/`slots:`, and
  parent-owned `new QWidget(parent)` are allowed. Everything else uses
  `std::unique_ptr` or values.
- **Names from the reference implementations** are converted to the guide's
  style:
  - Python: `encode()` → `Encoder::Encode()`, `trackPitch` →
    `PitchTracker::Track()`;
  - C: `tms5220_step()` → `Tms5220::Step()`, `struct tms6100` → class
    `Tms6100`.

  Each re-implemented function has a comment naming its reference original,
  and the datasheet section where the original cites one, so the two can be
  compared.

**The checks:**

| Tool | Files | Configuration |
|---|---|---|
| `clang-format` | `.cc`, `.h` | `.clang-format` with `BasedOnStyle: Google`; check mode (`--dry-run -Werror`) in hooks and CI, never auto-fixing in CI |
| `cpplint` | `.cc`, `.h` | Google's own style checker; `CPPLINT.cfg` at `app/` disables only the checks that conflict with the agreed rules, each with a comment |
| `clang-tidy` | `.cc`, `.h` | `.clang-tidy` enabling `google-*`, `readability-identifier-naming` (configured to the table above), `bugprone-*`, `modernize-*`, `performance-*`, `cppcoreguidelines-*` (with a short, commented list of disabled checks), and `clang-analyzer-*`; `WarningsAsErrors: '*'`. It uses `compile_commands.json` from the build directory |
| Compiler | all C/C++ | `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` (GCC/Clang), `/W4 /WX` (MSVC) |
| `check-spdx.sh` | all source files | §1.1 |
| `check-header-guards.sh` | `.h` | The `SPEAK5220_…_H_` guards of the table above; cpplint cannot add the prefix, so its own guard check is disabled |
| `ruff` (lint and format) | `.py` helper scripts | Default rules plus import sorting |
| `shellcheck` | `.sh` | |
| `nixfmt` | `.nix` | The formatter the flake already declares |
| `gersemi` | `CMakeLists.txt`, `.cmake` | CMake formatting |
| `actionlint` | `.github/workflows/*.yml` | |

The application has **no C sources**: everything under `app/src/` is
C++, so every C++ check above applies to all of it. Python appears only in
the offline golden-data and corpus scripts in `app/scripts/` (§1.4). Those scripts are
linted with `ruff`, but they are never part of the build or of the
application.

**How the checks are enforced:**

- **Pre-commit hooks**, declared in `flake.nix` with
  [git-hooks.nix](https://github.com/cachix/git-hooks.nix):
  - Entering `nix develop` installs them automatically, so nobody has to
    remember to.
  - They run on the staged files only. Formatters run in check mode: a
    failure prints the `clang-format -i` (or `ruff format`) command to fix
    it.
  - `clang-tidy` needs a configured build directory (`build/app`). If
    there is none, the hook fails with a message saying how to create one;
    it is not skipped silently.
- **CI** runs the same hooks on all files (`nix flake check`, which runs
  git-hooks.nix's check). So a commit made with `--no-verify`, or from
  outside the dev shell, still fails before merging. `main` is protected:
  it accepts only changes that pass this job.
- **Commits made by Claude** go through the same hooks. `--no-verify` is
  never used.
- **Developer convenience:** a `format` CMake target (`cmake --build
  build/app --target format`) applies `clang-format`, `ruff format`,
  `nixfmt` and `gersemi` in place, and a `lint` target runs the read-only
  checks.

### 1.4 Reference implementations, then retirement

The Python encoder and the C emulator in `investigation/` are the
references **until the C++ re-implementations are accepted**. Until then
they are test oracles only:

- Scripts in `app/scripts/golden/` are run by hand in the Nix shell. They
  convert the test inputs they use (the phase 11 corpus is stored as FLAC)
  to WAV, so the C++ tests read WAV only. They run the Python encoder and
  the built C emulator binary (`tms5220emu`) on chosen inputs, and write the
  results to `app/golden/`: frame lists, packed bytes, sample codes,
  intermediate analysis values, PHROM images and PESQ scores. The results
  are committed, together with the commit hash of the references they came
  from.
- The application's build and its CTest suite read only those committed
  files. They never compile, link or run anything in `investigation/`, so
  the application builds on Windows and macOS without Python or the C
  sources.
- The C++ emulator is accepted only when it reproduces the C emulator's
  output **sample for sample** (Phase 1).
- The C++ encoder is accepted only when it reproduces the Python encoder's
  frames on the test corpus (§3.2, Phase 2).

**After acceptance, the originals are retired (decision D5):**

- The C++ becomes the only maintained implementation. The `investigation/`
  encoder and emulator stay in the repository for history, but are no
  longer developed or referenced by the application's tooling.
- The golden data becomes a fixed **regression baseline**:
  - **Emulator:** its golden data never changes, unless the datasheet
    itself changes.
  - **Encoder:** improvements are made in C++ and judged by PESQ and the
    evaluation measures, not by agreement with Python. When an intended
    improvement changes the frames, the encoder baseline is regenerated
    from the C++ itself, in its own commit that records the change and the
    quality before and after.
- The `app/scripts/golden/` scripts are kept alongside the data so the
  baseline's origin can be traced, but they are not maintained after
  retirement.

---

## 2. Architecture

### 2.1 Repository layout

```
app/                            the Speak5220 application (new)
  CMakeLists.txt
  .clang-format  .clang-tidy  CPPLINT.cfg
  cmake/                        helper modules, packaging config
  scripts/                      check-spdx.sh; golden/ offline reference scripts (§1.4); run-projects.sh,
                                output-hashes.sh, corpus-check.sh, benchmark.sh, generate_corpus.py (TESTING.md)
  src/
    core/                       no Qt GUI dependency: library "s5core"
      dsp/                      FFT and resampler wrappers, IIR/FIR filters, Levinson
      audio/                    WAV I/O (libsndfile), audio buffers
      lpc/                      frame model, pack/parse, tables (ENERGY, PITCH, K, CHIRP)
      emulator/                 Tms5220 and Tms6100 classes (from the C reference emulator)
      encoder/                  Encoder, PitchTracker, ... (from tms5220enc.py)
      synth/                    renders frames to 8 kHz samples with the emulator classes
      quality/                  PESQ and secondary measures
      util/                     Status/StatusOr, hashing, logging setup (spdlog, §2.6)
      packaging/                Acorn PHROM, TI indexed, Speak External, C header, .bin
      bbc/                      BBC BASIC tokeniser, DFS .ssd writer
      project/                  project model, (de)serialisation
    gui/                        Qt Widgets GUI: the "speak5220" executable
      main_window/ phrase_list/ phrase_editor/ waveform/ encoder_panel/ export/
      undo/                     QUndoCommand per core editing operation (§2.5)
    cli/                        optional headless "speak5220-cli" (batch encode/export)
  tests/                        Google Test suites only (TESTING.md):
    support/                      shared helpers, gMock mocks of the interfaces
    unit/  functional/            → speak5220_unit_tests, speak5220_functional_tests
    gui/{support,unit,functional}/ → speak5220_gui_unit_tests, speak5220_gui_functional_tests
  projects/                     hand-authored fixture projects, stored unpacked (TESTING.md)
  golden/                       committed reference data from the retired references (§1.4)
  resources/                    icons, .desktop, AppStream metainfo, .qrc
  packaging/                    flatpak manifest, WiX/CPack config, macOS Info.plist
docs-tech/                      this plan; design/high-level-design.md (Phase 0)
```

`s5core` is a plain C++20 static library that depends on QtCore at most. All
DSP, emulation, encoding, packaging and file formats live there, so they can
be tested without a GUI and reused by the CLI. It contains no C sources and
no code copied from `investigation/`.

### 2.2 Main components

| Component | Responsibility |
|---|---|
| `AudioIO` | Read WAV (any rate, any channel count, mixed to mono); write WAV (16- or 24-bit) |
| `Resampler` | Sinc resampling to 8 kHz from any input rate (libsamplerate `SRC_SINC_BEST_QUALITY`) |
| `Fft` | A thin wrapper over pocketfft for the real FFTs the encoder and PESQ use |
| `Frame`, `FrameCodec` | Coded frames (silence, stop, repeat, unvoiced, voiced); `pack()`/`parse()` with the §8.1/§12.1 bit order |
| `Tms5220`, `Tms6100` | The chip and VSM emulators as C++ classes, re-implemented from `investigation/emulator/` (§2.3) |
| `Encoder` | Re-implementation of `encode()`/`encodeDetailed()` with the full `EncoderOptions` set |
| `Synth` | Runs `Tms5220` on a frame list through the VSM path or the Speak External path; returns codes and 16-bit samples |
| `Quality` | PESQ (P.862 narrowband, as P.862.1 MOS-LQO) plus cheap measures (level error, spectral distance) |
| `Optimiser` | Searches encoder options for the best PESQ, within the 64-PHROM project budget, optionally with a per-phrase byte limit |
| `Packager` | Acorn PHROM image, TI indexed image, Speak External stream, raw `.bin`, C header |
| `BbcExport` | Tokenised BBC BASIC program plus data files on an Acorn DFS `.ssd` image |
| `Project` | Phrases, their audio, settings and cached encodings; the editing operations (§2.5); load/save |

Classes depend on interfaces, not on concrete classes, and receive them
through their constructors (dependency inversion, as set out in
[TESTING.md](../TESTING.md)). Everything that does I/O, wraps a third-party
library, uses threads or time, or is a large pipeline stage sits behind an
`I`-prefixed interface (`IWavReader`, `IResampler`, `IFft`, `IEncoder`,
`ISynth`, `IQualityMetric`, `IProjectStore`, `ITaskRunner`, `IAudioOutput`,
`IClock`, …), so its callers can be unit tested with mocks. TESTING.md lists
each interface, its real implementation and where it is mocked.

### 2.3 The emulator in C++

The C emulator (`tms5220.c`, about 1,100 lines, and `tms6100.c`) is rewritten
as C++ classes in `s5core/emulator/`. The design stays faithful to the
reference: the chip is stepped one slot (6.25 µs) at a time, and the same
datasheet gaps have the same defaults. The rewrite changes how the code is
structured, not the arithmetic.

| C reference | C++ |
|---|---|
| `struct tms5220` + `tms5220_*()` functions | class `Tms5220` with private state and member functions (`Step()`, `Write()`, `Read()`, `Status()`, `IsTalking()`) |
| `tms5220_config` and `tms5220_default_config()` | `struct Tms5220Config` with default member initialisers; one field per "DATASHEET GAP" choice, each commented with its `choices.md` entry |
| VSM callback `int fn(void*, int func, int nibble)` | interface `IVsmBus` (`Load()`, `Read()`, `Dummy()`, `Branch()`), implemented by `Tms6100` and mocked in unit tests |
| sample and event callbacks | an `ISampleSink` interface (and an event observer interface); no `void*` context |
| `struct tms6100` with a 1 MB `malloc` | class `Tms6100` holding a `std::vector<uint8_t>` (or a `std::span` over the phrase's bytes) |
| static tables and arithmetic helpers (`booth`, `wrap14`, noise LFSR) | `constexpr` tables and functions in `lpc/` and `emulator/`, shared with the encoder's synthesiser where they are the same |

`Synth` drives the classes:

- **VSM path** (the default for playback): the phrase's packed bytes are
  loaded into a `Tms6100` at address 0, then the host sequence Load Address ×
  5, Speak runs. This matches how a PHROM word is spoken.
- **Speak External path**: the 9-byte rule and FIFO feeding are driven from
  §12.9. This matches the BBC BASIC export, which starts 2 samples earlier
  with different noise.
- Output: codes × 64 as 16-bit at 8 kHz, the same convention as `--wav` in
  the encoder. The leading idle (code −1) of about 25 ms is kept but marked,
  so the waveform view can align the encoded audio with the original.

**Acceptance:** golden runs of `tms5220emu` (§1.4) on the emulator's own
self-test scenarios, the synthetic vectors, and the encoder's outputs for a
corpus subset, through both paths. The C++ must give **identical codes
sample for sample, and identical status/flag events slot for slot**. The C
self-test cases (`test_tms5220.c`) are re-written as C++ unit tests.

`Synth` runs on a worker thread and caches its result per phrase (keyed by a
hash of the packed bytes plus the path).

### 2.4 Third-party libraries

| Need | Library | Licence | Notes |
|---|---|---|---|
| GUI, threads | Qt 6 (Widgets, Concurrent, Multimedia) | LGPLv3 / GPLv3 | Qt Widgets rather than QML: the app is desktop-only and dominated by custom-painted waveforms |
| Audio playback | Qt Multimedia `QAudioSink` | LGPLv3 | 8 kHz mono Int16, pull mode, so the playhead position is available |
| WAV read/write | libsndfile | LGPL-2.1+ | Every WAV variant (PCM 8–32-bit, float, `WAVE_FORMAT_EXTENSIBLE`); only WAV is used |
| Sinc resampling | libsamplerate | BSD-2 | `SRC_SINC_BEST_QUALITY` |
| FFT | pocketfft (C++ header-only version) | BSD-3 | The FFT that NumPy itself uses, which helps the C++ encoder match the Python golden data |
| Project container | libzip | BSD-3 | See §4 |
| Project metadata | yaml-cpp | MIT | YAML 1.2 (§4) |
| PESQ | The project's own code (`quality/`) | GPLv3-or-later | Written from scratch from the published ITU-T P.862 and P.862.1 texts (D6) |
| Logging | spdlog | MIT | `info`, `debug` and `trace`, to the console, a file or both (§2.6) |
| Tests | Google Test with Google Mock | BSD-3 | Run through CTest; layout and lanes in TESTING.md |

Libraries are chosen on merit, provided their licences are GPLv3-compatible.
The rule that everything is Qt/C++ (§1.4) applies to the project's own code,
not to its dependencies. Audio input and output is WAV only: there is no
FLAC or other compressed format.

The IIR/FIR filtering stays in the project's own code, because it has to
reproduce SciPy's `sosfiltfilt` exactly (§3.2). A general filter library
would not help with that.

### 2.5 GUI, CLI and the core

Both executables are thin front ends over `s5core`:

```
                 s5core (static library, QtCore only)
   emulator · encoder · synth · quality · optimiser · packaging ·
   BBC export · project model and editing operations · load/save
          ▲                                    ▲
          │ links                              │ links
  speak5220 (GUI)                      speak5220-cli
  Qt Widgets + Multimedia:             QCoreApplication + QCommandLineParser:
  views, waveforms, playback,          new / add / encode / export
  dialogs, undo/redo
```

- **Everything that produces a result is in `s5core`**, so the GUI and the
  CLI give byte-identical exports for the same project.
- **Editing operations are core functions.** `Project` exposes each edit
  (add, remove, move, sort, rename, set description, set encoder options)
  through the `IProjectEditor` interface. Each operation returns a `Status`,
  together with what is needed to reverse it (for example, the previous
  order or the removed phrases).
- **Undo/redo lives in the GUI.** `QUndoStack` is part of QtGui, which
  `s5core` does not depend on. `app/src/gui/undo/` holds one `QUndoCommand`
  per editing operation. Each command calls `IProjectEditor` in `redo()` and
  applies the reverse in `undo()`. A failed operation is reported and is not
  pushed onto the stack.
- **The CLI calls the editing operations directly** and has no undo.

### 2.6 Logging

All logging goes through [spdlog](https://github.com/gabime/spdlog), set up
once at start-up by `util/logging` and used by both executables.

**Levels.** Three levels can be selected, following AGENTS.md §3.2:

| Level | What it shows |
|---|---|
| `info` (default) | Normal lifecycle messages, stage transitions, validation summaries |
| `debug` | Everything at `info`, plus configuration details, branch decisions, resolved paths and the chosen encoder options |
| `trace` | Everything at `debug`, plus per-frame, per-sample and per-slot state: pitch candidates, lattice sums, FIFO contents |

Warnings and errors are always logged, whichever level is selected.

**Destinations.** Messages go to the console, to a file, or to both:

- **Console:** stderr, coloured when it is a terminal. On by default.
- **File:** off by default. When a file is given, every message at the
  selected level is written to it as well as to the console. Each line
  carries a timestamp, the level, the thread and the module.
- **Console off:** `--no-console-log` leaves only the file, which is useful
  for `trace` runs that would flood a terminal. On Windows, the GUI has no
  console, so the file is the only way to see its log.

**Controls.**

| Option | Effect |
|---|---|
| `--log-level info\|debug\|trace` | Selects the level (both executables) |
| `--log-file PATH` | Also writes the log to `PATH`, replacing any existing file |
| `--no-console-log` | Turns console output off (needs `--log-file`) |

The GUI also has these as preferences (Phase 10). A command-line option
overrides the preference for that run.

**Implementation.**

- One `spdlog` sink per destination (a colour console sink, a basic file
  sink), shared by a named logger per module (`encoder`, `emulator`,
  `quality`, `project`, `export`, `gui`, …). The module name appears in each
  line.
- The sinks are the thread-safe (`_mt`) variants, because encoding,
  synthesis and Optimise run on worker threads.
- File output uses spdlog's asynchronous logger, so `trace` output from hot
  loops does not stall them.
- `trace` calls in the per-slot and per-sample loops are guarded, so they
  cost almost nothing when the level is lower. They are compiled into
  release builds, so a user can produce a trace without a special build.
- Logging never changes results: output is identical at every level.
- The setup is unit tested by attaching an in-memory sink (spdlog's
  `ringbuffer_sink`) instead of the console and file. Writing to a real file
  is a functional test (TESTING.md).

---

## 3. Key technical decisions

### 3.1 Audio pipeline on import

1. Read the WAV with libsndfile into float (−1…1). Mix to mono by
   averaging channels.
2. Resample to 8 kHz with libsamplerate's best sinc converter. If the input is
   already 8 kHz, copy it unchanged.
3. Normalise **only** if the input clips or is very quiet. Ask the user first;
   the encoder sets its own level (`targetRms`).
4. Store the 8 kHz copy in the project as **24-bit PCM**, so it is not
   requantised to 16 bits. Record the source file name, original rate,
   channels, duration and a SHA-256 of the source in the phrase metadata.

The original file is not kept (decision D1). Re-importing the source file is
the way back if a phrase ever needs resampling again.

### 3.2 The encoder in C++

`tms5220enc.py` is about 1,500 lines of NumPy/SciPy. The C++ follows the
Python structure function by function (`analyse`, `trackPitch`, `_yin`,
`_kindsAndK`, `_repeatRun`, `_setLevels`, `_addFades`, `_guardK510`, …), so
each function can be tested against its Python original.

SciPy pieces to replace:

| Python | C++ replacement |
|---|---|
| `scipy.signal.butter(..., output="sos")` | The encoder only filters at 8 kHz with fixed corners, so the second-order sections are generated once by a script in the Nix shell and committed as constants, with the script kept for regeneration |
| `sosfiltfilt` | Port of SciPy's algorithm, including its odd-extension padding (`padlen`) and `sosfilt_zi` initial conditions, which are needed for identical results |
| `lfilter` | Direct-form FIR/IIR |
| `np.fft.rfft`/`irfft` | pocketfft, the same FFT NumPy uses |
| `np.hanning` | Closed form |
| `resample_poly` | Not needed: the app always gives the encoder 8 kHz audio |

`synthesize()`, which the closed-loop level passes use, is re-implemented as its own
fast reference-algorithm synthesiser. It is checked against the C++ emulator
sample for sample, as the Python test already does.

**Acceptance (Phase 2):** on the 243 files in
`investigation/captures/phase11/inputs/`, with default options and with a
fixed set of non-default option combinations:

- identical frame lists on at least 99 % of files;
- on the rest, differences are confined to frames where Python logs a
  near-tie (to be instrumented), and PESQ is within ±0.02 of the Python
  result.

Bit-exact floating point across compilers is not a goal. `-ffast-math` is
banned in `s5core`.

### 3.3 PESQ and the optimiser

- PESQ is measured as in `speechqual.py`: P.862 narrowband at 8 kHz, as
  P.862.1 MOS-LQO, reference = the 8 kHz original, degraded = the emulator
  output with the 25 ms lead aligned.
- PESQ is the project's own code, licensed GPLv3-or-later (D6). It is
  written from scratch from the published ITU-T P.862 and P.862.1
  Recommendation texts:
  - The ITU's reference C source, and the C code inside the Python `pesq`
    package, are **not** read or used.
  - The Python package is used only as a black box, to produce the golden
    scores the C++ is checked against (§1.4), to within ±0.01 on the corpus.
- **Optimise** searches the encoder options in `EncoderOptions`:
  - `targetRms` or `targetPeak`;
  - `repeats` and `repeatLambda`;
  - `gateDb`;
  - `preEmphasis` (`chirp` or μ);
  - `window`, `lagWindowHz`;
  - `voicingThreshold`, `voicingEvidence`;
  - `iterations`;
  - `fadeUnvoiced`;
  - `levelAgc`, `compression`.

  Method:
  1. A coarse grid over the options with the most effect (`preEmphasis`,
     `voicingThreshold`, `voicingEvidence`, `repeatLambda`, `targetRms`).
  2. Coordinate descent (or Nelder–Mead on the continuous options) from the
     best grid point.
  3. **The project budget** (decision D4) is 64 PHROMs (64 × 16 KB). A
     candidate encoding is rejected if it would push the project past 64
     PHROMs under the list-order assignment of §4.1.
  4. Optionally, a **per-phrase byte limit** as well: maximise PESQ subject
     to bytes ≤ N, or maximise PESQ − λ·kbit/s with a user-chosen λ.
- Optimising runs as a cancellable background job (`QtConcurrent` with a
  thread pool, one candidate per thread). Progress, the best score so far and
  the best settings update live. The user can accept the result or revert to
  the previous settings. Every evaluated candidate is kept in a history table
  that can be shown.
- Batch: **Optimise all** runs over a selection of phrases.

**Presenting PESQ to the user.** A raw PESQ score is hard to read for LPC
speech. The scale runs from 1.0 to 4.5, but no TMS5220 encoding comes close
to 4.5. On the project's 243 test inputs, the encoder's median is about 2.03
(most files fall between 1.79 and 2.32), and an unquantised LPC-10 vocoder
(the "anchor" of `speechqual.py`) scores about 2.30. A score of 2.1 is
therefore good, though it looks poor on PESQ's own scale. So the GUI shows
PESQ two ways:

- **The score itself**, to two decimals (MOS-LQO), always available.
- **A quality scale** for the phrase, from **Bad** to **As good as LPC can
  achieve**:
  - **The ceiling** is the phrase's own **anchor score**: PESQ for an
    unquantised, floating-point LPC-10 vocoder of the same 8 kHz source
    (order 10, 25 ms frames, the chip's 8-step K interpolation, pulse or
    noise excitation, no quantisation), as `speechqual.py --anchor` defines
    it. It is computed once per source and cached. It depends on the
    recording, so each phrase gets its own ceiling.
  - **The floor** is PESQ's minimum, 1.0.
  - **The position** is `(score − 1.0) / (anchor − 1.0)`, clamped to the
    scale. An encoding that matches or beats the anchor is shown at the top.
  - **Bands** with plain-language labels (for example *Bad*, *Poor*,
    *Fair*, *Good*, *As good as LPC can achieve*) divide the scale. Their
    thresholds are calibrated on the corpus in Phase 3, and confirmed by
    listening.
- **The scale uses text as well as colour**, so it is readable without
  colour vision. A tooltip explains what PESQ measures, why 4.5 is out of
  reach for LPC, and that PESQ was not validated for vocoders, so it ranks
  encodings rather than predicting a listener's score.
- **Before and after:** while Optimise runs, the scale shows the starting
  score and the best so far.

### 3.4 Encoding is cached, not authoritative

A phrase stores its **encoder settings** and **source audio**. The frames are
a cache derived from them, saved in the project so a project opens instantly.
The cache is invalidated by a hash of (source audio, settings, encoder
version). A phrase can also be **locked**: frames imported from an existing PHROM
image (Phase 10) are never re-encoded.

---

## 4. Project file format

A project is one file, `*.s5proj`: a ZIP container (libzip). It can be
opened with any ZIP tool for inspection or recovery. The same layout can also
be stored **unpacked**, as a directory. The application opens and saves
either form through `IProjectStore`. The test fixtures in `app/projects/` use
the unpacked form, so they can be diffed and reviewed (TESTING.md).

```
project.yaml                    format version, project metadata, phrase order
phrases/<uuid>/source.wav       8 kHz mono, 24-bit PCM
phrases/<uuid>/frames.yaml      cached encoding (frames, encoder version, settings hash)
```

The cached packed bytes are not stored, because they are rebuilt from the
frames. **Export → Raw data** gives them for inspection.

The metadata files are **YAML 1.2**, read and written with yaml-cpp. Keys
are `snake_case`, the same names as the C++ fields they map to.

`project.yaml`, sketched:

```yaml
format: speak5220-project
version: 1
project:
  name: "My PHROM"
  header_strings: ["(C)2026 Simon Inns", "My PHROM", "1.00"]
  ascii_map:
    "A": "A"
    " ": "PAUSE"
  default_encoder:
    target_rms: 20.0
    repeat_lambda: 70.0
    pre_emphasis: "chirp"
phrases:
  - id: "6f2c…"
    name: "HELLO"
    description: "Greeting, recorded 2026-10-10"
    source:
      file: "hello.wav"
      rate: 44100
      channels: 2
      sha256: "…"
    encoder:                      # only the settings that differ from default_encoder
      target_rms: 18.0
      voicing_threshold: 0.35
    locked: false
    play_start: 0
exports:                          # presets remembered by the Export dialog, run by the CLI
  - name: "phrom"
    format: "acorn-phrom"
    scope: "all"
    layout: "separate"            # one .bin per PHROM, or "concatenated"
  - name: "bbc-disc"
    format: "bbc-ssd"
    scope: "all"
```

`frames.yaml`, sketched (one flow mapping per frame keeps it compact and
readable):

```yaml
encoder_version: "1.0.0"
settings_hash: "…"
frames:
  - {energy: 9, repeat: 0, pitch: 34, k: [12, 20, 8, 7, 9, 8, 7, 4, 3, 2]}
  - {energy: 9, repeat: 1, pitch: 35}
  - {energy: 4, repeat: 0, pitch: 0, k: [20, 18, 9, 6]}
  - {energy: 0}
  - {energy: 15}
quality:                          # cached with the encoding; the anchor is cached per source
  pesq: 2.08
  anchor: 2.31
```

- **Strings are always written quoted.** Without quotes, YAML reads some
  words as booleans or numbers. PHROM words such as `NO`, `YES`, `ON` and
  `OFF` would become booleans in YAML 1.1 readers, and ASCII-map keys such
  as `" "` and `":"` must be quoted anyway. A test writes and reads back
  every name in the Acorn PHROM A word list.
- On load, values are type-checked against the expected schema. Unknown keys
  are kept and written back unchanged, so older versions don't drop data
  added by newer ones.
- **Saving** is atomic: libzip writes a temporary file and renames it over
  the old one when the archive is closed. The previous version is kept as `*.s5proj~`.
- **Loading** checks `format`/`version` and migrates older versions.
- Phrase **names** follow Acorn's style (decision D2):
  - the characters of the Acorn PHROM A word list: upper-case `A`–`Z`,
    digits, space, and `' ( ) , - .`;
  - at most 16 characters (Acorn's longest is 14);
  - unique within the project;
  - lower case typed into the name field is converted to upper case, and
    any other character is refused, with a message.
- **C identifiers** for the header exports are derived from the names:
  other characters become `_`, runs of `_` collapse, and a leading digit
  gets a prefix (`ONE,WON` → `ONE_WON`, `-ING` → `ING`,
  `2- (TWEN-)` → `W_2_TWEN`). Clashes get a numeric suffix. The mapping is
  listed in a comment in the header.
- The **description** is free UTF-8 text, kept only in the project.

### 4.1 Multiple PHROMs

A PHROM is one 16 KB bank, and PHROMs can be stacked, so a project can hold
more speech than one PHROM (decision D4):

- **The project budget is 64 PHROMs** (64 × 16 KB = 1 MB). It is a limit
  chosen for testing, not a hardware one: a BBC Micro can select only 16
  PHROM numbers (0–15) at a time. Imports, re-encoding and the optimiser all
  respect the budget.
- **Automatic assignment, in list order.** Phrases fill PHROM 1 in list
  order. When the next phrase doesn't fit, PHROM 2 starts, and so on. Earlier
  PHROMs are not back-filled, so the result is predictable, and reordering
  the list moves phrases between PHROMs.
- **What a phrase costs** in its PHROM: its packed bytes, plus its name (one
  byte per character, stored before the data), plus a 2-byte pointer. Each
  PHROM also has its fixed overhead: the 64-byte header, the 95 ASCII
  pointers, the 0xFF end marker and the one-byte null word (§12.8).
- **A phrase too large for an empty PHROM** is flagged as an error on that
  phrase and is left out of every PHROM until it is shortened or
  re-encoded.
- **Word numbers** start at 127 in every PHROM.
- **The ASCII map** is held once for the project. Each PHROM includes only
  the entries whose word is in that PHROM, and its other ASCII entries
  point to its own null word.
- **Header strings:** each PHROM uses the project's header strings, with the
  PHROM number appended to the second string (`My PHROM 2`). They can be
  overridden per PHROM.
- **Size is shown in KB and in PHROMs:** the total size, the number of
  PHROMs used (of 64), and the free space in the last PHROM.
- **Exceeding the budget:**
  - An import that would need a 65th PHROM is refused, with a message.
  - A settings change whose re-encoding would need a 65th PHROM is not
    applied. The phrase keeps its previous encoding, and the user is told
    why.

---

## 5. User interface

```
+--------------------------------------------------------------------------------+
| File  Edit  Phrase  Export  Help                                               |
+----------------+---------------------------------------------------------------+
| Phrases        | Name [HELLO        ]   Description [Greeting ...           ]  |
| +  -  ^  v  AZ |                                                               |
| > HELLO        |  Original (8 kHz)                 | Encoded (emulator)        |
|   GOODBYE      |  [waveform with playhead]         | [waveform with playhead]  |
|   SEVEN        |  [>||] [|<] [-] [+] [fit]         | [>||] [|<] [-] [+] [fit]  |
|   ...          |-------------------------------------------------------------- |
|                |  Encoding: 41 frames, 1 248 bits (156 B), 1 217 bit/s         |
|                |  voiced 48% unvoiced 21% repeat 19% silence 12%               |
|                |  Quality  Bad [=====================|---] As good as LPC     |
|                |           PESQ 2.08 (LPC best for this phrase 2.31): Good     |
|                |  Settings: [targetRms] [repeatLambda] [preEmphasis] ...       |
|                |  [Re-encode] [Optimise] [Reset to project defaults]           |
+----------------+---------------------------------------------------------------+
| Status: 37.2 KB · 3 PHROMs of 64 · 4.1 KB free in PHROM 3                      |
+--------------------------------------------------------------------------------+
```

- **Phrase list** (left dock, `QListView` over a `PhraseListModel`):
  - `+` adds a phrase: a file dialog for WAV, also accepting drag and
    drop of files onto the list.
  - `−` removes the selected phrases, with a confirmation.
  - Up/down buttons and drag-and-drop reorder phrases. The order is the PHROM
    word order (word numbers from 127).
  - A–Z sorts alphabetically as one undoable command.
  - Each row shows an indicator for stale/encoding/locked and the size in
    bytes.
  - A divider labelled `PHROM 2`, `PHROM 3`, … marks where each PHROM starts
    (§4.1). A phrase too large for any PHROM is marked in red.
- **Phrase editor** (right): name and description fields with validation, and
  the two waveform panes side by side in a `QSplitter`.
- **Waveform widget** (custom `QWidget`, shared by both panes):
  - min/max-per-pixel rendering from a precomputed multi-resolution peak
    cache;
  - zoom in/out (buttons, Ctrl+wheel, fit), horizontal scroll (scroll bar,
    wheel);
  - a **playhead** driven by `QAudioSink::processedUSecs()` on a 30–60 Hz
    timer;
  - a **play-start marker**: click to set, a "reset" button returns it to 0;
  - play/pause per pane, with Space for the focused pane;
  - an optional frame-grid overlay (25 ms) and frame-kind colouring on the
    encoded pane. A hover tooltip on the encoded pane shows the frame's
    energy, pitch and K indices.
  - The two panes share a time axis option (linked zoom/scroll), offset by the
    emulator's start latency, so the same sound lines up.
- **Encoder panel**: statistics (frames, bits, bytes, bit rate, kind
  breakdown, peak code, limiter activity), the **PESQ score and quality
  scale** (§3.3, "Presenting PESQ to the user"), a settings form generated
  from `EncoderOptions` metadata (name, range, default, tooltip from
  `usage.md`), and Re-encode, Optimise and Reset buttons. Changing a setting
  re-encodes in the background after a short debounce.
- **Undo/redo** (`QUndoStack`, in the GUI layer; §2.5) for add, remove,
  move, sort, rename, describe and settings changes.
- **Status bar**: the project size in KB and in PHROMs (§4.1), for example
  `37.2 KB · 3 PHROMs of 64 · 4.1 KB free in PHROM 3`.

---

## 6. Export formats

| Export | Scope | Contents |
|---|---|---|
| **Acorn PHROM images** (`.bin`, 16 KB each) | Selected phrases or all | `acornPhrom()` layout (§12.8): header strings, ASCII map, pointer table, bit-reversed names before each word. One image per PHROM (§4.1), either as separate files (`name_1.bin`, `name_2.bin`, …) or as one concatenated file (n × 16 KB) |
| **TI indexed images** (`.bin`) | Selected or all | `tiIndexed()` layout (§12.8), one per PHROM, at most 255 words each |
| **Raw data** (`.bin`) | One phrase | The packed bytes to the stop code; option for the Speak External form (≥ 9 bytes, §12.9) |
| **C header, PHROM** (`.h`) | Selected or all | One `const uint8_t name_phrom_N[16384]` per PHROM, plus an `enum`/table giving each word's PHROM, number, name, offset and length |
| **C header, Speak External** (`.h`) | Selected or all | One `const uint8_t` array per phrase (Speak External form), with a length constant and a lookup table |
| **WAV** | Selected or all | The encoded (emulator) output, or the 8 kHz original; one file per phrase, named from the phrase name |
| **BBC Micro DFS disc** (`.ssd`) | Selected or all | See below |

Header generation uses include guards, a generated-file comment with the
project name and date, and an SPDX header. The licence of the generated file
is configurable, because users own their speech data; the default is
`GPL-3.0-or-later`.

**BBC Micro disc image:**

- An Acorn DFS single-sided 80-track `.ssd` (option: 40 track): catalogue in
  sectors 0–1, the disc title, `!BOOT` with `*OPT 4,3` set so Shift+Break
  runs it.
- Files on the disc:
  - `!BOOT` → `CHAIN "SPEAK"`;
  - `SPEAK`, a **tokenised BBC BASIC II** program;
  - `DATA`, **one combined data file** holding every phrase's stream, with
    an index (offset and length per phrase) at its start. One file is used
    because an Acorn DFS catalogue holds only 31 files.
- The program shows a menu of the phrases and speaks the selected one.
  Speaking is done by a **6502 routine** (decision D3), assembled at run
  time by BASIC's inline assembler. It does not use the MOS speech queue
  (`SOUND &FF60`/`&FF00`).
- **What the routine does**, following §12.9:
  1. Loads the phrase from `DATA` into a buffer with OSGBPB, so phrases are
     read from disc one at a time. A whole project need not fit in the BBC's
     memory.
  2. Sends Speak External (&60) with OSBYTE &9F.
  3. Writes the first 16 bytes (at most), one OSBYTE &9F call per byte.
     Speech starts once 9 are in the FIFO.
  4. Polls the status with OSBYTE &9E. Each time **Buffer Low** (bit 6) is
     set, it writes up to 8 more bytes.
  5. After the byte holding the stop code, writes nothing more.
  6. Waits until **Talk Status** (bit 7) clears, then returns to BASIC.
- **Where the method comes from.** OSBYTE &9E (read speech processor) and
  &9F (write speech processor) are described in Section 6 of the
  [Acorn Speech System User Guide](https://chrisacorns.computinghistory.org.uk/docs/Acorn/Manuals/Acorn_SpeechSystemUG.pdf),
  and Section 5 has its own example programs. The guide's &9F example
  writes until it reaches a zero byte, and never checks Buffer Low. The
  routine here instead uses explicit lengths and refills on Buffer Low.
- **Bit order:**
  - Both of the guide's examples bit-reverse every byte before sending it
    (its `REVERSE` routine).
  - Its "ZERO" example data parses correctly only after that reversal: 25
    frames, with the stop code in the last of the 143 bytes. Unreversed, the
    stop is reached after 89 bytes.
  - So the guide's data is in the order a VSM read gives (as stored in a
    PHROM image), and the reversal turns it into FIFO order.
  - Our Speak External stream (`SpeakExternalStream`, §12.9) is already in
    FIFO order, so the routine sends its bytes **unreversed**.
  - The "ZERO" bytes become a unit test: reversed, they parse to exactly 143
    bytes; unreversed, they do not.
- **Stream rules:**
  - Every stream has at least 9 bytes (§12.9).
  - Bytes are written one at a time, so no even-length padding is needed.
  - The exporter checks each phrase against the buffer size the program
    reserves.
- **To confirm on an emulator and on hardware** (Phase 9, task 1):
  - the bit order;
  - that the MOS speech interrupt handler doesn't interfere when its own
    speech queue is empty, given that the routine drives the chip directly.
- A BASIC tokeniser in `s5core/bbc` handles the keyword table, line number
  encoding (`&8D`) and line layout. Its tests compare against programs saved
  by a real BBC Micro or an emulator.

---

## 7. Phased implementation

Each phase lists the phases it depends on. Each task has a description and
acceptance criteria. Every task also meets these rules:

- tests follow [TESTING.md](../TESTING.md): unit tests for the logic, and
  functional tests only where a unit test cannot meet the objective;
- the pre-commit checks of §1.3 pass;
- documentation is updated in the same task, as set out in
  [AGENTS.md](../AGENTS.md) §9.3.

### Phase 0: Scaffolding, build and quality gates

Depends on: nothing.

1. **CMake skeleton.** Create `app/` with the layout in §2.1. The top-level
   `CMakeLists.txt` has the in-source guard, C++20 and the warning flags of
   §1.3. It builds `s5core`, the `speak5220` GUI (an empty `QMainWindow`)
   and `speak5220-cli` (prints its version).
   - **Acceptance:** `cmake -S app -B build/app -G Ninja` and
     `cmake --build build/app` succeed; configuring inside `app/` is refused
     with a message; both executables start.
2. **Nix integration.** Extend `flake.nix`:
   - the dev shell gets Qt 6 (`qtbase`, `qtmultimedia`, `qttools`),
     `libsndfile`, `libsamplerate`, `pocketfft`, `libzip`, `yaml-cpp`,
     `spdlog`, `gtest` (with gMock), `cmake`, `ninja`, `pkg-config`, and
     the linters `clang-tools`, `cpplint`, `ruff`, `shellcheck`,
     `nixfmt-rfc-style`, `gersemi` and `actionlint`;
   - `packages.speak5220` is built with `qt6.wrapQtAppsHook`, and its check
     phase runs `ctest -L unit`.
   - **Acceptance:** `nix build .#speak5220` succeeds and runs the `unit`
     lane; `nix run .#speak5220` opens the window on this machine.
3. **Test tree.** Create `app/tests/` as laid out in TESTING.md. One CMake
   function labels each directory's tests, and the aggregate target
   `speak5220_tests` builds every test binary. Add one placeholder test per
   directory.
   - **Acceptance:** `ctest -L unit` and `ctest -L functional` each run only
     their own tests; moving a test file between `unit/` and `functional/`
     changes its label with no other edit.
4. **Format and lint configuration.** Add `.clang-format`, `.clang-tidy`,
   `CPPLINT.cfg`, `app/scripts/check-spdx.sh` (§1.1), and the `format` and
   `lint` CMake targets (§1.3).
   - **Acceptance:** `lint` passes on the skeleton; a file with a missing
     SPDX line, or a `File:` line that does not match its name, fails
     `check-spdx.sh`; `format` fixes a mis-formatted file.
5. **Pre-commit hooks.** Add [git-hooks.nix](https://github.com/cachix/git-hooks.nix)
   as a flake input, declare the hooks of §1.3, and install them from the
   dev shell.
   - **Acceptance:** after `nix develop`, a staged change that is
     mis-formatted, or that mis-names a function, is rejected; without a
     build directory, the `clang-tidy` hook fails with instructions instead
     of skipping.
6. **CI.** A GitHub Actions workflow on GitHub's free hosted `ubuntu-latest`
   runner installs Nix with `cachix/install-nix-action` (the installer
   only; no Cachix binary cache). Dependencies come from the public NixOS
   binary cache. The workflow runs `nix flake check` (all hooks on all
   files), the build, both test lanes, and both lanes again under
   ASan/UBSan. `main` is protected so that the workflow must pass. No paid
   or subscription service is used (D8).
   - **Acceptance:** CI is green on the skeleton; the deliberate violations
     of task 5 fail CI too.
7. **Repository documents.** Add `app/` to the `README.md` directory map
   and an application section to `BUILD.md`. Create
   `docs-tech/design/high-level-design.md` from §2 of this plan.
   - **Acceptance:** both documents describe the skeleton as built; the HLD
     exists and links back to this plan.

### Phase 1: Core foundations and the emulator

Depends on: Phase 0.

1. **Status and logging.** `util/`: `Status`/`StatusOr<T>`, and the logging
   of §2.6: the `info`, `debug` and `trace` levels, the console and file
   sinks, the per-module loggers, and the `--log-level`, `--log-file` and
   `--no-console-log` options in both executables.
   - **Acceptance:**
     - unit tests for status propagation;
     - unit tests with an in-memory sink show each level passing exactly the
       messages of §2.6, with warnings and errors passing at every level;
     - a functional test runs `speak5220-cli --log-level debug --log-file
       PATH` and finds the same messages in the file and on the console, and
       none on the console with `--no-console-log`.
2. **Frames and bit stream.** `lpc/`: the coding tables
   ([§8.1](../docs/technical/datasheet/coded-parameters.md)), `Frame`, `FrameKind()`,
   `FrameBits()`, `FrameCodec::Pack()`/`Parse()` with the bit order of
   [§12.1](../docs/technical/datasheet/authoring.md#121-the-bit-budget), and `BitReverse()`
   ([§6](../docs/technical/datasheet/commands.md)).
   - **Acceptance:** unit tests re-implement the frame and bit cases of
     `test_tms5220enc.py` (random round trips, hand-assembled bytes,
     invalid fields rejected, packing stops at the stop frame). The
     Speech System User Guide's "ZERO" stream (§6) parses to exactly 143
     bytes after bit reversal.
3. **Emulator classes.** `emulator/`: `Tms5220`, `Tms6100`,
   `Tms5220Config`, `IVsmBus` and `ISampleSink`, as described in §2.3.
   Sources: [§4](../docs/technical/datasheet/vsm-interface.md), [§5](../docs/technical/datasheet/io-structure.md),
   [§8.5](../docs/technical/datasheet/frame-timing.md), [§8.8](../docs/technical/datasheet/lattice-filter.md),
   [§8.9](../docs/technical/datasheet/reference-algorithm.md), and the
   [emulator's implementation choices](../docs/technical/emulator/choices.md).
   - **Acceptance:** the cases of `investigation/emulator/test_tms5220.c`
     pass as unit tests, with `IVsmBus` and `ISampleSink` mocked; every row
     of the §8.8 multiplier examples passes.
4. **Emulator reference data.** Scripts in `app/scripts/golden/` run
   `tms5220emu` on its self-test scenarios, the synthetic vectors, and
   encoder output for a corpus subset, through both paths. The results are
   committed to `app/golden/` with the reference commit hash (§1.4).
   - **Acceptance:** rerunning the scripts reproduces the committed files;
     functional tests show the C++ emulator matching them sample for sample
     and event for event.
5. **Synthesis.** `synth/`: `Synth` (VSM and Speak External paths, idle
   marking, caching), and the encoder's own reference-algorithm
   synthesiser (`synthesize()` in the Python).
   - **Acceptance:** unit tests check the host command sequences against a
     mocked `IVsmBus`; functional tests show the synthesiser matching the
     emulator's VSM path on the reference frame lists.
6. **Packaging.** `packaging/`: `AcornPhrom`, `TiIndexed` and
   `SpeakExternalStream`
   ([§12.8](../docs/technical/datasheet/authoring.md#128-word-tables),
   [§12.9](../docs/technical/datasheet/authoring.md#129-speak-external-streams)).
   - **Acceptance:** unit tests read every layout back (bit-reversed header,
     pointers, the 0xFF, names stored backwards, ASCII aliases, the null
     word, overflow, the 9-byte rule); functional tests match the reference
     images byte for byte.
7. **Audio and DSP adapters.** `AudioIO` over libsndfile (WAV only),
   `Resampler` over libsamplerate and `Fft` over pocketfft, behind
   `IWavReader`, `IWavWriter`, `IResampler` and `IFft`.
   - **Acceptance:** functional tests: WAV round trips at every supported
     format; no aliasing above 4 kHz on a resampled swept sine; `Fft`
     matches a direct DFT.

### Phase 2: The encoder in C++

Depends on: Phase 1.

1. **Encoder reference data.** `app/scripts/golden/encoder.py` records
   `encodeDetailed()` intermediates for a corpus subset, and final frames
   for the whole corpus with default and fixed non-default options.
   `app/scripts/generate_corpus.py` makes the WAV copies of the corpus
   (TESTING.md).
   - **Acceptance:** the data is committed to `app/golden/` and the scripts
     reproduce it.
2. **Analysis primitives.** The `sosfiltfilt` port with the committed
   second-order-section constants (and their generating script), `lfilter`,
   `Levinson`, `StepUp`, `QuantiseK` and the windows (§3.2).
   - **Acceptance:** unit tests on small hand-computed cases; functional
     tests match the reference intermediates within the stated tolerances.
3. **Pitch and voicing.** `IPitchTracker` (YIN, candidates, tracking) and
   `IVoicingClassifier`
   ([Voicing and pitch](../docs/technical/encoder/how-it-works.md#voicing-and-pitch)).
   - **Acceptance:** unit tests on synthetic pulse trains and noise;
     functional tests match the reference pitch and voicing tracks.
4. **Repeats, levels and endings.** `IRepeatSearch`, `ILevelSetter`
   (closed loop, limiter, AGC, compression), the fades and the K5–K10 guard
   ([Energy](../docs/technical/encoder/how-it-works.md#energy),
   [Repeat and silence decisions](../docs/technical/encoder/how-it-works.md#repeat-and-silence-decisions),
   [Start and end](../docs/technical/encoder/how-it-works.md#start-and-end)).
   - **Acceptance:** unit tests on synthetic inputs; functional tests match
     the reference intermediates.
5. **Encoder orchestration.** `Encoder` (implementing `IEncoder`) composes
   the stages. `EncoderOptions` carries metadata (range, default, help
   text) for the UI.
   - **Acceptance:** unit tests with mocked stages check the order of
     stages, how options are passed and how errors propagate; a
     determinism test encodes the same input twice and gets the same
     frames.
6. **Acceptance and performance.** `app/scripts/corpus-check.sh` checks the
   §3.2 criteria; `app/scripts/benchmark.sh` measures speed.
   - **Acceptance:** a corpus subset passes in the `functional` lane; the
     full corpus passes `corpus-check.sh`; encoding takes under 0.05 s per
     second of speech on one core.
7. **Windows and macOS CI.** Add build-and-test jobs for both lanes on
   Windows (MSVC, Qt from `aqtinstall`, libraries from vcpkg) and macOS
   (R6). During development they **do not run on every push**: they run
   only for a release (a `v*` tag), or when started by hand
   (`workflow_dispatch`).
   - **Acceptance:** a manually started run passes both lanes on Windows and
     macOS; pushes to `main` do not start these jobs.

### Phase 3: Quality measurement

Depends on: Phase 2.

1. **Quality reference data.** A script in `app/scripts/golden/` records
   PESQ scores (as `speechqual.py` measures them), the anchor scores
   (`speechqual.py --anchor`) and the `evaluate.py` measures for the corpus.
   - **Acceptance:** the data is committed and the script reproduces it.
2. **PESQ.** P.862 narrowband with the P.862.1 mapping, behind
   `IQualityMetric` (§3.3). It is written from scratch from the
   Recommendation texts (D6): level alignment, IRS filtering, time
   alignment, the auditory transform, disturbance processing and the
   MOS-LQO mapping. Each stage cites its section of the Recommendation
   (AGENTS.md §4.3.6), and is unit tested on synthetic signals.
   - **Acceptance:** functional tests are within ±0.01 of the reference
     scores on a corpus subset; `corpus-check.sh` passes on the full corpus.
3. **Cheap measures.** Level error and mel spectral distance, as in
   `evaluate.py`, behind `IQualityMetric`, for live feedback.
   - **Acceptance:** functional tests match the reference values.
4. **Anchor and quality scale.** The anchor vocoder (§3.3, "Presenting PESQ
   to the user") and a `QualityScale` that turns a score and an anchor into
   a position and a band. The band thresholds are calibrated on the corpus,
   and the result is recorded in the HLD.
   - **Acceptance:** functional tests match the reference anchor scores
     within ±0.02; unit tests for the position, the clamping and the band
     boundaries; the corpus calibration places the encoder's typical
     results in the upper bands.

### Phase 4: Project model, persistence and CLI

Depends on: Phase 3.

1. **Project model and editing operations.** `Project`, `Phrase` and the
   editing operations behind `IProjectEditor` (§2.5), the phrase name rules
   of D2, and the derivation of C identifiers (§4).
   - **Acceptance:** unit tests for every operation, including its reverse;
     every name in the Acorn PHROM A word list is accepted, and lower case
     and other characters are handled as specified.
2. **PHROM allocation.** List-order assignment, per-phrase cost and the
   64-PHROM budget (§4.1).
   - **Acceptance:** unit tests for a project that fills exactly one PHROM,
     one that spills into a second, a phrase too large for an empty PHROM,
     and an addition that would need a 65th PHROM.
3. **Encoding cache.** Hash-based invalidation, locked phrases, and
   re-encoding that respects the budget (§3.4, §4.1).
   - **Acceptance:** unit tests with a mocked `IEncoder`: a settings change
     re-encodes; a change that would exceed the budget keeps the previous
     encoding and reports why.
4. **Serialisation and stores.** `ProjectSerializer` (YAML, §4) over
   `IProjectStore`, with `ZipProjectStore` (atomic save, `~` backup,
   rejection of unsafe ZIP paths) and `DirectoryProjectStore`.
   - **Acceptance:** unit tests with a mocked store (quoted strings,
     unknown keys preserved, version migration); functional tests: ZIP and
     directory forms load identically, a simulated crash during save leaves
     the previous file intact, and a project saved under `de_DE` reloads
     identically.
5. **Import pipeline.** §3.1, run as a background job through
   `ITaskRunner`.
   - **Acceptance:** unit tests with a mocked reader and resampler;
     functional tests import 44.1 kHz stereo and 8 kHz mono fixtures.
6. **CLI.** `speak5220-cli` with `new`, `add`, `encode` and `export`, and an
   `--output` directory.
   - **Acceptance:** a functional test creates a project, adds and encodes
     a phrase, saves, and reloads it identically.
7. **Fixtures and first user documentation.** The `app/projects/general/`
   and `app/projects/phrom/` fixtures (TESTING.md). `docs/application/`,
   with the project-file and CLI reference pages, added to `mkdocs.yml`.
   - **Acceptance:** the fixtures load in functional tests;
     `mkdocs build --strict` passes.

### Phase 5: Main window, phrase list and undo

Depends on: Phase 4.

1. **Main window.** Menus, actions, shortcuts, recent files, the
   dirty-state title, and a save prompt on close.
   - **Acceptance:** GUI functional tests (offscreen) for open, save, and
     closing with unsaved changes.
2. **Phrase list.** `PhraseListModel`: `+`/`−`, up/down, drag-and-drop
   reordering, A–Z sort, multi-selection, file drops to import, and the
   PHROM dividers (§5).
   - **Acceptance:** GUI unit tests on the model with a mocked
     `IProjectEditor`; dividers appear at the allocator's boundaries.
3. **Phrase details.** Name and description fields with the D2 validation.
   - **Acceptance:** GUI unit tests: lower case is converted; refused
     characters show a message and leave the name unchanged.
4. **Undo and redo.** `QUndoStack` with one `QUndoCommand` per editing
   operation in `app/src/gui/undo/` (§2.5).
   - **Acceptance:** GUI unit tests with a mocked `IProjectEditor`: each
     command's `redo()` and `undo()` call the right operations; undoing a
     sort restores the previous order; a failed operation is not pushed.
5. **Status bar.** The project size in KB and in PHROMs (§4.1).
   - **Acceptance:** GUI unit tests format the text for given allocator
     results.

### Phase 6: Waveforms and playback

Depends on: Phase 5.

1. **Waveform view.** `WaveformView`: the peak cache, zoom, scroll, the
   play-start marker and its reset (§5).
   - **Acceptance:** unit tests for the peak cache and the
     sample-to-pixel mapping; a GUI functional test renders offscreen.
2. **Player.** `Player` over `IAudioOutput` (a `QAudioSink` adapter) and
   `IClock`: play, pause and resume from the marker, one pane at a time,
   and resampling when the device does not accept 8 kHz.
   - **Acceptance:** unit tests with a mocked output and a fake clock; a
     manual check that the playhead follows the sound on Linux with
     PipeWire.
3. **Encoded pane.** The emulator output through `ISynth`, the frame
   overlay and the hover information.
   - **Acceptance:** unit tests map pointer positions to the right frame,
     including the emulator's start latency.
4. **Linked time axes.** Linked zoom and scroll, offset by the emulator's
   start latency.
   - **Acceptance:** unit tests show the same sound at the same position in
     both panes.

### Phase 7: Encoder panel and optimiser

Depends on: Phase 6.

1. **Encoder panel.** The statistics view, the PESQ score and quality
   scale (§3.3), the settings form generated from `EncoderOptions`,
   debounced background re-encoding (timed through `IClock`), per-phrase and
   project-default settings, and Reset.
   - **Acceptance:**
     - GUI unit tests: a setting change re-encodes once after the debounce;
       Reset restores the project defaults;
     - the scale shows the score, its band label and the tooltip, and stays
       readable with colour removed;
     - an encoding at or above the phrase's anchor shows as *As good as LPC
       can achieve*.
2. **Optimiser search.** §3.3 over `IEncoder`, `ISynth`, `IQualityMetric`
   and `ITaskRunner`: the grid, the descent, the project budget, the
   per-phrase limit and deterministic tie-breaking.
   - **Acceptance:** unit tests with mocks and an inline task runner; a
     functional test gets the same result with 1 and 8 threads.
3. **Optimise in the GUI.** Progress, cancel, accept/revert, the candidate
   history, and Optimise all.
   - **Acceptance:** GUI unit tests with an inline task runner; cancelling
     leaves the phrase's settings unchanged.
4. **Optimiser evaluation.** The `app/projects/benchmark/` fixtures, and
   Optimise results in `corpus-check.sh` and `benchmark.sh`.
   - **Acceptance:** Optimise raises PESQ over the defaults on most corpus
     files within the budget and any per-phrase limit; the timings are
     recorded.

### Phase 8: Export: binary, C and audio

Depends on: Phase 7.

1. **Binary exports.** Acorn PHROM images (separate or concatenated), TI
   indexed images, and raw `.bin` in plain or Speak External form (§6).
   - **Acceptance:** functional tests match the reference data byte for
     byte for the same frames.
2. **C headers.** The PHROM and Speak External forms (§6), with the
   identifier mapping comment, the SPDX header and a configurable licence.
   - **Acceptance:** functional tests compile the generated headers with
     the C and C++ compilers.
3. **WAV export.** The encoded output or the 8 kHz original.
   - **Acceptance:** functional tests reload the files with the expected
     rate and length.
4. **Export dialog and presets.** Scope, format and per-format options, the
   size checks of §4.1, and the `exports:` presets that the CLI can run.
   - **Acceptance:** GUI unit tests map dialog settings to presets; a
     functional test runs a preset through the CLI.
5. **Output hashes.** `app/scripts/run-projects.sh` and
   `app/scripts/output-hashes.sh`, with a recorded baseline for the
   `general` and `phrom` suites.
   - **Acceptance:** rerunning matches the baseline; a deliberate change to
     an exporter is detected.

### Phase 9: Export: BBC Micro disc

Depends on: Phase 8.

1. **Hardware spike.** A hand-written BASIC program with the 6502 routine
   of §6 speaks one encoder-made phrase, with its bytes unreversed. It is
   run on an emulator with speech support, and on a BBC Micro with the
   speech upgrade.
   - **Acceptance:** the bit order, the Buffer Low refill and the MOS
     interrupt behaviour are confirmed (R5), and the results are recorded
     in the HLD.
2. **BASIC tokeniser.** BBC BASIC II keywords, line numbers (`&8D`
   encoding) and line layout.
   - **Acceptance:** unit tests match programs saved on a BBC Micro or an
     emulator, byte for byte.
3. **DFS disc image.** The `.ssd` writer: catalogue, title, boot option,
   40 or 80 tracks, and the 31-file limit.
   - **Acceptance:** unit tests check the catalogue fields; a functional
     test reads the image back with an independent reader.
4. **Player program and data file.** The generated BASIC program with the
   6502 routine, and the combined `DATA` file with its index (§6),
   including the buffer-size checks.
   - **Acceptance:** unit tests check the data file layout; the `.ssd`
     boots on an emulator, shows the menu and speaks every phrase of the
     `app/projects/bbc/` fixtures; the same is checked by hand on a Gotek;
     the `bbc` suite is added to the hash baseline.
5. **PHROM images on a BBC Micro.** A generated PHROM image is tested in a
   BBC Micro's speech system.
   - **Acceptance:** the result is recorded in the HLD and reported. It
     answers the open question in
     [Appendix B](../docs/technical/datasheet/gaps.md#the-acorn-phrom-format-and-the-bbc-micro),
     which is updated only with the user's agreement (AGENTS.md §9.4).

### Phase 10: Polish

Depends on: Phase 9.

1. **Preferences.** Audio device, default encoder settings, theme, waveform
   colours, and the log level and log file (§2.6).
   - **Acceptance:** GUI unit tests show the settings stored and applied.
2. **About box.** Licence and third-party notices.
   - **Acceptance:** every library in §2.4 is listed with its licence.
3. **Error reporting.** Corrupt WAV files, corrupt projects and audio
   device failures.
   - **Acceptance:** functional tests: each case gives a clear message and
     no crash.
4. **Accessibility and display.** Keyboard navigation, accessible names,
   high-DPI and dark mode.
   - **Acceptance:** the getting-started walkthrough can be completed with
     the keyboard alone; high-DPI and dark-mode checks are recorded.
5. **Import of existing PHROM images.** Words from an Acorn PHROM image
   become locked phrases with no source audio (§3.4).
   - **Acceptance:** functional test: an exported image imports back to the
     same frames; locked phrases are never re-encoded.
6. **User documentation complete.** `docs/application/` covers every
   feature (AGENTS.md §9.2).
   - **Acceptance:** every YAML key, encoder option, CLI option and export
     format is documented; `mkdocs build --strict` passes.

### Phase 11: Cross-platform packaging and release

Depends on: Phase 10.

1. **Flatpak.** A manifest on the KDE runtime (`org.kde.Platform` 6.x),
   with libsndfile, libsamplerate, libzip and yaml-cpp as modules where the
   runtime lacks them, plus the `.desktop` file, AppStream metainfo and
   icons. Built with `flatpak-builder` in Actions.
   - **Acceptance:** the bundle installs, starts and plays audio on a Linux
     desktop.
2. **Windows MSI.** The MSVC build, `windeployqt`, and CPack's WiX
   generator. The MSI is **not code-signed**, because signing needs a paid
   certificate (D8). The user documentation explains the SmartScreen
   warning this causes and how to proceed.
   - **Acceptance:** the MSI installs on a clean Windows 11, and the
     application starts and plays audio.
3. **macOS DMG.** A universal (arm64 + x86_64) build if Qt's binaries allow
   it, otherwise arm64; `macdeployqt`; CPack's DragNDrop generator. The
   application is ad-hoc signed only and **not notarised**, because
   notarisation needs a paid Apple Developer membership (D8). The user
   documentation explains how to open it past Gatekeeper.
   - **Acceptance:** the DMG installs on macOS, and the application starts
     and plays audio.
4. **Release workflow.** A `v*` tag runs the Windows and macOS test jobs
   (Phase 2, task 7) and the Linux checks, then builds all three installers
   and drafts a GitHub Release.
   - **Acceptance:** a test tag produces a draft release with all three
     artifacts.

---

## 8. Testing strategy

Testing follows [TESTING.md](../TESTING.md), which is the source of truth.
In short:

- **Dependency inversion.** Classes depend on `I`-prefixed interfaces,
  injected through their constructors (§2.2), so each method is unit tested
  with gMock mocks.
- **Google Test with Google Mock** for every suite, run through CTest.
- **Two lanes, chosen by directory.** Tests under `app/tests/unit/` (and
  `app/tests/gui/unit/`) are labelled `unit`; tests under
  `app/tests/functional/` (and `gui/functional/`) are labelled `functional`.
  - Unit tests are about 80% of all tests. They never touch the file system,
    the audio device, threads or the clock.
  - Functional tests cover the library adapters, the reference data in
    `app/golden/`, the CLI end to end and the GUI on the offscreen platform.
- **Fixtures and reference data live outside `app/tests/`:** fixture
  projects (unpacked) in `app/projects/`, and the reference baseline in
  `app/golden/` (§1.4). Mechanically derived data, such as the WAV copies of
  the corpus, is generated rather than committed.
- **Determinism:** the same input gives the same output on every run and at
  every thread count, and YAML is locale-independent.
- **Scripts, run by hand:**
  - `run-projects.sh` and `output-hashes.sh`: byte-identical export checks
    against a recorded baseline;
  - `corpus-check.sh`: the full-corpus acceptance of §3.2 and §3.3;
  - `benchmark.sh`: timing.
- **CI:**
  - every push runs the pre-commit checks, both lanes on Linux, and both
    lanes under ASan/UBSan on Linux;
  - Windows and macOS run both lanes only for a release (a `v*` tag) or when
    started by hand, not on every push;
  - the packaged Nix build runs the `unit` lane only.
- **Style and lint** (§1.3) are checked on every commit by the pre-commit
  hooks, and on every push by CI. A phase is not done while any check
  fails, and checks are never disabled to get a phase through: an exception
  to the guide is added to the configuration, with a comment saying why.

---

## 9. Decisions

| # | Question | Decision |
|---|---|---|
| D1 | Keep the original input file in the project? | **No.** Only the 8 kHz copy is stored (§3.1). |
| D2 | Phrase name rules? | **Acorn style.** Upper case, digits, space and `' ( ) , - .`; at most 16 characters; C identifiers derived automatically (§4). |
| D3 | How does the BBC disc speak a phrase? | **A 6502 routine from the start**, writing the chip with OSBYTE &9F and refilling on Buffer Low. The MOS `SOUND` queue is not used (§6). |
| D4 | A size budget for the project? | **64 PHROMs (64 × 16 KB = 1 MB)**, a testing limit. PHROMs are filled automatically in list order. Imports, re-encoding and the optimiser never take the project past the budget. Size is shown in KB and in PHROMs (§4.1). |
| D5 | What happens to the Python encoder and C emulator? | **Retired once the C++ is accepted.** The golden data becomes a fixed regression baseline, and the C++ becomes the only maintained implementation (§1.4). |
| D6 | Where does the PESQ implementation come from? | **Written from scratch** from the published ITU-T P.862 and P.862.1 texts, and licensed GPLv3-or-later with the rest of the project. The ITU reference code is not used (§3.3). |
| D7 | When do the Windows and macOS CI jobs run? | **Only for a release** (a `v*` tag) or when started by hand, during development. Linux CI runs on every push (Phase 2, task 7). |
| D8 | Which CI and distribution services are used? | **Free ones only.** GitHub's hosted runners (`ubuntu-latest` for Linux) with Nix installed by a free action and the public NixOS binary cache. No paid caches, remote builders or Nix CI services, and no paid code signing: the MSI is unsigned and the DMG is not notarised (Phase 11). |

---

## 10. Risks

| # | Risk | Mitigation |
|---|---|---|
| R1 | **PESQ accuracy.** Our implementation, written from scratch from the P.862 text (D6), may not reproduce the reference scores closely, because the text may not pin down every detail that the ITU reference code does. It is also the largest single item in the plan | Build it stage by stage, with unit tests per stage, and check it against golden scores from the Python package (a black box, never distributed). If the scores cannot be matched to ±0.01, record the remaining difference in the HLD; the optimiser can fall back to the cheap measures while the gap is closed |
| R2 | The C++ encoder drifts from Python through floating-point differences in near-tie decisions | Re-implement function by function with intermediate golden data; accept on frames plus PESQ, not bits alone (§3.2) |
| R2a | The C++ emulator's restructuring (classes, observers) changes timing, for example the slot at which a host write lands or the order of events | Accept only on identical codes **and** events (§2.3); keep the slot-stepped design, and change structure, not order of evaluation |
| R3 | The optimiser is too slow | A fast C++ encoder (Phase 2 target), parallel candidates, cached analysis shared between candidates where options do not affect it (pitch tracking is independent of the level options) |
| R4 | `QAudioSink` at 8 kHz is unsupported on some devices | Resample to the device's preferred rate on output; use the device clock for the playhead |
| R5 | BBC Micro behaviour is partly unverified: whether a new PHROM image is read, the bit order for Speak External, and whether the MOS speech interrupt handler interferes with the 6502 routine | Phase 9, task 1: a hand-written spike on an emulator and on hardware before the generator is built |
| R6 | Windows/macOS builds diverge from Nix (library versions, compiler differences). Their CI jobs run only on release, so a divergence can go unnoticed between releases | Pin the same Qt version everywhere, and pin vcpkg with a baseline. The Windows and macOS jobs exist from Phase 2 (task 7), and can be started by hand at any time |
