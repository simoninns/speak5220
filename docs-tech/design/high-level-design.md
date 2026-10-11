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
| `s5core`: `util/` (Status, logging, version, numbers as text, SHA-256, task runners, file digests) | Implemented |
| `s5core`: `lpc/` (tables, frames, bit stream) | Implemented |
| `s5core`: `emulator/` (`Tms5220`, `Tms6100`, host procedures) | Implemented, matching the C reference sample for sample |
| `s5core`: `synth/` (`Synth`, `ReferenceSynthesizer`) | Implemented |
| `s5core`: `packaging/` (Acorn PHROM, TI indexed, Speak External) | Implemented |
| `s5core`: `audio/`, `dsp/` (WAV files and in-memory WAV, resampler, FFT adapters; filters, NumPy-ordered sums, LPC analysis) | Implemented |
| `s5core`: `encoder/` (`Encoder` and its stages, `EncoderOptions`) | Implemented, matching the Python reference's frames on the whole corpus |
| `s5core`: `quality/` (PESQ, the anchor, the cheap measures, `QualityScale`) | Implemented, matching the reference tools' values on the whole corpus |
| `s5core`: `project/` (`Project` and its editing operations, PHROM allocation, the encoding cache, the `.s5proj` file in ZIP and directory form, import, PHROM export) | Implemented |
| Reference data in `app/golden/` (emulator, packaging, encoder, quality) | Implemented |
| Fixture projects in `app/projects/` (`general/`, `phrom/`) | Implemented |
| `speak5220` GUI | Implemented: the main window, projects (open, save, recent files, the unsaved-changes prompt), the phrase list with PHROM dividers and drag and drop, phrase names and descriptions, undo/redo, the status bar (§15) |
| `speak5220-cli` | Implemented: `new`, `add`, `encode`, `export` (Acorn PHROM images), `--output`; version and logging options |
| Test tree and lanes (TESTING.md) | Implemented |
| Format, lint, pre-commit hooks, CI on Linux | Implemented |
| CI on Windows and macOS (release tags and manual runs) | Implemented; a manual run passes both lanes on both |
| Nix dev shells and package | Implemented |
| Optimiser, the other export formats, BBC export, the GUI's waveforms, playback, encoder panel and Export dialog | Planned |

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
                                benchmark.sh, generate_corpus.py,
                                generate_filter_constants.py (§8)
    golden/                     generate_golden.py and its scenario scripts,
                                encoder.py, quality.py (offline, run by
                                hand: §13)
  golden/                       reference data from the retired references (§13)
  projects/                     fixture projects, unpacked: general/, phrom/ (§14)
  src/
    core/                       library "s5core" (QtCore at most)
      util/                     Status/StatusOr, logging, logging options, version,
                                number text, SHA-256, task runners, file digests
      lpc/                      coding tables, Frame, FrameCodec, BitReverse
      emulator/                 Tms5220, Tms6100, ChipDriver, interfaces
      synth/                    Synth, ReferenceSynthesizer
      packaging/                AcornPhrom, TiIndexed, SpeakExternalStream
      audio/                    WAV interfaces, the libsndfile adapters, the
                                24-bit grid of project audio
      dsp/                      resampler and FFT interfaces and adapters;
                                SOS/FIR filters and their constants,
                                NumPy-ordered sums and windows, LPC analysis
      encoder/                  Encoder and its stages, EncoderOptions (§8)
      quality/                  PESQ, PesqMetric, the output alignment, the
                                anchor vocoder, the cheap measures,
                                QualityScale (§9)
      project/                  Project, IProjectEditor, the PHROM allocator,
                                EncodingCache, ProjectSerializer, the project
                                stores, PhraseImporter, the PHROM export (§14)
    gui/                        library "speak5220_gui" + the "speak5220" executable
      main_window/              MainWindow, ProjectDocument, IDialogs, the
                                recent-file list, the status bar's text
      phrase_list/              PhraseListModel, its delegate and panel
      phrase_editor/            PhraseDetails (name and description)
      undo/                     one QUndoCommand per editing operation,
                                ProjectEdits
    cli/                        the "speak5220-cli" executable and its commands
  tests/
    .clang-tidy                 the app's settings, plus one test exception
    support/                    speak5220_test_support (helpers, mocks, golden readers)
    unit/  functional/          speak5220_unit_tests, speak5220_functional_tests
    gui/support/                speak5220_gui_test_main (QApplication, offscreen)
    gui/unit/  gui/functional/  speak5220_gui_unit_tests, speak5220_gui_functional_tests
```

Planned additions, each created with its first code (plan §2.1):

- `src/core/`: `bbc/`;
- `src/gui/`: `waveform/`, `encoder_panel/`, `export/`;
- `app/projects/bbc/` and `app/projects/benchmark/` (fixture projects),
  `app/resources/` (icons, `.desktop`, metainfo), `app/packaging/` (Flatpak,
  WiX/CPack, macOS);
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
- There is no version number. The version is the short (7-character) git
  commit hash, with `-dirty` appended when tracked files have uncommitted
  changes (untracked files do not count). The target `s5version` runs
  `cmake/GitVersion.cmake` at every build, so the version follows commits
  made without reconfiguring; it rewrites the generated
  `core/util/version_string.h` (the constant `kVersionString`) only when the version changes. The cache
  variable `SPEAK5220_GIT_VERSION` overrides git: the Nix package, whose
  sources carry no `.git`, sets it from the flake's `shortRev` or
  `dirtyShortRev`, which it also uses as the package version. Outside a git
  checkout the version is `unknown`.
- `SPEAK5220_BUILD_TESTS` (default `ON`) adds `app/tests/`.
- `cmake/Dependencies.cmake` finds the libraries of §10: Qt 6 (Core,
  Widgets), spdlog, yaml-cpp and libzip by their CMake packages; libsndfile and libsamplerate
  by header and library (their pkg-config files name private dependencies a
  shared build does not need); pocketfft by its header, as an interface
  target that defines `POCKETFFT_NO_MULTITHREADING`. Google Test is found by
  `app/tests/`.
- GCC reports a false `-Wmaybe-uninitialized` inside pocketfft's templates
  at `-O3`; a GCC-only pragma silences it around pocketfft's `#include` in
  `dsp/fft.cpp`.
