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
| `s5core`: `util/` (Status, logging, version) | Implemented |
| `s5core`: `lpc/` (tables, frames, bit stream) | Implemented |
| `s5core`: `emulator/` (`Tms5220`, `Tms6100`, host procedures) | Implemented, matching the C reference sample for sample |
| `s5core`: `synth/` (`Synth`, `ReferenceSynthesizer`) | Implemented |
| `s5core`: `packaging/` (Acorn PHROM, TI indexed, Speak External) | Implemented |
| `s5core`: `audio/`, `dsp/` (WAV, resampler, FFT adapters; filters, NumPy-ordered sums, LPC analysis) | Implemented |
| `s5core`: `encoder/` (`Encoder` and its stages, `EncoderOptions`) | Implemented, matching the Python reference's frames on the whole corpus |
| Reference data in `app/golden/` (emulator, packaging, encoder) | Implemented |
| `speak5220` GUI | Implemented as a skeleton: an empty `MainWindow`, logging options |
| `speak5220-cli` | Implemented as a skeleton: version and logging options |
| Test tree and lanes (TESTING.md) | Implemented |
| Format, lint, pre-commit hooks, CI on Linux | Implemented |
| CI on Windows and macOS (release tags and manual runs) | Implemented; not yet run |
| Nix dev shells and package | Implemented |
| Quality, optimiser, project, BBC export, the GUI's views | Planned |

---

## 2. Repository layout

```
app/
  CMakeLists.txt                top level: in-source guard, C++20, options
  .clang-format  .clang-tidy  CPPLINT.cfg  .gersemirc
  vcpkg.json                    the libraries for Windows and macOS (§3)
  cmake/                        CompilerWarnings.cmake, Dependencies.cmake,
                                QualityTargets.cmake
  scripts/                      check-spdx.sh, check-header-guards.sh,
                                clang-format-check.sh, clang-tidy.sh,
                                format.sh, lint.sh; corpus-check.sh,
                                corpus_pesq.py, benchmark.sh,
                                generate_corpus.py,
                                generate_filter_constants.py (§8)
    golden/                     generate_golden.py and its scenario scripts,
                                encoder.py (offline, run by hand: §12)
  golden/                       reference data from the retired references (§12)
  src/
    core/                       library "s5core" (QtCore at most)
      util/                     Status/StatusOr, logging, logging options, version
      lpc/                      coding tables, Frame, FrameCodec, BitReverse
      emulator/                 Tms5220, Tms6100, ChipDriver, interfaces
      synth/                    Synth, ReferenceSynthesizer
      packaging/                AcornPhrom, TiIndexed, SpeakExternalStream
      audio/                    WAV interfaces and the libsndfile adapters
      dsp/                      resampler and FFT interfaces and adapters;
                                SOS/FIR filters and their constants,
                                NumPy-ordered sums and windows, LPC analysis
      encoder/                  Encoder and its stages, EncoderOptions (§8)
    gui/                        library "speak5220_gui" + the "speak5220" executable
      main_window/              MainWindow
    cli/                        the "speak5220-cli" executable
  tests/
    .clang-tidy                 the app's settings, plus one test exception
    support/                    speak5220_test_support (helpers, mocks, golden readers)
    unit/  functional/          speak5220_unit_tests, speak5220_functional_tests
    gui/support/                speak5220_gui_test_main (QApplication, offscreen)
    gui/unit/  gui/functional/  speak5220_gui_unit_tests, speak5220_gui_functional_tests
```

Planned additions, each created with its first code (plan §2.1):

- `src/core/`: `quality/`, `bbc/`, `project/`;
- `src/gui/`: `phrase_list/`, `phrase_editor/`, `waveform/`,
  `encoder_panel/`, `export/`, `undo/`;
- `app/projects/` (fixture projects), `app/resources/` (icons, `.desktop`,
  metainfo), `app/packaging/` (Flatpak, WiX/CPack, macOS);
- more scripts in `app/scripts/`: `run-projects.sh`, `output-hashes.sh`.

