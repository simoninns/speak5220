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
| `s5core`: `util/` (Status, logging, version, numbers as text, SHA-256, task runners, file digests, clock) | Implemented |
| `s5core`: `lpc/` (tables, frames, bit stream, frame statistics) | Implemented |
| `s5core`: `emulator/` (`Tms5220`, `Tms6100`, host procedures) | Implemented, matching the C reference sample for sample |
| `s5core`: `synth/` (`Synth`, `ReferenceSynthesizer`, the output pins) | Implemented |
| `s5core`: `packaging/` (Acorn PHROM, TI indexed, Speak External) | Implemented |
| `s5core`: `audio/`, `dsp/` (WAV files and in-memory WAV, resampler, FFT adapters; filters, NumPy-ordered sums, LPC analysis, the squelch, the output filters) | Implemented |
| `s5core`: `encoder/` (`Encoder` and its stages, `EncoderOptions`) | Implemented, matching the Python reference's frames on the whole corpus |
| `s5core`: `quality/` (the libfvad and SPTK adapters, the signal preparation, the spectral envelope, pitch and voicing, ESTOI, the overall score and rating, the assessor; the encoded audio for display; the anchor vocoder) | Implemented, matching independent implementations on the corpus subset; the score calibrated and confirmed by listening |
| `s5core`: `project/` (`Project` and its editing operations, PHROM allocation, the encoding cache, the `.s5proj` file in ZIP and directory form, import, PHROM export) | Implemented |
| `s5core`: `optimiser/` (`Optimiser`: the grid, the coordinate descent, the budget and byte limit) | Implemented (§17) |
| Reference data in `app/golden/` (emulator, packaging, encoder, quality) | Implemented |
| Fixture projects in `app/projects/` (`general/`, `phrom/`, `benchmark/`) | Implemented |
| `speak5220` GUI | Implemented: the main window, projects (open, save, recent files, the unsaved-changes prompt), the phrase list with PHROM dividers and drag and drop, phrase names and descriptions, undo/redo, the status bar (§15); the original and encoded waveforms, the play-start marker, playback, light and dark colours (§16); the encoder panel, re-encoding, Optimise and Optimise all (§17) |
| `speak5220-cli` | Implemented: `new`, `add`, `encode`, `export` (Acorn PHROM images), `--output`; version and logging options |
| Test tree and lanes (TESTING.md) | Implemented |
| Format, lint, pre-commit hooks, CI on Linux | Implemented |
| CI on Windows and macOS (release tags and manual runs) | Implemented; a manual run passes both lanes on both |
| Nix dev shells and package | Implemented |
| The other export formats, BBC export, the GUI's Export dialog | Planned |

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
                                benchmark.sh, arctic-project.sh,
                                generate_corpus.py,
                                generate_filter_constants.py (§8)
    golden/                     generate_golden.py and its scenario scripts,
                                encoder.py, measures.py (offline, run by
                                hand: §13)
  golden/                       reference data from the retired references (§13)
  projects/                     fixture projects, unpacked: general/, phrom/,
                                benchmark/ (§14, §17)
  src/
    core/                       library "s5core" (QtCore at most)
      util/                     Status/StatusOr, logging, logging options, version,
                                number text, SHA-256, task runners, file digests,
                                IClock
      lpc/                      coding tables, Frame, FrameCodec, BitReverse,
                                FrameStatistics
      emulator/                 Tms5220, Tms6100, ChipDriver, interfaces
      synth/                    Synth, ReferenceSynthesizer, OutputPin
      packaging/                AcornPhrom, TiIndexed, SpeakExternalStream
      audio/                    WAV interfaces, the libsndfile adapters, the
                                24-bit grid of project audio
      dsp/                      resampler and FFT interfaces and adapters;
                                SOS/FIR filters and their constants,
                                NumPy-ordered sums and windows, LPC analysis,
                                the squelch
      encoder/                  Encoder and its stages, EncoderOptions (§8)
      quality/                  the quality measures, the adapters over
                                libfvad and SPTK, the overall score,
                                QualityAssessor, the anchor vocoder (§9),
                                the output alignment and
                                RenderEncodedAudio (§16)
      project/                  Project, IProjectEditor, the PHROM allocator,
                                EncodingCache, ProjectSerializer, the project
                                stores, PhraseImporter, the PHROM export (§14)
      optimiser/                Optimiser (§17)
    gui/                        library "speak5220_gui" + the "speak5220" executable
      main_window/              MainWindow, ProjectDocument, IDialogs, the
                                recent-file list, the view preferences,
                                the status bar's text
      phrase_list/              PhraseListModel, its delegate and panel
      phrase_editor/            PhraseDetails (name and description)
      encoder_panel/            EncoderPanel, EncoderSettingsForm,
                                QualityRatingView, the encode-and-measure
                                helpers and text (§17)
      optimise/                 OptimiseSession, OptimiseDialog (§17)
      waveform/                 PhraseWaveforms, WaveformPane, WaveformView,
                                PeakCache, TimeAxis, the frame map (§16)
      playback/                 Player, IAudioOutput, QtAudioOutput,
                                PlayPauseIcon (§16)
      theme/                    ThemeColours, the desktop colour scheme
                                and its portal adapter (§16)
      undo/                     one QUndoCommand per editing operation,
                                ProjectEdits
    cli/                        the "speak5220-cli" executable and its commands
  tests/
    .clang-tidy                 the app's settings, plus one test exception
    support/                    speak5220_test_support (helpers, mocks, golden readers)
    unit/  functional/          speak5220_unit_tests, speak5220_functional_tests
    gui/support/                speak5220_gui_test_main (QApplication, offscreen),
                                the GUI's mocks and fakes, test palettes
    gui/unit/  gui/functional/  speak5220_gui_unit_tests, speak5220_gui_functional_tests
