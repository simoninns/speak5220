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
| `s5core`: `packaging/` (Acorn PHROM, TI indexed, Speak External, C headers) | Implemented |
| `s5core`: `bbc/` (the BBC BASIC II tokeniser, DFS disc images, the BBC Micro disc and its player) | Implemented (§19) |
| `s5core`: `audio/`, `dsp/` (WAV files and in-memory WAV, resampler, FFT adapters; filters, NumPy-ordered sums, LPC analysis, the noise gate, the de-esser, the normaliser, the output filters) | Implemented |
| `s5core`: `preprocess/` (the noise gate, bass cut, treble tilt, de-esser and normaliser before encoding, and their presets) | Implemented (§8, plan Phase 11) |
| `s5core`: `encoder/` (`Encoder` and its stages, `EncoderOptions`) | Implemented, matching the Python reference's frames on the whole corpus |
| `s5core`: `quality/` (the libfvad and SPTK adapters, the signal preparation, the spectral envelope, pitch and voicing, ESTOI, the overall score and rating, the assessor; the encoded audio for display; the anchor vocoder) | Implemented, matching independent implementations on the corpus subset; the score calibrated and confirmed by listening |
| `s5core`: `project/` (`Project` and its editing operations, PHROM allocation, the encoding cache, the `.s5proj` file in ZIP and directory form, import, the exports) | Implemented |
| `s5core`: `optimiser/` (`Optimiser`: the grid, the coordinate descent, the budget and byte limit) | Implemented (§17) |
| Reference data in `app/golden/` (emulator, packaging, encoder, quality, BBC BASIC II tokenisations, output hashes) | Implemented |
| Fixture projects in `app/projects/` (`general/`, `phrom/`, `bbc/`, `benchmark/`) | Implemented |
| `speak5220` GUI | Implemented: the main window, projects (open, save, recent files, the unsaved-changes prompt), the phrase list with PHROM dividers, drag and drop and changed-phrase marks, revert to saved, phrase names and descriptions, undo/redo, the status bar (§15); the Original, Preprocess and Encode tabs with their waveforms, the play-start marker, playback, light and dark colours (§16); the encoder panel, re-encoding, Optimise and Optimise all (§17); the Export dialog (§18) |
| `speak5220-cli` | Implemented: `new`, `add`, `encode`, `export` (presets, or one format with its options), `--output`; version and logging options |
| Test tree and lanes (TESTING.md) | Implemented |
| Format, lint, pre-commit hooks, CI on Linux | Implemented |
| CI on Windows and macOS (release tags and manual runs) | Implemented; a manual run passes both lanes on both |
| Nix dev shells and package | Implemented |
| Export: Acorn PHROM images, C headers, WAV, presets, the output hashes (§18) | Implemented |
| Export: the BBC Micro disc (§19) | Implemented; checked on MAME's emulated BBC Micro. The BBC Micro hardware checks (plan Phase 10, tasks 1 and 6) are still to be run |

---

## 2. Repository layout

```
app/
  CMakeLists.txt                top level: in-source guard, C++20, options
  .clang-format  .clang-tidy  CPPLINT.cfg  .gersemirc
  vcpkg.json                    the libraries for Windows and macOS (§3)
  cmake/                        CompilerWarnings.cmake, Dependencies.cmake,
                                QualityTargets.cmake, GitVersion.cmake,
                                EmbedText.cmake (§19)
  scripts/                      check-spdx.sh, check-header-guards.sh,
                                clang-format-check.sh, clang-tidy.sh,
                                format.sh, lint.sh; corpus-check.sh,
                                benchmark.sh, arctic-project.sh,
                                generate_corpus.py,
                                generate_filter_constants.py (§8);
                                phrom-check.sh, phrom_roundtrip.py,
                                option_sweep.py, phoneme-check.sh,
                                phoneme_corpus.py, phoneme_prompts.csv (§9);
                                run-projects.sh, output-hashes.sh (§18),
                                bbc-check.sh (§19)
    sweeps/                     option sets for option_sweep.py (§9)
    bbc/                        the MAME scripts of the BBC Micro check:
                                player.lua, tokenise.lua, basic_in_mame.py,
                                emulator_check.py (§19)
    golden/                     generate_golden.py and its scenario scripts,
                                encoder.py, measures.py, basic_programs.py
                                (offline, run by hand: §13, §19)
  golden/                       reference data from the retired references (§13)
  projects/                     fixture projects, unpacked: general/, phrom/,
                                bbc/, benchmark/ (§14, §17, §19)
  src/
    core/                       library "s5core" (QtCore at most)
      util/                     Status/StatusOr, logging, logging options, version,
                                number text, SHA-256, task runners, file digests,
                                IClock
      lpc/                      coding tables, Frame, FrameCodec, BitReverse,
                                FrameStatistics
      emulator/                 Tms5220, Tms6100, ChipDriver, interfaces
      synth/                    Synth, ReferenceSynthesizer, OutputPin
      packaging/                AcornPhrom, TiIndexed, SpeakExternalStream,
                                CHeader (§18)
      audio/                    WAV interfaces, the libsndfile adapters, the
                                24-bit grid of project audio
      dsp/                      resampler and FFT interfaces and adapters;
                                SOS/FIR filters and their constants,
                                NumPy-ordered sums and windows, LPC analysis,
                                the noise gate, de-esser and normaliser
      preprocess/               Preprocess(), PreprocessOptions and the
                                presets (§8)
      encoder/                  Encoder and its stages, EncoderOptions (§8)
      quality/                  the quality measures, the adapters over
                                libfvad and SPTK, the overall score,
                                QualityAssessor, the anchor vocoder, the
                                loudness figures and the frame track
                                comparison (§9), the output alignment and
                                RenderEncodedAudio (§16)
      bbc/                      BasicTokeniser, DfsDisc, BbcDisc and the
                                player's source, speak.bas (§19)
      project/                  Project, IProjectEditor, the PHROM allocator,
                                EncodingCache, ProjectSerializer, the project
                                stores, PhraseImporter, the exports (§14,
                                §18)
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
      export/                   ExportDialog (§18)
      waveform/                 PhraseWaveforms (the phrase's tabs),
                                WaveformPane, WaveformView, PeakCache,
                                TimeAxis, the frame map, GripSplitter (§16)
      playback/                 Player, IAudioOutput, QtAudioOutput,
                                PlayPauseIcon (§16)
      theme/                    ThemeColours, the desktop colour scheme
                                and its portal adapter (§16)
      undo/                     one QUndoCommand per editing operation,
                                ProjectEdits
    cli/                        the "speak5220-cli" executable and its commands
  resources/                    the icon set (hicolor PNGs, .ico, .icns),
                                the About box's logo, the .qrc, the Windows
                                .rc and the Linux .desktop file, generated
                                by scripts/generate-icons.sh from assets/logo/
  tests/
    .clang-tidy                 the app's settings, plus one test exception
    support/                    speak5220_test_support (helpers, mocks, golden
                                readers); dfs_catalogue.py, the independent
                                DFS reader (§19)
    unit/  functional/          speak5220_unit_tests, speak5220_functional_tests
    gui/support/                speak5220_gui_test_main (QApplication, offscreen),
                                the GUI's mocks and fakes, test palettes
    gui/unit/  gui/functional/  speak5220_gui_unit_tests, speak5220_gui_functional_tests
```

Planned additions, each created with its first code (plan §2.1):