`s5core` is a plain C++20 static library that depends on QtCore at most (for
the shared command-line options). All DSP, emulation, encoding, packaging and
file formats live there, so they can be tested without a GUI and reused by
the CLI. It contains no C sources and no code copied from `investigation/`.

---

## 3. Build

**Implemented.**

- CMake ≥ 3.24 with Ninja. `app/CMakeLists.txt` refuses any build
  directory inside `app/`, so generated files never mix with the sources
  that the check scripts scan. The Nix package builds in `../build` for the
  same reason.
- C++20 without extensions. Every target links the interface library
  `speak5220_compile_options`, which carries `-Wall -Wextra -Wpedantic
  -Wconversion -Wshadow -Werror -ffp-contract=off` (GCC/Clang) or `/W4 /WX
  /utf-8 /permissive- /external:W0` with `_CRT_SECURE_NO_WARNINGS` (MSVC),
  and, with `SPEAK5220_SANITIZE=ON`, `-fsanitize=address,undefined`.
  `-ffp-contract=off` stops fused multiply-add contraction (Clang's default,
  and GCC's on targets with FMA), so the encoder rounds as NumPy does on
  every platform (§8); `-ffast-math` is never used. Debug and
  sanitizer builds also define `_GLIBCXX_ASSERTIONS`, so `operator[]` on
  standard containers is range-checked (§4).
- The version is set once, in `project(... VERSION ...)`. `s5core` receives
  it as `SPEAK5220_VERSION`, and `flake.nix` reads it for the package.
- `SPEAK5220_BUILD_TESTS` (default `ON`) adds `app/tests/`.
- `cmake/Dependencies.cmake` finds the libraries of §9: Qt 6 (Core,
  Widgets) and spdlog by their CMake packages; libsndfile and libsamplerate
  by header and library (their pkg-config files name private dependencies a
  shared build does not need); pocketfft by its header, as an interface
  target that defines `POCKETFFT_NO_MULTITHREADING`. Google Test is found by
  `app/tests/`.
- GCC reports a false `-Wmaybe-uninitialized` inside pocketfft's templates
  at `-O3`; a GCC-only pragma silences it around pocketfft's `#include` in
  `dsp/fft.cc`.
- `format` and `lint` targets wrap `app/scripts/format.sh` and
  `app/scripts/lint.sh`.
- **Windows and macOS:** Qt from aqtinstall, pinned to the flake's Qt
  version (6.11.2); the other libraries from vcpkg in manifest mode
  (`app/vcpkg.json`, pinned by `builtin-baseline`; libsndfile without its
  compressed-format features), through vcpkg's CMake toolchain file.

**Nix** (`flake.nix`):

- `devShells.app`: the application's libraries, Google Test, Qt tools, the
  linters, and the pre-commit hooks (installed by its `shellHook`). It sets
  `QT_PLUGIN_PATH` so the GUI built in `build/app` runs.
- `devShells.default`: the research shell (KiCad, Pico SDK, Python), plus
  everything in `devShells.app`. The golden-data scripts run here (§12).
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
| `clang-tidy` (`app/.clang-tidy`, `app/tests/.clang-tidy`) | Hook (needs `build/app`), `lint`, CI build job |
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
  reason, in `app/.clang-tidy`, `app/tests/.clang-tidy` and
  `app/CPPLINT.cfg`. In particular, clang-tidy's static rule against
  computed array indices is replaced by run-time range checks
  (`_GLIBCXX_ASSERTIONS`) in the Debug and sanitizer builds, because the
  emulator and DSP code index arrays in every sample.

**CI** (`.github/workflows/app.yml`), on every push and pull request, on
`ubuntu-latest` with Nix (decision D8): `nix flake check`; build, both lanes
and `clang-tidy`; both lanes under ASan/UBSan (Debug); `nix build
.#speak5220`. Protecting `main` so that this workflow must pass is a
repository setting on GitHub.

**Windows and macOS CI** (`.github/workflows/app-platforms.yml`, decision
D7): `windows-latest` (MSVC, through `ilammy/msvc-dev-cmd`) and
`macos-latest`, each building with Ninja and running both lanes. They run
only for a `v*` tag or when started by hand (`workflow_dispatch`), never on
an ordinary push. Qt comes from `jurplel/install-qt-action` (aqtinstall),
cached; vcpkg (`lukka/run-vcpkg`) builds the other libraries from source on
each run. The jobs have not yet been run: plan Phase 2 task 7's acceptance
is a manually started run that passes.

---

## 5. Tests

**Implemented** as laid out in [TESTING.md](../../TESTING.md).

- `speak5220_add_test_lane()` in `app/tests/CMakeLists.txt` globs a lane
  directory recursively (`CONFIGURE_DEPENDS`), builds one binary from it,
  and labels its tests with the directory's name, `unit` or `functional`.
  Tests are discovered when CTest runs (`DISCOVERY_MODE PRE_TEST`).
- The aggregate target `speak5220_tests` builds every test binary.
- `speak5220_test_support` (`app/tests/support/`) is a static library:
  - `emulator_mocks.h`: gMock mocks of `IVsmBus`, `ISampleSink` and
    `ITms5220Observer`;
  - `frame_builder`: a bit-stream builder independent of `FrameCodec`, and
    random and speech-like frames;
  - `golden_data`: readers for the files in `app/golden/` (codes, lines,
    tab-separated tables, bytes, frame CSV); a missing file is an error
    naming its path;
  - `emulator_run`: the reference program's scenario-script and bulk modes
    on the C++ emulator, with its event-log format, for the golden
    comparisons;
  - `encoder_mocks.h`: gMock mocks of `IFft` and the encoder's stage
    interfaces;
  - `encoder_golden`: readers for `app/golden/encoder/` (option sets,
    reference encodings with their near-tie frames, JSON intermediates via
    `QJsonDocument`, WAV inputs as doubles) and `CompareEncoding()`, which
    applies the acceptance rule of §8.
- The functional lane has the CLI's path (`SPEAK5220_CLI_PATH`) and the
  reference data's directory (`SPEAK5220_GOLDEN_DIR`) compiled in.
- **By-hand tests.** A few functional tests carry Google Test's `DISABLED_`
  prefix, so CTest lists them as disabled and never runs them. The scripts
  run them with `--gtest_also_run_disabled_tests`, passing inputs and an
  output file through environment variables: the full-corpus check
  (`EncoderCorpusCheck`, `corpus-check.sh`) and the encoder's timing
  (`EncoderBenchmark`, `benchmark.sh`).
- GUI test binaries use `speak5220_gui_test_main`, which selects the
  offscreen platform unless `QT_QPA_PLATFORM` is already set, then creates a
  `QApplication` and runs Google Test.

---

## 6. Main components

Plan §2.2, with the names as built.

| Component | Status and responsibility |
|---|---|
| `Status`, `StatusOr<T>` | **Implemented** (`core/util/status`): error values in place of exceptions, modelled on Abseil's |
| Logging | **Implemented** (`core/util/logging`, `logging_options`): §11 |
| `Version()`, `kApplicationName` | **Implemented** (`core/util/version`) |
| Coding tables | **Implemented** (`core/lpc/coding_tables.h`): datasheet §8.1 Tables 8-4 to 8-7, `constexpr` |
| `Frame`, `FrameCodec`, `BitReverse()` | **Implemented** (`core/lpc/`): frame kinds and lengths, validation, `Pack()`/`Parse()` with the §8.1/§12.1 bit order |
| `Tms5220`, `Tms6100`, `ChipDriver` | **Implemented** (`core/emulator/`): §7 |
| `Synth` (`ISynth`) | **Implemented** (`core/synth/synth`): §7 |
| `ReferenceSynthesizer` | **Implemented** (`core/synth/reference_synthesizer`): the encoder's reference-algorithm synthesiser (`synthesize()` in the Python), which matches the emulator's VSM path |
| `AcornPhrom`, `TiIndexed`, `SpeakExternalStream` | **Implemented** (`core/packaging/`): the §12.8 and §12.9 layouts, byte-identical to the Python encoder's |
| `SndfileWavReader`, `SndfileWavWriter` (`IWavReader`, `IWavWriter`) | **Implemented** (`core/audio/`): any WAV variant read and mixed to mono as floats; 16- or 24-bit PCM written, clipped |
| `SincResampler` (`IResampler`) | **Implemented** (`core/dsp/resampler`): libsamplerate `SRC_SINC_BEST_QUALITY` |
| `PocketFft` (`IFft`) | **Implemented** (`core/dsp/fft`): real FFTs with NumPy's conventions |
| Filters, sums, LPC analysis | **Implemented** (`core/dsp/iir_filter`, `array_math`, `lpc_analysis`, `filter_constants.h`): §8 |
| `MainWindow` | **Implemented** as an empty window (`gui/main_window`) |
| `Encoder` (`IEncoder`) | **Implemented** (`core/encoder/`): re-implementation of `encode()`/`encodeDetailed()` with the full `EncoderOptions` set (§8) |
| `Quality` | Planned: PESQ (P.862 narrowband, as P.862.1 MOS-LQO) plus cheap measures |
| `Optimiser` | Planned: searches encoder options for the best PESQ, within the 64-PHROM project budget |
| Raw `.bin`, C header export | Planned |
| `BbcExport` | Planned: tokenised BBC BASIC program plus data files on an Acorn DFS `.ssd` image |
| `Project` | Planned: phrases, their audio, settings and cached encodings; the editing operations; load/save |

Classes depend on `I`-prefixed interfaces, injected through their
constructors. Everything that does I/O, wraps a third-party library, uses
threads or time, or is a large pipeline stage sits behind one. TESTING.md
lists each interface, its real implementation and where it is mocked.

---

## 7. The emulator

**Implemented** (plan §2.3). The C emulator
(`investigation/emulator/tms5220.c`, `tms6100.c`) is rewritten as C++
classes in `core/emulator/`, stepped one slot (6.25 µs) at a time, with the
same defaults for the datasheet's gaps. The structure changes; the
arithmetic and order of evaluation do not.

| C reference | C++ |
|---|---|
| `struct tms5220` + `tms5220_*()` | class `Tms5220`: `Step()`, `Write()`, `Read()`, `StatusByte()`, `IsTalking()`, `SetPhase()`, `NextBoundary()` |
| `tms5220_config`, `tms5220_default_config()` | `struct Tms5220Config` with default member initialisers, one commented field per datasheet-gap choice; `SetConfigParameter()` sets one by its C name |
| VSM callback | interface `IVsmBus` (`Load()`, `Read()`, `Dummy()`, `Branch()`), implemented by `Tms6100` |
| sample callback | interface `ISampleSink` |
| event callback | interface `ITms5220Observer` and `struct Tms5220Event` (VSM, host, flag and milestone events) |
| `struct tms6100` with a 1 MB `malloc` | class `Tms6100` holding a `std::vector<uint8_t>` |
| `booth()`, `wrap14()`, noise LFSR, DAC model | `constexpr` functions in `chip_arithmetic.h` (`Booth()`, `BoothClosedForm()`, `BoothDigits()`, `Wrap14()`, `Asr()`, `NoiseStep()`), shared with `ReferenceSynthesizer`; `DacCurrentMicroamps()` |
| the host operations of `tms5220emu.c` | class `ChipDriver`: whole host cycles, `UntilIdle()`, `Boundary()`, and the bulk-mode procedures `SpeakFromVsm()` and `SpeakExternal()` |

Notes:

- The status read is named `StatusByte()`, not the plan's `Status()`, so it
  does not hide the `Status` type inside the class.
- `Tms5220` keeps its scheduled actions and parameter writes in fixed arrays
  and takes them in the reference's order, so `Step()` never allocates.
- Collaborators are borrowed raw pointers and may be null, as the C
  callbacks may be.

**`Synth`** (`ISynth`) renders packed bytes from power-up with the
reference program's bulk procedure (Speak latched 1961 slots before a frame
boundary, then run until idle plus one frame):