```

Planned additions, each created with its first code (plan §2.1):

- `src/core/`: `bbc/`;
- `src/gui/`: `export/`;
- `app/projects/bbc/` (fixture projects),
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
- `cmake/Dependencies.cmake` finds the libraries of §10: Qt 6.8 or later
  (Core, Widgets, Multimedia, and D-Bus on Linux and the BSDs; 6.8 for
  `QStyleHints::setColorScheme()`), spdlog, yaml-cpp and libzip by their CMake packages; libsndfile and libsamplerate
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
  - `measures_golden`: readers for `app/golden/quality/measures/` (the
    scores, masks and pitch tracks) and `PrepareReferenceEncodings()`, which
    speaks and prepares the subset's reference encodings as the quality
    measures do;
  - `synth_mocks.h`: a gMock mock of `ISynth`;
  - `quality_mocks.h`: gMock mocks of `IQualityMetric`,
    `IQualityAssessor`, `IVoiceActivity`, `ILpcAnalysis` and
    `IPitchEstimator`, and `AssessmentScoring()`, an assessment with a
    chosen overall score;
  - `synthetic_speech`: a deterministic speech-like signal (two vowels, a
    fricative, pauses), a delay and white noise, for the quality unit
    tests;
  - `project_mocks.h`: gMock mocks of `IEncoder`, `IWavReader`,
    `IWavCodec`, `IResampler`, `IFileDigest`, `IProjectStore` and
    `IProjectEditor` (with `SetDefaultEncoder()`);
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
  (`EncoderCorpusCheck`, `QualityCorpusCheck` and `OptimiserCorpusCheck`,
  `corpus-check.sh`) and the timings (`EncoderBenchmark`,
  `QualityBenchmark` and `OptimiserBenchmark`, `benchmark.sh`).
- GUI test binaries use `speak5220_gui_test_main`, which selects the
  offscreen platform unless `QT_QPA_PLATFORM` is already set, then creates a
  `QApplication` and runs Google Test. `app/tests/gui/support/gui_mocks.h`
  has gMock mocks of `IDialogs` and `IAudioOutput`, an in-memory
  `IRecentFilesStore` and `IViewPreferences`, a `FakeClock`,
  `TestPlayback` (a `Player` over a
  mocked output, and a mocked synth, for windows not testing playback),
  `TestEncoding` (`EncodingTools` over a mocked encoder and quality
  assessor, a fake clock and an inline runner), and `IdsAre()`, a matcher for spans of phrase ids; `test_palettes.h` has
  light and dark palettes like Fusion's. The GUI functional lane
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
| `OutputPin`, `PinSamples()`, `ClippedSamples()` | **Implemented** (`core/synth/output_pin`): the encoded audio from the SPEAKER pin, each code clipped to the DAC's −128…+127 (`emulator::kDacMin`, `kDacMax`), or from the I/O pin's unclipped 10-bit codes, for listening and display only (§16) |
| `AcornPhrom`, `TiIndexed`, `SpeakExternalStream` | **Implemented** (`core/packaging/`): the §12.8 and §12.9 layouts, byte-identical to the Python encoder's |
| `SndfileWavReader`, `SndfileWavWriter` (`IWavReader`, `IWavWriter`) | **Implemented** (`core/audio/`): any WAV variant read and mixed to mono as floats; 16- or 24-bit PCM written, clipped |
| `SndfileWavCodec` (`IWavCodec`) | **Implemented** (`core/audio/sndfile_wav_io`): the same, to and from bytes in memory (libsndfile's virtual I/O), with integer PCM read and 24-bit PCM written exactly (§14) |
| `SincResampler` (`IResampler`) | **Implemented** (`core/dsp/resampler`): libsamplerate `SRC_SINC_BEST_QUALITY` |
| `PocketFft` (`IFft`) | **Implemented** (`core/dsp/fft`): real FFTs with NumPy's conventions |
| Filters, sums, LPC analysis | **Implemented** (`core/dsp/iir_filter`, `array_math`, `lpc_analysis`, `filter_constants.h`): §8 |
| `OutputFilter`, `FilterDacOutput()`, `FilterSamples()` | **Implemented** (`core/dsp/output_filter`): the DAC's held samples at 48 kHz through a computer's audio stage (none, or the BBC Micro's), for listening and display only (§16) |
| `Squelch()`, `SquelchGain()`, `SquelchedSpans()` | **Implemented** (`core/dsp/squelch`): mutes stretches below a level relative to the loudest, for the `squelch_db` option (§8), and reports them for the waveform's shading (§16) |
| `MainWindow`, `ProjectDocument` (`IDialogs`, `IRecentFilesStore`, `IViewPreferences`) | **Implemented** (`gui/main_window/`): menus, projects, imports, the status bar (§15) |
| `PhraseListModel`, `PhraseListDelegate`, `PhraseListPanel` | **Implemented** (`gui/phrase_list/`): the phrase list (§15) |
| `PhraseDetails` | **Implemented** (`gui/phrase_editor/`): name and description (§15) |
| `ProjectEdits` and the undo commands | **Implemented** (`gui/undo/`): one `QUndoCommand` per `IProjectEditor` operation (§15) |
| `PhraseWaveforms`, `WaveformPane`, `WaveformView`, `PeakCache`, `TimeAxis` | **Implemented** (`gui/waveform/`): the original and encoded waveforms, their shared time axis and controls, the play start (§16) |
| `Player`, `QtAudioOutput` (`IAudioOutput`), `SteadyClock` (`IClock`) | **Implemented** (`gui/playback/`, `core/util/clock`): playback and the playhead (§16) |
| `ThemeColours`, `ColourSchemeFollower`, `PortalColourScheme` (`IDesktopColourScheme`) | **Implemented** (`gui/theme/`): light and dark colours, following the desktop (§16) |
| `RenderEncodedAudio()` | **Implemented** (`core/quality/encoded_audio`): the frames spoken and lined up with the source (§16) |
| `Encoder` (`IEncoder`) | **Implemented** (`core/encoder/`): re-implementation of `encode()`/`encodeDetailed()` with the full `EncoderOptions` set (§8) |
| `QualityAssessor` (`IQualityAssessor`), `OverallQualityMetric` (`IQualityMetric`), `ScoreQuality()` | **Implemented** (`core/quality/`): the quality measures, the overall score and the rating (§9) |
| `Optimiser` | **Implemented** (`core/optimiser/`): searches encoder options for the best overall quality score, within the 64-PHROM project budget and a per-phrase byte limit (§17) |
| `EncoderPanel`, `EncoderSettingsForm`, `QualityRatingView` | **Implemented** (`gui/encoder_panel/`): statistics, the quality rating, the settings form and debounced re-encoding (§17) |
| `OptimiseSession`, `OptimiseDialog` | **Implemented** (`gui/optimise/`): Optimise and Optimise all in the GUI (§17) |
| `FvadVoiceActivity` (`IVoiceActivity`), `SptkLpcAnalysis` (`ILpcAnalysis`), `SptkPitchEstimator` (`IPitchEstimator`) | **Implemented** (`core/quality/voice_activity`, `lpc_envelope`, `pitch_estimator`): the adapters over libfvad and SPTK 4 for the quality measures of plan Phase 8 (§9, §10) |
| `Project` (`IProjectEditor`) | **Implemented** (`core/project/project`): phrases, their audio, settings and cached encodings; the editing operations (§14) |
| `AllocatePhroms()` | **Implemented** (`core/project/phrom_allocator`): list-order assignment to PHROMs and the 64-PHROM budget (§14) |
| `EncodingCache` | **Implemented** (`core/project/encoding_cache`): the settings hash and re-encoding within the budget, over `IEncoder` (§14) |
| `ProjectSerializer`, `ZipProjectStore`, `DirectoryProjectStore` (`IProjectStore`) | **Implemented** (`core/project/`): the `.s5proj` file (§14) |
| `PhraseImporter` | **Implemented** (`core/project/phrase_importer`): plan §3.1 over `IWavReader`, `IResampler` and `IFileDigest`, inline or through `ITaskRunner` (§14) |
| `InlineTaskRunner`, `ThreadPoolTaskRunner` (`ITaskRunner`) | **Implemented** (`core/util/task_runner`): background jobs (§14) |
| `RunExport()`, `BuildAcornPhroms()` | **Implemented** (`core/project/project_export`): a project's Acorn PHROM images (§14) |
| TI, raw `.bin`, C header and WAV export | Planned |
| `BbcExport` | Planned: a double-sided Acorn DFS `.dsd` image with the fixed player program, and each PHROM's phrase data and index |

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
  bytes (FNV-1a hash, full comparison), unless the synth is made with
  `SynthCache::kOff`: Optimise speaks hundreds of candidates once each,
  so the synth behind its quality measures keeps nothing (§17). `Synthesize()` is
  thread-safe; the caller runs it on a worker thread.

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
| `ILevelSetter` → `LevelSetter` | `_shapeLevels()`, `_setLevels()`, `_gainOf()`, `_effectiveK()`, `_outLevels()` | Slow gain control; open-loop energies, five closed-loop passes through `ReferenceSynthesizer`, the too-loud check, the peak limiter at 120 and the safety net |

Between the stages, `Encoder` drops a faint opening (`TrimFaintOpening()`),
sets the gain from `target_rms`, and turns the plan and energies into
frames with `frame_endings` (`ToFrames()`, `GuardK510()`, `AddFades()`,
from `_toFrames()`, `_guardK510()` and `_addFades()`), and last quietens
the pauses (`QuietPauses()`, below), which the reference does not do. Before the levels are set, with `resonance_hold_db`, it holds
resonant filters through a fade (`HoldResonantFilters()`, in
`repeat_search`; below), which the reference does not have either. The quantisers
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

**Options.** `EncoderOptions` has 9 of the 15 options of `tms5220enc.py`
`DEFAULTS`, and two the reference does not have: the squelch
(`squelch_db`, off by default) and the resonance hold
(`resonance_hold_db`, 14 dB by default). The reference's other six are
fixed at its defaults, since moving them made no difference worth keeping
over the corpus or did what another option does (§9, "Fixed options"):
`targetPeak` unset (the level comes from `target_rms`), `peakLimit` 120
(`kPeakLimit`, in `level_setter`), `repeats` on (`kMaxRepeatRun`),
`iterations` 5 (`kLevelPasses`), `fadeUnvoiced` on and `compression` 1
(off). Quiet pauses, once an option, are always on. The defaults are the
reference's but for four chosen on the corpus for the overall quality
score (§9, "Default settings"): `target_rms` 24, `window` 256,
`voicing_evidence` 5 and `level_agc` 1.25. The reference data is compared
with the reference's own defaults written out and the resonance hold off
(`ReferenceEncoderOptions()` in the test support, §13), and with the
reference's frames with their pauses quietened (`ReferenceFrames()`), so
it does not depend on the application's defaults. `EncoderOptionInfos()`
gives each one's key (the Python name, or `squelchDb` and
`resonanceHoldDb` in its style), label, help text, type, range and
default, for the GUI's settings form and for project files, and marks the
options the GUI shows only with its advanced settings (`advanced`).
The ranges are the application's own; Python had none.

**Quiet pauses** (datasheet §12.2, "Silence inside an utterance rings";
evidence p12-01 to p12-06). A silence frame keeps the K of the sound
before it, and with no excitation the chip's lattice settles into a limit
cycle a code or two either side of zero. The DAC's −1 to 0 step makes it a
buzz about 23 dB below the speech for the whole pause, measured on chip 1.
The emulator rings too, but not reliably where the chip does: in one
measured pause it is silent where the chip rings, and rings after the
neutral frame where the chip is silent (datasheet §8.8, evidence p12-07
and p12-08). So the chip, not the emulator, decides the design: on the
chip the neutral frame silenced all three pauses measured. The
squelch made this common, since squelched pauses become long runs of
silence frames. `QuietPauses()` replaces the second silence frame of
every pause of three or more (`kMinQuietPause`, `kQuietPauseAt`) with
`NeutralFrame()`: voiced, energy 1, the last voiced pitch, every K at its
Table 8-6 entry nearest zero. The chip then holds neutral K, and the rest
of the pause is code 0, at the bench's noise floor (47 dB below the
speech). The second frame, not the first: in the first, the K would glide
to neutral while the sound before fades (voiced to voiced is interpolated),
which dulled it and lowered PESQ (the measure then) by 0.02 in the
median over the corpus;
after a silence frame the change is inhibited, so the K jump while the
energy is 0. A silence frame must follow, or the next sound would glide in
from the neutral frame. A repeat straight after a quietened pause is sent
in full with the K it would have had (an unvoiced repeat as an unvoiced
frame, which zeroes K5–K10 as the repeat would); a pause before a voiced
repeat of an unvoiced frame, whose K5–K10 zeroing no full frame can give,
is left alone. Over the 243 corpus files with a 30 dB squelch, 110 have
such pauses (288 in all); the encodings grow by 1.7%, and PESQ (the
measure then) was unchanged within its noise (median change +0.0003; 53
files higher, 48 lower). It was an option, on by default, until the
options were reduced (§9, "Fixed options"); the score cannot hear the
buzz, and turning it off only brings the buzz back.

**The resonance hold** (datasheet §8.7). A high voice can make the LPC
fit lock onto a harmonic: a voiced frame with K1 near −1 and K2 near +1,
a resonance 30 to 50 dB high whose filter gain to white noise,
1 / ∏(1 − kᵢ²) (`FilterGainDb()`), reaches 20 dB, with the energy low to
match. From one voiced frame to the next the chip interpolates the K
values in eight equal steps, but that gain changes steeply as a K nears
±1, so moving from such a filter to an ordinary one loses most of the gain
in the first step: on `cfch_small.wav` (a user's recording) the level fell
26 dB in 9 ms where the source fell 10, heard as a pop, twice in 3 s.
Lowering the energy of the frame or limiting each filter's gain or pole
bandwidth in the K search did not remove the pops by ear;
keeping the resonant filter for the next frame did. So
`HoldResonantFilters()`, on the plan before the levels are set, turns a
full voiced frame into a repeat of the voiced frame before it when that
frame's filter gain is more than `resonance_hold_db` above its own and the
level is falling (its frame level below the one before): the sound fades
through the resonant filter, the level setter gives the held frame its
energy as usual (often silence, at the end of a sound), and the change of
filter comes a frame later from a lower level. A held frame is not held
again. Changes the chip does not interpolate (from unvoiced or silence)
are left alone. On `cfch_small.wav` both pops became too faint to notice
by ear. The default, 14 dB, catches both of that file's drops (19.2 and
16.7 dB) with a margin; over the corpus, 101 of 9,554 voiced-to-voiced
changes drop more than 12 dB, in 58 files (§9, "The resonance hold").
Optimise does not search it.

**The squelch.** Breaths and room noise 20 to 35 dB down sit above the
encoder's 45 dB gate, so the encoder reproduces them; PESQ, the measure
when the squelch was added, even rewarded it. When `squelch_db` is set, `SquelchedSource()` (`dsp::Squelch()`)
mutes the source's stretches more than that many dB below its loudest
10 ms block (blocks every 2.5 ms), keeping 60 ms after the sound and
10 ms before it so word endings and onsets survive, with 5 ms ramps so
the cut makes no click. The encoder applies it before its first stage,
and every measurement against the source uses the same squelched source:
`MeasureFrames()` and the `Optimiser`'s candidates. The quality measures
leave out what their voice activity mask does not count as speech, so a
muted breath is not asked of an encoding. The squelch is not searched by
Optimise, since scores against differently squelched sources do not
compare. Loudness alone cannot tell a breath from a soft
"h" or a trailing "s"; the hold keeps word endings, and the user chooses
the level.

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
  criterion of 99%, again after the "loud" and "plain" sets were
  regenerated without the fixed options (§9, "Fixed options"). The option
  sets are taken over `ReferenceEncoderOptions()` (the defaults with the
  resonance hold off), and the reference's frames are compared with their
  pauses quietened (`ReferenceFrames()`), since the reference has neither.
  No near-tie frames are needed: at a relative disturbance
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

**Implemented** (plan §3.3, Phase 8; decision D10). `core/quality/`
compares the chip's output with the original over speech-active frames,
with three complementary measures that a parametric vocoder can be judged
by, and combines them into an overall score and a rating from *Poor* to
*Good*. The encoder panel shows them, Optimise maximises the score, and
the project caches the figures with each encoding (§14, §17).


| Component | Files | Does |
|---|---|---|
| `FvadVoiceActivity` (`IVoiceActivity`) | `voice_activity` | libfvad, the WebRTC detector, at 8 kHz in 10 ms frames; a fresh detector per call |
| `SptkLpcAnalysis` (`ILpcAnalysis`) | `lpc_envelope` | SPTK 4's autocorrelation, Levinson recursion and filter-to-spectrum: an all-pole fit of a frame and its envelope in dB |
| `SptkPitchEstimator` (`IPitchEstimator`) | `pitch_estimator` | SPTK 4's REAPER (the default) or Harvest, every 10 ms, 50 to 400 Hz |
| `PrepareSignals()`, `PrepareAlignedSignals()` | `signal_preparation` | The placement, band-limit, mask and level below |
| `MeasureEnvelopeDistortion()` | `envelope_distortion` | Log spectral distortion between LPC envelope shapes |
| `MeasurePitchErrors()`, `ComparePitch()` | `pitch_errors` | Gross pitch error, F0 RMSE in cents, voicing decision error |
| `Estoi()`, `MeasureIntelligibility()` | `estoi` | A port of pystoi's ESTOI, with the mask applied |
| `MeasureQuality()`, `MeasurePrepared()` | `quality_measures` | All of it for one encoding |
| `ScoreQuality()`, `ScoreCalibration` | `quality_score` | The component scores, the overall score and the rating (below) |
| `QualityAssessor` (`IQualityAssessor`) | `quality_assessor` | The measures and score of one encoding, with libfvad, SPTK's LPC and REAPER: what the encoder panel and Optimise use; `kMeasuresVersion` |
| `OverallQualityMetric` (`IQualityMetric`) | `quality_assessor` | The overall score as the one number Optimise maximises |
| `ToCachedQuality()`, `FromCachedQuality()` | `cached_quality` | The figures a project caches with an encoding (§14), and the assessment they give back |
| `RenderEncodedAudio()`, `AlignOutput()` | `encoded_audio`, `output_alignment` | The chip's output spoken and placed against the source by `speechqual.py`'s alignment (`align()`, `refine()`, `placed()`, `matchLevel()`): for display (§16) and for the measures |
| `AnchorVocoder()` | `anchor_vocoder`, `numpy_normal`, `normal_ziggurat_tables.h` | `speechqual.py`'s unquantised LPC-10 vocoder, for the calibration (below) |

**Preparing the signals** (plan §3.3's six steps, as fixed in Phase 8):

1. The original is the source as given to the encoder (squelched when the
   squelch is on), at 8 kHz.
2. The chip's output is spoken through the VSM path and placed against the
   source by `RenderEncodedAudio()`, with its idle code taken as 0, which
   removes the chip's DC offset.
3. Both are band-limited by the same order-4 Butterworth band-pass, 80 to
   3600 Hz, forwards and backwards (`dsp::kQualityBand` through
   `SosFiltFilt()`, SciPy's `sosfiltfilt`), so without delay.
4. **The alignment** is that of the encoded waveform (§16):
   `speechqual.py`'s correlation of level envelopes in 2.5 ms steps over
   every overlap, refined within ±25 ms in 0.625 ms steps by mel-cepstral
   distance. The plan asked for a cross-correlation of the signals around
   the chip's start latency. That was tried on the corpus: the latency is
   not known from the frames alone (the encoder leaves out leading
   silence, by up to 0.6 s), and the vocoder's output does not follow the
   original's phase, so a plain cross-correlation of the band-limited
   waveforms strayed by more than 140 samples (18 ms) from the envelope
   alignment on a tenth of the files, and by thousands on the worst. The
   envelope alignment matched the reference tools on all 972 reference
   encodings, and the waveform view shows the same alignment the measures
   use.
5. The output is scaled to the original's power over the mask's samples.
6. **The mask** is libfvad's decision at its most aggressive (mode 3) for
   each 10 ms frame of the band-limited original, less the frames 40 dB or
   more below its loudest. On the corpus, mode 3 marks 1 % of the quiet
   frames (45 dB down) more than 100 ms from speech as speech, against
   29 % at mode 2; the frames it keeps near speech are its hangover at the
   ends of words, which the 40 dB floor (ESTOI's own) removes.

**The measures:**

- **Spectral envelope.** Each 25 ms frame (the chip's) whose centre the
  mask keeps is Hamming-windowed and fitted with an order-10 all-pole
  model (the chip's order); the distortion is the RMS difference in dB of
  the two envelope *shapes* (1/|A|, the gain left out) over 100 to 3500 Hz
  on a 512-point grid. The shape alone is compared, as the classic
  criteria of transparent LPC quantisation (a mean of 1 dB, under 2 % of
  frames over 2 dB, none over 4 dB) are defined; with the gain left in,
  the chip's coarse energy steps dominate (a median of 5.0 dB and 54 % of
  frames over 4 dB on the corpus, against 3.4 dB and 28 % for the shape).
  A frame whose original is silent is skipped; one whose output is silent
  is compared with a flat shape.
- **Pitch and voicing.** Both prepared signals are tracked every 10 ms;
  frame i counts if mask frame i does. The gross pitch error is the
  percentage of frames voiced in both whose F0 differs by more than 20 %;
  the F0 RMSE, in cents, is over the other frames voiced in both; the
  voicing decision error is the percentage of counted frames whose voicing
  differs. **REAPER is the default** (risk R1): on the corpus its median
  errors between original and output are lower than Harvest's (gross 4.6 %
  against 8.6 %, RMSE 57 against 83 cents, voicing 7.8 % against 10.8 %),
  Harvest makes gross octave errors on some files (over 60 % on two of the
  eight subset files), and REAPER is about 40 times faster. Both find the
  F0 of 8 kHz pulse trains within 1 %.
- **Intelligibility.** ESTOI as pystoi 0.4.1 computes it
  (`extended=True`), of the prepared signals with the samples outside the
  mask set to zero, so pystoi's own removal of frames 40 dB below the
  loudest drops them. pystoi adds random noise of 2.2e-16 before each
  normalisation to avoid dividing by zero; the port leaves a row or column
  without variation at zero instead, so it is deterministic. ESTOI needs
  30 frames (about 0.4 s of sound); with less it is absent, where pystoi
  returns 1e-5.

**Matching the reference** (`app/golden/quality/measures/`, §13, the
eight subset files in the four option sets): the alignment and the mask
are identical on all 32 encodings; the mean distortion is within 7e-12 dB
of the NumPy reference's; SPTK's REAPER and Harvest give the original
tools' tracks frame for frame, and the same errors; ESTOI is within 5e-13
of pystoi's. The plan's tolerances were ±0.01 dB and ±0.005.

**Performance** (`benchmark.sh`, one core, the eight-file subset):
`QualityAssessor` takes 0.047 s per second of speech, as long as PESQ with
its synthesis and alignment took before it; most of it is the synthesis
and the alignment's spectral search. Harvest in place of REAPER would add
about 0.13 s.

**The overall score and rating** (`quality_score`, plan Phase 8, task 12;
**calibrated and confirmed by listening**). Each figure is mapped
linearly to 0..1 between a *poor* and a *good* value
(`ScoreCalibration`); the envelope component is the mean of its two
figures' scores, the pitch component the mean of its three, and the
intelligibility component ESTOI's. The overall score is their geometric
mean with equal weights, so one poor component pulls it down; without
ESTOI (35 of the corpus's 1,701 measurements, all short phrases) the other
two share its weight.

| Figure | Poor | Good | Where the good end comes from |
|---|---|---|---|
| Mean envelope distortion | 5.0 dB | 1.8 dB | The anchor's median (1.80) |
| Frames over 4 dB | 60 % | 2 % | The anchor's median (1.9) |
| Gross pitch error | 50 % | 0 % | The default encodings' best tenth (0) |
| F0 RMSE | 120 cents | 40 cents | The default encodings' best tenth (39.5) |
| Voicing decision error | 30 % | 3.5 % | The default encodings' best tenth (3.4) |
| ESTOI | 0.55 | 0.82 | The anchor's median (0.821) |

The good ends are what unquantised LPC reaches (the anchor vocoder) for
the envelope and ESTOI; for pitch the anchor is no reference, since its
own pitch tracker is cruder than the encoder's (its median voicing error
is 10 %), so the best tenth of the default encodings is used. The poor
ends are round values just beyond the worst 1 % of the encodings (the four
reference option sets as they were then, the application's defaults and
Optimise's results: 4.30 dB, 49.7 %, 107 cents, 25.4 %, 0.578), except the gross
pitch error, whose worst 1 % (96 %, encodings an octave out) is capped at
half the voiced frames.

**Bands:** *Poor* below 0.50, *Fair* from 0.50, *Good* from 0.70.
Calibrated with `corpus-check.sh`'s `measures.tsv` (all 243 files; Optimise
from the defaults, then still maximising PESQ, with the default
encoding's size as its limit):

| Encodings | Median score | 10th percentile | Poor | Fair | Good |
|---|---|---|---|---|---|
| Reference, default options | 0.70 | 0.57 | 3 % | 47 % | 49 % |
| Reference, lean | 0.62 | 0.48 | 13 % | 62 % | 25 % |
| Reference, loud | 0.68 | 0.54 | 6 % | 52 % | 42 % |
| Reference, plain | 0.69 | 0.56 | 5 % | 47 % | 48 % |
| Application defaults | 0.70 | 0.57 | 4 % | 47 % | 49 % |
| Optimised (for PESQ) | 0.69 | 0.57 | 4 % | 48 % | 49 % |
| Anchor vocoder | 0.89 | 0.78 | 1 % | 3 % | 96 % |

Unquantised LPC is *Good* on nearly every file; the encoder's defaults are
*Good* or *Fair* in equal parts, and *Poor* on a few files; the lean
options (fewer bits) score lowest. The median components of the default
encodings are envelope 0.53, pitch 0.82 and intelligibility 0.84: the
envelope, the chip's quantised K, is what holds encodings back.
Optimising PESQ barely moves the score (the median is unchanged), which
is the reason for replacing it. The rank agreement of the overall score
with PESQ is weak: Spearman 0.27 over the reference encodings, 0.31 over
the optimised ones and 0.37 over the anchor.

**The listening check** (2026-10-11): `corpus-check.sh --listening`
writes five files per band, spread over its scores, as the original and
the chip's output at the original's level (`listening/BAND/`, with every
file's score in `listening/index.tsv`). The user listened to all fifteen,
paying attention to the pairs either side of each threshold (0.497 and
0.503, 0.699 and 0.700) and to the files rated down by one component
(ESTOI 0 on the lowest Poor file); the bands were confirmed as they are.

**Default settings** (plan Phase 8, task 14; 2026-10-12). Optimise was run
over the corpus from the old defaults (the reference's), and from them
with the squelch at 30 and 40 dB, each file's limit at its default
encoding's size, and the best options studied across files. Optimise
raised `target_rms` on 156 files (most often to 24), turned `level_agc`
on for 89, lengthened `window` on 79 (shortened it on 19) and moved
`voicing_evidence` both ways (70 up, 58 down); the other options mostly
stayed. The squelch did not help: the best scores' median was 0.771 at
30 dB and 0.773 at 40 dB, against 0.774 without. 26 candidate sets were
then scored over the corpus (`OptionsCorpusCheck`), and four defaults
changed, agreed with the user:

| Option | Old (the reference's) | New |
|---|---|---|
| `target_rms` | 20 | 24 |
| `window` | 240 | 256 |
| `voicing_evidence` | 4 | 5 |
| `level_agc` | 1 | 1.25 |

| On the 243 files | Old defaults | New defaults |
|---|---|---|
| Overall score: 10th percentile, lower quartile, median, upper quartile, 90th percentile | 0.565, 0.622, 0.699, 0.767, 0.826 | 0.576, 0.651, 0.720, 0.775, 0.840 |
| Good / Fair / Poor | 120 / 114 / 9 | 141 / 93 / 9 |
| Total size | 100,294 bytes | 100,591 bytes (+0.3 %) |
| Optimised (limit: each file's default size): median, Good / Fair / Poor | 0.774, 188 / 53 / 2 | 0.777, 191 / 50 / 2 |

The score rose on 183 files and fell by more than 0.05 on 2 (the worst
fall 0.08); the typical change is small (median +0.015) and the largest
+0.28. A window of 260 scored a median of 0.724 but is off the slider's
steps of 8; 256 is on them, and its mean is the same (0.710). The slow gain
control's slider steps became 0.05, so that 1.25 lies on them. Encoding
with the new defaults takes 4 % longer (0.0109 s per second of speech
against 0.0105, one core). Optimise from them took 5 % longer (1.72 s per
second of speech on 16 threads, against 1.64), trying a median of 310
candidates against 294.

**The resonance hold** (2026-10-12; §8). Thresholds of 8 to 20 dB were
scored over the corpus with the other defaults; the score measures
neither pops nor their absence, so the threshold was chosen by ear on
`cfch_small.wav` and kept where the score barely moved:

| Threshold | Files changed | Median, mean score | Good / Fair / Poor | Files falling over 0.02 (worst) | Total size |
|---|---|---|---|---|---|
| Off | — | 0.720, 0.7098 | 141 / 93 / 9 | — | 100,591 bytes |
| 8 dB | 122 | 0.716, 0.7068 | 134 / 100 / 9 | 19 (−0.24) | 99,436 bytes |
| 12 dB | 49 | 0.717, 0.7091 | 138 / 96 / 9 | 6 (−0.08) | 100,173 bytes |
| 14 dB (the default) | 27 | 0.718, 0.7091 | 138 / 96 / 9 | 3 (−0.08) | 100,358 bytes |
| 16 dB | 17 | 0.718, 0.7095 | 140 / 94 / 9 | 2 (−0.08) | 100,460 bytes |
| 20 dB | 2 | 0.720, 0.7098 | 141 / 93 / 9 | 0 | 100,581 bytes |

16 dB would only just catch `cfch_small.wav`'s second pop (16.7 dB), so
14 dB was taken. The hold costs no measurable time: encoding the corpus took 0.0109 s per second of speech with it and 0.0110 without (one core).

**Fixed options** (2026-10-12; §8, "Options"). To make the encoder easier
to understand, each option was moved off its default, one at a time, and
the corpus scored (`OptionsCorpusCheck`; the defaults, with the
resonance hold at 14 dB, give a median of 0.718, a mean of 0.7091 and
100,358 bytes). Where Optimise left an option at its default on 208 or
more of the 243 files, or does not search it, and moving it changed
little or did what another option does, it was fixed at the reference's
default, agreed with the user:

| Option | Fixed at | Moved to | Files changed | Mean score change | Files up / down by over 0.02 (worst) | Total size |
|---|---|---|---|---|---|---|
| `iterations` | 5 | 3; 8 | 82; 20 | +0.0005; −0.0001 | 0 / 0; 0 / 1 (−0.02) | 100,352; 100,346 bytes |
| `fadeUnvoiced` | On | Off | 64 | −0.0003 | 0 / 0 | 100,335 bytes |
| `quiet_pauses` | On | Off | 108 | +0.0007 | 2 / 0 | 98,831 bytes |
| `peakLimit` | 120 | 127; 100 | 188; 234 | +0.0025; −0.0115 | 9 / 2; 5 / 56 | 100,360; 100,348 bytes |
| `compression` | 1 (off) | 1.25 | 242 | −0.0053 | 42 / 67 (−0.89) | 101,528 bytes |
| `repeats` | On | Off | 243 | +0.0142 | 75 / 9 | 121,946 bytes (+21 %) |
| `targetPeak` | Unset | 100 | 241 | −0.0426 (median 0.673) | 6 / 168 | 99,054 bytes |

The score does not hear what fading hiss and quiet pauses are for (an
abrupt end, a buzz on the chip), so those are on. Turning repeats off is
what a higher `repeat_lambda` does more gently (140 gives a mean change
of +0.0103 at 111,912 bytes), and `targetPeak` is a second way to set the
level. The options kept each move 20 files or more by over 0.02 when
moved by a step (for example `window` 224: 13 up, 46 down; `gate_db` 50:
54 up, 43 down; `voicing_evidence` 3: 1 up, 24 down), and Optimise moves
them on many files. The defaults' encodings are unchanged. The reference
data's "loud" and "plain" sets, which changed the fixed options, were
regenerated without them ("loud" sets `targetRms` 40 in place of
`targetPeak` 100, so it still reaches the limiter), and the reference's
frames are compared with their pauses quietened.

Optimise, which searched `targetPeak`, `repeats`, `iterations`,
`fadeUnvoiced` and `compression` too, was run over the corpus again
(limit: each file's default size). From the defaults with the resonance
hold, its best scores' median is 0.773 (Good / Fair / Poor 189 / 52 / 2,
98,441 bytes), against 0.777 (191 / 50 / 2, 98,816 bytes) from the
defaults before the hold and with every option searched; the hold alone
lowers the starting median by 0.002. It tries a median of 268 candidates
per file instead of 310, and takes 1.69 s per second of speech on 16
threads instead of 1.72.

---


**The anchor vocoder** is order 10, 25 ms frames, K and gain
interpolated over 8 steps, a pulse per pitch period or Gaussian noise, pre-
and de-emphasis 0.9375, nothing quantised: what unquantised LPC reaches,
which places the good ends of the calibration. Its noise is the
reference's sample for sample: `NumpyNormal` reproduces NumPy's default
generator (PCG64, XSL-RR 128/64, from the state of
`numpy.random.default_rng(1)`) and its ziggurat normal sampler, with
NumPy's tables, bit for bit.

## 10. Third-party libraries

| Need | Library | Licence | Status |
|---|---|---|---|
| GUI, threads | Qt 6 (Widgets, Concurrent, Multimedia) | LGPLv3 / GPLv3 | Core, Gui (`QUndoStack`), Widgets and Multimedia linked; D-Bus on Linux and the BSDs, for the desktop's colour scheme (§16) |
| Audio playback | Qt Multimedia `QAudioSink` | LGPLv3 | Linked by `speak5220_gui` (§16) |
| WAV read/write | libsndfile | LGPL-2.1+ | Linked by `s5core` |
| Sinc resampling | libsamplerate | BSD-2 | Linked by `s5core` |
| FFT | pocketfft (C++ header-only) | BSD-3 | Used by `s5core`; packaged in `flake.nix` |
| Project container | libzip | BSD-3 | Linked by `s5core` |
| Project metadata | yaml-cpp | MIT | Linked by `s5core` (public: the migration interface takes a `YAML::Node`) |
| ESTOI | The project's own code (`core/quality/estoi`), a port of pystoi's (MIT), checked against it (§9) | GPLv3-or-later | Implemented |
| Voice activity | libfvad (the WebRTC detector as a C library) | BSD-3 | Linked by `s5core` (`FvadVoiceActivity`); built by `flake.nix` and a vcpkg overlay port, since neither nixpkgs nor vcpkg has it |
| LPC analysis, pitch and voicing | SPTK 4, with its own copies of REAPER (Apache-2.0) and WORLD's Harvest (Modified BSD) | Apache-2.0 | Linked by `s5core` (`SptkLpcAnalysis`, `SptkPitchEstimator`); from nixpkgs, and a vcpkg overlay port. SPTK's REAPER and Harvest are the originals with warning fixes, in SPTK's namespace, with the same defaults, so they need no packages of their own; the reference data takes its tracks from the originals (§13) |
| Overall perceptual score | ViSQOL v3 | Apache-2.0 | Left out (plan §3.3, R1): Bazel-only upstream, with Abseil, protobuf, Armadillo and a machine-learned model in its speech mode |
| Logging | spdlog | MIT | Linked by `s5core` |
| Tests | Google Test with Google Mock | BSD-3 | Linked by the test binaries |

---

## 11. GUI, CLI and the core

Both executables are thin front ends over `s5core`.

**Implemented:**

- The GUI's widgets are the static library `speak5220_gui`, so the GUI
  tests link them; the `speak5220` executable adds only `main()`, which
  starts following the desktop's colour scheme (§16), creates the real
  `QtDialogs`, `SettingsRecentFilesStore`, `ThreadPoolTaskRunner`, `Synth`
  and `Player` (over `QtAudioOutput`, `SteadyClock` and `SincResampler`),
  and the `EncodingTools` (the encoder, `QualityAssessor` over an
  uncached `Synth`, the clock, and a one-thread runner for Optimise's
  search), and opens `speak5220 [PROJECT]` if given. The
  window is described in §15, §16 and §17.
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
- Not yet: the GUI preferences for these options (Phase 11).

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
    them change every option the application has ("loud" and "plain"
    regenerated without the fixed options, §9, "Fixed options").
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
  encoder's corpus check scores both with the overall quality score (§9).
- `golden/quality/measures/`, written by `app/scripts/golden/measures.py`
  (same rules; it also records the versions of NumPy, SciPy, py-webrtcvad,
  pystoi, pyworld and REAPER's tool), for the reference encodings of the
  eight subset files in every option set: independent implementations of
  the quality measures (§9, "The quality measures"):
  - the preparation in NumPy and SciPy (`sosfiltfilt`), with the alignment
    of `speechqual.py`;
  - `masks/SET.tsv`: the voice activity masks from py-webrtcvad, the
    original WebRTC code;
  - `tracks/SET/NAME.tsv`: the F0 tracks of both prepared signals from
    REAPER's own command-line tool and from WORLD's Harvest through pyworld,
    the originals of SPTK's copies;
  - `scores/SET.tsv`: the alignment lag, the mask's frame count, the log
    spectral distortion from the script's own NumPy LPC analysis, the pitch
    and voicing errors of both trackers, and ESTOI from pystoi. Scores are
    written to 12 significant digits, since NumPy's sums can differ in the
    last bit from run to run.
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
`CachedEncoding` (frames, encoder version, settings hash, and the cached
quality figures, `CachedQuality`). `ProjectContents` holds the `ProjectSettings` (name, header
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
| `SetPlayStart(id, sample)` | the previous play start | `SetPlayStart(id, result)` |
| `SetDescription(id, text)` | the previous text | `SetDescription(id, result)` |
| `SetEncoding(id, {overrides, encoding})` | the previous pair | `SetEncoding(id, result)` |
| `SetDefaultEncoder(options)` | the previous defaults | `SetDefaultEncoder(result)` |

`SetEncoding()` takes the settings and the encoding made from them
together, so a settings change and its re-encoding are one undoable step,
and the encoding can be computed in the background before the step is
applied. A locked phrase refuses it. Renaming does not change the ASCII map,
which names phrases by name, so a rename and its reverse are exact.
`SetDefaultEncoder()` (through `SetSettings()`) checks the options and
that every phrase's overrides still apply; encodings made from the old
defaults become stale but are not redone, so it cannot affect the budget.
`OverridesFor(defaults, options)` gives the overrides (each option whose
text differs) that turn the defaults into given options, so the GUI and
the optimiser store only what a phrase changes.

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
it is over budget (so such a project can still shrink).
`Project::BudgetCheckFor(id)` returns a `PhraseBudgetCheck`: a copy of the
other phrases' costs that answers, for any packed size of that phrase,
whether the project would stay within the same limit. The optimiser
calls it once per candidate, from worker threads. The plan names
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
does not stop the rest. `encoder::kEncoderVersion` changes whenever the
same audio and options can give different frames: `1.1.0` added quiet
pauses, on by default, so encodings made by `1.0.0` are stale. The
reference data did not change, since it is compared with quiet pauses
off (§13).

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
  `default_encoder` in full, so a project never depends on the defaults
  and changing one never changes a project's encodings. Frames are one flow mapping each, with only
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
- **Cached quality** (`quality` in `frames.yaml`, plan §4): the figures of
  the quality measures and the overall score, with the `measures_version`
  that gave them (`quality::kMeasuresVersion`). Figures from another
  version, or incomplete, are dropped on load (`ReadQuality()`), so the
  GUI measures them again; the `pesq` and `anchor` keys written before the
  measures are dropped too, not kept as unknown keys, since the
  application wrote them itself. Other keys of the mapping are kept.
- **Versions.** `format` must be `speak5220-project`; `version` is migrated
  by `MigrateProjectDocument()` through `ProjectMigrationSteps()` (none yet:
  version 1 is the first), and a newer version is refused.
- **Every option listed.** After the rest of the file is read,
  `CheckEveryOptionListed()` refuses a project whose
  `project.default_encoder` is missing or lacks an option, naming the
  missing ones. Before the application's first release the format
  changed without a new version (plan Phase 8, task 14): the squelch and
  quiet pauses were left out while at their defaults, and are now always
  written, so a project saved before is refused rather than read with
  whatever the defaults have become.
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
header export (Phase 9).

**Export** (`RunExport()`). Only the `acorn-phrom` format exists so far,
built from the allocation: one `AcornPhrom::Build()` per PHROM, with the
project's header strings and the PHROM number appended to the second (or
that PHROM's own strings), and the ASCII entries whose phrase the PHROM
holds. Layouts `separate` (`NAME_1.bin`, …) and `concatenated`
(`NAME.bin`). A phrase with no frames, or too large for a PHROM, fails the
export; the CLI also refuses stale encodings. The other formats and the
Export dialog are Phase 9.

**Fixtures** (`app/projects/`, unpacked; TESTING.md): `general/one-phrase`,
`general/many-phrases` (Acorn-style names with punctuation, an ASCII map, an
override, two presets), `general/frame-kinds` (a locked phrase with every
frame kind), `phrom/one-phrom` (exactly 16 384 bytes),
`phrom/two-phroms` (spills into a second PHROM) and
`benchmark/synthetic-speech` (two speech-like clips of 1.5 and 3.4 s, for
timing Optimise, §17). The phrases with audio were
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
MainWindow ── IDialogs, IRecentFilesStore, IViewPreferences, ITaskRunner,
 │             ISynth, Player, EncodingTools (injected)
 ├── ProjectDocument: Project + QUndoStack + ProjectEdits + path
 ├── PhraseListModel (reads a Project snapshot, edits through ProjectEdits)
 │    └── PhraseListPanel: QListView + PhraseListDelegate + buttons
 ├── PhraseDetails (name, description; edits through ProjectEdits)
 ├── QSplitter
 │    ├── PhraseWaveforms (§16; the play start through ProjectEdits)
 │    └── EncoderPanel (§17; settings and encodings through ProjectEdits)
 ├── OptimiseSession + OptimiseDialog (§17)
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
  even for a command that makes itself obsolete. Each of the nine
  single-operation commands applies its `IProjectEditor` operation in
  `redo()` and the returned reverse in `undo()` (table in §14).
  `SetEncodingsCommand` applies `SetEncoding()` to several phrases as one
  step (Optimise, Re-encode); if one fails, those already changed are put
  back and nothing is pushed, and undo reverses them latest first. An undo
  or redo that fails regardless is logged and the command made obsolete,
  so the stack drops it.
- **`PhraseListModel`** is a flat list model of a snapshot: the main
  window calls `SetProject()` on every `Changed()`, which resets the model.
  `PhraseListPanel` remembers the selected ids before a reset and selects
  them again after, so the selection follows phrases that move. Rows carry
  roles for the id, the encoding state, the placement, the cost, the PHROM
  and word number, and `kPhromStartRole` for the first phrase of each
  PHROM after the first, taken from `PhromAllocation::phroms`. The
  delegate draws that row taller, with a "PHROM N" band; it draws the name
  of a phrase too large for a PHROM in the theme's warning colour (§16). Whether an encoding is current
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
  organisation and application name "Speak5220"). What the user chose to
  see and hear (whether the encoder panel shows its advanced settings, and
  the output filter chosen in the **Audio > Output Filter** menu, by its
  `OutputFilterInfo::key`, and the pin chosen in **Audio > Chip Output**,
  by its `OutputPinInfo::key`) is kept the same way through
  `IViewPreferences` (`SettingsViewPreferences`). `MainWindow::SetOutputFilter()`
  passes the filter to the `Player` and to `PhraseWaveforms`, ticks its
  menu item and saves it. `MainWindow::SetOutputPin()` does the same with
  the pin for `PhraseWaveforms` and the `OptimiseDialog`, first stopping
  encoded audio that was taken from the other pin.
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
  (Phase 9), and is left out rather than shown disabled.
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

---

## 16. Waveforms, playback and colours

**Implemented** (plan §5, Phase 6) in `app/src/gui/waveform/`,
`gui/playback/`, `gui/theme/` and `core/quality/encoded_audio`. The user's
view is [`docs/application/user-manual/waveforms.md`](../../docs/application/user-manual/waveforms.md).

**Structure.**

```
PhraseWaveforms ── ProjectEdits, Player, ISynth, ITaskRunner (injected)
 ├── WaveformPane "Original (8 kHz)" ─┐ stacked, equal heights
 ├── WaveformPane "Encoded"          ─┘
 │    each: title, note
 │          + WaveformView (PeakCache, TimeAxis, ThemeColours)
 ├── QScrollBar (shared)
 └── play Original, play Encoded, reset play start, Frame grid,
     zoom out, zoom in, fit (shared)