- The AppStream metainfo in `app/resources/`, and `app/packaging/` (Flatpak,
  WiX/CPack, macOS).

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
    `QJsonDocument`, WAV inputs as doubles), `EncoderOptionsFromText()`
    (the `KEY=VALUE,...` options the by-hand checks take) and
    `CompareEncoding()`, which applies the acceptance rule of §8;
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
  `corpus-check.sh`), the timings (`EncoderBenchmark`,
  `QualityBenchmark` and `OptimiserBenchmark`, `benchmark.sh`), and the
  encoder-quality diagnostics (`PhromRoundTripCheck`, `phrom-check.sh`;
  `PhonemeCheck`, `phoneme-check.sh`; §9).
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
| `OutputPin`, `PinSamples()`, `ClippedSamples()` | **Implemented** (`core/synth/output_pin`): the encoded audio from the SPEAKER pin, each code clipped to the DAC's −128…+127 (`emulator::kDacMin`, `kDacMax`) and converted by `DacOutput()` with the DAC's measured bit weights (datasheet §8.2, Table 8-2; the step between −1 and 0 is about six steps), or from the I/O pin's unclipped 10-bit codes, for listening and display only (§16) |
| `AcornPhrom`, `TiIndexed`, `SpeakExternalStream` | **Implemented** (`core/packaging/`): the §12.8 and §12.9 layouts, byte-identical to the Python encoder's |
| `CHeader` | **Implemented** (`core/packaging/c_header`): PHROM images or Speak External streams as a C header (§18) |
| `SndfileWavReader`, `SndfileWavWriter` (`IWavReader`, `IWavWriter`) | **Implemented** (`core/audio/`): any WAV variant read and mixed to mono as floats; 16- or 24-bit PCM written, clipped |
| `SndfileWavCodec` (`IWavCodec`) | **Implemented** (`core/audio/sndfile_wav_io`): the same, to and from bytes in memory (libsndfile's virtual I/O), with integer PCM read and 24-bit PCM written exactly (§14) |
| `SincResampler` (`IResampler`) | **Implemented** (`core/dsp/resampler`): libsamplerate `SRC_SINC_BEST_QUALITY` |
| `PocketFft` (`IFft`) | **Implemented** (`core/dsp/fft`): real FFTs with NumPy's conventions |
| Filters, sums, LPC analysis | **Implemented** (`core/dsp/iir_filter`, `array_math`, `lpc_analysis`, `filter_constants.h`): §8 |
| `OutputFilter`, `FilterDacOutput()`, `FilterSamples()` | **Implemented** (`core/dsp/output_filter`): the DAC's held samples at 48 kHz through a computer's audio stage (none, or the BBC Micro's), for listening and display only (§16) |
| `NoiseGate()`, `NoiseGateGain()`, `GatedSpans()` | **Implemented** (`core/dsp/noise_gate`): mutes stretches below a level relative to the loudest, opening and closing at a set speed, for the `noise_gate_db` and `noise_gate_ms` options (§8), and reports the stretches it turns down by more than 6 dB for the waveform's shading (§16) |
| `DeEss()`, `DeEsserGain()` | **Implemented** (`core/dsp/de_esser`): turns a recording's treble band down where it is most of the sound, for the `de_ess_db` option (§8) |
| `Normalise()`, `NormaliserGain()`, `SpeechLoudness()` | **Implemented** (`core/dsp/normaliser`): speech-gated loudness, a soft-knee compressor, make-up gain and a lookahead peak limiter, for the `normalise_db` and `normalise_ratio` options (§8) |
| `CentredRms()`, `SpreadDips()`, `SpreadPeaks()` | **Implemented** (`core/dsp/envelope`): the level and gain envelopes the noise gate, de-esser and normaliser share (§8) |
| `MainWindow`, `ProjectDocument` (`IDialogs`, `IRecentFilesStore`, `IViewPreferences`) | **Implemented** (`gui/main_window/`): menus, projects, imports, the status bar (§15) |
| `PhraseListModel`, `PhraseListDelegate`, `PhraseListPanel` | **Implemented** (`gui/phrase_list/`): the phrase list (§15) |
| `PhraseDetails` | **Implemented** (`gui/phrase_editor/`): name and description (§15) |
| `ProjectEdits` and the undo commands | **Implemented** (`gui/undo/`): one `QUndoCommand` per `IProjectEditor` operation (§15) |
| `PhraseWaveforms`, `WaveformPane`, `WaveformView`, `PeakCache`, `TimeAxis`, `GripSplitter` | **Implemented** (`gui/waveform/`): the Original, Preprocess and Encode tabs, their waveforms, shared time axis and controls, the play start (§16) |
| `Player`, `QtAudioOutput` (`IAudioOutput`), `SteadyClock` (`IClock`) | **Implemented** (`gui/playback/`, `core/util/clock`): playback and the playhead (§16) |
| `ThemeColours`, `ColourSchemeFollower`, `PortalColourScheme` (`IDesktopColourScheme`) | **Implemented** (`gui/theme/`): light and dark colours, following the desktop (§16) |
| `RenderEncodedAudio()` | **Implemented** (`core/quality/encoded_audio`): the frames spoken and lined up with the source (§16) |
| `Preprocess()`, `PreprocessOptions`, `PreprocessPresets()` | **Implemented** (`core/preprocess/`): the noise gate, bass cut, treble tilt, de-esser and normaliser that make the preprocessed recording an encoding aims at and is measured against, and the presets (§8) |
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
| `RunExport()`, `BuildAcornPhroms()`, `ExportFileNames()`, `CheckEncodingsCurrent()`, `WriteExportedFiles()` | **Implemented** (`core/project/project_export`): a preset's Acorn PHROM images, C headers or WAV files (§18) |
| `ExportDialog` | **Implemented** (`gui/export/`): the Export dialog (§18) |
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
function's comment names its Python original, and says where the C++
departs from it (below, and "Level setting").

**Three stages** (plan Phase 11). A phrase is shown and measured as the
original recording, the **preprocessed** recording made from it, and the
encoding of that. `core/preprocess/` holds the preprocessing, which the
reference does not have: `PreprocessOptions` (`noise_gate_db` and its
speed `noise_gate_ms`, `bass_cut_hz`, `treble_tilt`, `de_ess_db`,
`normalise_db` and its ratio `normalise_ratio`, all off by default) and
`Preprocess()`, which gates the original (`dsp::NoiseGate()`), then cuts
its bass
(`ButterworthHighPass()`, an order-2 Butterworth high-pass designed as
`scipy.signal.butter()` does, run forwards and backwards: −6 dB at the
cut, 24 dB per octave below it), then lifts its treble (1 − μz⁻¹), then
de-esses it (`dsp::DeEss()`), then normalises it (`dsp::Normalise()`):
the studio order of gate, equaliser, de-esser, compressor. It returns two
signals: `samples`, the preprocessed recording, and `voicing`, the gated
recording before the filters and the de-esser, times the normaliser's
gain worked out from `samples`, so the encoder's extent and voicing see
the same levels as its spectra.
`PreprocessPresets()` gives the GUI's presets, *None* and *BBC Micro*
(200 Hz, 0.4; §9, "Voicing and source shaping, from Acorn's PHROM"), and
`MatchingPreset()` the preset some options match (ignoring the gate's
speed and the normaliser's ratio while they are off). Everything that
measures an encoding (the quality measures in the GUI, Optimise, the
corpus checks) compares it with `Preprocess(...).samples`.

The preprocessing options are part of `EncoderOptions`, as its
`preprocess` member, so a phrase's settings stay one value: one set of
defaults, one set of overrides, one settings hash and one undoable step.
`EncoderOptionInfos()` marks each option's stage (`OptionStage`), which
the settings form, the project file and the documentation group by.
`IEncoder::Encode()` takes the original and preprocesses it itself, so
every caller encodes the same way; the stages work on the preprocessed
recording as below.

**Stages.** `Encoder` (`IEncoder`) composes five injected stages, in the
order of `encodeDetailed()`. `MakeEncoder()` returns one with the standard
stages over `PocketFft`.

| Stage (interface → class) | Python | Does |
|---|---|---|
| `IFrameAnalyser` → `FrameAnalyser` | `analyse()` | `Segment()`: the 60 Hz high-pass, the level envelope, the gate, the onset and offset, the frame centres from c0. `Features()`: each frame's level, its loudness (the level after the loudness high-pass, not in the reference) and the lag-windowed autocorrelations (pre-emphasised for voiced, plain for unvoiced) |
| `IPitchTracker` → `YinPitchTracker` | `trackPitch()`, `_yin()`, `_candidates()`, `_tiltAndCrossings()` | The voicing and period every 5 ms: YIN on the 1 kHz low-pass, up to five candidates per hop, and a Viterbi path, its period held near RAPT's where RAPT finds voicing (the anchor, which the reference does not have, below) |
| `IVoicingClassifier` → `VoicingClassifier` | the voicing part of `analyse()` | Each frame's voicing by majority of its three nearest hops (`CentredVoicing()`); onsets voiced from the hops the chip plays (`VoiceOnsets()`, which the reference does not have, below); then the octave fix (`CorrectOctaves()`). `SmoothVoicingRuns()`, which the reference does not have either, then removes islands shorter than `voicing_min_run` frames (below) |
| `IRepeatSearch` → `RepeatSearch` | `_kindsAndK()`, `_segment()`, `_repeatRun()` | K quantised per frame; silence, full frames and repeats by dynamic programming over bits plus `repeatLambda` × distortion |
| `ILevelSetter` → `LevelSetter` | `_shapeLevels()`, `_setLevels()`, `_gainOf()`, `_effectiveK()`, `_outLevels()` | Slow gain control; open-loop energies, five closed-loop passes through `ReferenceSynthesizer`, the too-loud check, the limiter (a ceiling of 160 and a clip budget, where the reference holds every code within 120) and the safety net, all on loudness ("Level setting", below) |

Between the stages, `Encoder` drops a faint opening (`TrimFaintOpening()`,
on the frames' levels), shapes the frames' loudness (the reference shapes
their levels), sets the gain from `target_rms`, and turns the plan and energies into
frames with `frame_endings` (`ToFrames()`, `GuardK510()`, `AddFades()`,
from `_toFrames()`, `_guardK510()` and `_addFades()`, but with fewer
fades, below), and last quietens
the pauses (`QuietPauses()`, below), which the reference does not do. Before the levels are set, with `resonance_hold_db`, it holds
resonant filters through a fade (`HoldResonantFilters()`, in
`repeat_search`; below), which the reference does not have either. Two
more steps the reference does not have come between the voicing and the
features. `SmoothVoicingRuns()` gives a run of voiced or unvoiced frames
shorter than `voicing_min_run`, with the other kind on both sides, its
neighbours' voicing: unvoiced islands first, with the mean of the
neighbours' periods, then voiced islands. The segmentation, pitch and
voicing are found in the preprocessed recording before its filters
(`voicing`); with `bass_cut_hz` or `treble_tilt` set, the preprocessed
recording (`samples`) is segmented again, and the features take its
high-passed signal, gate and loudest level, while the extent, the frame
centres, the pitch and the voicing stay as before. Brightening the
recording first and finding the voicing in it made 70% of one phrase's
frames unvoiced, heard as noise on a BBC Micro. The quantisers
(`QuantiseK()`, `PitchIndex()`, `EnergyIndex()`) are in `quantiser`, and the
filters (`HighPass60Hz()`, `LowPass1kHz()`, `PreEmphasisFilter()`,
`LoudnessHighPass()`) in `encoder_filters`.

**Level setting** (encoder-quality plan, Phase 3; 2026-10-13). Two
departures from `_setLevels()`, measured in §9, "Level and loudness, from
the PHROM round trip":

- *Loudness.* The level the energies aim at, and the level the closed loop
  measures of its output, are taken after the loudness high-pass
  (`LoudnessHighPass()`: the first-order high-pass at 279 Hz of the BBC
  Micro's speech path, §16, by the prewarped bilinear transform), so bass
  does not count as loudness: `FrameFeatures::loudness`,
  `MeasureOutputLoudness()`, and `GainOf()` with `loudness` for the
  open-loop estimate (the filter's power spectrum weighted by the
  high-pass's). `target_rms` is therefore the RMS over the sounding frames
  after that high-pass. The faint-opening trim, the gate and the resonance
  hold keep the plain level. A bass-heavy recording, whose rumble held its
  level down, comes out louder; the quality measures' `high_passed_rms` is
  the same filter.
- *Limiter.* The reference holds every output code within 120
  (`peakLimit`), so the peaks of speech, about 15 dB over its RMS, set the
  level rather than `target_rms`. Acorn's PHROM A drives the DAC into
  clipping on half its words, 0.21% of its samples on average
  (encoder-quality investigation §4.1). The limiter now allows that: no
  frame's peak over `kPeakCeiling` (160), no lattice wrap (datasheet
  §12.3), and at most `kClipBudget` (1 in 1000) of the phrase's samples
  beyond the DAC's ±127. Each pass lowers a frame whose peak passes the
  ceiling by the steps that bring it under (3 dB each), and while the
  phrase clips more than the budget, lowers by a step the frames whose
  windows clip the most, enough of them to cover the excess; a peak or
  wrap outside every window lowers the loudest frame. The safety net
  lowers everything until all three hold. The ceiling and the budget are
  constants (plan D2).

**Onsets and fades** (encoder-quality plan, Phase 4; 2026-10-13). Two
departures from `analyse()` and `_addFades()`, measured in §9, "Onsets
and fades, from the PHROM round trip":

- *Onsets.* The chip plays a frame's voicing from the frame's centre to
  the next frame's, 25 ms of input (datasheet §8.6 and §8.7), but the
  centred vote looks at the centre ±5 ms. A vowel starting just after a
  centre was voiced a frame late, and the late frame was played as noise
  at the vowel's level. So `VoiceOnsets()` voices an unvoiced frame whose
  next frame is voiced when most of the five hops from its own centre on
  (hops 0 to +4) are voiced, with the median period of those hops; a
  frame it voices does not count as voiced for the frame before. Offsets
  keep the centred vote, since a forward vote there ends voicing early.
- *Fades.* The reference follows every unvoiced frame before silence or
  the end with repeats at a third of the energy each, down to energy
  value 2: one to three frames of hiss after every final S, CH or K,
  which Acorn's PHROM does not have (its words decay in their own frames).
  `AddFades()` fades only an unvoiced frame at energy index 6 (value 8) or
  more, which would stop audibly (datasheet §12.2), and then by one frame
  at a third of its energy (`kFadeFromEnergy`, `kFadeStep`). The K5-K10
  guard follows as before.

**The pitch anchor** (encoder-quality plan, Phase 5; 2026-10-13). A
departure from `trackPitch()`, measured in §9, "The pitch anchor". YIN
works on the 1 kHz low-passed signal, which can repeat more strongly at a
harmonic than at the period: a first formant on the 4th harmonic, with a
period that wanders from cycle to cycle, put `six` (Daniel) at 471 Hz for
a voice of about 107 Hz, and the prior towards the utterance's median
period, itself taken from the same dips, then held the whole word there
(encoder-quality investigation §4.3).

- `YinPitchTracker` takes an `IPitchEstimator` as its anchor;
  `MakeEncoder()` gives it SPTK's RAPT (`SptkPitchEstimator`,
  `PitchAlgorithm::kRapt`: 50 to 400 Hz every 10 ms) on `BandLimit()` of
  the high-passed signal. `AnchorPeriods()` puts RAPT's F0 on the hops:
  an even hop takes its value, an odd hop the geometric mean of the two
  either side, or the voiced one of them.
- Where the anchor finds a hop voiced, the prior holds the period within
  0.2 octave of the anchor's and costs 20 per octave beyond
  (`kAnchorFree`, `kAnchorPrior`), more than a harmonic's deeper dip
  gains; elsewhere it is the reference's, towards the median period. The
  anchor never decides the voicing, which stays with YIN's Viterbi path
  and `voicing_threshold` and `voicing_evidence`; if it fails, the track
  is the reference's.
- REAPER, which the plan proposed and the quality measures use, does not
  serve at 8 kHz: it rates whole words unvoiced (3 of the 64 TTS words
  on the encoder's signal, and which ones changes with the level), and
  drops an octave on low and high voices (`church` and `match` (Eddy)
  and `book` (Fred) at 65 to 71 Hz, where the odd harmonics of that F0
  are 20 to 30 dB below the even and the voice is about 130 Hz; `cat`
  and `kiss` (Flo) at 58 Hz, against about 250). As the voicing source it
  whispers whole words; as the anchor it moves five words of the phoneme
  set an octave down to fix two. Harvest and RAPT agree with the spectra
  on those words, but Harvest takes 0.044 s per second of speech, more
  than five times the rest of the encoder, and RAPT 0.0003.
  SPTK's RAPT keeps its state per call and seeds its dither the same way
  each time, so an encoding is the same on any thread.

Without an anchor, `YinPitchTracker` is the reference's tracker, and the
reference data checks it so (§13).

The plan names four stage interfaces. `IFrameAnalyser` is a fifth, so the
orchestration can be unit tested with every stage mocked.

**Results.** `EncodeResult` holds the frames, which always end in a stop,
and, unless the input held no sound, `EncodeDetails`: the high-passed input,
the planned frames' centres, c0, the lead and the dropped opening, the gain,
the plan, the shaped levels, the energies and the limiter's statistics.
`Encode()` refuses a rate other than 8000 Hz, a non-finite sample, and
options outside their ranges (`ValidateEncoderOptions()`).

**Options.** `EncoderOptions` has 9 of the 15 options of `tms5220enc.py`
`DEFAULTS`, and five the reference does not have: the three
preprocessing options (`preprocess`, above, all off by default), the
resonance hold (`resonance_hold_db`, 14 dB by default) and the voicing
smoothing (`voicing_min_run`, 2 frames by default). The reference's other six are
fixed at its defaults, since moving them made no difference worth keeping
over the corpus or did what another option does (§9, "Fixed options"):
`targetPeak` unset (the level comes from `target_rms`), `peakLimit`
(replaced by the ceiling and clip budget above, `kPeakCeiling` and
`kClipBudget` in `level_setter`), `repeats` on (`kMaxRepeatRun`),
`iterations` 5 (`kLevelPasses`), `fadeUnvoiced` on (with fades only
after loud unvoiced sound, above) and `compression` 1 (off). Quiet pauses, once an option, are always on. The defaults are the
reference's but for five (§9, "Default settings"): `window` 256,
`voicing_evidence` 5 and `level_agc` 1.25, chosen on the corpus for the
overall quality score, and `voicing_threshold` 0.6 and `repeat_lambda`
300, chosen with `voicing_min_run` against Acorn's PHROM (§9, "Voicing and
source shaping, from Acorn's PHROM" and "The voicing threshold and the
repeat trade-off, from the PHROM round trip"). `target_rms` is the
reference's 20, but of loudness, where the reference's is of the plain
level (§9, "Level and loudness, from the PHROM round trip"). The
reference data is encoded with the reference's own defaults written out
(`voicing_threshold` 0.3 and `repeat_lambda` 70 among them) and the options
it does not have off (`ReferenceEncoderOptions()` in the test support,
§13), so it does not depend on the application's defaults. `EncoderOptionInfos()`
gives each one's key (the Python name, or one in its style, such as
`noiseGateDb`, for an option it does not have), label, help text, type,
range and default, for the GUI's settings form and for project files,
its stage (`stage`), and marks the encoder options the GUI shows only
with its advanced settings (`advanced`). An option that a value turns
off names it (`off_text`: `treble_tilt` 0, `voicing_min_run` 1), for the
GUI to show as "off". The preprocessing options come first.
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
chip the neutral frame silenced all three pauses measured. A noise gate
(at first a hard squelch) makes this common, since gated pauses become
long runs of silence frames. `QuietPauses()` replaces the second silence frame of
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
is left alone. Over the 243 corpus files with a 30 dB squelch (the hard
gate the noise gate replaced), 110 have
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

**The noise gate.** Breaths and room noise 20 to 35 dB down sit above the
encoder's 45 dB gate, so the encoder reproduces them; PESQ, the measure
when this was first tried, even rewarded it. When `noise_gate_db` is set,
the preprocessing's first step (`dsp::NoiseGate()`) turns down the
source's stretches more than that many dB below its loudest level, the
RMS of a 20 ms window centred on each sample (long enough to span a low
voice's pitch period, so the level does not ripple). Below the level the
gain is −80 dB, where the sample is made silent. It was first a downward
expander, 3 dB of gain lost for each dB under the level, but a breath
just under the level was then only turned down 15 dB or so, and the
normaliser's compressor lifted what was left back over the encoder's
silence gate, so a shaded, gated stretch was encoded as sound. Silence
stays silence through the rest of the preprocessing. The gain then moves no faster than the gate's speed allows,
linearly in dB: it closes from open to −80 dB over `noise_gate_ms`
(100 ms by default) after the sound falls, so word endings fade, and
opens over a quarter of that, before the sound rises (the whole
recording is known), so onsets are not cut. It replaced a squelch that
muted below the level outright, with a 60 ms hold and 5 ms ramps, which
was too hard: soft word endings and the gaps between words were cut off
abruptly. The encoder applies it before its first stage, and every
measurement against the source uses the same preprocessed source:
`MeasureFrames()` and the `Optimiser`'s candidates. The quality measures
leave out what their voice activity mask does not count as speech, so a
gated breath is not asked of an encoding. The gate is not searched by
Optimise, since scores against differently gated sources do not compare.
Loudness alone cannot tell a breath from a soft "h" or a trailing "s";
the slow close keeps word endings, and the user chooses the level and
the speed.

**The de-esser and the normaliser.** Both follow what broadcast and
studio processing do to make speech even, done offline, so their
detectors look ahead instead of lagging. The de-esser
(`dsp::DeEss()`, `de_ess_db`) is split-band: the band above 2.5 kHz
(`kDeEssBandHz`; an 8 kHz recording keeps only the bottom of an "s"'s
4 to 8 kHz hiss) is split off with the zero-phase `ButterworthHighPass()`,
whose complement is the rest exactly, so the output is
x − (1 − g)·band and is the input wherever g is 1. The gain falls by the
band's share of the sound (each a 10 ms centred RMS) plus `de_ess_db`,
clamped to 0 to `de_ess_db`: the whole depth in pure hiss, nothing where
the band is that far below the whole (vowels, 20 dB and more down), so one
setting is both threshold and depth and the result does not depend on the
recording's level. The gain then moves by at most 1 dB per ms either way
(`SpreadDips()`). It runs after the treble tilt, which lifts the hiss it
tames. On the 33 phrases of an ARCTIC test project at 10 dB, the band
fell by a median 6.5 dB in frames whose band share is over −3 dB, the
rest of them by 0.4 dB, and frames with a share under −20 dB were
unchanged. The voicing recording does not get it, as it lowers the
spectral tilt and zero-crossing evidence of hiss.

The normaliser (`dsp::Normalise()`, `normalise_db`, `normalise_ratio`)
measures loudness as ITU-R BS.1770 does (`SpeechLoudness()`), with
100 ms blocks every 25 ms for phrases of a second or two, the absolute
gate at −70 dBFS and the relative gate 10 dB under the mean of the rest,
and without K-weighting, which is defined for 48 kHz and makes little
difference over speech's band. It scales the recording to the target,
then compresses: a 20 ms centred RMS detector, a soft knee 6 dB wide
(Giannoulis, Massberg and Reiss, 2012) at a threshold 10 dB under the
target, so nearly all the speech is compressed rather than only its
loudest syllables (with the threshold at the target, a 12 dB gap between
syllables closed by only 2 dB at 4:1), attack 1 dB per ms ahead and
release 0.1 dB per ms. Make-up gain brings the loudness back to the
target, and a peak limiter, whose gain is the most each sample allows
under −1 dBFS spread by 2 dB per ms ahead and 0.5 dB per ms after,
guarantees the ceiling. All its levels are relative, so recordings of
the same speech at any level give the same output (above the −70 dBFS
gate). The encoder already sets its output level from `target_rms`
(over sounding frames) and smooths slow changes with `level_agc`, so the
target level mainly sets what the Preprocess tab plays; what reaches the
encoding is the evening out. On the ARCTIC project at −20 dBFS, 3:1, the
33 phrases' loudness went from −26.3 to −19.5 dBFS to −20.00 ± 0.02, the
gap between their 95th and 25th percentile 20 ms levels from 19.6 to
14.8 dB on average, and no peak passed −1.4 dBFS. Its gain is given to
the voicing recording too. Neither is searched by Optimise, for the
noise gate's reason.

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

**Acceptance** (met, plan §3.2, against the Python reference; since
encoder version 1.3.0 the reference frames are the C++ encoder's own, §13):

- On all 243 corpus files with the four option sets in
  `app/golden/encoder/option_sets.tsv`, the frames were identical to the
  Python reference's for every encoding (972 of 972), against the
  criterion of 99%, again after the "loud" and "plain" sets were
  regenerated without the fixed options (§9, "Fixed options"). The option
  sets are taken over `ReferenceEncoderOptions()` (the reference's
  defaults, with the options it does not have off), and the reference's
  frames were compared with their pauses quietened, since the reference
  quietens none. No near-tie frames were needed: at a relative disturbance
  of 1e-9 the reference changes no frame on any file (§13).
- Every intermediate value of the corpus subset agrees with the reference
  to a relative 1e-9 (the level setting's, since 1.3.0, with the C++
  encoder's own).
- Encoding takes 0.0104 s per second of speech on one core over the whole
  corpus (`benchmark.sh`), against the target of 0.05.

**Performance.** `ReferenceSynthesizer` runs the closed loop and the
limiter, about ten times per phrase. The pitch tracker does two
1024-point forward FFTs and one inverse per 5 ms hop, and RAPT tracks the
whole signal once (0.0003 s per second of speech). None of it has been
optimised: none needs it.

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
| `MeasureLoudness()`, `BbcHighPass()`, `AWeighted()` | `loudness` | The loudness of the chip's output (below, "Diagnostics") |
| `CompareFrameTracks()`, `DescribeFrameTrack()`, `AddComparison()` | `frame_track_comparison` | An encoding's frames against reference frames of the same speech (below, "Diagnostics") |

**Preparing the signals** (plan §3.3's six steps, as fixed in Phase 8):

1. The original is the source as the encoder aims at it: the phrase's
   recording preprocessed (`Preprocess()`, §8), at 8 kHz.
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
with the squelch (the hard gate the noise gate later replaced) at 30 and
40 dB, each file's limit at its default
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

**Voicing and source shaping, from Acorn's PHROM** (2026-10-12; §8).
Encoded phrases sounded rougher and noisier than Acorn's own PHROM phrases
on every playback path. Acorn's phrases were re-encoded from the chip's
output (bench captures of 164 words of PHROM A, which the chip can
reproduce exactly), so their frames are the ground truth for that audio:
the encoder scored a median of 0.890 on them, against 0.668 on the ARCTIC
recordings, so the method is sound, but it changed between voiced and
unvoiced 3.8 times a second where the PHROM does 1.6, and called 14.6 %
of the PHROM's voiced audio unvoiced (40 % of that inside vowels: bursts
of hiss). Raising `voicing_threshold` and removing short islands of
voicing (`SmoothVoicingRuns()`) moved it towards the PHROM:

| On Acorn's PHROM, re-encoded | Voicing changes per second | Unvoiced frames | Median score (worst tenth) |
|---|---|---|---|
| The PHROM's own frames | 1.6 | 19 % | — |
| `voicing_threshold` 0.3 (old default) | 3.8 | 30 % | 0.890 (0.588) |
| 0.4 | 3.2 | 26 % | 0.915 (0.670) |
| 0.4, `voicing_min_run` 2 (the defaults) | 2.7 | 27 % | 0.928 (0.695) |
| 0.4, `voicing_min_run` 3 | 2.2 | 26 % | 0.934 (0.739) |

Over the corpus of 243 recordings (`OptionsCorpusCheck`):

| Option set | Median, mean score | 10th percentile | Good / Poor | Files up / down by over 0.005 | Total size |
|---|---|---|---|---|---|
| Old defaults (0.3, smoothing off) | 0.718, 0.709 | 0.577 | 138 / 9 | — | 100,358 bytes |
| `voicing_threshold` 0.4 | 0.724, 0.712 | 0.584 | 141 / 9 | 61 / 37 | 101,239 bytes |
| 0.4, `voicing_min_run` 2 (the defaults) | 0.722, 0.712 | 0.584 | 140 / 9 | 65 / 49 | 101,566 bytes |
| 0.4, `voicing_min_run` 3 | 0.714, 0.706 | 0.577 | 131 / 10 | 65 / 91 | 102,351 bytes |
| 0.4, smoothing off, `bass_cut_hz` 200, `treble_tilt` 0.4 (scored against the preprocessed source) | 0.709, 0.704 | 0.581 | 128 / 9 | 95 / 127 | 102,072 bytes |

So `voicing_threshold` became 0.4 and `voicing_min_run` 2, agreed with
the user: 3 comes nearer the PHROM but lowers more corpus scores than it
raises. The reference data is unchanged, since it is compared with the
reference's 0.3 and the smoothing off (§13).

The source shaping copies what the PHROM's frames show of how Acorn's
phrases were made. Their long-term spectrum, against the ARCTIC recordings
of a similar voice, has about 11 dB less at 125 Hz and up to 5 dB more
treble (a fit: an order-2 high-pass at 200 Hz and 1 − 0.4z⁻¹, 3.7 dB RMS
residual), and their vowels are driven at a median energy of 55 against
23. A BBC Micro's speech path cuts the bass (279 Hz, first order;
measured on a BBC Micro to within about 1 dB, §16), so speech that keeps it is
quieter there, and its quiet frames are mostly the DAC's −1 to 0 step
(§16, datasheet §8.2). On the ARCTIC phrases, 200 Hz and 0.4 cut the
quiet frames (energy 4 or less) from 25 % to 15 % and raised the vowels
above 300 Hz by about 1 dB. A first try that shaped the source before the
whole encoder turned 70 % of the frames unvoiced (the brighter sound
fooled the voicing), hence pitch and voicing from the source before the
filters. The bass cut and tilt are off by default: they score a little
lower against their own target, and are for a small speaker, not for
every use. They are offered as the *BBC Micro* preprocessing preset (§8,
"Three stages"). They were first encoder options, scored against a
source the GUI did not show, so making a phrase clearer on a BBC Micro
seemed to lower its score; the preprocessing stage (plan Phase 11) makes
what the score compares against visible and audible.

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
frames were compared with their pauses quietened. `peakLimit` was later
replaced by a ceiling and a clip budget ("Level and loudness, from the
PHROM round trip", below).

Optimise, which searched `targetPeak`, `repeats`, `iterations`,
`fadeUnvoiced` and `compression` too, was run over the corpus again
(limit: each file's default size). From the defaults with the resonance
hold, its best scores' median is 0.773 (Good / Fair / Poor 189 / 52 / 2,
98,441 bytes), against 0.777 (191 / 50 / 2, 98,816 bytes) from the
defaults before the hold and with every option searched; the hold alone
lowers the starting median by 0.002. It tries a median of 268 candidates
per file instead of 310, and takes 1.69 s per second of speech on 16
threads instead of 1.72.

**The voicing threshold and the repeat trade-off, from the PHROM round
trip** (2026-10-13; §8, "Options"; encoder-quality plan, Phase 2). The
round trip ("Diagnostics", below) compares the encoder's frames for the
157 speech words of Acorn PHROM A with the PHROM's own, which are the
ground truth for that audio. At `voicing_threshold` 0.4 the encoder left
94 of the PHROM's voiced frames unvoiced inside vowels (bursts of hiss),
and at `repeat_lambda` 70 it made 1184 repeats where the PHROM has 321,
holding one filter through moving formants. Both were swept with
`option_sweep.py` over the corpus of 243 recordings (with the round
trip), the investigation's 64 TTS files and the phoneme set (`say`), the
other options at their defaults:

| Corpus of 243 (`repeat_lambda` 300 but the first row) | Median, mean score | 10th percentile | Good / Fair / Poor | Files up / down by over 0.005 | Median VDE | Total size | Round trip: agreement; voiced made unvoiced inside / at onsets / at offsets; unvoiced made voiced |
|---|---|---|---|---|---|---|---|
| Old defaults (0.4, `repeat_lambda` 70) | 0.722, 0.712 | 0.583 | 140 / 94 / 9 | — | 8.4 % | 101,566 bytes | 93.7 %; 94 / 43 / 19; 18 |
| `voicing_threshold` 0.4 | 0.730, 0.727 | 0.599 | 158 / 80 / 5 | 162 / 29 | 8.5 % | 119,949 bytes | 93.7 %; 92 / 44 / 19; 20 |
| 0.5 | 0.730, 0.729 | 0.600 | 159 / 79 / 5 | 167 / 42 | 8.8 % | 120,971 bytes | 96.3 %; 33 / 33 / 12; 25 |
| 0.55 | 0.730, 0.729 | 0.604 | 158 / 79 / 6 | 173 / 47 | 9.0 % | 121,396 bytes | 96.9 %; 22 / 29 / 10; 26 |
| 0.6 (the defaults) | 0.730, 0.732 | 0.606 | 158 / 81 / 4 | 171 / 44 | 9.0 % | 122,016 bytes | 97.2 %; 17 / 28 / 7; 26 |
| 0.65 | 0.735, 0.733 | 0.612 | 159 / 78 / 6 | 166 / 45 | 9.4 % | 122,560 bytes | 97.4 %; 10 / 25 / 6; 31 |
| 0.7 | 0.734, 0.730 | 0.615 | 159 / 76 / 8 | 168 / 41 (one file −0.89) | 9.9 % | 123,203 bytes | 97.5 %; 3 / 23 / 5; 39 |

| At `voicing_threshold` 0.6 | Corpus: median, mean score | Corpus size | TTS (64): median, mean score | TTS size | Round trip: repeats (the PHROM's 321); voiced filter shape error | Round-trip fade-outs |
|---|---|---|---|---|---|---|
| Old defaults (0.4, 70) | 0.722, 0.712 | 101,566 bytes | 0.705, 0.685 | 9,977 bytes | 1184; 2.17 dB | 42 |
| `repeat_lambda` 70 | 0.720, 0.715 | 103,392 bytes | 0.718, 0.706 | 10,109 bytes | 1197; 2.18 dB | 38 |
| 140 | 0.730, 0.727 | 115,273 bytes | 0.748, 0.721 | 11,248 bytes | 743; 2.05 dB | 28 |
| 200 | 0.732, 0.729 | 119,150 bytes | 0.745, 0.725 | 11,741 bytes | 550; 2.02 dB | 27 |
| 300 (the defaults) | 0.730, 0.732 | 122,016 bytes | 0.747, 0.726 | 12,188 bytes | 395; 1.98 dB | 25 |

So `voicing_threshold` became 0.6 and `repeat_lambda` 300, agreed with
the user. 0.6 has the best mean and the fewest Poor files over the corpus
and the PHROM's voicing on 97.2 % of frames; above it the PHROM's hiss
starts to be voiced (31 frames at 0.65, 39 at 0.7) and at 0.7 one
recording collapses. On the phoneme set, the consonants' frames voiced
rise only from 69 to 81 of 553 (S 16 to 23, CH 30 to 33), the files with
a gross pitch error over 20 % fall from 29 to 23 of 152, and the loud
unvoiced frames before voicing from 18 to 13. 300 comes nearest the
PHROM's repeats, with the least filter shape error, and gives the best
mean; the cost is 20 % more bytes
(ARCTIC's 33: 16,857 to 20,298). The median VDE over the 33 ARCTIC
recordings rises from 13.9 % to 18.4 % (16.0 % at 0.5), against a
tracker's voicing of the source rather than the ground truth; it was
accepted for the round trip's figures. The slider's typical range of
`repeat_lambda` became 0 to 600. The slow gain control was swept too, to
lower the round trip's unvoiced energy bias (+0.78 index): at 1 and 1.1
it falls only to +0.60 and +0.67, while the corpus median falls to 0.727
and 0.728 and more files fall than rise (85 against 53 at 1, the worst by
0.26), so `level_agc` stays 1.25 and the bias is left to the level
setting (plan, Phase 3). The reference data is unchanged, since it is
compared with the reference's 0.3 and 70 (§13).

**Level and loudness, from the PHROM round trip** (2026-10-13; §8, "Level
setting"; encoder-quality plan, Phase 3). The limiter held every code
within 120, so on recordings the peaks, not `target_rms`, set the level:
the corpus's median peak was 117 and its sounding RMS 20.5 against a
target of 24, while the PHROM clips 0.21 % of its samples. And the level
counted bass, which the BBC Micro's speech path removes: the ARCTIC
recordings came out 3 to 5 dB quieter than the PHROM through it. Swept
with `option_sweep.py` as Phase 2 was, with the encoder that has only the
new limiter (`target_rms` of the plain level) and the one that also aims
at loudness:

| Corpus of 243 | Median, mean score | 10th percentile | Good / Fair / Poor | Total size | Median sounding RMS, peak; clipped (mean, worst file) | Median BBC Micro / A-weighted RMS |
|---|---|---|---|---|---|---|
| Phase 2 (limit 120, `target_rms` 24 of level) | 0.730, 0.732 | 0.606 | 158 / 81 / 4 | 122,016 bytes | 20.5, 117; 0, 0 | 12.7 / 9.7 |
| Clip budget, 24 of level | 0.734, 0.737 | 0.613 | 164 / 75 / 4 | 122,022 bytes | 22.3, 143; 0.057 %, 0.100 % | 13.8 / 10.9 |
| Clip budget and loudness, 16 | 0.740, 0.736 | 0.610 | 166 / 73 / 4 | 122,540 bytes | 21.9, 141; 0.051 %, 0.100 % | 13.9 / 10.7 |
| 18 | 0.741, 0.740 | 0.614 | 167 / 73 / 3 | 122,620 bytes | 23.4, 143; 0.063 %, 0.098 % | 14.8 / 11.3 |
| 20 (the default) | 0.742, 0.742 | 0.627 | 172 / 69 / 2 | 122,687 bytes | 24.6, 143; 0.066 %, 0.100 % | 15.5 / 11.8 |
| 24 | 0.742, 0.739 | 0.617 | 170 / 68 / 5 | 122,796 bytes | 26.4, 144; 0.072 %, 0.101 % | 16.3 / 12.4 |

| Round trip (157 words) | Energy index bias, voiced / unvoiced | Sounding RMS, BBC Micro RMS, A-weighted RMS (PHROM: 21.8, 17.1, 14.5) | Peak, clipped (PHROM: 124, 0.21 %) | Fade-outs |
|---|---|---|---|---|
| Phase 2 | +0.36 / +0.78 | 20.2, 15.2, 13.4 (−0.7, −1.0, −0.6 dB) | 109, 0 | 25 |
| Clip budget, 24 of level | +0.46 / +0.78 | 21.4, 16.0, 14.0 | 125, 0.020 % | 25 |
| Loudness, 16 | +0.12 / +0.28 | 19.0, 14.8, 13.0 (−1.2, −1.2, −0.9 dB) | 115, 0.011 % | 14 |
| 18 | +0.37 / +0.55 | 20.3, 15.9, 13.9 (−0.6, −0.6, −0.3 dB) | 123, 0.019 % | 18 |
| 20 (the default) | +0.58 / +0.80 | 21.5, 16.5, 14.6 (−0.1, −0.3, +0.0 dB) | 126, 0.023 % | 23 |
| 24 | +0.87 / +1.24 | 22.9, 17.4, 15.2 (+0.4, +0.2, +0.4 dB) | 128, 0.026 % | 29 |

| ARCTIC (33) | Median, mean score | Median ESTOI | Sounding RMS, peak | BBC Micro / A-weighted RMS | Frames the limiter lowered |
|---|---|---|---|---|---|
| Phase 2 | 0.698, 0.695 | 0.764 | 19.6, 118 | 11.6 / 8.5 | 490 of 3612 |
| Clip budget, 24 of level | 0.704, 0.703 | 0.766 | 21.9, 145 | 13.1 / 9.4 | 163 |
| Loudness, 16 | 0.707, 0.705 | 0.766 | 22.4, 145 | 13.3 / 9.7 | 244 |
| 18 | 0.710, 0.705 | 0.768 | 23.5, 146 | 14.0 / 10.2 | — |
| 20 (the default) | 0.708, 0.701 | 0.765 | 24.7, 146 | 14.5 / 10.6 | 590 |
| 24 | 0.695, 0.693 | 0.754 | 26.0, 146 | 14.9 / 11.0 | 895 |
| 30 | 0.678, 0.676 | 0.745 | 27.3, 146 | 15.7 / 11.5 | 1224 |
| *BBC Micro* preset: Phase 2 | 0.664, 0.667 | 0.745 | 17.7, 119 | 13.2 / 11.6 | 338 |
| Preset, loudness 20 | 0.667, 0.671 | 0.747 | 19.9, 147 | 14.8 / 12.4 | 161 |
| Preset, loudness 24 | 0.660, 0.663 | 0.738 | 21.5, 150 | 15.8 / 13.2 | 264 |

On the TTS set (64) the default scores a median of 0.730 and a mean of
0.741 (Phase 2: 0.747, 0.726; the median moves by a few short words),
with a sounding RMS of 23.8 and A-weighted 11.3 (20.5 and 9.3). On the
phoneme set the loud unvoiced frames before voicing rise from 13 to 17
(the level is higher; the plan's Phase 4 is about onsets), the consonant
frames voiced stay 81 of 553, and the files over 20 % gross pitch error
are 22 of 152 (23). Encoding takes 0.0079 s per second of speech on one
core (0.0085 before). No file clips more than 0.101 % of
its samples, and none wraps.

The clip budget alone gives the PHROM's sounding RMS at `target_rms` 24
(21.9) with a third of the limiting, and raises the score. Aiming at
loudness then makes `target_rms` a loudness, so the same number is louder
on a bass-heavy recording; 20, agreed with the user, gives the PHROM's
own words the PHROM's loudness (all three measures within 0.3 dB) and the
best corpus mean. 16 brings the energy indices closest (+0.12 / +0.28)
but leaves those words 1 dB quieter than the PHROM: quiet frames come out
a little high while the loudest are limited, so a mean index bias is no
measure of loudness. Above 20 the score falls as the limiter flattens
more frames (ESTOI falls), and the consonants grow louder against the
vowels. The plan's bounds for ARCTIC's A-weighted RMS (14.5 within 1.5 dB
with the *BBC Micro* preset, within 3 dB without) are not met: ARCTIC is
darker than the PHROM, and its pauses lower its whole-talk RMS, which the
PHROM's single words do not have. With the preset, ARCTIC through a BBC
Micro is 1.6 dB louder than in Phase 2 (14.8 against 13.2), without it
2.9 dB (14.5 against 11.6). The reference data was regenerated from the
C++ encoder (§13; encoder-quality plan D7), and the corpus check's 972
encodings match it; Optimise from the new defaults raises the score of
240 of the 243 files.

---


**Onsets and fades, from the PHROM round trip** (2026-10-13; §8, "Onsets
and fades"; encoder-quality plan, Phase 4). On the round trip, 8 of the
PHROM's 106 voicing onsets came a frame late and 6 three frames late,
each late frame played as hiss at the vowel's level, and the encoder
ended 23 of the 157 words in a fade-out where the PHROM has 3. Measured
with `phrom-check.sh`, `option_sweep.py` (the corpus of 243, ARCTIC's 33,
the investigation's 64 TTS files) and the phoneme check (`say`), at the
defaults:

| | Round trip: agreement; voiced made unvoiced inside / at onsets / at offsets; unvoiced made voiced | Onsets on time / one late (of 106); offsets one late (of 204) | Fade-outs (PHROM 3); repeats | Pitch, mean difference | Corpus: median, mean score; median VDE | Corpus size |
|---|---|---|---|---|---|---|
| Phase 3 | 97.2 %; 16 / 28 / 7; 26 | 72 / 8; 72 | 23; 393 | 0.40 semitones | 0.742, 0.742; 9.1 % | 122,687 bytes |
| Fades only | 97.2 %; 16 / 28 / 7; 26 | 72 / 8; 72 | 4; 367 | 0.40 | 0.741, 0.742; 9.0 % | 122,648 bytes |
| Onsets and fades (the encoder) | 97.9 %; 10 / 14 / 6; 27 | 80 / 0; 72 | 4; 358 | 0.43 | 0.738, 0.742; 11.1 % | 124,477 bytes |

ARCTIC scores a median of 0.709 and a mean of 0.706 (0.708, 0.701), with
a VDE of 20.4 % (18.9 %); the TTS set 0.735 and 0.742 (0.730, 0.741). On
the phoneme set the loud unvoiced frames before voicing fall from 17 to
10 and the fade frames from 45 to 11, of which one is a fade the encoder
added (after a final SH at energy 6); the rest are decays in the speech's
own frames, which the PHROM has too. The consonant frames voiced rise from
81 to 99 of 553, 17 of them in initial consonants: the frame before the
vowel, which the chip plays mostly over the vowel. The files over 20 %
gross pitch error fall from 22 to 20 of 152. Encoding takes 0.0080 s per
second of speech on one core.

The fades cost nothing on the score and bring the fade-outs to the
PHROM's. The onset rule puts 80 of the PHROM's onsets on time and none a
frame late, with the noise bursts before vowels gone from the round trip
(4 at energy 8 or more before, 0 after), at the cost of one more PHROM
hiss frame voiced (27, over the plan's bound of 26) and of the corpus's
VDE and pitch score: those compare the encoding with a tracker's voicing
of the source, frame by frame, and the rule voices a frame the source
voices only in its second half. The reference data was regenerated from
the C++ encoder (§13), and the corpus check's 972 encodings match it;
Optimise from the new defaults raises the score of 242 of the 243 files.

**The pitch anchor** (2026-10-13; §8, "The pitch anchor"; encoder-quality
plan, Phase 5). The gross pitch error (GPE) and F0 RMSE compare REAPER's
tracks of the source and of the chip's output, and at 8 kHz REAPER is
often wrong itself (§8): it puts `church` (Eddy) at 71 Hz, so an
encoding at the voice's 131 Hz scores 100 % GPE, and it reads Flo's
`seven`, coded at 296 to 157 Hz against a source REAPER puts at 276 to
182 Hz, at 55 to 90 Hz in the chip's output. So the candidates were also
judged by the coded GPE: each voiced frame's coded period against the
source's F0 at the frame's centre, from REAPER and from Harvest, which
avoids reading the chip's output. Measured with `option_sweep.py` (the
investigation's 64 TTS files, ARCTIC's 33, the corpus of 243), the phoneme
set (150 files) encoded by the CLI, and `phrom-check.sh`, at the
defaults; GPE and F0 RMSE are medians over the files, encoding time from
`benchmark.sh` (one core):

| Tracker | TTS: files over 20 % GPE; F0 RMSE; score median, mean | Phoneme set: files over 20 % coded GPE, against REAPER / Harvest | ARCTIC: GPE; F0 RMSE; score median | Corpus: GPE; files over 20 %; score median, mean | Round trip: agreement; pitch, mean difference | Encoding, s per s of speech |
|---|---|---|---|---|---|---|
| YIN (Phase 4) | 8; 43 cents; 0.735, 0.742 | 16 / 7 | 5.6 %; 63 cents; 0.709 | 4.7 %; 13; 0.738, 0.741 | 97.9 %; 0.43 semitones | 0.0081 |
| REAPER's voicing and F0, YIN's period | 6; 50; 0.719, 0.689 | | 7.3 %; 72; 0.674 | 4.8 %; 10; 0.740, 0.730 | 97.4 %; 0.44 | 0.0089 |
| YIN with a two-pass prior and a harmonic test | 10; 43; 0.737, 0.729 | | 5.6 %; 66; 0.707 | 4.5 %; 12; 0.744, 0.742 | 97.7 %; 0.42 | 0.0091 |
| YIN, REAPER as the anchor | 7; 44; 0.749, 0.745 | 13 / 18 | 3.8 %; 68; 0.706 | 3.7 %; 6; 0.741, 0.739 | 97.9 %; 0.42 | 0.0108 |
| YIN, Harvest as the anchor | 7; 44; 0.745, 0.754 | 12 / 4 | 5.6 %; 65; 0.708 | 4.8 %; 28; 0.735, 0.736 | 97.7 %; 0.42 | 0.052 |
| YIN, RAPT as the anchor (the encoder) | 7; 43; 0.749, 0.755 | 13 / 6 | 5.6 %; 63; 0.709 | 4.4 %; 12; 0.736, 0.741 | 98.1 %; 0.43 | 0.0084 |

REAPER's voicing whispers whole words and costs ARCTIC 0.035 of its
median score. The two-pass prior and harmonic test (a full-band
normalised autocorrelation at 2, 3 and 4 times the lag) cannot move
`six` (Daniel): its period wanders, so even the full band repeats best at
the harmonic. REAPER as the anchor scores best against REAPER and worst
against Harvest, having taken on REAPER's own octave drops; Harvest as
the anchor scores best against Harvest but is slow. RAPT as the anchor
fixes `six` (471 Hz to 107 Hz) and `sixty six` (182 to 116 Hz) (Daniel)
and changes no other listening word (`cat`, `church`, `science`,
`seven`, each voice), for no measurable time: Optimise takes 7.97 s per
second of speech on one thread (7.83).

On the round trip it voices 26 of the PHROM's hiss frames (27), with 4
offsets voiced too short (6), and 76 offsets a frame late (72); 80
onsets on time. The phoneme set's files over 20 % GPE fall from 20 to 19
of 152, its loud unvoiced frames before voicing from 10 to 8, and its
mean score rises from 0.718 to 0.724. The plan's bounds on GPE are not
met as written, because they count REAPER's errors: 7 TTS files stay
over 20 %, none of them over 20 % by the coded GPE against Harvest (two
others are, `kick` and `chess` (Eddy), at 33 % and 29 %); ARCTIC's
median is 5.6 % (5 % asked); and the round trip's pitch difference is
0.43 semitones (0.4). The reference data was regenerated from the C++
encoder (§13), and the corpus check's 972 encodings match it; Optimise
from the new defaults raises the score of 242 of the 243 files.

**The anchor vocoder** is order 10, 25 ms frames, K and gain
interpolated over 8 steps, a pulse per pitch period or Gaussian noise, pre-
and de-emphasis 0.9375, nothing quantised: what unquantised LPC reaches,
which places the good ends of the calibration. Its noise is the
reference's sample for sample: `NumpyNormal` reproduces NumPy's default
generator (PCG64, XSL-RR 128/64, from the state of
`numpy.random.default_rng(1)`) and its ziggurat normal sampler, with
NumPy's tables, bit for bit.

**Diagnostics** (docs-tech/encoder-quality-plan.md, Phase 1). Two
measures that are not part of the score help find what the encoder does
wrong; the investigation behind them is
[encoder-quality-investigation.md](../encoder-quality-investigation.md).

- `MeasureLoudness()`: from the chip's codes and the frames, the sounding
  RMS (the RMS of `MeasureOutputLevels()` over the voiced, unvoiced and
  repeat frames; `target_rms` aims at the same after the 279 Hz
  high-pass), the peak code, the share of
  samples beyond ±127 that the DAC clips, the lattice wraps, and from
  sample 200 the RMS after the BBC Micro's 279 Hz first-order high-pass
  (§16) and the A-weighted RMS (IEC 61672-1 through the bilinear
  transform at 8 kHz, as cascaded first-order sections). The score scales
  the output to the source, so it cannot see loudness; these can.
- `CompareFrameTracks()`: both tracks are spoken by the reference
  synthesiser and lined up by their output levels (the frame shift within
  ±3 with the least mean level difference in dB); then, frame by frame,
  the voicing (a repeat takes it from its pitch), with the reference's
  voiced frames that the encoding does not voice split by where they fall
  in the reference's voiced run (inside, its first frame, its last), the
  lag of the encoding's voicing onset and offset after each of the
  reference's, and for frames of the same voicing the energy index
  difference, the filter shape error (the RMS in dB of the difference of
  the two lattice filters' log responses less its mean) and the pitch
  difference in semitones. `DescribeFrameTrack()` counts each track's
  repeats and repeat runs, its bits, and its fade-outs (unvoiced repeats
  of falling energy after an unvoiced frame that end the sound, as
  `AddFades()` makes them).

They are used by the CLI's `measure` command (§11) and by three by-hand
tools, which judge the encoder-quality plan's later phases:

- `phrom-check.sh`: `phrom_roundtrip.py` parses every word of Acorn PHROM
  A from its image (the manifest in `investigation/proms/`), checks that
  its frames pack back to the image's bytes (datasheet §12.11), and speaks
  it with `speak5220-cli synth`; `PhromRoundTripCheck` encodes each of the
  157 speech words' output and compares the encoding with the PHROM's own
  frames, with `CompareFrameTracks()` and `MeasureLoudness()`, per word and
  in total.
- `option_sweep.py`: for each option set of a table
  (`app/scripts/sweeps/`), a project with those defaults (`speak5220-cli
  new --option`) is encoded and measured, optionally with the round trip,
  and one row of medians is printed per set; `--only` takes some of the
  files (`sweeps/investigation-tts.txt`: the investigation's 64 TTS files
  out of the phoneme set).
- `phoneme-check.sh`: `phoneme_corpus.py` renders `phoneme_prompts.csv`
  (words and sentences isolating S, SH, CH, K, T and F) with Piper or
  macOS's `say`, recording each file's SHA-256; `PhonemeCheck` encodes each
  file and reports its score and pitch errors, how many of its consonant's
  frames (before the first, or after the last, frame at which REAPER finds
  the source voiced) are voiced, the consonant's level against the vowel in
  the source and the encoding, loud unvoiced frames before voicing, fade
  frames and the share of repeats in voiced sound.

On the defaults before the plan's Phase 2 (`voicing_threshold` 0.4,
`repeat_lambda` 70), the round trip gives 93.7 % voicing agreement over
2770 matched frames, 1184 repeats and 42 fade-outs; the investigation's figures
(93.9 %, 2737, 1164, 37) are reproduced exactly when the words are spoken
by the reference synthesiser, whose output ends one idle frame sooner than
the emulator's: the encoder is sensitive to the length of the idle tail
(plan, Phase 1).

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
| LPC analysis, pitch and voicing | SPTK 4, with its own copies of REAPER (Apache-2.0), WORLD's Harvest (Modified BSD) and Snack's RAPT (Tcl/Tk licence) | Apache-2.0 | Linked by `s5core` (`SptkLpcAnalysis`, `SptkPitchEstimator`); from nixpkgs, and a vcpkg overlay port. SPTK's REAPER and Harvest are the originals with warning fixes, in SPTK's namespace, with the same defaults, so they need no packages of their own; the reference data takes its tracks from the originals (§13). Its RAPT, the encoder's pitch anchor (§8), is Snack's `get_f0` made reentrant |
| Overall perceptual score | ViSQOL v3 | Apache-2.0 | Left out (plan §3.3, R1): Bazel-only upstream, with Abseil, protobuf, Armadillo and a machine-learned model in its speech mode |
| Logging | spdlog | MIT | Linked by `s5core` |
| Tests | Google Test with Google Mock | BSD-3 | Linked by the test binaries |

---

## 11. GUI, CLI and the core

Both executables are thin front ends over `s5core`.

**Implemented:**

- The GUI's widgets are the static library `speak5220_gui`, so the GUI
  tests link them; the `speak5220` executable adds only `main()`, which
  starts following the desktop's colour scheme or the theme chosen
  (§16), creates the real
  `QtDialogs`, `SettingsRecentFilesStore`, `ThreadPoolTaskRunner`, `Synth`
  and `Player` (over `QtAudioOutput`, `SteadyClock` and `SincResampler`),
  and the `EncodingTools` (the encoder, `QualityAssessor` over an
  uncached `Synth`, the clock, and a one-thread runner for Optimise's
  search), and opens `speak5220 [PROJECT]` if given. The
  window is described in §15, §16 and §17.
- `speak5220-cli` uses `QCoreApplication` and `QCommandLineParser`. It has
  `--help`, `--version` and the logging options of §12, and with no
  command prints `speak5220-cli <version>` to stdout.
- Its commands (`cli/cli_commands`) are `new`, `add`, `encode`,
  `export`, `measure` and `synth`, each a thin sequence of core calls:
  `MakeProjectStore()` and `ProjectSerializer` to open and save
  (`SetEncoderOption()` for `new --option`), `PhraseImporter` and
  `Project::InsertPhrases()` to add, `EncodingCache::Refresh()` to encode,
  `CheckEncodingsCurrent()`, `RunExport()` and `WriteExportedFiles()` to
  export (§18), `QualityAssessor` over each phrase's preprocessed source and
  `MeasureLoudness()` to measure (§9), and `ReadFramesYaml()`,
  `FrameCodec::Pack()` and `Synth`'s VSM path to speak a frames file. `--output DIR` writes the project (or the
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
- Not yet: the GUI preferences for these options (Phase 12).

---

## 13. Reference data

**Implemented** for the emulator, packaging, encoder and quality measures
(plan §1.4).
`app/golden/` holds what the retired references produced, and since
encoder version 1.3.0 the encoder's own frames and level setting (below);
the build and tests read only these files and never run anything in
`investigation/`.

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
- `golden/encoder/`, written by `app/scripts/golden/encoder.py`: the
  stages up to the plan from the Python encoder (same rules: refuses local
  changes, records the commit, the SHA-256 and the NumPy and SciPy
  versions), the level setting and the frames from the C++ encoder, whose
  level setting, onsets, fades and pitch anchor depart from the
  reference's (§8, "Level setting", "Onsets and fades" and "The pitch
  anchor"; agreed with the user, encoder-quality plan D7). The pitch track
  is the Python's, without the anchor, and is checked against
  `YinPitchTracker` without one. Each frame's voicing in the analysis is
  the Python's, without `VoiceOnsets()`, and is checked against the
  stages the two share (`CentredVoicing()` and `CorrectOctaves()`); the
  stages after it take it as their input. The script runs the by-hand
  functional test `EncoderReferenceWriter` on the corpus's WAV copies and
  records `kEncoderVersion`; `--check` compares:
  - `inputs/`: WAV copies of an eight-file corpus subset (public-domain and
    CC0 voices only, one or two per category of prompt);
  - `intermediates/`: per subset file, with the reference's default
    options, as JSON: `encodeDetailed()`'s filtered signals (every 16th
    sample), the pitch track with CMNDF rows and candidates for six hops,
    each frame's analysis and own quantisation and the plan, and in
    `encode` the C++ encoder's faint-opening trim, shaped levels, per-frame
    gains, gain, energies and frames;
  - `frames/SET.tsv`: for each option set, every corpus file's frame count,
    near-tie frames and packed frames, from the C++ encoder;
  - `option_sets.tsv`: the default set, and three fixed sets that between
    them change every option the application has ("loud" and "plain"
    regenerated without the fixed options, §9, "Fixed options").
- **Near-tie frames.** A frame is a near tie if its coding changes when the
  input is disturbed at the level of floating-point noise. The script
  encodes each file as given and three times disturbed (scaled by 1 + 1e-9;
  plus white noise at 1e-9 of its RMS, twice with different seeds); a frame
  that differs in any run is listed, and a file whose frame count changes
  has every frame listed. Plan §3.2 accepts a difference only at such
  frames. The Python reference listed none; the C++ encoder gives the same
  frames whatever the thread count and platform, so it lists none either.
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
    the originals of SPTK's copies. The flake builds REAPER's tool, like
    SPTK, without fused multiply-add contraction: with it, Clang on arm64
    moved a frame of one original's track;
  - `scores/SET.tsv`: the alignment lag, the mask's frame count, the log
    spectral distortion from the script's own NumPy LPC analysis, the pitch
    and voicing errors of both trackers, and ESTOI from pystoi. Scores are
    written to 12 significant digits, since NumPy's sums can differ in the
    last bit from run to run.
- `golden/output-hashes/SUITE.sha256`, written by
  `app/scripts/output-hashes.sh --record`: not data from the retired
  references, but the SHA-256 of every file the `general`, `phrom` and
  `bbc` fixture suites' presets export (§18, §19), so a change can be
  shown to leave the exports byte-identical.
- `golden/bbc/programs/`, written by `app/scripts/golden/basic_programs.py`:
  not from the retired references either, but BBC BASIC II's own
  tokenisation of three test programs, typed in on MAME's emulated BBC
  Micro (§19).
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
`EncoderOptionInfos()` order, the preprocessing's and the encoder's (the
options in effect, so an override equal to a new default keeps an
encoding current), and the audio's 24-bit codes.
`IsCurrent()` compares it, and the version, with the stored encoding; a
locked phrase is always current. `ChangeSettings()` encodes with the new
overrides and stores them through `SetEncoding()`, which refuses a result
over the budget, leaving the previous settings and encoding (the message
says so). `Refresh()` encodes every stale phrase in list order, storing
each as it is made, so the budget is checked phrase by phrase and a failure
does not stop the rest. `encoder::kEncoderVersion` changes whenever the
same audio and options can give different frames: `1.1.0` added quiet
pauses, on by default, so encodings made by `1.0.0` are stale; `1.2.0`
made the noise gate mute everything under its level (the test projects
do not use the gate, so only their version and settings hash changed). The
reference data did not change, since it is compared with quiet pauses
off (§13). `1.3.0` aims the level at loudness and gives the limiter a
clip budget (§8, "Level setting"): the test projects with recordings were
encoded again, their exports' hashes recorded again, and the reference
data regenerated from the C++ encoder (§13). `1.4.0` voices onsets from
the hops the chip plays and fades only loud hiss (§8, "Onsets and
fades"), and `1.5.0` holds the pitch track near RAPT's (§8, "The pitch
anchor"), each with the same regeneration.

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
- **Two options mappings** (plan Phase 11). The options of each stage
  (`EncoderOptionInfo::stage`) are written in their own mapping:
  `project.default_preprocess` and `project.default_encoder`, and a
  phrase's `preprocess` and `encoder` overrides, which are one
  `EncoderOverrides` map in memory, split by stage when written.
  `ReadOptionsMapping()` refuses an option in the other stage's mapping,
  naming the mapping it belongs under, and keeps unknown keys per
  mapping (`default_preprocess_unknown`, `preprocess_unknown`).
- **Every option listed.** After the rest of the file is read,
  `CheckEveryOptionListed()` refuses a project whose
  `project.default_preprocess` or `project.default_encoder` is missing or
  lacks an option of its stage, naming the missing ones. Before the
  application's first release the format changes without a new version
  (plan Phase 8, task 14, and Phase 11): the squelch and quiet pauses
  were once left out while at their defaults, the preprocessing
  options were once in `default_encoder`, and the squelch (`squelch_db`)
  was replaced by the noise gate (`noise_gate_db`, `noise_gate_ms`), and
  the de-esser and normaliser (`de_ess_db`, `normalise_db`,
  `normalise_ratio`) were added, so a project saved before is refused rather than read with whatever the
  defaults have become. A leftover `squelch_db` is an unknown key, kept
  but ignored.
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
(plan §4), with `_2`, `_3`, … for clashes; the C headers and the WAV
files' names use them (§18).

**Export** (`RunExport()`) is described in §18.

**Fixtures** (`app/projects/`, unpacked; TESTING.md): `general/one-phrase`,
`general/many-phrases` (Acorn-style names with punctuation, an ASCII map, an
override, six presets: every format, both PHROM layouts, both scopes and
both WAV forms), `general/frame-kinds` (a locked phrase with every
frame kind; a Speak External header and encoded WAV), `phrom/one-phrom`
(exactly 16 384 bytes), `phrom/two-phroms` (spills into a second PHROM;
a PHROM header too) and
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
 ├── PhraseWaveforms (§16; the tabs; the play start through ProjectEdits)
 │    ├── Original
 │    ├── Preprocess: EncoderPanel::PreprocessPanel() under the waveform
 │    └── Encode: EncoderPanel (§17; settings and encodings through
 │         ProjectEdits) under the waveform
 ├── OptimiseSession + OptimiseDialog (§17)
 └── status bar: ProjectSizeText(Project::Allocation())
```

- **`ProjectDocument`** owns the core `Project`, the `QUndoStack` and the
  path, and a copy of the project as last opened or saved (`Saved()`;
  empty for a new project). It is modified while the stack is away from
  its clean index, and
  emits `Changed()` on every index change (edit, undo, redo) and when a
  project is opened or started, and `SavedChanged()` after a save, on
  which the window refreshes the list as on `Changed()`. A phrase is
  modified (`IsPhraseModified()`) when it differs from its saved version
  (`Phrase`'s `operator==`, so its place in the list does not count) or
  has none. Opening and saving use
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
  back and nothing is pushed, and undo reverses them latest first.
  `ReplacePhrasesCommand` (Phrase > Revert to Saved, with the selected
  modified phrases' saved versions) is `RemovePhrases()` of them all then
  `InsertPhrases()` of the versions at the indices returned, so each keeps
  its place and names swapped between them are free; if the insertion
  fails (a saved name another phrase has taken since, or the budget) the
  removed phrases go back and nothing is pushed. Its reverse is the same
  with the phrases it removed. An undo
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
  of a phrase too large for a PHROM in the theme's warning colour (§16),
  and `NameText()`, the name with " *" after it, for a row with
  `kModifiedRole`, which `SetProject()` sets from the saved project it is
  given (`ProjectDocument::Saved()`). Whether an encoding is current
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
  see and hear (whether the encoder panel shows its advanced settings,
  the phrase's tab shown, by its `PhraseTabKey()`, and the
  output filter chosen in the **Audio > Output Filter** menu, by its
  `OutputFilterInfo::key`, and the pin chosen in **Audio > Chip Output**,
  by its `OutputPinInfo::key`, and the theme chosen in **View > Theme**,
  by its `ThemeChoiceKey()`) is kept the same way through
  `IViewPreferences` (`SettingsViewPreferences`). `MainWindow::SetOutputFilter()`
  passes the filter to the `Player` and to `PhraseWaveforms`, ticks its
  menu item and saves it. `MainWindow::SetOutputPin()` does the same with
  the pin for `PhraseWaveforms` and the `OptimiseDialog`, first stopping
  encoded audio that was taken from the other pin.
  `MainWindow::SetTheme()` ticks its menu item, saves the choice and
  emits `ThemeChanged()`, which `main()` passes to the
  `ColourSchemeFollower`.
- **Shutdown order.** Child widgets are deleted by `QObject` after the
  window's members, so `~MainWindow()` deletes the panel and the details
  (which use the model and the document) first, then the encoder panel
  and the tabs that hold it (in that order, as the tabs own the panel) and disconnects the
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
- The Export menu came with the Export dialog (Phase 9, §18), between
  Phrase and Audio; until then it was left out rather than shown disabled.
- `speak5220 [PROJECT]` opens a project at start-up; the plan does not
  mention it.
- The `*` on phrases changed since the last save, and Phrase > Revert to
  Saved, are not in the plan.

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
  (also on a real `Project`), that a failed operation is not pushed, and
  that reverting puts phrases that swapped names back in place, all or
  none; a functional test marks, reverts and saves phrases of a fixture;
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
 ├── QTabBar: Original | Preprocess | Encode (PhraseTab)
 └── GripSplitter (vertical), shared by the tabs
      ├── waveform area: QStackedWidget, a page per tab of
      │    WaveformPane "Original (8 kHz)", "Preprocessed (8 kHz)" or
      │    "Encoded": title, note
      │    + WaveformView (PeakCache, TimeAxis, ThemeColours)
      │    + QScrollBar
      │    + play/pause, reset play start, (Frame grid,) zoom out,
      │      zoom in, fit
      └── settings area: QStackedWidget, a QScrollArea per tab holding
           its settings (SetControls()), or "No settings"
Player ── IAudioOutput (QtAudioOutput: QAudioSink), IClock, IResampler
ColourSchemeFollower ── IDesktopColourScheme (PortalColourScheme: D-Bus)
```

- **Time.** All three views count time in samples of the phrase's source
  audio (`TimeAxis`: the source sample at pixel 0 and samples per pixel,
  with fit, clamp, zoom about a pixel and scroll). A view's own samples
  are placed by a source offset: sample `s + offset` is drawn at source
  time `s`. The original's offset is 0.
- **No phrase.** With no phrase shown, each view shows a placeholder under
  a faint backdrop (`WaveformView::Clear()`): the logo, which comes from
  the executable's resources (`app/resources/speak5220.qrc`), so the GUI
  tests, which do not link them, draw none. Any samples take it away.
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
  gain, and the encoded waveform's note gives the gain in dB before any note
  passed in (such as "stale"). The frames and exports are unchanged.
- **Output filter.** With a filter (`PhraseWaveforms::SetOutputFilter()`),
  the encoded waveform shows the level-matched samples through
  `FilterSamples()`: `FilterDacOutput()` at 48 kHz averaged back to one
  value per 8 kHz sample, so the view's sample positions, offset and frame
  grid are unchanged. The unfiltered samples go to the `Player`, which
  applies the filter itself. A change redraws from the render kept,
  restoring the extent and axis, and the note names the filter after the
  level gain. The original is never filtered.
- **Output pin.** The encoded audio is taken from the SPEAKER pin by
  default (`PhraseWaveforms::SetOutputPin()`): `PinSamples()` clips each
  code to the DAC's range and converts it with the DAC's measured steps
  (`DacOutput()`) before the level gain, as the chip does before its
  analogue output, and the note gives `ClippedSamples()` when any are
  clipped. With the I/O pin the 10-bit codes are shown and played
  unclipped, and the note says so. The level gain is measured on the
  unclipped codes, so it is the same for both pins. A change redraws as
  for the filter. The quality measure and the encoder always use the
  unclipped codes; a WAV export always uses the SPEAKER pin.
- **Checked on a BBC Micro** (2026-10-12). A BBC Micro Model B with the
  speech system was recorded at its speaker pins playing a disc of test
  phrases (a 200 Hz tone, buzz, noise, energy and pitch steps, silence,
  and encodings), against bench captures of the same streams. With the
  recorder's low-cut off, the response matched the BBC Micro filter to
  within about 1 dB from 100 Hz to 2 kHz, and fell 2.9 dB by 3.15 kHz, as
  the DAC's held samples do; so the filter stayed as it is. The energy
  steps showed the DAC's −1 to 0 step: steps whose codes cross it (2, 3,
  5 and 7) came out 4 to 11 dB louder than with an ideal DAC. With
  `DacOutput()` the BBC playback chain matches the recorded steps to
  within about 1 dB from step 2 up (step 1 is under the BBC's noise
  floor), against 5.5 dB RMS before. That BBC Micro's chip ran at
  601 kHz, 6 % low (its clock is set by a trimmer); its pitch steps fell
  exactly on Table 8-5's periods at that clock.
- **Shared time axis.** Every view scrolls over the union of the content
  extents of those with samples, and each view's `AxisChanged()` gives
  its axis to the others (a flag stops the echo); with the same axis, the
  same sound is at the same x. A `QStackedWidget` lays out only the page
  shown, so a hidden view keeps its last size (before a tab is first
  shown, not the area's): when the view that
  changed is fitted (`WaveformView::IsFitted()`), the others are fitted
  to their own widths instead of copying its axis, and they fit again
  when shown and resized. New contents (another phrase, or its encoded
  audio arriving) are placed without following and then every view is
  fitted to the union, so a phrase starts as Fit shows it. Each tab's
  zoom and fit buttons and scroll bar act on its own view, and the
  others follow; each scroll bar follows its view. The zoom and fit
  buttons are enabled only when their view has samples.
- **Tabs and areas.** The tab bar switches both stacks; the one splitter
  between them (stretch 3 to 2) is not part of any tab, so the areas
  keep their sizes and the waveforms their place from tab to tab, for
  comparing them. `PhraseWaveforms::SetControls()` puts a widget in a
  tab's `QScrollArea` (widget-resizable), so settings taller than the
  area scroll instead of resizing it. The main window puts `EncoderPanel::PreprocessPanel()` on the Preprocess tab and
  the `EncoderPanel` on the Encode tab. The tab shown
  (`CurrentTabChanged()`) is kept by the main window in
  `IViewPreferences`.
- **Preprocessed recording** (plan Phase 11). The Preprocess tab shows
  and plays the recording preprocessed (`PlaybackSource::kPreprocessed`).
  `EncoderPanel::ShownPreprocessing()` is the preprocessing the phrase
  shown would be encoded with as the form has it, edits not yet applied
  included (in the defaults scope, the form's defaults under the
  phrase's overrides; none for no phrase, a locked one or one without
  audio). `UpdateView()` emits `PreprocessingChanged()` when it changes,
  and the main window passes it to `PhraseWaveforms::SetPreprocessing()`,
  so the waveform follows the sliders at once, before the 600 ms
  debounce. `UpdatePreprocessed()` runs `Preprocess()` again
  (milliseconds for a phrase), scaled down to full scale if a treble tilt
  takes it past (`preprocessed_gain_`, named in the note; the normaliser's
  limiter keeps it under); a new
  preprocessing redraws the waveform keeping its extent and axis
  (`RedrawPreprocessed()`) and stops it if it is playing. The note names
  the stages after the gate ("bass cut 200 Hz, treble tilt 0.4, de-esser
  10 dB, normalised to -20 dBFS at 3:1"), or says there is no
  preprocessing.
- **Noise gate shading.** On the preprocessed recording, where the gate
  acts, `PhraseWaveforms` finds the stretches
  `preprocess::GatedSpans()` turns down by more than 6 dB (gain under
  `dsp::kGatedGain`, 0.5) when the gate or the phrase changes, gives them
  to its view's `WaveformView::SetShading()`, and puts the gate first in
  the note ("noise gate 30 dB, 100 ms: shaded stretches are turned down",
  or that nothing is quiet enough to gate). A soft gate turns sound down
  by degrees, so 6 dB marks where it has clearly taken hold; the
  waveform itself shows the fades. The view veils the stretches with the
  base colour at alpha 170 over the waveform, so it is dimmed, not
  hidden, and hatches them with the palette's Mid colour, so they read
  without colour; hovering says they are gated. The original is never
  shaded.
- **Play start.** A click asks for the marker at that source sample;
  `PhraseWaveforms` rounds and limits it to the audio and applies
  `ProjectEdits::SetPlayStart()` (`SetPlayStartCommand`, "Move Play
  Start"), so it is saved as `play_start` and undoable. Every tab's view
  shows it, so it carries from one tab to another. A phrase without audio
  has none. Playback follows a move (and each tab's reset button, even
  when the play start is already 0): a waveform playing starts again from
  the new play start, and one paused stops, so the next play starts there
  rather than resuming; the Optimise window's audio is left alone.
- **Playback.** `Player` plays one buffer at a time: `Play(source,
  samples, start)`, `Pause()`, `Resume()` (a new buffer from the paused
  position) and `Stop()`. One tab's waveform plays at a time. The
  original and preprocessed recordings play from the play start; the
  encoded audio from `play_start + offset`, limited to its samples. The
  playhead is the clock time since the device started, but no more than
  `IAudioOutput::ProcessedMicroseconds()`, so it neither runs ahead of the
  audio nor jumps with the device's buffer; a 33 ms `QTimer` moves it, and
  it stops at the end or when the output reports finished. If the device
  does not take 8 kHz (`PlaybackRate()`), the buffer is resampled with
  `IResampler`. With an output filter (`Player::SetOutputFilter()`),
  the chip's output (`IsChipOutput()`: every source but the original and
  preprocessed recordings) is played as `FilterDacOutput()` gives it: each sample held for 6 values at 48 kHz, through the filter's
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
  change. The waveforms are the exception: `WaveformView` draws with
  `WaveformPalette(palette())`, the palette with a black base and window
  and light text, placeholder and mid (the highlight kept, for the focus
  frame), and so with the dark `ThemeColours`, in either scheme. The
  quality rating's bands and marks are the same full-strength colours in
  both sets.
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
- **Choosing a theme.** **View > Theme** offers `ThemeChoice` Auto (the
  default), Light and Dark. The follower requests
  `SchemeFor(choice, desktop)`: Light or Dark as chosen, or for Auto the
  desktop's preference, which it keeps up to date while another choice
  is in force so going back to Auto applies it at once. `main()` gives
  the follower the saved choice before the window is created, so the
  window never shows in the other scheme first.

**Deviations from the plan.**

- The plan puts the waveforms side by side in one phrase editor; they
  are on a tab each instead (Original, Preprocess, Encode), each tab's
  waveform above that stage's settings in two areas that keep their
  sizes from tab to tab, so each has the height to be read, its controls
  are beside it, and switching tabs compares them in place.
- The plan offsets the encoded waveform by the emulator's start latency. The
  encoder leaves out leading silence and the project does not record
  where its first frame starts, so a fixed latency would not line the
  waveforms up; the offset is found by `speechqual.py`'s alignment instead.
- The play start is a project edit, undoable, which the plan does not
  say; it is saved in the project as `play_start`, so changing it marks
  the project modified.
- One play start serves every tab (the encoded audio plays from the same
  sound), rather than one marker per waveform.
- The plan's Phase 6 item 1 asks for the dark-mode cause to be fixed in
  the build. No plugin was missing: the default GTK 3 theme does not
  report GNOME's preference, so the fix is in the application, through
  the portal.

**Acceptance** (plan Phase 6):

- unit tests check the colours of a light and a dark palette (different,
  the waveform palette black and dark in both, and at least 4.5:1 for the warning text and 3:1 for the marks against
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
 ├── PreprocessPanel(): its own "Settings for", kept the same, and the
 │   form's PreprocessSection() (on the Preprocess tab)
 ├── EncoderSettingsForm (from EncoderOptionInfos()): its
 │   EncoderSection()
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

The preprocessing is never varied: every candidate keeps the start's,
and is scored against the source preprocessed by it, computed once per
search.

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
  source as the phrase's preprocessing leaves it (`Preprocess()`), and
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

**The settings form.** One row per `EncoderOptionInfo`, in two columns,
in three groups, each under a heading row, in two sections that can be
placed apart (`PreprocessSection()`, `EncoderSection()`, each one grid
so its columns line up; the main window puts them on different tabs):
the preprocessing options under "Preprocessing", with a chooser
of `PreprocessPresets()` and "Custom" (`PresetBox()`); the encoder's four
basic options under "Encoder" (target RMS, repeat trade-off,
pre-emphasis, voicing threshold: the ones with the clearest effect on
the sound or the size, among them four of the five options Optimise's
grid searches, all but the voicing evidence); then a checkable
`QToolButton` and the seven with `advanced` set. Choosing a preset sets
`options_.preprocess` and signals `Edited()` once if that changes it;
after any change the chooser shows `MatchingPreset()`, or "Custom". The advanced rows
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
(the noise gate, the bass cut, the de-esser, the normaliser, the
resonance hold) a check box and a slider; the pre-emphasis
a chirp/first-order chooser and a slider for μ. An option with no effect
while another is unset (`EncoderOptionInfo::enabled_by`: the gate's
speed needs the gate, the compression the normaliser) has its label and slider disabled while it is. Values are read back
through `SetEncoderOption()` with `FormatNumber()`; the form keeps the
options it was given and changes only the edited one.

**The quality rating.** `RatingBar` draws the three bands (*Poor* from 0,
*Fair* from `ScoreCalibration::kFairFrom`, *Good* from `kGoodFrom`) in
`ThemeColours::quality_bands` (each a different colour, full-strength red,
amber and green in both schemes, with a near-black mark), each band's name
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
as the Encode tab plays it; the dialog keeps the `EncodedAudio` and
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
  text, components, figures, tooltip and marks; the noise gate turned on
  by its check box, and its speed usable only with it; a phrase measured
  against its preprocessed source (the noise gate, and the BBC Micro
  preset); the preprocessing reported as the
  form has it, shown on the waveform and shaded on the original; the
  preset chooser; Reset restoring both sections; the mark's contrast with every band in light and dark;
- GUI unit tests of Optimise with inline runners: accepting is one
  undoable step, Optimise all runs in list order and leaves out locked
  phrases, the history, the byte limit, and cancelling leaves the
  settings unchanged; the window's stages;
- `corpus-check.sh` and `benchmark.sh` run Optimise (results above).

---

## 18. Export

**Implemented** (plan §4, §6, Phase 9) in `core/packaging/c_header`,
`core/project/project_export` and `gui/export/`. The user's view is
[`docs/application/user-manual/export.md`](../../docs/application/user-manual/export.md),
the layouts
[`docs/application/reference/export-formats.md`](../../docs/application/reference/export-formats.md).

**Structure.**

```
ExportDialog ── Project (read), ExportFileNames(), CheckEncodingsCurrent(),
                the folder (the files that exist)
MainWindow::ExportProject()
 ├── IDialogs::AskExport() ─ ExportRequest (preset, folder); the
 │   dialog asks before replacing files (confirm_overwrite)
 └── ITaskRunner job: RunExport() ── ISynth, IWavCodec (WAV only)
                      WriteExportedFiles()
speak5220-cli export ── CheckExportPreset(), CheckEncodingsCurrent(),
                        RunExport(), WriteExportedFiles()
RunExport(project, preset, base, ExportTools)
 ├── CheckExportBase(), CheckExportPreset(), PhrasesInScope()
 ├── acorn-phrom: BuildAcornPhroms() ── AllocatePhroms(), AcornPhrom
 ├── c-header-phrom: BuildAcornPhroms(), CHeader::ForPhroms()
 ├── c-header-speak-external: SpeakExternalStream, CHeader::ForStreams()
 ├── wav: FrameCodec, ISynth (VSM path), PinSamples(SPEAKER), IWavCodec
 └── bbc-dsd: BuildBbcDiscPhroms(), bbc::BbcDisc (§19)
```

**Presets** (`ExportPreset`, the `exports:` list of `project.yaml`):
`name`, `format`, `scope`, and as the format needs them `phrases` (scope
`selected`), `layout` (`acorn-phrom`), `licence` (the C headers) and
`audio` (`wav`). The serializer writes only the keys that are set; the
presets did not need a new format version, since older files read unchanged and an older
Speak5220 keeps the new keys as unknown keys. `CheckExportPreset()`
refuses an unknown format or scope, a bad value, and an option the format
does not take, so a mistyped preset fails rather than being half applied.
`ExportFormats()` lists the formats, their labels and the options each
takes, for the dialog and the checks.

**Behaviour.**

- **Names.** Files are named from the base: the preset's name (or the
  project file's, for `--format` and the dialog's default).
  `CheckExportBase()` keeps it one file name (no `/ \ : * ? " < > |`,
  control characters, `.` or `..`), so a preset in a project file cannot
  write outside the chosen folder. WAV files take the phrase's C
  identifier, unique within the export.
- **Scope.** `selected` exports the named phrases in list order. For the
  PHROM formats they are allocated afresh (`BuildAcornPhroms()` over the
  subset), as the whole project would be, so word numbers start at 127 in
  each PHROM.
- **C headers** (`CHeader`). An SPDX line with the preset's licence
  (default `GPL-3.0-or-later`; letters, digits, spaces and `. + - ( )`,
  so it cannot break the comment), the project's name (with `*/` and line
  breaks defused), the name-to-identifier list, an include guard and
  `<stdint.h>`. The PHROM header has one `static const uint8_t
  n_phrom_N[16384]` per image, an enum of the words and a table of each
  word's PHROM, number, offset, length and name; the Speak External header
  one array per phrase with a `N_LENGTH_ID` constant, an enum and a table
  of name, data and length (`uint16_t`, so a stream over 65 535 bytes is
  refused). Phrase constants all start `N_WORD_` or `N_LENGTH_` and nothing
  else does, so names cannot clash whatever the phrases are called. There
  is no date (below).
- **WAV.** `encoded`: the packed frames spoken through `ISynth`'s VSM
  path, each sample through `PinSamples(kSpeaker)` (the DAC's clipping and
  measured steps),
  as 16-bit PCM at 8 kHz: the encoded waveform with the Audio menu at its
  defaults. `original`: the phrase's audio as held, 24-bit, so it
  round-trips exactly. Both go through `IWavCodec` into bytes, so
  `RunExport()` writes no file itself.
- **Stale encodings.** `CheckEncodingsCurrent()` refuses a phrase in scope
  whose encoding is not current (the WAV of the original excepted). The
  CLI and the main window check it before running; the dialog shows it.
- **The dialog** (`ExportDialog`) shows only the chosen format's
  options, and lists the files the export will write (`ExportFileNames()`:
  the same checks as `RunExport()`, without speaking any phrase or
  writing a header) or why it cannot run; Export is enabled only for an
  export that can run, into a chosen folder. It neither loads nor saves
  presets: they are written in the project file for the CLI, and the GUI
  keeps them when it saves. `IDialogs::AskExport()` shows it, so the main
  window's tests answer without a modal dialog.
- **Existing files.** The dialog's `accept()` looks for the files the
  export will write in the folder (`ExistingFiles()`); if any exist it
  asks (`ExportStart::confirm_overwrite`, a question box from
  `QtDialogs`), and if the user declines the dialog stays open with its
  settings, so the name or folder can be changed. With no one to ask,
  existing files are never replaced. The CLI replaces files without
  asking, as a script expects.
- **Running.** `MainWindow::ExportProject()` runs the export on the
  `ITaskRunner` with a copy of the project, and writes the files there; the result comes back
  queued, to the status bar or an error. The folder is offered again next
  time.
- **Scripts.** `run-projects.sh` encodes a copy of each fixture of a suite
  (`encode --output`) and runs its presets into
  `build/app/project-output/SUITE/NAME/export/`, recording the files each
  run wrote in `written.txt`, so nothing needs deleting between runs.
  `output-hashes.sh` hashes those files and compares the manifest with
  `app/golden/output-hashes/SUITE.sha256` (`--record` records it).

**Deviations from the plan.**

- Plan §6 puts the date in a header's comment. A date changes the bytes
  every day, which would defeat the output hashes, so headers carry the
  project's name and no date (decided with the user).
- The TI indexed and raw `.bin` exports were dropped from the plan, since
  their output cannot practically be tested; `TiIndexed` stays in
  `core/packaging/`, unused by the exports.
- The plan has one "C header" export in two forms; they are two formats,
  `c-header-phrom` and `c-header-speak-external`, so a preset names one
  format and the options it takes (decided with the user).
- The dialog does not load or save presets (decided with the user);
  presets are for the CLI.

**Acceptance** (plan Phase 9, met):

- C headers: unit tests read the arrays back byte for byte, and check the
  table, the names and their mapping, the comment, the licence and prefix
  checks and the stream limit; a functional test compiles both headers of
  `general/many-phrases`, included twice, as C11 and C++17 with
  `-Wall -Wextra -Wpedantic -Werror` (`/W4 /WX` with MSVC), using every
  table.
- WAV: unit tests with a mocked synth and codec check the SPEAKER clipping,
  16- and 24-bit and the names; functional tests reload the files at
  8 kHz with the chip output's length, and the original sample for
  sample.
- Export dialog and presets: GUI unit tests map the dialog's fields to
  presets (each format's options, both scopes), and check the summary and
  when Export is enabled; main window tests cover a cancelled export and
  a refused stale phrase; GUI functional tests write a header into a
  temporary folder, and check that the dialog asks before replacing an
  existing file and stays open when the user declines (a new name then
  goes ahead without asking), accepts when the user agrees, and keeps
  the file with no one to ask. Functional tests run presets of every format through
  the CLI, `--format` with `--phrase`, `--licence` and `--audio`, and
  refuse an unknown phrase, an option of another format and a preset name
  that leaves the folder.
- Output hashes: the baseline for `general` (15 files) and `phrom` (4) is
  recorded; rerunning matches it, and changing the header's bytes per
  line from 12 to 16 was reported as a difference in exactly the four
  headers.

---

## 19. The BBC Micro disc

**Implemented** (plan §6, Phase 10) in `core/bbc/` and
`core/project/project_export` (`BuildBbcDiscPhroms()`, the `bbc-dsd`
format). The user's view is
[`docs/application/user-manual/bbc-disc.md`](../../docs/application/user-manual/bbc-disc.md),
the layout
[`docs/application/reference/export-formats.md`](../../docs/application/reference/export-formats.md#bbc-micro-disc).

**Structure.**

```
RunExport(bbc-dsd) ── BuildBbcDiscPhroms() ── BuildAcornPhroms() (placement,
                      │                       the size checks), PhromHeaderStrings(),
                      │                       SpeakExternalStream
                      └── BbcDisc::Build(project name, PHROMs)
                           ├── BasicTokeniser::Tokenise(PlayerSource())  → SPEAK
                           ├── BbcDisc::PhromData(), BbcDisc::Index()    → PHROMn, INDEXn
                           └── DfsDisc::DoubleSided(side 0, side 1)      → NAME.dsd
speak.bas ── cmake/EmbedText.cmake (at build time) ── player_source.h (kPlayerSource)
```

**Components.**

- **`BasicTokeniser`** turns BBC BASIC II text into the bytes SAVE writes:
  `0x0D`, the line number (high byte first), the line's length, its
  tokens; `0x0D 0xFF` at the end. Its rules are BASIC II's own: the keyword
  table in the ROM's order (first match wins, so `ENDPROC` before `END`),
  with each keyword's flags: conditional (not tokenised before a letter,
  digit or `_`: `ENDX`), start and middle of statement (a pseudo-variable
  takes its statement token, +0x40, at the start: `TIME=0`), FN/PROC
  names copied, REM and DATA copying the rest of the line, and line
  numbers after GOTO, GOSUB, THEN, ELSE, RESTORE, TRACE and the like,
  continuing after commas and spaces. A line number reference is encoded
  (`0x8D` and three bytes) only up to 32767; a larger one stays as digits
  and ends line-number mode. Strings, `&` hex numbers, and a star command
  at the start of a statement are copied. Names are skipped whole, so a
  keyword inside one is not tokenised, but one starting a name is
  (`TOTAL` is `TO TAL`), as in BASIC II. Abbreviations (`P.`) are
  accepted. It refuses lines that are unnumbered, out of order, above
  32767, empty, not printable ASCII, or over 255 bytes once tokenised.
  The rules were found by typing programs into BASIC II on MAME and
  reading back what it stored; the unit tests hold single lines of that,
  and the functional test whole programs (`golden/bbc/`).
- **`DfsDisc`** writes Acorn DFS sides of 400 sectors: the catalogue in
  sectors 0 and 1 (title split 8 + 4, entries × 8, boot option, the
  sector count 0x190), entries in descending order of start sector, the
  18-bit load, exec and length with their top bits packed into byte 6,
  and the files from sector 2 in whole sectors. Titles are padded with
  spaces. `DoubleSided()` interleaves the sides' tracks. It refuses bad
  names (1-7 printable characters, none of `space # * . : "`), duplicate
  names, more than 31 files, files that do not fit, titles over 12
  characters and boot options over 3.
- **`BbcDisc`** builds `PHROMn` (the streams in order), `INDEXn` (a 32-byte
  header: the title in bytes 0-29, the count in 30-31; 20-byte records:
  the name in 0-15, the stream's offset and length, little-endian) and the
  disc: `!BOOT` (`CHAIN "SPEAK"`, boot option 3), `SPEAK` (load &FF1900,
  exec &FF8023, as SAVE gives a program at PAGE &1900), PHROMs 1-4 on side
  0 and 5-8 on side 1, both titled with the project's name. A PHROM's data
  over 16 KB, a name over 16 characters and more than 8 PHROMs are
  refused.
- **`BuildBbcDiscPhroms()`** places the phrases as `BuildAcornPhroms()`
  does (so the size checks and word order are the same), titles each
  PHROM with the second of its header strings, and refuses more than 8
  PHROMs with a message that `ExportFileNames()` also gives, so the Export
  dialog says so before the user exports. The format takes no options.
- **The player** (`speak.bas`, about 3.5 KB tokenised): MODE 7; checks for
  the speech system (OSBYTE &EB, X = 0 without it); keeps a 16 KB buffer
  at HIMEM − &4000, lowering HIMEM to it, and stops with a message if
  fewer than &900 bytes are left above TOP, saying how low PAGE must be;
  assembles its routine with BASIC's inline assembler; reads `INDEX1`-`4`
  on drive 0, and `INDEX5`-`8` on drive 2 only if `INDEX4` exists (so a
  disc of 4 PHROMs or fewer works in a single-sided drive); lists the
  PHROMs; on a choice, opens its index and `*LOAD`s its `PHROMn` into the
  buffer; lists 16 phrases a page, reading only their records; speaks a
  phrase by letter, or all in order on RETURN. An ON ERROR handler makes
  Escape stop play all (back to the page) or go back to the PHROM list;
  any other error closes the files and reports itself.
- **The routine** (the plan's §6 method): Speak External (&60) with
  OSBYTE &9F, the first 16 bytes, then 8 more each time OSBYTE &9E shows
  Buffer Low, nothing after the last byte; then it waits for Talk Status
  to set (a stream of 16 bytes or fewer starts only at the next frame
  boundary, datasheet §12.9, so the status can still be clear: without
  this wait, play all would send the next Speak External while a short
  phrase spoke), with a timeout of 65 536 polls, and then for it to
  clear. Bytes go unreversed, one OSBYTE call each. Zero page &70-&76.

**Hardware spike (plan Phase 10, task 1)**, run on MAME 0.287's BBC Micro
Model B (OS 1.20, BASIC II, Acorn DNFS 1.20, the Acorn speech system),
with ARCTIC prompt a0023 encoded (834 bytes, 4.70 s):

- **Bit order:** the stream sent unreversed is our emulator's output for
  that phrase: the recorded envelope correlates with ours at 0.92, and it
  took 4.71 s by the BBC's clock against our 4.70 s. Sent bit-reversed it
  is noise (0.22). Unreversed is right, as §6 reasoned from the guide.
- **Buffer Low refill:** 834 bytes, about a hundred refills of 8, with no
  gap or early end.
- **The MOS interrupt handler** does not interfere: with its own speech
  queue empty it only reads the status, and the phrases' lengths match.
- **Memory:** with the player, PAGE &1900, TOP &26AC, HIMEM lowered to
  &3C00: 5460 bytes for variables, stack and routine, of which at least
  4352 stayed free through the 8-PHROM check (measured from BASIC's stack
  pointer, to within 255 bytes on the safe side).
- **Not yet confirmed on hardware**: the same checks on a BBC Micro with
  the speech upgrade and Acorn DFS, with the exported discs, are still to
  be run.

**PHROM images on a BBC Micro (plan Phase 10, task 6).** On MAME, with an
exported Acorn PHROM image in place of the PHROM A (`cm62024.bin`), OS
1.20's own speech code (`SOUND -1,W,0,0`) speaks the image's words 127,
133 and 153 and the ASCII entry for `1`, each matching our emulator's
output (lengths within 4 ms; envelope correlation 0.94 and 0.98). That is
the MOS reading the image's header, pointers and ASCII entries as we
write them; it says nothing about the chip, PHROM or wiring, so the
hardware test is still to be run, and Appendix B of the datasheet is
unchanged until it has been and the user agrees (AGENTS.md §9.4).

**The emulator check** (`app/scripts/bbc-check.sh`, by hand: MAME and its
ROM set are not in the repository). `player.lua` boots each disc with
Shift+Break, runs a list of steps (wait for text, type, press ESCAPE,
which MAME's natural keyboard does not type), and logs the status line
and screens with emulated times; `emulator_check.py` checks the screens
against the disc's index files, the phrases spoken against those asked
for, and the recording of each phrase against our emulator's output:
its length to within 60 ms, and its envelope to a correlation of at least
0.75 at the best lag within 25 ms (MAME's output passes through the BBC's
audio stage and a 48 kHz mixer). Our chip driver gives up after 60 s of
chip time (`ChipDriver::kMaxWaitSlots`), so the WAV of a longer phrase,
such as `both-sides`' 64 s LONGEST, is cut short; such a phrase is
checked against its frame count instead.

**Deviations from the plan.**

- Plan §6 loads each phrase from `PHROMn` with OSGBPB just before it is
  spoken. Acorn DFS reads OSGBPB data a sector per revolution: on MAME a
  16 KB phrase took about 14 s to load, a 1 KB one about 1 s. The player
  instead `*LOAD`s the whole `PHROMn` into the same 16 KB buffer when the
  PHROM is chosen (2.6 s for a full one), and speaks from there, so play
  all has no gaps (decided with the user). Memory use is unchanged.
- The plan's spike and player checks name "an emulator with speech
  support"; that is MAME, driven by scripts, with its ROMs kept in
  git-ignored `local/mame/roms/`. The tokeniser's reference programs were
  saved from MAME's BASIC II rather than from a real BBC Micro (both
  decided with the user).

**Acceptance** (plan Phase 10):

- Task 1: met on MAME (above); the hardware run is outstanding.
- Task 2: the unit tests' lines and the three reference programs match
  BASIC II byte for byte, and `bbc-check.sh` checks that the disc's SPEAK
  is what BASIC II makes of `speak.bas`.
- Task 3: unit tests check the catalogue's fields, the entry order, where
  each file and each side's tracks lie, and the refusals; a functional
  test exports both `bbc` fixtures through the CLI and reads them back
  with `tests/support/dfs_catalogue.py` (Python, written from the DFS
  layout, not from the C++), comparing every file's SHA-256.
- Task 4: on MAME, with discs of 1, 5 and 8 PHROMs, the player lists the
  PHROMs, pages through 27 phrases, speaks each phrase asked for, plays
  all in order, stops play all on Escape after the phrase being spoken,
  speaks the largest phrase a PHROM holds, and says so on a machine
  without the speech system. The Gotek check by hand is outstanding.
- Task 5: unit tests check the data and index layouts, the file placement
  and the 8-PHROM limit (9 refused with a message, in the export and the
  dialog); `bbc-check.sh` builds an 8-PHROM disc and has 9 refused by the
  CLI; the `bbc` suite is in the hash baseline (3 discs).
- Task 6: the MAME result above; the hardware test is outstanding.