- **VSM path:** the bytes in a `Tms6100` at address 0, Load Address × 5,
  then Speak.
- **Speak External path:** Speak External, the first 16 bytes, then 8 more
  whenever a status read every 1 ms shows Buffer Low. A stream under 9 bytes
  never starts (datasheet §12.9) and is reported as an error.
- Output: the codes from the sample period of the first frame read, the
  same codes × 64 as 16-bit samples, the length of the leading idle, and the
  period of the first code.
- Results are cached per phrase in a hash map keyed by the path and the
  bytes (FNV-1a hash, full comparison). `Synthesize()` is thread-safe; the
  caller runs it on a worker thread.

**Acceptance** (met): the C++ emulator reproduces every reference run in
`app/golden/emulator/` (§12), codes sample for sample and events line for
line, and the C self-test cases pass as unit tests with the VSM bus, sample
sink and observer mocked.

---

## 8. The encoder

**Implemented** (plan §3.2, Phase 2). `core/encoder/` re-implements
`tms5220enc.py`'s `encodeDetailed()` function by function; each C++
function's comment names its Python original.

**Stages.** `Encoder` (`IEncoder`) composes five injected stages, in the
order of `encodeDetailed()`. `MakeEncoder()` returns one with the standard
stages over `PocketFft`.

| Stage (interface → class) | Python | Does |
|---|---|---|
| `IFrameAnalyser` → `FrameAnalyser` | `analyse()` | `Segment()`: the 60 Hz high-pass, the level envelope, the gate, the onset and offset, the frame centres from c0. `Features()`: each frame's level and the lag-windowed autocorrelations (pre-emphasised for voiced, plain for unvoiced) |
| `IPitchTracker` → `YinPitchTracker` | `trackPitch()`, `_yin()`, `_candidates()`, `_tiltAndCrossings()` | The voicing and period every 5 ms: YIN on the 1 kHz low-pass, up to five candidates per hop, and a Viterbi path |
| `IVoicingClassifier` → `VoicingClassifier` | the voicing part of `analyse()` | Each frame's voicing by majority of its three nearest hops, then the octave fix |
| `IRepeatSearch` → `RepeatSearch` | `_kindsAndK()`, `_segment()`, `_repeatRun()` | K quantised per frame; silence, full frames and repeats by dynamic programming over bits plus `repeatLambda` × distortion |
| `ILevelSetter` → `LevelSetter` | `_shapeLevels()`, `_setLevels()`, `_gainOf()`, `_effectiveK()`, `_outLevels()` | Slow gain control and compression; open-loop energies, closed-loop passes through `ReferenceSynthesizer`, the too-loud check, the peak limiter and the safety net |