Player ── IAudioOutput (QtAudioOutput: QAudioSink), IClock, IResampler
ColourSchemeFollower ── IDesktopColourScheme (PortalColourScheme: D-Bus)
```

- **Time.** Both views count time in samples of the phrase's source
  audio (`TimeAxis`: the source sample at pixel 0 and samples per pixel,
  with fit, clamp, zoom about a pixel and scroll). A view's own samples
  are placed by a source offset: sample `s + offset` is drawn at source
  time `s`. The original's offset is 0.
- **Encoded audio.** `RenderEncodedAudio()` packs the cached frames,
  speaks them through `ISynth`'s VSM path and finds the offset: the sound
  (first to last non-idle code, idle at 0) is lined up with the source
  after the 60 Hz high-pass by `AlignByEnvelope()` and
  `RefineBySpectrum()`, the alignment the quality measures use, and the offset is
  the first sound's index less the lag. The level gain
  (`LevelMatchGain()`, the scale of `MatchLevel()`) is measured on the
  output placed by that lag, against the source's speech frames, and
  converted from codes to fractions of 16-bit full scale. With no source,
  or an output that never leaves idle, the offset is 125
  (`kFirstFrameStretch`) and the gain 1. It runs as
  a job on the `ITaskRunner`; the result comes back queued and is kept per
  phrase id with the frames it was made from, so reselecting a phrase does
  not render it again.
- **Drawing.** `PeakCache` keeps each block's lowest and highest sample at
  16, 64, 256, ... samples; a range takes the largest whole blocks inside
  it and recurses at its ends (O(log n)). From 4 samples per pixel out,
  each pixel is one line between the peaks of its samples and the last
  sample of the pixel before, so neighbouring lines meet and the trace is
  unbroken; closer in, the samples are joined by lines. Behind the encoded waveform each frame's
  stretch, encoded samples `[125 + 200 j, 325 + 200 j)` (from
  `encoder::kReachedSample`: half-way reached to half-way to the next), is
  filled by its kind, with an optional grid line where it starts; the
  hover text (`DescribeFrame()`) gives the frame's kind and indices.
- **Levels.** The chip's codes x 64 (`synth::kSampleScale`) take a
  quarter of 16-bit full scale, and the encoder aims at an RMS of 20
  codes, so the encoded audio is about 15 dB below a normalised
  recording. `PhraseWaveforms` shows and plays it multiplied by the level
  gain, and the encoded pane's note gives the gain in dB before any note
  passed in (such as "stale"). The frames and exports are unchanged.
- **Output filter.** With a filter (`PhraseWaveforms::SetOutputFilter()`),
  the encoded pane shows the level-matched samples through
  `FilterSamples()`: `FilterDacOutput()` at 48 kHz averaged back to one
  value per 8 kHz sample, so the view's sample positions, offset and frame
  grid are unchanged. The unfiltered samples go to the `Player`, which
  applies the filter itself. A change redraws from the render kept,
  restoring the extent and axis, and the note names the filter after the
  level gain. The original is never filtered.
- **Output pin.** The encoded audio is taken from the SPEAKER pin by
  default (`PhraseWaveforms::SetOutputPin()`): `PinSamples()` clips each
  code to the DAC's range before the level gain, as the chip clips before
  its analogue output, and the note gives `ClippedSamples()` when any are
  clipped. With the I/O pin the 10-bit codes are shown and played
  unclipped, and the note says so. The level gain is measured on the
  unclipped codes, so it is the same for both pins. A change redraws as
  for the filter. The quality measure, the encoder and exports always use
  the unclipped codes.
- **Shared time axis.** The panes are stacked with equal stretch, so the
  views are the same size. Both scroll over the union of their content extents, and each
  view's `AxisChanged()` sets the same axis on the other (a flag stops the
  echo); with the same axis, the same sound is at the same x. New
  contents (another phrase, or its encoded audio arriving) are placed
  without following and then both views are fitted to the union, so a
  phrase starts as Fit shows it. The shared
  zoom and fit buttons and the scroll bar act on the original's view (the
  encoded view's when there is no original), and the other follows; the
  scroll bar follows either view.
- **Squelch shading.** `EncoderPanel::ShownSquelch()` is the squelch the
  phrase shown would be encoded with as the form has it, edits not yet
  applied included (in the defaults scope, the form's defaults under the
  phrase's overrides; none for no phrase, a locked one or one without
  audio). `UpdateView()` emits `SquelchChanged()` when it changes, and the
  main window passes it to `PhraseWaveforms::SetSquelch()`, so the shading
  follows the slider at once, before the 600 ms debounce. `PhraseWaveforms`
  finds the stretches `encoder::SquelchedSpans()` lowers (gain under 1)
  when the squelch or the phrase changes, gives them to the original's
  `WaveformView::SetShading()`, and sets the original pane's note
  ("squelch 30 dB: shaded stretches are muted", or that nothing is quiet
  enough). The view veils them with the base colour at alpha 170 over the
  waveform, so it is dimmed, not hidden, and hatches them with the
  palette's Mid colour, so they read without colour; hovering says they
  are squelched. The audio shown and played is the recording as imported.
- **Play start.** A click asks for the marker at that source sample;
  `PhraseWaveforms` rounds and limits it to the audio and applies
  `ProjectEdits::SetPlayStart()` (`SetPlayStartCommand`, "Move Play
  Start"), so it is saved as `play_start` and undoable. Both views show
  it. A phrase without audio has none. Playback follows a move (and the
  reset button, even when the play start is already 0): a pane playing
  starts again from the new play start, and one paused stops, so the next
  play starts there rather than resuming.
- **Playback.** `Player` plays one buffer at a time: `Play(source,
  samples, start)`, `Pause()`, `Resume()` (a new buffer from the paused
  position) and `Stop()`. The original plays from the play start; the
  encoded audio from `play_start + offset`, limited to its samples. The
  playhead is the clock time since the device started, but no more than
  `IAudioOutput::ProcessedMicroseconds()`, so it neither runs ahead of the
  audio nor jumps with the device's buffer; a 33 ms `QTimer` moves it, and
  it stops at the end or when the output reports finished. If the device
  does not take 8 kHz (`PlaybackRate()`), the buffer is resampled with
  `IResampler`. With an output filter (`Player::SetOutputFilter()`),
  every source but the original is played as `FilterDacOutput()` gives
  it: each sample held for 6 values at 48 kHz, through the filter's
  second-order sections, the whole buffer at `Play()` so the filters'
  state at any start is as from the beginning, then resampled from 48 kHz
  if the device does not take it. The playhead still counts 8 kHz
  samples. Changing the filter stops the chip's output if it is playing
  or paused. `QtAudioOutput` plays mono 16-bit from a `QBuffer` in pull
  mode on the default device, using the device's channel count (each
  sample copied to every channel) if it refuses mono.
- **Colours.** `ColoursFor(palette)` returns the light or the dark set of
  `ThemeColours` (warning, wave, zero line, frame grid, playhead, marker,
  the four frame kinds) by whether the palette's window is darker than
  its text; each widget that uses them recomputes them on
  `QEvent::PaletteChange`. Everything else comes from the palette. The
  play/pause icons are drawn in the palette's button-text colour (the
  style's media icons are fixed black images) and redrawn on a palette
  change.
- **Following the desktop.** On GNOME, Qt 6.11 picks its GTK 3 platform
  theme, which reports the GTK theme's scheme (light) and palette, not
  GNOME's dark preference; the XDG desktop portal's theme reports dark
  but keeps the GTK palette. So `ColourSchemeFollower` reads the
  preference from `IDesktopColourScheme` and requests it with
  `QStyleHints::setColorScheme()` (unset for no preference), which switches
  the style to its dark or light palette and sends every widget a palette
  change. `PortalColourScheme` reads `org.freedesktop.appearance
  color-scheme` with the portal's `ReadOne()` (or the older `Read()`),
  waiting at most a second, and follows `SettingChanged`. It is built
  where Qt D-Bus exists (Linux, the BSDs); elsewhere
  `NoDesktopColourScheme` leaves Qt to follow the system.

**Deviations from the plan.**

- The plan offsets the encoded pane by the emulator's start latency. The
  encoder leaves out leading silence and the project does not record
  where its first frame starts, so a fixed latency would not line the
  panes up; the offset is found by `speechqual.py`'s alignment instead.
- The play start is a project edit, undoable, which the plan does not
  say; it is saved in the project as `play_start`, so changing it marks
  the project modified.
- One play start serves both panes (the encoded pane plays from the same
  sound), rather than one marker per pane.
- The plan's Phase 6 item 1 asks for the dark-mode cause to be fixed in
  the build. No plugin was missing: the default GTK 3 theme does not
  report GNOME's preference, so the fix is in the application, through
  the portal.

**Acceptance** (plan Phase 6):

- unit tests check the colours of a light and a dark palette (different,
  and at least 4.5:1 for the warning text and 3:1 for the marks against
  the background and the frame fills), the portal's values, that each
  preference and change is applied, and that the phrase details follow a
  palette change;
- unit tests check the peak cache against a plain scan, the
  sample-to-pixel mapping, fit, zoom and scroll limits;
- unit tests check `Player` with a mocked output and a fake clock (the
  playhead, pause and resume, the end, resampling, failures);
- unit tests map encoded samples and pointer positions to frames,
  including the start latency, and show the same sound at the same place
  in both panes, and that the shared controls move both;
- a GUI functional test speaks a fixture phrase through the real
  emulator and draws both panes offscreen;
- still to be checked by hand: that the playhead follows the sound on
  Linux with PipeWire, and that the window starts light or dark to match
  the desktop and follows a change, on GNOME, KDE, Windows and macOS.

---

## 17. The encoder panel and Optimise

**Implemented** (plan §3.3, §5, Phases 7 and 8) in `core/optimiser/`,
`gui/encoder_panel/` and `gui/optimise/`. The user's view is
[`docs/application/user-manual/encoder.md`](../../docs/application/user-manual/encoder.md).

**Structure.**

```
EncoderPanel ── ProjectEdits, EncodingTools, ISynth, ITaskRunner
 ├── state, FramesText/KindsText/LevelText (FrameStatistics, EncodingFacts)
 ├── QualityRatingView: RatingBar, the QualityText line, the components'
 │   bars and the FiguresText line (QualityAssessment)
 ├── "Settings for": this phrase | project defaults
 ├── EncoderSettingsForm (from EncoderOptionInfos())
 └── Re-encode, Optimise..., Reset