- `format` and `lint` targets wrap `app/scripts/format.sh` and
  `app/scripts/lint.sh`.
- **Windows and macOS:** Qt from aqtinstall, pinned to the flake's Qt
  version (6.11.2); the other libraries from vcpkg in manifest mode
  (`app/vcpkg.json`, pinned by `builtin-baseline`; libsndfile and libzip
  without their optional compression and encryption features), through
  vcpkg's CMake toolchain file.
- The root `.gitattributes` stops git converting line endings in the
  reference data: `.bin`, `.wav`, `.flac` and `.ssd` files are binary, and
  the text files in `app/golden/` and `app/projects/` keep LF. Without it, a Windows checkout
  (`core.autocrlf=true`) turns LF bytes in small binaries into CR LF.

**Nix** (`flake.nix`):

- `devShells.app`: the application's libraries, Google Test, Qt tools, the
  linters, and the pre-commit hooks (installed by its `shellHook`). It sets
  `QT_PLUGIN_PATH` so the GUI built in `build/app` runs.
- `devShells.default`: the research shell (KiCad, Pico SDK, Python), plus
  everything in `devShells.app`. The golden-data scripts run here (§13).
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
each run. A manually started run passes both lanes on both platforms (plan
Phase 2 task 7).

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
    applies the acceptance rule of §8;
  - `quality_golden`: readers for `app/golden/quality/` (each reference
    encoding's lag, output length, PESQ and cheap measures; the anchor
    scores);
  - `synth_mocks.h`: a gMock mock of `ISynth`;
  - `synthetic_speech`: a deterministic speech-like signal (two vowels, a
    fricative, pauses), a delay and white noise, for the quality unit
    tests;
  - `project_mocks.h`: gMock mocks of `IEncoder`, `IWavReader`,
    `IWavCodec`, `IResampler`, `IFileDigest`, `IProjectStore` and
    `IProjectEditor`;
  - `project_builder`: fixed phrase ids, frames that pack to a chosen
    number of bytes, and phrases and projects built from them.
