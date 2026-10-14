# Testing vision and strategy

This document covers the Speak5220 application in `app/`, and sets the
testing rules it is built to, following the plan in
[docs-tech/application-plan.md](docs-tech/application-plan.md). The test
tree, its lanes, the CI jobs, the emulator's, encoder's, quality,
optimiser and project interfaces, the reference data, the `general`,
`phrom` and `benchmark` fixture projects, `corpus-check.sh` and
`benchmark.sh` below exist; the other interfaces, fixture projects and
scripts are added by the plan's later phases.

The research code in `investigation/` (the C emulator and the capture
tools) has its own self-tests and is not covered here.

## Common

### Vision

Good software should have the following traits:
- It is easy to modify/maintain
- It is easy to test quickly and automatically
- It is easy to use

### Strategy to implement vision

#### Architecture

```
MONOLITH PATTERN                    DEPENDENCY INVERSION PATTERN
  ┌─────────────────────┐             ┌────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐
  │                     │             │                │   │  «Interface»         │   │  «Interface»         │
  │  ┌───────────────┐  │             │                ├──►│  public method B()   │   │  public method C()   │
  │  │ public        │  │             │  Class         │   ├──────────────────────┤   ├──────────────────────┤
  │  │ method A()    │  │             │  public        │   │  Class               ├──►│  Class               │
  │  ├───────────────┤  │    ════►    │  method A()    │   │  public method B()   │   │  public method C()   │
  │  │ private       │  │             │                │   └──────────────────────┘   └──────────────────────┘
  │  │ method B()    │  │             │                │
  │  ├───────────────┤  │             └────────────────┘
  │  │ private       │  │
  │  │ method C()    │  │                  class 1          class 2 (with interface)    class 3 (with interface)
  │  └───────────────┘  │
  │                     │
  │  monolithic class 1 │
  └─────────────────────┘
```

Instead of writing monolithic classes (as in the above example), we
architect classes that are smaller and depend upon abstractions (interfaces)
rather than concrete objects. In C++, an interface is a class composed
entirely of pure virtual methods, with a virtual destructor. Interface names
start with `I` (`IVsmBus`, `IQualityMetric`), which satisfies the Google
naming rules (§1.3 of the plan).

Depending upon abstractions instead of concrete objects is known as the
Dependency Inversion design pattern.

##### How classes receive their dependencies

Instead of a class calling the constructor of one of its dependencies (and
thus having a tightly coupled link), we *inject* the abstract dependencies
(interfaces) into the class constructor. Formal dependency injection
frameworks tend to work better when only interfaces are injected into a
constructor, so a separate *init* method may be wise for non-interface
dependencies.

**Important**: The init method should not appear in an interface, only in
the concrete class. It is an implementation detail, so only code that
handles instantiation (that is, factories) should know about it.

**Caveat**: If using this 'init method' convention, you may have to use raw
pointers instead of references (`ISomeObject*` instead of `ISomeObject&`),
because the compiler does not allow a reference to be temporarily null
before init is called.

**Errors**: The project does not use exceptions (Google style). An init
method, or any operation that can fail, returns a `Status` or `StatusOr<T>`,
and tests assert on the returned status rather than on a thrown exception.

##### std::shared_ptr<ISomeInterface> vs using ISomeInterface&

It is more convenient to just pass around references to interfaces
(including for testing), so this is the default. But if there is a question
of an interface going out of scope (and thus being de-allocated) prematurely,
wrap the interface in `std::shared_ptr` to be safe.

##### What if the class needs to instantiate an object dynamically, such as opening a file?

In this case, follow the Abstract Factory design pattern: define an
interface with methods that return more interfaces. Any object created by
such an interface should be wrapped in `std::shared_ptr` to ensure it
doesn't get de-allocated prematurely.

#### Testing architecture