Between the stages, `Encoder` drops a faint opening (`TrimFaintOpening()`),
sets the gain (by `targetRms`, or by a first pass with the limit at 511 for
`targetPeak`), and turns the plan and energies into frames with
`frame_endings` (`ToFrames()`, `GuardK510()`, `AddFades()`, from
`_toFrames()`, `_guardK510()` and `_addFades()`). The quantisers
(`QuantiseK()`, `PitchIndex()`, `EnergyIndex()`) are in `quantiser`, and the
fixed filters (`HighPass60Hz()`, `LowPass1kHz()`, `PreEmphasisFilter()`) in
`encoder_filters`.

The plan names four stage interfaces. `IFrameAnalyser` is a fifth, so the
orchestration can be unit tested with every stage mocked.

**Results.** `EncodeResult` holds the frames, which always end in a stop,
and, unless the input held no sound, `EncodeDetails`: the high-passed input,
the planned frames' centres, c0, the lead and the dropped opening, the gain,
the plan, the shaped levels, the energies and the limiter's statistics.
`Encode()` refuses a rate other than 8000 Hz, a non-finite sample, and
options outside their ranges (`ValidateEncoderOptions()`).

**Options.** `EncoderOptions` has the 15 options of `tms5220enc.py`
`DEFAULTS`, with the same defaults. `EncoderOptionInfos()` gives each one's
key (the Python name), label, help text, type, range and default, for the
GUI's settings form and for project files. The ranges are the application's
own; Python had none.