- The functional lane has the CLI's path (`SPEAK5220_CLI_PATH`), the
  reference data's directory (`SPEAK5220_GOLDEN_DIR`) and the fixture
  projects' directory (`SPEAK5220_PROJECTS_DIR`) compiled in, and links
  libzip (to build unsafe archives for the store's tests).
- **By-hand tests.** A few functional tests carry Google Test's `DISABLED_`
  prefix, so CTest lists them as disabled and never runs them. The scripts
  run them with `--gtest_also_run_disabled_tests`, passing inputs and an
  output file through environment variables: the full-corpus check
  (`EncoderCorpusCheck` and `QualityCorpusCheck`, `corpus-check.sh`) and
  the timings (`EncoderBenchmark` and `QualityBenchmark`, `benchmark.sh`).
- GUI test binaries use `speak5220_gui_test_main`, which selects the
  offscreen platform unless `QT_QPA_PLATFORM` is already set, then creates a
  `QApplication` and runs Google Test. `app/tests/gui/support/gui_mocks.h`
  has a gMock mock of `IDialogs`, an in-memory `IRecentFilesStore`, and
  `IdsAre()`, a matcher for spans of phrase ids. The GUI functional lane
  has `SPEAK5220_PROJECTS_DIR` compiled in.

---

## 6. Main components

Plan §2.2, with the names as built.

| Component | Status and responsibility |
|---|---|
| `Status`, `StatusOr<T>` | **Implemented** (`core/util/status`): error values in place of exceptions, modelled on Abseil's |
| Logging | **Implemented** (`core/util/logging`, `logging_options`): §12 |
| `Version()`, `kApplicationName` | **Implemented** (`core/util/version`) |
| Coding tables | **Implemented** (`core/lpc/coding_tables.h`): datasheet §8.1 Tables 8-4 to 8-7, `constexpr` |
| `Frame`, `FrameCodec`, `BitReverse()` | **Implemented** (`core/lpc/`): frame kinds and lengths, validation, `Pack()`/`Parse()` with the §8.1/§12.1 bit order |
| `Tms5220`, `Tms6100`, `ChipDriver` | **Implemented** (`core/emulator/`): §7 |
| `Synth` (`ISynth`) | **Implemented** (`core/synth/synth`): §7 |
| `ReferenceSynthesizer` | **Implemented** (`core/synth/reference_synthesizer`): the encoder's reference-algorithm synthesiser (`synthesize()` in the Python), which matches the emulator's VSM path |
| `AcornPhrom`, `TiIndexed`, `SpeakExternalStream` | **Implemented** (`core/packaging/`): the §12.8 and §12.9 layouts, byte-identical to the Python encoder's |
| `SndfileWavReader`, `SndfileWavWriter` (`IWavReader`, `IWavWriter`) | **Implemented** (`core/audio/`): any WAV variant read and mixed to mono as floats; 16- or 24-bit PCM written, clipped |
| `SndfileWavCodec` (`IWavCodec`) | **Implemented** (`core/audio/sndfile_wav_io`): the same, to and from bytes in memory (libsndfile's virtual I/O), with integer PCM read and 24-bit PCM written exactly (§14) |
| `SincResampler` (`IResampler`) | **Implemented** (`core/dsp/resampler`): libsamplerate `SRC_SINC_BEST_QUALITY` |
| `PocketFft` (`IFft`) | **Implemented** (`core/dsp/fft`): real FFTs with NumPy's conventions |
| Filters, sums, LPC analysis | **Implemented** (`core/dsp/iir_filter`, `array_math`, `lpc_analysis`, `filter_constants.h`): §8 |
| `MainWindow`, `ProjectDocument` (`IDialogs`, `IRecentFilesStore`) | **Implemented** (`gui/main_window/`): menus, projects, imports, the status bar (§15) |
| `PhraseListModel`, `PhraseListDelegate`, `PhraseListPanel` | **Implemented** (`gui/phrase_list/`): the phrase list (§15) |
| `PhraseDetails` | **Implemented** (`gui/phrase_editor/`): name and description (§15) |
| `ProjectEdits` and the undo commands | **Implemented** (`gui/undo/`): one `QUndoCommand` per `IProjectEditor` operation (§15) |
| `Encoder` (`IEncoder`) | **Implemented** (`core/encoder/`): re-implementation of `encode()`/`encodeDetailed()` with the full `EncoderOptions` set (§8) |
| `ScorePesq()`, `PesqMetric`, `LevelErrorMetric`, `SpectralDistanceMetric` (`IQualityMetric`), `AnchorScore()`, `QualityScale` | **Implemented** (`core/quality/`): PESQ (P.862 narrowband, as P.862.1 MOS-LQO), the cheap measures, the anchor and the quality scale (§9) |
| `Optimiser` | Planned: searches encoder options for the best PESQ, within the 64-PHROM project budget |
| `Project` (`IProjectEditor`) | **Implemented** (`core/project/project`): phrases, their audio, settings and cached encodings; the editing operations (§14) |
| `AllocatePhroms()` | **Implemented** (`core/project/phrom_allocator`): list-order assignment to PHROMs and the 64-PHROM budget (§14) |
| `EncodingCache` | **Implemented** (`core/project/encoding_cache`): the settings hash and re-encoding within the budget, over `IEncoder` (§14) |
| `ProjectSerializer`, `ZipProjectStore`, `DirectoryProjectStore` (`IProjectStore`) | **Implemented** (`core/project/`): the `.s5proj` file (§14) |
| `PhraseImporter` | **Implemented** (`core/project/phrase_importer`): plan §3.1 over `IWavReader`, `IResampler` and `IFileDigest`, inline or through `ITaskRunner` (§14) |
| `InlineTaskRunner`, `ThreadPoolTaskRunner` (`ITaskRunner`) | **Implemented** (`core/util/task_runner`): background jobs (§14) |
| `RunExport()`, `BuildAcornPhroms()` | **Implemented** (`core/project/project_export`): a project's Acorn PHROM images (§14) |
| TI, raw `.bin`, C header and WAV export | Planned |
| `BbcExport` | Planned: tokenised BBC BASIC program plus data files on an Acorn DFS `.ssd` image |

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
`app/golden/emulator/` (§13), codes sample for sample and events line for
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
  of 1e-9 the reference changes no frame on any file (§13).
- Every intermediate value of the corpus subset agrees with the reference
  to a relative 1e-9.
- Encoding takes 0.0104 s per second of speech on one core over the whole
  corpus (`benchmark.sh`), against the target of 0.05.

**Performance.** `ReferenceSynthesizer` runs the closed loop and the
limiter, about ten times per phrase. The pitch tracker does two
1024-point forward FFTs and one inverse per 5 ms hop. Neither has been
optimised: neither needs it.

---

## 9. Quality measurement

**Implemented** (plan §3.3, Phase 3). `core/quality/` measures how well an
encoding reproduces its source, as the retired tools did:
`investigation/tools/speechqual.py` (PESQ and the anchor) and
`investigation/encoder/evaluate.py` (the cheap measures). Each C++ function
names its Python original, or the ITU-T section it implements.

| Component | Files | Does |
|---|---|---|
| `ScorePesq()` | `pesq`, `pesq_stages.h`, `pesq_alignment`, `pesq_model`, `pesq_fft`, `pesq_tables.h` | ITU-T P.862 narrowband at 8 kHz, mapped to MOS-LQO by P.862.1: level alignment, IRS receive filtering, voice activity, time alignment with utterance splitting, the auditory transform, the disturbance densities, bad-interval realignment and the aggregation |
| `PesqMetric` (`IQualityMetric`) | `pesq_metric` | Speaks the frames through `ISynth`'s VSM path, cuts the output to its sound (idle at 0), lines it up with the source after the 60 Hz high-pass (`AlignOutput()`), and scores it with `ScorePesq()` |
| `AlignOutput()` | `output_alignment` | `speechqual.py`'s `align()` (level envelopes in 2.5 ms steps), `refine()` (±25 ms by mel-cepstral distance), `placed()` and `matchLevel()` |
| `MeasureCheap()`, `LevelErrorMetric`, `SpectralDistanceMetric` | `cheap_measures` | `evaluate.py`'s level error (`lvMeanAbs`, `lvBias`, `lvP90`) and mel spectral distance (`specDist`), comparing each frame's input with `ReferenceSynthesizer`'s output where the frame's values are reached; no alignment search |
| `AnchorVocoder()`, `AnchorScore()` | `anchor_vocoder`, `numpy_normal`, `normal_ziggurat_tables.h` | `speechqual.py`'s `lpcVocoder()` and its pitch tracker, and the PESQ of its output: the ceiling of the quality scale |
| `QualityScale` | `quality_scale` | A score's position between PESQ's floor and the phrase's anchor, and its band |

`IQualityMetric::Measure(source, encoding)` takes the source as given to
the encoder and its `EncodeResult`; `Direction()` says whether larger values
are better (PESQ) or smaller (the cheap measures).

**Where PESQ comes from** (plan decision D6). The Recommendation describes
each stage but leaves most of its numbers and details to the ANSI-C
reference implementation, which Annex A makes part of P.862 and gives
precedence over the text: the Bark bands, the hearing thresholds, the
filter curves and coefficients, the scaling factors, the thresholds of the
voice activity detector and the alignment, and the weights of the score.
The C++ is the project's own code, written to this project's rules and
licensed GPL-3.0-or-later. Its constants and the details the text leaves
open are taken from the reference implementation (as the Python `pesq`
0.0.4 package bundles it: the 2005 code of Amendment 2), and each stage
cites its section of P.862. The ITU's texts, code and conformance data are
not in the repository.

**Matching the reference.** A PESQ score is the outcome of many threshold
decisions, so the C++ keeps the reference's arithmetic:

- its own radix-2 FFT in single precision (`pesq_fft`), the reference's
  order of operations, rather than pocketfft;
- single precision wherever the reference uses it, the same promotions to
  double, and the same order of evaluation;
- the reference's shared scratch buffer, laid out as it is: in the
  utterance split, a long utterance's crude alignment overwrites the
  window of the fine alignment that follows, and the scores depend on it;
- both inputs divided by their joint peak and rounded to single precision,
  as the Python package does before calling the C code.

**The anchor.** The anchor vocoder is order 10, 25 ms frames, K and gain
interpolated over 8 steps, a pulse per pitch period or Gaussian noise, pre-
and de-emphasis 0.9375, nothing quantised. Its noise must be the
reference's sample for sample: on corpus files, other noise moves the
anchor's PESQ by up to 0.07. `NumpyNormal` therefore reproduces NumPy's
default generator (PCG64, XSL-RR 128/64, from the state of
`numpy.random.default_rng(1)`) and its ziggurat normal sampler, with
NumPy's tables, bit for bit.

**The quality scale** (plan §3.3, "Presenting PESQ to the user"). The
position is (score − 1.0) / (anchor − 1.0), clamped to [0, 1]; an encoding
at or above its anchor is at the top. The bands, with plain-language labels
for the GUI:

| Band | Position |
|---|---|
| Bad | below 0.40 |
| Poor | 0.40 to below 0.60 |
| Fair | 0.60 to below 0.75 |
| Good | 0.75 to below 0.90 |
| As good as LPC can achieve | 0.90 and above |

The thresholds were calibrated on the reference encodings of the whole
corpus (243 files; anchors from 1.83 to 3.60, median 2.29):

| Option set | Median PESQ | Median position | Bad | Poor | Fair | Good | As good as LPC |
|---|---|---|---|---|---|---|---|
| default | 2.03 | 0.78 | 0% | 11% | 28% | 44% | 18% |
| lean | 1.95 | 0.73 | 1% | 13% | 44% | 33% | 9% |
| loud | 1.98 | 0.74 | 1% | 16% | 35% | 37% | 12% |
| plain | 1.97 | 0.73 | 0% | 14% | 43% | 34% | 9% |

With the defaults the typical encoding is *Good*, and 89% are *Fair* or
better; the other sets, each of which changes several options away from
their defaults, sit a little lower. The plan asks for the bands to be
confirmed by listening; that has not been done yet.

**Acceptance** (met, plan Phase 3):

- PESQ: on all 972 reference encodings of the corpus (243 files, four
  option sets), the same score as the Python package to the last bit of
  its single-precision result, against ±0.01; the alignment lag and the
  output length are the same for every encoding.
- The anchor: the same score on all 243 files, against ±0.02.
- The cheap measures: within 1e-6 of `evaluate.py`'s on every encoding.
- The corpus calibration above places the default encodings' median in
  *Good* (a functional test checks this from the reference data).
- Beyond the plan: the C++ PESQ, run locally on the ITU's 8 kHz conformance
  pairs for variable delay (Annex A, test 2(b), 40 pairs), gives the
  reference's raw score for every pair to the three decimals the ITU
  lists. The data is the ITU's and is not committed; Supplement 23 (test
  1(b)) was not available.

**Performance** (`benchmark.sh`, one core, the eight-file corpus subset,
seconds per second of speech): PESQ alone 0.0076; `PesqMetric` (synthesis,
alignment and PESQ) 0.047; the anchor score 0.012; the cheap measures
0.0013. Most of `PesqMetric`'s time is the output alignment's spectral
search (81 candidate lags). Nothing has been optimised yet; Optimise
(Phase 7) will show whether it needs to be.

---

## 10. Third-party libraries

| Need | Library | Licence | Status |
|---|---|---|---|
| GUI, threads | Qt 6 (Widgets, Concurrent, Multimedia) | LGPLv3 / GPLv3 | Core, Gui (`QUndoStack`) and Widgets linked; the rest in the Nix shell |
| Audio playback | Qt Multimedia `QAudioSink` | LGPLv3 | In the Nix shell |
| WAV read/write | libsndfile | LGPL-2.1+ | Linked by `s5core` |
| Sinc resampling | libsamplerate | BSD-2 | Linked by `s5core` |
| FFT | pocketfft (C++ header-only) | BSD-3 | Used by `s5core`; packaged in `flake.nix` |
| Project container | libzip | BSD-3 | Linked by `s5core` |
| Project metadata | yaml-cpp | MIT | Linked by `s5core` (public: the migration interface takes a `YAML::Node`) |
| PESQ | The project's own code (`core/quality/`), its constants and details from the ITU reference implementation (§9) | GPLv3-or-later | Implemented |
| Logging | spdlog | MIT | Linked by `s5core` |
| Tests | Google Test with Google Mock | BSD-3 | Linked by the test binaries |

---

## 11. GUI, CLI and the core

Both executables are thin front ends over `s5core`.

**Implemented:**

- The GUI's widgets are the static library `speak5220_gui`, so the GUI
  tests link them; the `speak5220` executable adds only `main()`, which
  creates the real `QtDialogs`, `SettingsRecentFilesStore` and
  `ThreadPoolTaskRunner`, and opens `speak5220 [PROJECT]` if given. The
  window is described in §15.
- `speak5220-cli` uses `QCoreApplication` and `QCommandLineParser`. It has
  `--help`, `--version` and the logging options of §12, and with no
  command prints `speak5220-cli <version>` to stdout.
- Its commands (`cli/cli_commands`) are `new`, `add`, `encode` and
  `export`, each a thin sequence of core calls: `MakeProjectStore()` and
  `ProjectSerializer` to open and save, `PhraseImporter` and
  `Project::InsertPhrases()` to add, `EncodingCache::Refresh()` to encode,
  `RunExport()` to export. `--output DIR` writes the project (or the
  exported files) into a directory of the caller's choosing, so the fixture
  projects can be encoded and exported without being changed (TESTING.md).
  It imports sequentially and encodes inline: there is nothing to wait for.
  Exit codes: 0 success, 1 a failed command, 2 an unusable command line.
- Both executables add the logging options with `AddLoggingOptions()`, read
  them with `LogSettingsFromParser()`, and exit with code 2 and a message on
  stderr for an unusable value.

Everything that produces a result is in `s5core`, so the GUI and CLI will
give byte-identical exports. The editing operations are core functions
behind `IProjectEditor` (§14), each returning what is needed to reverse it;
the CLI calls them directly and has no undo.

The GUI's undo/redo wraps the same operations (§15).

---

## 12. Logging

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
  `emulator`, `synth`, `audio`, `encoder`, `quality`, `project`, `export`, …), created on first use with the current
  sinks. `ShutdownLogging()` flushes and stops the worker at exit.
