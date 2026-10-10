# speak5220

The Texas Instruments TMS5220 voice synthesis processor, measured on real hardware: a reference datasheet, an emulator and a speech encoder written from it, and the capture board, firmware and tools that made the measurements.

- **Documentation:** <https://simoninns.github.io/speak5220/> (source in [`docs/`](docs/index.md))
- **Building:** [`BUILD.md`](BUILD.md): cloning, the Nix development shell, the firmware, the emulator and the documentation site

## The Speak5220 application

A cross-platform Qt/C++ application for creating and editing TMS5220 LPC speech data and building new PHROMs is **in development**. It will:

- import WAV speech and encode it, with an optimiser that tunes the encoder for the best PESQ score;
- play the original and the encoded speech, the latter through a built-in emulator;
- export Acorn PHROM and TI indexed images, C headers, raw data, WAV, and BBC Micro disc images that speak through Speak External.

So far only its scaffolding exists: the build, an empty main window, a command-line tool that prints its version, the test tree, and the format, lint and CI checks. The plan is in [`docs-tech/application-plan.md`](docs-tech/application-plan.md), and the design in [`docs-tech/design/high-level-design.md`](docs-tech/design/high-level-design.md).

## Repository layout

| Path | Contents |
|---|---|
| [`app/`](app/CMakeLists.txt) | The Speak5220 application: its CMake build, sources (`src/`), tests (`tests/`), reference data from the retired encoder and emulator (`golden/`), style configuration, check scripts and the golden-data generator (`scripts/`) |
| [`docs/`](docs/index.md) | The published documentation site: the reference datasheet and the research record |
| [`docs-tech/`](docs-tech/application-plan.md) | Developer documents that are not part of the site: the application's implementation plan and its high-level design |
| [`investigation/`](investigation/README.md) | The research: the capture board (`kicad/`), its firmware (`firmware/`), the host tools (`tools/`), the captures (`captures/`), the speech PROM manifests (`proms/`), and the reference emulator (`emulator/`) and encoder (`encoder/`) |
| [`assets/`](assets/README.md) | Artwork: the logo and the application icon |
| `flake.nix` | The Nix development shells, the application's package and its pre-commit hooks |
| [`BUILD.md`](BUILD.md) | Development environment and build instructions |
| [`TESTING.md`](TESTING.md) | Testing vision, strategy and rules for the application |
| [`AGENTS.md`](AGENTS.md) | Contribution and coding standards. `CLAUDE.md`, `.github/copilot-instructions.md`, `.cody/` and `.vibe/` point AI coding assistants to it |
| `.github/` | Issue and pull request templates, the application's CI workflow, and the workflow that publishes the documentation site |

## Contributing

Read [`AGENTS.md`](AGENTS.md) for the coding standards and [`TESTING.md`](TESTING.md) for the testing rules. Issues and pull requests use the templates in `.github/`. Report security vulnerabilities privately, through GitHub's security advisories for this repository.

## Licence

GNU General Public License v3.0 or later. See [`LICENSE`](LICENSE).