**Matching NumPy and SciPy.** The aim is the same frames, so the C++ follows
the reference's order of floating-point operations where that is cheap:

- `SosFiltFilt()` is SciPy's `sosfiltfilt()`: odd extension by its default
  padding length, steady-state initial conditions, and `sosfilt()`'s
  direct form II transposed. The sections, initial conditions and padding
  lengths of the two fixed filters are constants in `filter_constants.h`,
  written by `app/scripts/generate_filter_constants.py` from SciPy (`--check`
  compares).
- `FirFilter()` sums in `numpy.convolve()`'s order, which is how
  `lfilter()` filters with an FIR.
- `PairwiseSum()` and `Mean()` are NumPy's pairwise summation;
  `Percentile()` and `Median()` follow NumPy's linear method; the windows
  use NumPy's formulas; nearest hops round halves to even, as Python's
  `round()` does.
- The FFTs are pocketfft, which NumPy uses, with NumPy's normalisation.
- Dot products and BLAS-backed matrix products are summed in order. They
  cannot reproduce OpenBLAS's rounding, and do not need to (below).

**Acceptance** (met, plan §3.2):

- On all 243 corpus files with the four option sets in
  `app/golden/encoder/option_sets.tsv`, the frames are identical to the
  Python reference's for every encoding (972 of 972), against the
  criterion of 99%. No near-tie frames are needed: at a relative disturbance
  of 1e-9 the reference changes no frame on any file (§12).
