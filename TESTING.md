# Testing vision and strategy

This document covers the Speak5220 application in `app/`, and sets the
testing rules it is built to, following the plan in
[docs-tech/application-plan.md](docs-tech/application-plan.md). The test
tree, its lanes, the CI jobs, the emulator's interfaces and the emulator and
packaging reference data below exist; the other interfaces, the fixture
projects, the encoder's reference data and the scripts are added by the
plan's later phases.

The research code in `investigation/` (the Python encoder, the C emulator and
the capture tools) has its own self-tests and is not covered here.

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
| `IResampler` | `SincResampler` (libsamplerate) | The import pipeline |
| `IFft` | `PocketFft` (pocketfft) | Encoder stages, PESQ stages |
| `IPitchTracker`, `IVoicingClassifier`, `IRepeatSearch`, `ILevelSetter` | The encoder stages | `Encoder` (orchestration only) |
| `IEncoder` | `Encoder` | `Optimiser`, the project's re-encode logic |
| `ISynth` | `Synth` | `Optimiser`, `Player` |
| `IQualityMetric` | PESQ, cheap measures | `Optimiser` |
| `IProjectStore` | `ZipProjectStore` (libzip), `DirectoryProjectStore` | `ProjectSerializer` |
| `IProjectEditor` | `Project` (the core editing operations) | The GUI's undo commands, `PhraseListModel` |
| `ITaskRunner` | `QtConcurrent` thread pool | `Optimiser`, background jobs (run inline in tests) |
| `IAudioOutput` | `QAudioSink` adapter | `Player`, the playhead |
| `IClock` | Monotonic clock | The playhead timer, the debounce of re-encoding |

Code with no dependencies is unit tested directly, in memory, with no
mocks. That covers `FrameCodec` (pack/parse, bit order), `Booth`, the tables,
`AcornPhrom`, `TiIndexed`, `SpeakExternalStream`, the PHROM allocator, the
BASIC tokeniser, the DFS image writer, the YAML mapping, and the
name/identifier rules.

The adapters over third-party libraries (libsndfile, libsamplerate,
pocketfft, libzip, `QAudioSink`) are kept as thin as possible. They can only
be tested for real, so their tests are functional.

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
│                         the reference emulator's run modes, gMock mocks
├── unit/                 → speak5220_unit_tests,           ctest label "unit"
├── functional/           → speak5220_functional_tests,     label "functional"
└── gui/
    ├── support/          Shared QApplication entry point (offscreen platform)
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
| `app/projects/phrom/` | Multi-PHROM fixtures: a project that fills exactly one PHROM, one that spills into a second, and one at the 64-PHROM budget |
| `app/projects/bbc/` | Fixtures for the DFS disc export |
| `app/projects/benchmark/` | Fixed-size projects for `app/scripts/benchmark.sh` |
| `app/golden/emulator/` | The C emulator's runs (plan §1.4): codes and event logs for its self-test scenarios, synthetic vectors and encoder output, through both paths |
| `app/golden/packaging/` | The Python encoder's Acorn PHROM and TI indexed images and Speak External streams |
| `app/golden/` (later) | The encoder's frames and intermediates, PESQ scores |

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
  emulator (plan §1.4, decision D5) by
  `app/scripts/golden/generate_golden.py`, run by hand in the default Nix
  shell (`nix develop --command python3 app/scripts/golden/generate_golden.py`).
  `--check` regenerates it into a temporary directory and compares it with
  the committed files. It is a fixed regression baseline:
  - **Emulator data never changes**, unless the datasheet does.
  - **Encoder data is regenerated only for an intended improvement.** It is
    regenerated from the C++ itself, in its own commit that records the
    quality before and after.

**Small vectors belong in the test source.** A few dozen bytes, such as the
Speech System User Guide's "ZERO" stream, the §8.8 multiplier examples or a
hand-assembled frame, are written into the test as constants, which keeps
those tests in the unit lane. Comparisons against files in `app/golden/`
read the file system, so they are functional.

## Scripts and output

Everything writes through an output directory chosen by the caller (the
CLI's `--output DIR`), never into `app/projects/`, `app/golden/` or
`app/tests/`.
The one exception is `app/scripts/golden/generate_golden.py`, which writes
`app/golden/` itself; it is run by hand, and only to create or, by
agreement, regenerate the reference data (AGENTS.md §3.1).

- `app/scripts/run-projects.sh [general|phrom|bbc]` runs a fixture suite end to
  end through `speak5220-cli`. It encodes each fixture and runs its export
  presets (`exports:` in `project.yaml`) into `build/app/project-output/<suite>/`.
- `app/scripts/output-hashes.sh` regenerates those suites and compares SHA-256
  manifests of every export (PHROM images, C headers, `.ssd` images, WAV)
  against a recorded baseline (`--record` to record one). This is how a
  change is shown to leave the output byte-identical.
- `app/scripts/corpus-check.sh` runs the encoder and PESQ over the full
  generated corpus and checks the plan's acceptance criteria (§3.2, §3.3).
  It is slow, so it is run by hand, not by CTest.
- `app/scripts/benchmark.sh` times encoding, PESQ and Optimise on
  `app/projects/benchmark/` for each thread configuration, writing into
  `build/app/project-output/benchmark/`.
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
| By hand | `corpus-check.sh`, `output-hashes.sh`, `benchmark.sh`; the BBC hardware checklist (plan Phase 9) | Before a release, and for any change to the encoder, packaging or BBC export |

The default CI unit lane must stay fast and mocked. Do **not** add
functional tests to it.