```
INTEGRATION TEST PATTERN                    UNIT TEST PATTERN
  ┌──────────────────────────┐               ┌─────────────────────┐   ┌─────────────────────┐   ┌─────────────────────┐
  │ Integration test for     │               │ Unit test for       │   │ Unit test for       │   │ Unit test for       │
  │ method C. Must also      │               │ method A            │   │ method B            │   │ method C            │
  │ test method A and B.     │               └──────────┬──────────┘   └──────────┬──────────┘   └──────────┬──────────┘
  └────────────┬─────────────┘                          │                         │                         │
               │                                        ▼                         ▼                         ▼
               ▼                              ┌─────────────────────┐   ┌─────────────────────┐   ┌─────────────────────┐
  ┌────────────────────────┐                  │  «Class»            │   │  «Class»            │   │  «Class»            │
  │  ┌──────────────────┐  │    ════►         │  public method A()  │   │  public method B()  │   │  public method C()  │
  │  │ public           │  │                  └──────────┬──────────┘   └──────────┬──────────┘   └─────────────────────┘
  │  │ method A()       │  │                             │                         │
  │  ├──────────────────┤  │                             ▼                         ▼
  │  │ private          │  │                  ┌─────────────────────┐   ┌─────────────────────┐
  │  │ method B()       │  │                  │ Mock of interface   │   │ Mock of interface   │
  │  ├──────────────────┤  │                  │ containing          │   │ containing method   │
  │  │ private          │  │                  │ method B()          │   │ C()                 │
  │  │ method C()       │  │                  └─────────────────────┘   └─────────────────────┘
  │  └──────────────────┘  │
  │                        │                   unit test for only        unit test for only       unit test for only
  │  complicated,          │                   method A                  method B                 method C
  │  hard-to-maintain test │
  └────────────────────────┘
```

##### Encapsulation

Get into the mindset of not trying to test your dependencies' internal
implementations. You give your dependencies inputs and expect certain outputs
as a result. It is not your job to test your dependencies' internal
implementations, or even to know what those implementations are. Trying to
do so leads to tight coupling and bad architecture.

Staying intentionally ignorant of how dependencies are implemented
internally is known as encapsulation. Practising this discipline makes the
project easy to modify and maintain. Any module is free to change its
implementation without breaking the rest of the project. Modules depend
upon stable abstract interfaces rather than concrete classes.

##### Unit tests

Unit tests are small, lightweight tests that test only a small bit of code,
usually a single method.

Benefits of having a large suite of unit tests:
- They execute extremely quickly (thousands of tests can be run in seconds)
- They are effective at digging into corner cases that would otherwise be
  difficult (or slow) to test automatically
- They encourage encapsulation in the project architecture, which tends to
  make the project easier to modify and maintain
- They are self-contained, so they don't require any 'tribal knowledge' to
  set up
- If they fail, it's really easy to pinpoint the source of the problem

Rules for unit tests:
- All dependencies are mocked out. They never touch the file system, the
  network, the audio device, a thread pool, or the system clock. This makes
  them deterministic, which is very desirable.
- They usually test only a single method. If a method calls a small private
  method, this forces your test to test two methods. This is usually okay if
  the private method is very simple, but large private methods are an
  anti-pattern and should be moved to another class/interface.
- As a rule of thumb, unit tests should make up 80% of all tests for a
  well-designed project.

##### Functional (integration) tests

Functional tests test multiple methods, possibly even multiple classes
together, to get an overall idea of whether a section of the project works.

They tend to execute slowly, are poor at digging into corner cases, and tend
to be bigger (in lines of code) and harder to maintain.

Use them for a few 'happy path' cases, to ensure that everything is wired up
correctly. Unit tests aren't great at 'big picture' things, so a few
functional tests, possibly even end-to-end tests, are handy.

Functional tests may use the file system, the real third-party libraries and
the committed reference data. They are therefore harder to troubleshoot if
anything goes wrong, and may be harder for a newcomer to set up.

##### Mocks

Mocks are classes that implement interfaces, designed specifically for unit
testing.

They return expected results from methods when expected arguments are
passed into those methods. They specifically do NOT try to implement any
real functionality. Methods should be designed not to care how their
dependencies are implemented. This is crucial to maintaining good
encapsulation and keeping the project easy to modify and maintain.

For example, a mock of a *multiply* method would return 12 when it receives
3 and 4 as inputs. It would not actually multiply 3 and 4 and return the
result. Implementing real multiplication in a test like this leads to tight
coupling. Discipline yourself not to test your dependencies'
implementations.

One can make ad-hoc mocks from scratch, but it is much more efficient to use
a formal framework. This project uses **Google Test with Google Mock**.

###### Should I mock a class or an interface?

Technically, Google Test does support mocking a class instead of an
interface.