OptimiseSession ── ProjectEdits, EncodingTools, ITaskRunner (candidates)
 └── core::optimiser::Optimiser ── IEncoder, OverallQualityMetric, ITaskRunner
OptimiseDialog ── OptimiseSession, Player, ISynth, ITaskRunner (A/B)
EncodingTools: IEncoder, IQualityAssessor, IClock,
               ITaskRunner (Optimise's search, one at a time)
```

**The optimiser** (`Optimiser::Run()`, plan §3.3). The search starts from
the phrase's options and is in two stages:

1. **Grid.** The start, then the Cartesian product, around it, of the
   pre-emphasis (the start's, and the other kind: chirp, or first order
   with μ 0.9, the reference data's "lean" value), the voicing threshold
   (±0.05), the voicing evidence (±2), the repeat trade-off (×0.5, ×2)
   and the target RMS (±4): 162 points.
2. **Coordinate descent** from the best so far. Each round proposes one
   step down and one up along every searched option (the target RMS,
   repeat trade-off, silence gate, pre-emphasis μ, window, lag window,
   voicing threshold and evidence, slow gain control) and the other
   pre-emphasis kind, and evaluates them together. It moves to the best if its
   objective is higher than the current one's; otherwise every step is
   halved, down to its smallest, and the search ends when no step can be
   halved. Values are rounded to a few decimals (whole numbers for the
   window) so the options' text stays short, and kept within
   their ranges. Plan §3.3 mentions Nelder–Mead as an alternative for the
   continuous options; plain coordinate steps suffice so far.

The squelch is never varied: every candidate keeps the start's, and is
scored against the source squelched by it, computed once per search.

At most 400 evaluations and 10 rounds (`SearchPlan`). Options already tried
are not tried again (keyed by every option's text). Each candidate is
encoded; its packed size is checked against the per-phrase byte limit
(`kOverLimit`) and then the budget callback (`kOverBudget`), and only a
candidate within both is measured (`kScored`), since the quality
measures are most of the cost. The objective is the overall quality score (`OverallQualityMetric`), or
with a bit-rate weight λ, the score less λ per kbit/s (the dialog offers
0 to 3, 0.03 by default) (plan §3.3 step 4); a lower-is-better metric is negated.
The best is the scored candidate with the highest objective, ties going to
the lowest index.

**Determinism.** Each batch (the start, the grid, a round) is posted to the
workers one candidate per job; the thread running the search waits on a
condition variable (it must not be one of the workers, since it blocks)
and reports progress after each candidate finishes. Results are placed by
index and reduced in index order, so the history and the best are the
same whatever the thread count. The functional test runs a search on 1
and 8 threads and compares every candidate's options, outcome, size and
score bit for bit. Cancelling is an atomic flag checked before each
candidate; the history keeps the cancelled ones (`kCancelled`).

**The encoder panel.**

- `ShowPhrase(project, id)` is called on every change of the project. The
  panel keeps the phrase's audio, overrides and encoding, and whether the
  encoding is current (`EncodingCache::IsCurrent()`).
- A form edit records the options and a deadline 600 ms ahead on the
  `IClock`, and starts a single-shot `QTimer`; `DebounceTick()` applies
  the change once the clock passes the deadline, or waits again for the
  rest. So a burst of edits gives one encoding, and the tests drive it
  with a fake clock. A change still waiting is applied before another
  phrase is shown or the scope changes, and stays in the form while it
  waits or encodes.
- For a phrase, the change becomes `OverridesFor(defaults, options)` and
  an encoding job on the `ITaskRunner`: `EncodePhrase()` encodes, then
  `MeasureFrames()` assesses the quality (`IQualityAssessor`) against the
  source as the phrase's squelch leaves it (`SquelchedSource()`), and
  finds the peak code (the frames spoken through
  the shared, cached `ISynth`, which the waveforms then reuse). The result
  comes back queued and is stored with `SetEncoding()`, the quality's
  figures in the `CachedEncoding` (`ToCachedQuality()`); a result superseded by a later encoding of the
  same phrase is dropped. A refusal (the budget) is reported and the form
  reloaded.
- For the project's defaults, the change is `SetDefaultEncoder()`.
- Cached quality is shown as stored (`FromCachedQuality()`, rescored).
  Frames without it (encoded by the CLI, or with figures of other
  measures, dropped on load) are measured in the background for display
  only; the panel keeps the measurement, keyed by
  the frames, and does not change the project.
- The limiter's activity (`LevelStats`) comes from the encoder's details,
  so it is known only for encodings made in the session.

**The settings form.** One row per `EncoderOptionInfo`, in two columns:
first the five basic options (target RMS, repeat trade-off, squelch,
pre-emphasis, voicing threshold: the ones with the clearest effect on the
sound or the size, among them four of the five options Optimise's grid
searches, all but the voicing evidence), then a checkable `QToolButton`,
then the six with `advanced` set, in the same grid so the columns line up. The advanced rows
are hidden until the button is down; the button counts the advanced rows
that differ from the baseline ("Advanced settings (2 changed)"), so a
hidden change is never missed, and hidden options still apply.
`SetEditable()` disables the rows but not the button. `MainWindow` sets
the button from `IViewPreferences` and saves it when the user toggles it.
`EncoderOptionInfo` gives each option a `unit` and a typical range and
`step` (within its valid range), which the GUI's sliders use:
`OptionSlider` is a `DefaultTickSlider` (a `QSlider` that paints a tick
under its groove at the baseline's position) with the value's text and
unit beside it, all of one width so the sliders line up (the analysis
window in milliseconds; a ratio reads "off" at 1). Its positions are the
typical range in steps; a value outside it widens the range to include
it, and a value off the steps is shown exactly and kept until the slider
moves. Real and whole-number options get a slider; the optional ones
(the squelch, the resonance hold) a check box and a slider; the pre-emphasis
a chirp/first-order chooser and a slider for μ. Values are read back
through `SetEncoderOption()` with `FormatNumber()`; the form keeps the
options it was given and changes only the edited one.

**The quality rating.** `RatingBar` draws the three bands (*Poor* from 0,
*Fair* from `ScoreCalibration::kFairFrom`, *Good* from `kGoodFrom`) in
`ThemeColours::quality_bands` (each a different colour), each band's name
inside it, at most 520 pixels wide, the overall score's mark (a line and a
triangle in `quality_mark`) and an optional dashed mark for an earlier
score (Optimise's start). The mark's colour has a contrast ratio of at
least 4.5:1 with every band in both palettes, so it reads without colour
(WCAG's ratio is of luminance alone); the line under the bar gives the
rating and the score in words ("Fair · 0.62"). Under it, a `QProgressBar`
per component with its score as text ("not measured" for intelligibility
without ESTOI), and the figures behind them (`FiguresText()`); Optimise's
window shows the bar and the line alone, since its candidates carry only
the overall score. The tooltip explains each measure (`QualityToolTip()`).

**Optimise in the GUI.** `OptimiseSession::Start()` copies the project and
posts one job to `EncodingTools::optimiser_driver` (a one-thread
`ThreadPoolTaskRunner` in the application), which runs the optimiser on
each phrase in list order with its candidates on the window's runner. Each
phrase's budget is checked against the copy with the earlier phrases'
results stored in it, so the results fit together. Progress (the counts,
the start and best so far, and new history rows) comes back queued per
candidate. A phrase whose best differs from its start gets an
`EncodingChange` (overrides, frames, and the quality's figures, the best
frames assessed once more, since the search keeps only their score);
`Accept()` stores them
all with one `SetEncodingsCommand`, and cancelling or `Discard()` drops
them. The session's destructor cancels and waits for the driver, and the
window waits for it before its runner. `OptimiseDialog` is window-modal,
so the project cannot change under a search.

**Listening before accepting.** When a search finds changes, the dialog
offers each changed phrase (a combo box, hidden for one) with two play
buttons: Original, the frames it has now (what Revert keeps), and
Optimised, the frames found (what Accept keeps). Only the best
candidate's frames are kept by the search, and the stored frames are what
Revert leaves, so they are the honest comparison. Each is spoken with
`RenderEncodedAudio()` on the runner once per search (a generation count
drops speakings from an earlier one) and played at the recording's level
(`level_gain`) from the chosen pin (`OptimiseDialog::SetOutputPin()`),
as the encoded pane plays it; the dialog keeps the `EncodedAudio` and
makes the samples when a play starts. They play through the
window's `Player` as `PlaybackSource::kBeforeOptimise` and `kOptimised`,
through the output filter if one is chosen, which `PhraseWaveforms` gives no playhead; the dialog stops them on
Accept, Revert, Cancel or another phrase. The play/pause icons, drawn in
the palette's colours, are shared with the waveforms
(`gui/playback/playback_icon`).

**Re-encode.** Phrase > Re-encode posts one job that encodes and measures
the selected phrases that are not encoded or stale (by the list model's
state; unlocked, with audio) with their own settings, and stores them with
one `SetEncodingsCommand`. It is disabled when there are none; likewise
the panel's Re-encode button is enabled only for a phrase with no frames,
stale frames, or a change waiting.

**Deviations from the plan.**

- The plan names `QtConcurrent`; the optimiser uses `ITaskRunner`, as the
  other background jobs do (§14).
- Cancelling discards the results, as the plan's acceptance asks, rather
  than offering the best found so far.
- The project's defaults are edited in the panel (a scope chooser), as an
  undoable operation the plan did not list (`SetDefaultEncoder()`).
- Phrase > Re-encode (a selection, as one step) is not in the plan; it is
  needed after a change of defaults, or after adding phrases, which are
  added unencoded.
- The benchmark fixture is synthetic speech, like the other fixtures,
  rather than corpus recordings, which are derived data (TESTING.md).

**Results** (plan Phase 7, item 4).


- **Corpus** (`corpus-check.sh`, all 243 files, from the default options,
  each with a byte limit at its default encoding's size, maximising the
  overall quality score): the score rose on 241 files and stayed level on
  two; median gain 0.058, mean 0.074, largest 0.33; every best within its
  limit. The ratings moved from 120 *Good*, 114 *Fair* and 9 *Poor* to
  188, 53 and 2 (median 0.70 to 0.77). 1.6 s per second of speech on 16
  threads, 18 minutes for the 673 s of speech. The test asks for a rise
  on more than half the files.
- **Benchmark** (`benchmark.sh`, `benchmark/synthetic-speech`, 4.9 s of
  speech, no byte limit): 16.4 s per second of speech on one thread, 2.0
  s on 16; 270 to 295 candidates per phrase. A candidate costs what it
  did with PESQ (the measures take as long as PESQ with its alignment);
  the search goes further, since the score keeps finding improvements
  where PESQ stopped (212 to 240 candidates, 11.6 s on one thread). A
  phrase of a few seconds still takes a few seconds on a many-core
  machine.
- Most of a candidate's time is the quality measures (with the output
  alignment's spectral search, §9), then the encoder. Nothing has been optimised for
  speed; plan risk R3's shared analysis between candidates is not needed
  yet.

**Acceptance** (plan Phase 7):

- unit tests of the optimiser with a mocked encoder and metric on an
  inline runner: the grid around the start (and the target peak when set),
  descent to an optimum off the grid, ties to the lowest index, no options
  tried twice, candidates over the budget or the byte limit kept but never
  chosen and not measured, the bit-rate weight, a lower-is-better metric,
  failures, the evaluation limit, cancelling, progress, and options kept
  in range and short;
- a functional test finds the same result on 1 and 8 threads;
- GUI unit tests: a setting change re-encodes once after the debounce (a
  fake clock); Reset restores the project's defaults as one undoable
  step; the defaults scope; a refused change; the quality stored with
  the encoding, and cached quality shown without measuring; the rating's
  text, components, figures, tooltip and marks; the squelch turned on by
  its check box, and a squelched phrase measured against its squelched
  source; the squelch
  reported as the form has it and shaded on the original waveform; the mark's contrast with every band in light and dark;
- GUI unit tests of Optimise with inline runners: accepting is one
  undoable step, Optimise all runs in list order and leaves out locked
  phrases, the history, the byte limit, and cancelling leaves the
  settings unchanged; the window's stages;
- `corpus-check.sh` and `benchmark.sh` run Optimise (results above).