- Options in both executables: `--log-level info|debug|trace`,
  `--log-file PATH`, `--no-console-log` (refused without `--log-file`).
- Not yet: the GUI preferences for these options (Phase 10).

---

## 13. Reference data

**Implemented** for the emulator, packaging, encoder and quality measures
(plan §1.4).
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
  `corpus-check.sh`. Where an encoding differs from the reference, the
  encoder's corpus check scores both with the application's own PESQ
  (§9).
- `golden/quality/`, written by `app/scripts/golden/quality.py` from the
  retired quality tools (same rules; it also records the `pesq` package's
  version and the anchor's seed), for the reference encodings of
  `golden/encoder/frames/`:
  - `scores/SET.tsv`: for each option set and corpus file, the alignment
    lag and output length, PESQ as `speechqual.py` measures it, and
    `evaluate.py`'s level error and spectral distance;
  - `anchor.tsv`: each corpus file's anchor score (`speechqual.py
    --anchor`).
- The emulator's data is never regenerated unless the datasheet changes;
  the encoder's only for an intended improvement, from the C++ itself
  (AGENTS.md §3.1).

---

## 14. The project

**Implemented** (plan §3.1, §3.4, §4, Phase 4) in `core/project/`, with
`core/util/task_runner`, `file_digest`, `sha256` and `number_text`, and
`core/audio/pcm24.h`. The file format is documented for users in
[`docs/application/reference/project-file.md`](../../docs/application/reference/project-file.md).

**Model.** `Phrase` is a plain struct: a UUID id, name, description,
optional `SourceInfo`, the 8 kHz audio as floats, its `EncoderOverrides`
(option field name → text), `locked`, `play_start` and an optional
`CachedEncoding` (frames, encoder version, settings hash, cached PESQ and
anchor). `ProjectContents` holds the `ProjectSettings` (name, header
strings, ASCII map, default `EncoderOptions`, per-PHROM header strings), the
phrases in PHROM order, the export presets, and whatever the file held that
this version does not know. `Project` wraps it and keeps the rules: valid,
unique ids and names, consistent phrases (audio and source together; a
phrase without audio must be locked; frames valid; overrides valid against
the defaults). `Project::FromContents()` checks them when a file is read,
but not the budget, so an over-budget file still opens.

**Editing operations** (`IProjectEditor`, plan §2.5). Each operation checks
everything first and changes nothing on failure, and returns its reverse:

| Operation | Returns | Reverse |
|---|---|---|
| `InsertPhrases(placed)` | — | `RemovePhrases(ids)` |
| `RemovePhrases(ids)` | the phrases with their indices | `InsertPhrases(result)` |
| `MovePhrases(ids, destination)`, `SortPhrases()`, `SetOrder(order)` | the previous order | `SetOrder(result)` |
| `RenamePhrase(id, name)` | the previous name | `RenamePhrase(id, result)` |
| `SetDescription(id, text)` | the previous text | `SetDescription(id, result)` |
| `SetEncoding(id, {overrides, encoding})` | the previous pair | `SetEncoding(id, result)` |

`SetEncoding()` takes the settings and the encoding made from them
together, so a settings change and its re-encoding are one undoable step,
and the encoding can be computed in the background before the step is
applied. A locked phrase refuses it. Renaming does not change the ASCII map,
which names phrases by name, so a rename and its reverse are exact.

**PHROM allocation** (plan §4.1). `AllocatePhroms()` takes each phrase's
cost (or none, if not encoded) and fills PHROMs in list order, never
back-filling; a phrase larger than 16 128 bytes is placed nowhere. The
fixed overhead of 256 bytes per PHROM and the cost of packed bytes + name +
2 match `AcornPhrom::Build()` exactly (header 64, ASCII pointers 190, the
0xFF, the null word). PHROMs beyond the budget are still counted, so a
check can say how many a change would need. The packed size is counted from
the frame lengths (`PackedSize()`), not by packing, since every edit
recomputes every phrase's cost.

**The budget** (decision D4). Every editing operation refuses a result that
needs more PHROMs than the budget (64), or than the project already uses if
it is over budget (so such a project can still shrink). The plan names
imports, re-encoding and the optimiser; renaming and reordering are checked
as well, because both change costs or packing, and a project inside the
budget then stays inside it whatever is done to it.

**Encoding cache** (plan §3.4). `EncodingCache::SettingsHash()` is the
SHA-256 of the encoder version, every option's text in
`EncoderOptionInfos()` order (the options in effect, so an override equal
to a new default keeps an encoding current), and the audio's 24-bit codes.
`IsCurrent()` compares it, and the version, with the stored encoding; a
locked phrase is always current. `ChangeSettings()` encodes with the new
overrides and stores them through `SetEncoding()`, which refuses a result
over the budget, leaving the previous settings and encoding (the message
says so). `Refresh()` encodes every stale phrase in list order, storing
each as it is made, so the budget is checked phrase by phrase and a failure
does not stop the rest. `encoder::kEncoderVersion` (now `1.0.0`) changes
whenever the encoder's output does, in the commit that regenerates its
reference data.

**Audio on the 24-bit grid.** A project stores audio as 24-bit PCM, so the
importer rounds the 8 kHz samples to codes / 2^23 (`QuantiseToPcm24()`),
which floats hold exactly. `SndfileWavCodec` writes those codes and reads
integer PCM as integers, so a saved and reloaded phrase has the same
samples bit for bit, and its settings hash does not change. (libsndfile's
own float-to-24-bit conversion scales by 2^23 − 1, which moves codes near
full scale.) `SndfileWavReader` now reads integer PCM the same way, which
gives the same values as before.

**Serialisation** (`ProjectSerializer`, over yaml-cpp and `IWavCodec`):

- Writing goes through `YAML::Emitter`. Every string is double-quoted;
  numbers are written as text made by `FormatNumber()` (shortest exact,
  locale-independent) or `std::to_string`, never through yaml-cpp's stream
  output, which follows the global locale. Booleans are `true`/`false`,
  unset values `null`. Options are written in `EncoderOptionInfos()` order,
  `default_encoder` in full. Frames are one flow mapping each, with only
  the fields their kind carries.
- Reading is type-checked: numbers and booleans must be plain scalars,
  numbers are parsed by `ParseNumber()`/`ParseInteger()` (a stream in the
  classic locale, since `std::from_chars` for `double` is missing from some
  supported standard libraries), every value is range-checked, repeated keys
  are refused, and messages name the file and the key path
  (`project.yaml: phrases[2].play_start must be a whole number`). yaml-cpp's
  exceptions are caught at this boundary.
- **Unknown keys** are kept per mapping as (key, YAML text), and **unknown
  files** per phrase directory and per project. yaml-cpp drops a scalar's
  quoting when it writes a loaded node, so `EmitNode()` writes them back
  itself, quoted scalars quoted and flow collections in flow style.
- **Versions.** `format` must be `speak5220-project`; `version` is migrated
  by `MigrateProjectDocument()` through `ProjectMigrationSteps()` (none yet:
  version 1 is the first), and a newer version is refused.
- Phrase ids must be canonical lower-case UUIDs before they are used as
  paths.

The YAML keys are the snake_case C++ field names (plan §4), so
`EncoderOptionInfo` gained `field` (`target_rms`), with `key` kept as the
reference encoder's name (`targetRms`), which its documentation and the
reference data use. `SetEncoderOption()` and `EncoderOptionText()` convert
one option to and from text, for project files and later the CLI and GUI.

**Stores** (`IProjectStore`: `Read()` and `Write()` of the whole file set,
paths checked by `IsSafeProjectPath()`; `MakeProjectStore()` picks the form
by the `.s5proj` extension):

- `ZipProjectStore` (libzip): `zip_close()` writes a temporary file and
  renames it over the project only when complete; on failure the archive is
  discarded and the old file is untouched. The old file is first copied to
  `NAME.s5proj~`. Entries are written in path order with a fixed DOS date
  (1980-01-01), so the same project gives the same bytes. `SetCancelCheck()`
  installs libzip's cancel callback, which abandons a save part-way: a long
  save can be cancelled, and the functional tests use it to simulate a
  crash. Reading refuses unsafe names (directory entries included),
  duplicates, more than 100 000 entries or 1 GB.
- `DirectoryProjectStore`: writes `NAME.saving` completely, renames the old
  directory to `NAME~` and the new one into place (and back, if that
  fails). It never replaces a directory that holds files but no
  `project.yaml`, and refuses symbolic links when reading.

**Import** (`PhraseImporter`, plan §3.1): `IWavReader` reads and mixes to
mono; `IResampler` converts to 8 kHz unless the file already is; a peak
≥ 0.999 counts as clipping and < 0.0316 (−30 dBFS) as very quiet, and
either is scaled to a peak of 0.891 (−1 dBFS) only if the request asks
(the GUI asks the user first, the CLI has `--normalise`); the samples are
quantised to the 24-bit grid; `IFileDigest` gives the source's SHA-256. A
name is normalised from the request or made from the file name
(`SuggestPhraseName()`). `ImportInBackground()` posts the import to an
`ITaskRunner` and passes the result to a callback on the worker thread.

**Task runners** (`ITaskRunner`: `Post()` and `Wait()`). `InlineTaskRunner`
runs each job inside `Post()` (unit tests, the CLI); `ThreadPoolTaskRunner`
uses a `QThreadPool` of its own. The plan names QtConcurrent; a QtCore
thread pool does the same without linking Qt Concurrent into `s5core`.

**Names** (decision D2). `NormalisePhraseName()` converts lower case, trims
spaces, and refuses an empty name, more than 16 characters, or a character
outside A–Z, 0–9, space and `' ( ) , - .`, naming the character.
`CIdentifiersForNames()` derives the C identifiers for the header exports
(plan §4), with `_2`, `_3`, … for clashes; nothing uses them until the C
header export (Phase 8).

**Export** (`RunExport()`). Only the `acorn-phrom` format exists so far,
built from the allocation: one `AcornPhrom::Build()` per PHROM, with the
project's header strings and the PHROM number appended to the second (or
that PHROM's own strings), and the ASCII entries whose phrase the PHROM
holds. Layouts `separate` (`NAME_1.bin`, …) and `concatenated`
(`NAME.bin`). A phrase with no frames, or too large for a PHROM, fails the
export; the CLI also refuses stale encodings. The other formats and the
Export dialog are Phase 8.

**Fixtures** (`app/projects/`, unpacked; TESTING.md): `general/one-phrase`,
`general/many-phrases` (Acorn-style names with punctuation, an ASCII map, an
override, two presets), `general/frame-kinds` (a locked phrase with every
frame kind), `phrom/one-phrom` (exactly 16 384 bytes) and
`phrom/two-phroms` (spills into a second PHROM). The phrases with audio were
added and encoded with `speak5220-cli` from synthetic clips; the locked
frames were written directly. A functional test checks that each loads,
that its encodings are current, that its presets run, and that saving it
gives back its own files byte for byte. The 64-PHROM project is built by
its test instead: as YAML it would be about 12 MB.

**Performance.** Saving and reloading the 64-PHROM project (about 165 000
frames, 12 MB of YAML) takes about 4.4 s, nearly all of it yaml-cpp; a full
PHROM loads and exports in about 45 ms. Typical projects of a few PHROMs
open well within a second, so nothing has been optimised.

**Acceptance** (plan Phase 4, met):

- unit tests for every editing operation and its reverse; every name of the
  Acorn PHROM A word list (as listed and as stored) is accepted, and lower
  case and other characters are handled as specified;
- unit tests for a project that fills exactly one PHROM, one that spills
  into a second, a phrase too large for an empty PHROM, and an addition
  that needs a 65th PHROM;
- unit tests with a mocked `IEncoder`: a settings change re-encodes; one
  that would exceed the budget keeps the previous encoding and says why;
- unit tests with a mocked store and codec (quoted strings, unknown keys
  kept, version migration); functional tests: the ZIP and directory forms
  load identically, an interrupted save leaves the previous file intact,
  and a project saved under a decimal-comma locale reloads identically;
- unit tests of the import with a mocked reader and resampler; functional
  tests import 44.1 kHz stereo and 8 kHz mono files;
- a functional test creates a project with the CLI, adds and encodes a
  phrase, saves, and reloads it identically;
- the fixtures load in functional tests, and `mkdocs build --strict`
  passes.

---

## 15. The main window

**Implemented** (plan §2.5, §5, Phase 5) in `app/src/gui/`. The user's view
is [`docs/application/user-manual/main-window.md`](../../docs/application/user-manual/main-window.md).

**Structure.**

```
MainWindow ── IDialogs, IRecentFilesStore, ITaskRunner (injected)
 ├── ProjectDocument: Project + QUndoStack + ProjectEdits + path
 ├── PhraseListModel (reads a Project snapshot, edits through ProjectEdits)
 │    └── PhraseListPanel: QListView + PhraseListDelegate + buttons
 ├── PhraseDetails (name, description; edits through ProjectEdits)
 └── status bar: ProjectSizeText(Project::Allocation())
```

- **`ProjectDocument`** owns the core `Project`, the `QUndoStack` and the
  path. It is modified while the stack is away from its clean index, and
  emits `Changed()` on every index change (edit, undo, redo) and when a
  project is opened or started. Opening and saving use
  `MakeProjectStore()` and `ProjectSerializer`, as the CLI does. Opening a
  `project.yaml` opens its directory. Save keeps the project's form; Save
  As always writes a `.s5proj` file, adding the extension, and gives a
  project with no name the file's stem (outside the undo stack, since the
  project is saved with it at once).