However, this practice is discouraged. When one mocks an interface, it is
impossible for side effects to creep through into tests. If one mocks a
class, side effects are possible (for example, if one forgets to override a
virtual method in the base class).

## Interfaces in this project

Each dependency that does I/O, uses a third-party library, uses time or
threads, or is a large stage of the pipeline sits behind an interface. That
way its callers can be unit tested.

| Interface | Real implementation | Mocked when testing |
|---|---|---|
| `IVsmBus` | `Tms6100` | `Tms5220`, `Synth` (Load Address, Read, Branch sequences) |
| `ISampleSink` | `Synth`'s recorder, `ChipDriver` | `Tms5220` output |
| `ITms5220Observer` | `Synth`'s recorder, the test runner's event log | `Tms5220` events (VSM functions, host cycles, flags) |
| `IWavReader`, `IWavWriter` | `SndfileWavReader`, `SndfileWavWriter` (libsndfile) | The import pipeline, the WAV export |
| `IWavCodec` | `SndfileWavCodec` (libsndfile, in memory) | `ProjectSerializer`, the WAV export (`RunExport()`) |
| `IFileDigest` | `FileSha256Digest` | The import pipeline |
| `IResampler` | `SincResampler` (libsamplerate) | The import pipeline |
| `IFft` | `PocketFft` (pocketfft) | Encoder stages |
| `IFrameAnalyser`, `IPitchTracker`, `IVoicingClassifier`, `IRepeatSearch`, `ILevelSetter` | `FrameAnalyser`, `YinPitchTracker`, `VoicingClassifier`, `RepeatSearch`, `LevelSetter` | `Encoder` (orchestration only) |
| `IEncoder` | `Encoder` | `Optimiser`, the project's re-encode logic (`EncodingCache`), the GUI's encoder panel and Optimise |
| `ISynth` | `Synth` | `QualityAssessor`, `RenderEncodedAudio()`, `PhraseWaveforms`, `Optimiser`, the WAV export (`RunExport()`) |
| `IQualityMetric` | `OverallQualityMetric` | `Optimiser` |
| `IQualityAssessor` | `QualityAssessor` | The GUI's encoder panel and Optimise |
| `IVoiceActivity` | `FvadVoiceActivity` (libfvad) | The quality measures' voice activity mask |
| `ILpcAnalysis` | `SptkLpcAnalysis` (SPTK 4) | The spectral envelope measure |
| `IPitchEstimator` | `SptkPitchEstimator` (SPTK 4's REAPER, Harvest or RAPT) | The pitch and voicing measures; `YinPitchTracker`'s anchor |
| `IProjectStore` | `ZipProjectStore` (libzip), `DirectoryProjectStore` | `ProjectSerializer` |
| `IProjectEditor` | `Project` (the core editing operations) | The GUI's undo commands (`ProjectEdits`), `PhraseListModel`, `PhraseDetails`, `PhraseWaveforms` |
| `IDialogs` | `QtDialogs` (`QFileDialog`, `QMessageBox`, `ExportDialog`) | `MainWindow`: file choices, the save prompt, confirmations, the export to run, errors |
| `IRecentFilesStore` | `SettingsRecentFilesStore` (`QSettings`) | `MainWindow`'s recent-file list |
| `ITaskRunner` | `ThreadPoolTaskRunner` (a `QThreadPool`); `InlineTaskRunner` runs jobs at once | `Optimiser`, background jobs such as the import and the rendering of encoded audio (run inline in tests) |
| `IAudioOutput` | `QtAudioOutput` (`QAudioSink`) | `Player`, the playhead |
| `IClock` | `SteadyClock` (`std::chrono::steady_clock`) | The playhead, the debounce of re-encoding |
| `IDesktopColourScheme` | `PortalColourScheme` (the XDG desktop portal over Qt D-Bus); `NoDesktopColourScheme` on Windows and macOS | `ColourSchemeFollower` |

Code with no dependencies is unit tested directly, in memory, with no
mocks. That covers `FrameCodec` (pack/parse, bit order), `Booth`, the tables,
`AcornPhrom`, `TiIndexed`, `SpeakExternalStream`, `CHeader`, the PHROM allocator, the
BASIC tokeniser, the DFS image writer, the YAML mapping, and the
name/identifier rules.

The adapters over third-party libraries (libsndfile, libsamplerate,
pocketfft, libzip, libfvad, SPTK, `QAudioSink`, Qt D-Bus) are kept as thin
as possible. They can only be tested for real, so their tests are functional; the
audio output and the desktop portal need a device and a desktop session,
so they are checked by hand.

## Determinism requirements

- **Unit tests** must stay deterministic and isolated from the file system,
  the audio device, threads and the clock.
- **Functional tests** may use the file system (always under a
  `QTemporaryDir`) and the real libraries, but must stay deterministic and
  self-contained.
- **Encoder and emulator:** both are deterministic by design. The encoder
  uses no random numbers, and the emulator's noise generator starts from
  its power-up state. The same input must give the same output on every
  run.
- **Optimiser:** the result must not depend on the thread count. Candidates
  may be evaluated in parallel, but the best one is chosen by a
  deterministic reduction, with ties going to the lowest candidate index.
  Tests run it with an inline `ITaskRunner`, and one functional test
  compares 1 thread with 8.
- **Floating point:** `-ffast-math` is banned. Comparisons with reference
  data use the tolerances in the plan (§3.2, §3.3), and never a bare `==`
  on floating-point values.
- **Locale:** YAML numbers are written in the C locale. One functional test
  saves and reloads a project under a locale with a decimal comma (`de_DE`).
- **Failures:** a missing fixture or reference file is a hard failure, with
  a message naming the path. It is never a silent skip.

## Test naming

Tests are named by **behaviour**. Google Test does not allow underscores in
suite or test names, so names are `PascalCase`:

```cpp
TEST(FrameCodecTest, PacksStopFrameAsFourBits)
TEST(PhromAllocatorTest, StartsNewPhromWhenNextPhraseDoesNotFit)
TEST(OptimiserTest, RejectsCandidateThatExceedsProjectBudget)
```

## Repository layout

`app/tests/` holds automated tests and nothing else. Fixture projects live in
`app/projects/`, and reference data in `app/golden/`. Generated output never
lands in any of these trees.

```
app/tests/
├── CMakeLists.txt        Owns every test target
├── .clang-tidy           The app's clang-tidy settings, plus one test exception
├── support/              Shared helpers: golden-file readers, frame builders,
│                         the reference emulator's run modes, gMock mocks;
│                         dfs_catalogue.py, an independent DFS disc reader
│                         the functional tests run with Python 3 (skipped
│                         without it)
├── unit/                 → speak5220_unit_tests,           ctest label "unit"
├── functional/           → speak5220_functional_tests,     label "functional"
└── gui/
    ├── support/          Shared QApplication entry point (offscreen platform),
    │                     mocks of the main window's dialogs, settings and
    │                     audio output, a fake clock, mocked encoding tools,
    │                     light and dark palettes
    ├── unit/             → speak5220_gui_unit_tests,       label "unit"
    └── functional/       → speak5220_gui_functional_tests, label "functional"
```

**Classification is the directory, not a list.** A source file under
`unit/` is built into the unit binary, and every test it declares is
labelled `unit`. The same holds for `functional/`. Adding a test file is
enough: there is no pattern list to keep in step. The CMake function
`speak5220_add_test_lane()` in `app/tests/CMakeLists.txt` does this: it
globs each directory and takes the label from the directory's name, so a
new or moved file is picked up when the build next runs. A file whose tests split
across both categories must be split into two files, one per tree.

Use `functional` **only** when the objective cannot be met with a unit
test. Tests that touch the file system, real audio, the reference data or a
full pipeline (the CLI end to end, a GUI window) **must** be `functional`.

Run one lane with `ctest -L unit` or `ctest -L functional`. The aggregate
target `speak5220_tests` builds every test binary. The `unit` lane is the
fast, mocked, hermetic one, and it is the only lane the packaged Nix build
(`nix build .#speak5220`) runs. Keep it that way.

## Fixtures and reference data

| Directory | Contents |
|-----------|----------|
| `app/projects/general/` | Hand-authored feature fixtures: one phrase, many phrases, every frame kind, Acorn names with punctuation, ASCII maps |
| `app/projects/phrom/` | Multi-PHROM fixtures: a project that fills exactly one PHROM, and one that spills into a second. The project at the 64-PHROM budget is generated by its test, since its frames would take about 12 MB of YAML |
| `app/projects/bbc/` | Fixtures for the BBC Micro disc export: one PHROM with two pages of phrases (one padded to the 9-byte stream minimum), and five PHROMs, on both sides of the disc, the first holding the largest phrase a PHROM can. The projects at the 8-PHROM limit and over it are generated: in memory by the unit tests, and from `both-sides` by `bbc-check.sh` |
| `app/projects/benchmark/` | `synthetic-speech`: two speech-like clips (1.5 and 3.4 s), for timing Optimise in `app/scripts/benchmark.sh` |
| `app/golden/emulator/` | The C emulator's runs (plan §1.4): codes and event logs for its self-test scenarios, synthetic vectors and the Python encoder's output, through both paths |
| `app/golden/packaging/` | The Python encoder's Acorn PHROM and TI indexed images and Speak External streams |
| `app/golden/encoder/` | The C++ encoder's frames for the whole corpus with four option sets (no near-tie frames); for a corpus subset, WAV inputs and intermediates: the Python encoder's `encodeDetailed()` values up to the plan, and the C++ encoder's level setting |
| `app/golden/quality/measures/` | Independent implementations of the quality measures of plan §3.3 on the corpus subset's reference encodings: the voice activity masks, the pitch tracks and the scores |
| `app/golden/bbc/` | BBC BASIC II's tokenisation of three test programs, typed in on MAME's emulated BBC Micro by `app/scripts/golden/basic_programs.py` |
| `app/golden/output-hashes/` | The SHA-256 of every file the `general`, `phrom` and `bbc` fixture suites' export presets write, recorded by `output-hashes.sh --record` |

- **Fixture projects are stored unpacked**, as a directory with the same
  layout as a `.s5proj` ZIP (`project.yaml`, `phrases/<uuid>/…`). That way
  they can be diffed and reviewed. The application opens either form
  through `IProjectStore`.
- **Fixture audio is short and synthetic** where possible: tones, pulse
  trains, or a few seconds of speech. Fixtures stay small enough to commit.
- **Mechanically derived data is not committed.** The WAV copies of the
  phase 11 corpus are generated by `app/scripts/generate_corpus.py` into
  `build/app/generated-corpus/`. Generation needs a Python 3 interpreter with
  `soundfile`. Without it the build still succeeds, and only the corpus
  check (below) is unavailable.
- **The reference data** was produced from the retired Python encoder and C
  emulator (plan §1.4, decision D5). The Python encoder's output (the
  packaging data, the emulator's corpus phrases and the stages before level
  setting in the encoder's intermediates) is fixed. The rest is written by
  `app/scripts/golden/generate_golden.py` (the emulator's runs),
  `app/scripts/golden/encoder.py` (encoder) and
  `app/scripts/golden/measures.py` (the quality measures of plan §3.3,
  from py-webrtcvad, pystoi, REAPER, pyworld and NumPy), run by hand in the default Nix shell
  (`nix develop --command python3 app/scripts/golden/encoder.py`).
  `--check` regenerates the data into a temporary directory and compares it
  with the committed files. It is a fixed regression baseline:
  - **Emulator data never changes**, unless the datasheet does.
  - **Encoder data is regenerated only for an intended improvement.** It is
    regenerated from the C++ itself, in its own commit that records the
    quality before and after. The quality data follows the encoder data it
    measures.
  - **Where the encoder departs from the Python, its own output is the
    reference.** The option sets are read over `ReferenceEncoderOptions()`
    (`tests/support/encoder_golden.h`), the Python encoder's defaults with
    the options it does not have off, such as the resonance hold. The
    frames and the level-setting intermediates are the C++ encoder's
    (`app/scripts/golden/encoder.py` runs `EncoderReferenceWriter`), whose
    level setting departs from the Python's, and the stages before them are
    checked against the Python's values (the pitch track without the C++
    encoder's pitch anchor, and the voicing without its onset rule, which
    the unit tests check).
    A change behind an option that is off in those sets leaves the
    reference data as it is, and is tested by unit tests of its own.

**Small vectors belong in the test source.** A few dozen bytes, such as the
Speech System User Guide's "ZERO" stream, the §8.8 multiplier examples or a
hand-assembled frame, are written into the test as constants, which keeps
those tests in the unit lane. Comparisons against files in `app/golden/`
read the file system, so they are functional.

## Scripts and output

Everything writes through an output directory chosen by the caller (the
CLI's `--output DIR`), never into `app/projects/`, `app/golden/` or
`app/tests/`.
The exceptions are the scripts in `app/scripts/golden/` (`generate_golden.py`,
`encoder.py`, `measures.py` and `basic_programs.py`), which write
`app/golden/` themselves, and
`output-hashes.sh --record`, which writes `app/golden/output-hashes/`; they are run by
hand, and only to create or, by agreement, regenerate the reference data
(AGENTS.md §3.1).

- `app/scripts/run-projects.sh [general|phrom|bbc|all]` runs a fixture suite end to
  end through `speak5220-cli`. It encodes each fixture into a copy (the
  fixtures are never written) and runs the copy's export presets
  (`exports:` in `project.yaml`) into
  `build/app/project-output/<suite>/<fixture>/export/`, listing the files
  each run wrote in `written.txt` beside it.
- `app/scripts/output-hashes.sh` runs the `general`, `phrom` and `bbc`
  suites and compares SHA-256 manifests of every file they export (PHROM
  images, C headers, WAV, BBC Micro discs)
  against the recorded baseline in `app/golden/output-hashes/`, showing any
  difference and exiting 1. `--record` records the manifests as the
  baseline, for an intended change to an exporter or to the fixtures'
  presets, by agreement. This is how a change is shown to leave the output
  byte-identical.
- `app/scripts/bbc-check.sh` exports the `bbc` suite, then boots each disc
  on MAME's emulated BBC Micro Model B (speech system, Acorn DFS) with
  Shift+Break and drives the player through a Lua script
  (`app/scripts/bbc/`): it checks the PHROM list and every page against
  the disc's index files, that each phrase asked for and every phrase of
  play all is spoken, in order, as long as our emulator's output and
  following its envelope, that Escape stops play all after the phrase
  being spoken, and that the player says so on a machine without the
  speech system; it reports the least free memory seen. It also builds an
  8-PHROM disc, checks that 9 PHROMs are refused, and checks that the
  disc's player is what BBC BASIC II itself makes of its source. It needs
  MAME and the ROM set in `local/mame/roms/` (BUILD.md), so it is run by
  hand, writing to `build/app/bbc-check/`.
- `app/scripts/corpus-check.sh` runs the encoder over the full generated
  corpus with every option set and checks the plan's acceptance criteria
  (§3.2): at least 99% of encodings identical to the reference, the rest
  differing only at the reference's near-tie frames and within 0.02 of its
  overall quality score. Then it optimises every file from
  the default options (Phase 7), with a byte limit at the default
  encoding's size, and checks that the best is within the limit and
  scores above the defaults on most files (`SPEAK5220_OPTIMISE_STRIDE=N`
  takes every Nth file, for a quicker run). Last, it measures every file
  with the quality measures of plan §3.3 (Phase 8), as the reference
  encodings, the application's default and optimised encodings and the
  anchor vocoder render it, for the calibration of the overall score
  (`measures.tsv`). It writes its reports to `build/app/corpus-check/`.
  `corpus-check.sh --listening` instead writes the listening check's
  samples: for each rating band, files spread over its scores, as the
  original and the chip's output (`listening/`). It is slow, so it is run by hand in the default
  Nix shell, not by CTest.
- For choosing the encoder's defaults (plan Phase 8, task 14), the
  Optimise corpus check also writes each file's best options, and
  `SPEAK5220_OPTIMISE_START` (such as `noise_gate_db=30`) changes where it
  starts. `OptionsCorpusCheck.DISABLED_ScoresTheOptions` encodes the corpus
  with the options in `SPEAK5220_OPTIONS` (the same form; unset for the
  defaults) and writes each file's size, encoding time and score to
  `SPEAK5220_OPTIONS_REPORT`. Both are run by hand from the functional
  test binary, with `SPEAK5220_CORPUS_DIR` set to the generated corpus.
- `app/scripts/benchmark.sh` times encoding on one core, against the target
  of 0.05 s per second of speech, then the quality measures of plan §3.3
  with their synthesis and preparation, writing
  per-file timings into `build/app/project-output/benchmark/`. It takes the
  corpus subset by default, or any directory of WAV files. Then it times
  Optimise on every phrase of `app/projects/benchmark/synthetic-speech`,
  on one thread and on every core.
- `app/scripts/arctic-project.sh` builds a project of real speech for
  trying the GUI by hand: the 33 CMU ARCTIC prompts of the corpus
  recordings, added and encoded with the default options, in
  `build/app/test-projects/arctic.s5proj`. It is derived from committed
  files, so it is generated, not committed; it is run by hand in the
  default Nix shell (`nix develop --command app/scripts/arctic-project.sh`).
- The encoder-quality diagnostics
  ([encoder-quality-plan.md](docs-tech/encoder-quality-plan.md), Phase 1;
  HLD §9, "Diagnostics") judge changes to the encoder by figures:
  - `app/scripts/phrom-check.sh` writes every word of Acorn PHROM A as
    frames and as the chip's output (`phrom_roundtrip.py`, into
    `build/app/phrom-roundtrip/`; each word's frames must pack back to the
    image's bytes), encodes the 157 speech words and compares the
    encodings with the PHROM's frames (`PhromRoundTripCheck`), writing
    `build/app/phrom-check/phrom-roundtrip.tsv` and a summary.
  - `app/scripts/option_sweep.py` encodes a directory of WAV files with
    each option set of a table (by default
    `app/scripts/sweeps/encoder-quality.tsv`) through the CLI (`new
    --option`, `add`, `encode`, `measure`), with `--phrom` the round trip
    too, and prints one row of medians per set, into
    `build/app/option-sweep/`. `--only` takes some of the files: with
    `app/scripts/sweeps/investigation-tts.txt` on
    `local/phoneme-corpus/say/`, the investigation's 64 TTS files.
  - `app/scripts/phoneme-check.sh` renders the phoneme test set
    (`phoneme_prompts.csv`, by `phoneme_corpus.py`, with Piper on Linux or
    `--engine say` on macOS, into the gitignored `local/phoneme-corpus/`,
    recording each file's SHA-256), encodes it and reports per file how its
    consonant and vowel came out (`PhonemeCheck`), into
    `build/app/phoneme-check/`.

  Each takes the encoder options as `SPEAK5220_PHROM_OPTIONS` or
  `SPEAK5220_PHONEME_OPTIONS` (the form of `SPEAK5220_OPTIONS`). They are
  run by hand in the default Nix shell.
- **By-hand tests.** The full-corpus check, the timing and the
  encoder-quality diagnostics are functional
  tests with Google Test's `DISABLED_` prefix: CTest lists them as disabled
  and never runs them. The scripts run them with
  `--gtest_also_run_disabled_tests`, passing their inputs and report files
  in environment variables (`SPEAK5220_CORPUS_DIR`,
  `SPEAK5220_CORPUS_REPORT`, `SPEAK5220_QUALITY_REPORT`,
  `SPEAK5220_OPTIMISE_REPORT`, `SPEAK5220_BENCHMARK_DIR`,
  `SPEAK5220_BENCHMARK_REPORT`, `SPEAK5220_BENCHMARK_PROJECT`,
  `SPEAK5220_PHROM_DIR`, `SPEAK5220_PHROM_REPORT`,
  `SPEAK5220_PHONEME_DIR`, `SPEAK5220_PHONEME_REPORT`).
- The functional suites point their output at a `QTemporaryDir`, so
  `ctest -L functional` never writes into the source tree.

## Lanes

| Lane | What runs | Where |
|---|---|---|
| Pre-commit | Format and lint checks (plan §1.3) | Every commit, locally |
| `unit` | `ctest -L unit` | Every push in CI, on Linux; the packaged Nix build's check phase |
| `functional` | `ctest -L functional`, GUI on the offscreen platform | Every push in CI, on Linux |
| Windows and macOS | Both lanes on Windows and macOS | Only for a release (a `v*` tag) or when started by hand |
| Sanitisers | Both lanes under ASan and UBSan (Debug) | Every push in CI, on Linux |
| By hand | `corpus-check.sh`, `output-hashes.sh`, `benchmark.sh`, `bbc-check.sh`; the BBC Micro hardware check ([BBC Micro disc](docs/application/user-manual/bbc-disc.md)) | Before a release, and for any change to the encoder, packaging or BBC export |
| By hand | `phrom-check.sh`, `option_sweep.py`, `phoneme-check.sh` | For a change meant to improve the encoder's output (the encoder-quality plan) |

The default CI unit lane must stay fast and mocked. Do **not** add
functional tests to it.