- Every intermediate value of the corpus subset agrees with the reference
  to a relative 1e-9.
- Encoding takes 0.0104 s per second of speech on one core over the whole
  corpus (`benchmark.sh`), against the target of 0.05.

**Performance.** `ReferenceSynthesizer` runs the closed loop and the
limiter, about ten times per phrase. The pitch tracker does two
1024-point forward FFTs and one inverse per 5 ms hop. Neither has been
optimised: neither needs it.

---

## 9. Third-party libraries

| Need | Library | Licence | Status |
|---|---|---|---|
| GUI, threads | Qt 6 (Widgets, Concurrent, Multimedia) | LGPLv3 / GPLv3 | Core and Widgets linked; the rest in the Nix shell |
| Audio playback | Qt Multimedia `QAudioSink` | LGPLv3 | In the Nix shell |
| WAV read/write | libsndfile | LGPL-2.1+ | Linked by `s5core` |
| Sinc resampling | libsamplerate | BSD-2 | Linked by `s5core` |
| FFT | pocketfft (C++ header-only) | BSD-3 | Used by `s5core`; packaged in `flake.nix` |
| Project container | libzip | BSD-3 | In the Nix shell |
| Project metadata | yaml-cpp | MIT | In the Nix shell |
| PESQ | The project's own code | GPLv3-or-later | Planned |
| Logging | spdlog | MIT | Linked by `s5core` |
| Tests | Google Test with Google Mock | BSD-3 | Linked by the test binaries |

---

## 10. GUI, CLI and the core

Both executables are thin front ends over `s5core`.

**Implemented:**

- The GUI's widgets are the static library `speak5220_gui`, so the GUI
  tests link them; the `speak5220` executable adds only `main()`.
  `MainWindow` sets the title "Speak5220" and an initial size.
- `speak5220-cli` uses `QCoreApplication` and `QCommandLineParser`. It has
  `--help`, `--version` and the logging options of §11, and with no
  command prints `speak5220-cli <version>` to stdout.
- Both executables add the logging options with `AddLoggingOptions()`, read
  them with `LogSettingsFromParser()`, and exit with code 2 and a message on
  stderr for an unusable value.

**Planned** (plan §2.5): everything that produces a result is in `s5core`,
so the GUI and CLI give byte-identical exports. Editing operations are core
functions behind `IProjectEditor`, each returning a `Status` and what is
needed to reverse it. Undo/redo lives in the GUI (`gui/undo/`, one
`QUndoCommand` per operation). The CLI calls the editing operations directly
and has no undo.

---

## 11. Logging

**Implemented** (plan §2.6), in `core/util/logging`:

- Levels `info` (default), `debug` and `trace`; warnings and errors pass at
  every level.
- Destinations: a colour console sink on stderr (colour only on a
  terminal), a file sink (replacing the file), or both. Console lines are
  `[level] [module] message`; file lines add a timestamp and the thread.