- **Undo** (`gui/undo/`). `ProjectCommand` is a `QUndoCommand` whose
  `Execute()` runs the operation the first time; `ProjectEdits` pushes the
  command only if that succeeded, and the stack's own first `redo()` is
  skipped. A failed operation therefore never reaches the stack, and does
  not discard the redo history either, which `QUndoStack::push()` would do
  even for a command that makes itself obsolete. Each of the seven
  commands applies its `IProjectEditor` operation in `redo()` and the
  returned reverse in `undo()` (table in §14). An undo or redo that fails
  regardless is logged and the command made obsolete, so the stack drops
  it.
- **`PhraseListModel`** is a flat list model of a snapshot: the main
  window calls `SetProject()` on every `Changed()`, which resets the model.
  `PhraseListPanel` remembers the selected ids before a reset and selects
  them again after, so the selection follows phrases that move. Rows carry
  roles for the id, the encoding state, the placement, the cost, the PHROM
  and word number, and `kPhromStartRole` for the first phrase of each
  PHROM after the first, taken from `PhromAllocation::phroms`. The
  delegate draws that row taller, with a "PHROM N" band; a phrase too
  large for a PHROM has a red foreground. Whether an encoding is current
  (`EncodingCache::IsCurrent()`, which hashes the audio) is remembered per
  phrase, keyed by the default options, the phrase's overrides, the stored
  hash and version, and the audio's length, so an edit does not hash every
  phrase again; no operation changes a phrase's audio once imported.
- **Edits from the list.** Move Up puts the selected phrases, in order,
  before the row above the first of them (`MovePhrases(ids, first − 1)`);
  Move Down after the row below the last (`last + 2`). Dragging rows sends
  their ids under `application/x-speak5220-phrase-ids`, and dropping them
  is one `MovePhrases()`; a drop inside or beside the dragged block makes
  no command. The model implements neither `moveRows()` nor `removeRows()`,
  so `QListView` falls back to `dropMimeData()` and never removes the
  source rows itself. Dropped local files emit `FilesDropped()`. Failed
  edits emit `EditFailed()`, which the window shows through `IDialogs`.
- **`PhraseDetails`.** `FilterNameEdit()` applies decision D2 to each
  keystroke (`textEdited`): a–z become A–Z, and any other refused character
  restores the previous text and cursor and shows the message (in the core
  message's words, naming the character as typed). The field's maximum
  length is 16. Name and description are applied on `editingFinished`; a
  name that normalises to the current one makes no command, and a rename
  the project refuses shows the message and restores the name.
- **Imports.** `MainWindow::ImportFiles()` posts one job per batch to the
  `ITaskRunner`; the job runs `PhraseImporter` on each file (names taken
  are updated as it goes) and returns the results to the GUI thread with a
  queued `QMetaObject::invokeMethod()`. If any file clips or is very quiet,
  the window asks once through `IDialogs::AskNormalise()` and, if agreed,
  imports the batch again with `normalise` set, so the result is the same
  as the CLI's `--normalise`. The phrases go in as one `InsertPhrases()`
  command and are selected; failures are listed. A counter of opened
  projects drops results that arrive after another project was opened, and
  the window's destructor waits for the runner.
- **Dialogs and settings behind interfaces.** Modal dialogs cannot be
  answered offscreen, so every question goes through `IDialogs`
  (`QtDialogs` uses `QFileDialog` and `QMessageBox`); the recent-file list
  (`RecentFiles`, at most eight, newest first) is kept through
  `IRecentFilesStore` (`SettingsRecentFilesStore`, `QSettings` under the
  organisation and application name "Speak5220").
- **Shutdown order.** Child widgets are deleted by `QObject` after the
  window's members, so `~MainWindow()` deletes the panel and the details
  (which use the model and the document) first and disconnects the
  document; `~ProjectDocument()` disconnects the stack, which clears itself
  (and signals) as it is destroyed.
- **Status bar.** `ProjectSizeText()` formats
  `37.2 KB · 3 PHROMs of 64 · 4.1 KB free in PHROM 3` (KB of 1024 bytes,
  one decimal, C locale), "No PHROMs in use" for none, and appends "over
  budget" and the counts of phrases too large or not encoded.

**Deviations from the plan.**

- The plan's mocked `IProjectEditor` cannot also supply the list's
  contents, so the model reads a `Project` value (built by the test
  helpers) and sends its edits to the mock through `ProjectEdits`.
- The plan has no Export menu yet; it comes with the Export dialog
  (Phase 8), and is left out rather than shown disabled.
- `speak5220 [PROJECT]` opens a project at start-up; the plan does not
  mention it.

**Acceptance** (plan Phase 5, met):

- GUI functional tests (offscreen) open fixture projects, save as and save
  in both forms, and close with unsaved changes (save, discard, cancel, a
  cancelled save); unit tests cover the same prompts without files;
- GUI unit tests drive the model with a mocked `IProjectEditor` (move up
  and down, sort, remove, drag and drop, file drops, failures), and check
  that the dividers match `Project::Allocation()`;
- GUI unit tests type into the name field: lower case is converted, a
  refused character shows a message and leaves the name unchanged;
- GUI unit tests check each command's `redo()` and `undo()` against a
  mocked `IProjectEditor`, that undoing a sort restores the previous order
  (also on a real `Project`), and that a failed operation is not pushed;
- GUI unit tests format the status text for given allocations;
- the whole GUI test set passes under ASan and UBSan.