- `ConfigureLogging()` installs the sinks; with a file, loggers are
  spdlog's asynchronous loggers on one worker thread, blocking rather than
  dropping when the queue is full. `ConfigureLoggingWithSinks()` installs
  given sinks synchronously (tests use a ringbuffer sink).
- `Logger(module)` returns the named logger for a module (`cli`, `gui`,
  `emulator`, `synth`, `audio`, `encoder`, …), created on first use with the current
  sinks. `ShutdownLogging()` flushes and stops the worker at exit.
- Options in both executables: `--log-level info|debug|trace`,
  `--log-file PATH`, `--no-console-log` (refused without `--log-file`).
- Not yet: the GUI preferences for these options (Phase 10).

---

## 12. Reference data

**Implemented** for the emulator, packaging and encoder (plan §1.4).
`app/golden/` holds what the retired references produced; the build and
tests read only these files and never run anything in `investigation/`.

- `app/scripts/golden/generate_golden.py`, run by hand in the default Nix
  shell, builds `tms5220emu` from `investigation/emulator/` into a temporary
  directory and imports the Python encoder. It refuses to run if either
  reference has local changes, and records the last commit that changed them
  and the SHA-256 of each source in `REFERENCE.tsv`. With `--check` it
  regenerates into a temporary directory and compares.
- `golden/emulator/`: `runs.tsv` lists each run (name, mode, input, power-up
  phase, and whether the Python synthesiser matched it); `runs/` holds each
  run's codes (one per line, from power-up) and full event log; `scenarios/`
  the scenario scripts (from `app/scripts/golden/scenarios/`, mirroring the
  C self-tests) and their images; `phrases/` each phrase's packed bytes and
  frame list. The phrases are a subset of the synthetic vectors (up to four
  words per set, and the whole "special" set) and six corpus phrases
  encoded with the encoder's defaults, from public-domain and CC0 voices.
  Each is spoken through both paths at phase 0, and a few at other phases.
- `golden/packaging/`: an Acorn PHROM and a TI indexed image of the corpus
  phrases and the "special" vectors, with their word tables and ASCII map,
  and each word's Speak External stream.
- `golden/encoder/`, written by `app/scripts/golden/encoder.py` from the
  Python encoder (same rules: refuses local changes, records the commit,
  the SHA-256 and the NumPy and SciPy versions, `--check` compares):
  - `inputs/`: WAV copies of an eight-file corpus subset (public-domain and
    CC0 voices only, one or two per category of prompt);
  - `intermediates/`: per subset file, `encodeDetailed()`'s intermediates
    with the default options, as JSON: the filtered signals (every 16th
    sample), the pitch track with CMNDF rows and candidates for six hops,
    each frame's analysis and own quantisation, the plan, the faint-opening
    trim, the shaped levels, the per-frame gains, the gain and the energies;
  - `frames/SET.tsv`: for each option set, every corpus file's frame count,
    near-tie frames and packed frames;
  - `option_sets.tsv`: the default set, and three fixed sets that between
    them change every option.
- **Near-tie frames.** A frame is a near tie if its coding changes when the
  input is disturbed at the level of floating-point noise. The script
  encodes each file as given and three times disturbed (scaled by 1 + 1e-9;
  plus white noise at 1e-9 of its RMS, twice with different seeds); a frame
  that differs in any run is listed, and a file whose frame count changes
  has every frame listed. Plan §3.2 accepts a difference only at such
  frames. With the current reference none are listed.
- The full corpus is not committed: `app/scripts/generate_corpus.py` writes
  its WAV copies (8 kHz, 16-bit, lossless from the FLAC originals) for
  `corpus-check.sh`. Where an encoding differs from the reference,
  `corpus_pesq.py` scores both with `investigation/tools/speechqual.py`
  (PESQ as P.862.1 MOS-LQO) until the application's own PESQ exists
  (Phase 3).
- The emulator's data is never regenerated unless the datasheet changes;
  the encoder's only for an intended improvement, from the C++ itself
  (AGENTS.md §3.1).
