<p align="center"><img src="assets/logo/speak5220-logo.svg" alt="Speak5220 logo" width="480"></p>

# speak5220

Speak5220 is a desktop application for making speech for the Texas Instruments TMS5220 voice synthesis processor, the chip in the BBC Micro's speech system. You give it recordings of words and phrases; it encodes them into the data the chip speaks, lets you hear exactly how the chip will sound, and builds new speech ROMs (PHROMs) from them.

<p align="center"><img src="assets/gui-captures/gui-main-window-web.webp" alt="The Speak5220 main window: the phrase list, an encoded phrase's waveform with its frame grid, and its quality rating" width="800"></p>

- **Documentation:** <https://simoninns.github.io/speak5220/> (source in [`docs/`](docs/index.md))
- **Download:** installers for Linux (Flatpak), Windows (MSI) and macOS (DMG) on the [Releases](https://github.com/simoninns/speak5220/releases) page
- **Building:** [`BUILD.md`](BUILD.md): cloning, the Nix development shell, the application and the documentation site

## What it does

- **Imports speech** from WAV files of any sample rate, or the words of existing Acorn PHROM images, such as Acorn's own PHROM A.
- **Prepares the recording** for the chip, with a noise gate, bass cut, treble tilt and de-esser, and a preset for the BBC Micro's speaker.
- **Encodes it** into TMS5220 frames, and plays the result through a built-in emulator of the chip, so what you hear is what the chip will say.
- **Rates the result** from *Poor* to *Good*, by objective measures of the spectral envelope, pitch and intelligibility, and **optimises** the encoder's settings for the best quality or the smallest size.
- **Fills PHROMs**: phrases are packed into 16 KB PHROMs in order, with their names, header strings and ASCII map, and the window shows how much space is left.
- **Exports** Acorn PHROM images, C headers for your own firmware, WAV files, and BBC Micro disc images that speak the phrases through Speak External, without a PHROM.

It is a cross-platform Qt/C++ application with a GUI, `speak5220`, and a command-line tool, `speak5220-cli`, that does the same from scripts. The [user documentation](https://simoninns.github.io/speak5220/application/) says how to install and use it.

## How it was made

Speak5220's emulator and encoder are not guesses at how the TMS5220 works. A purpose-built board drove a real chip from a Raspberry Pi Pico 2, emulated its speech ROMs and recorded every output sample, so that the chip's behaviour could be measured. From those measurements came a reference datasheet that describes the chip sample for sample, and the application's emulator and encoder were written from that datasheet alone. The datasheet, and how the emulator and encoder work, are in the documentation's [Technical](https://simoninns.github.io/speak5220/technical/) section.

## Repository layout

| Path | Contents |
|---|---|
| [`app/`](app/CMakeLists.txt) | The Speak5220 application: its CMake build, sources (`src/`), tests (`tests/`), fixture projects for the tests (`projects/`), reference data from the retired encoder, emulator and quality tools, and the recorded hashes of the fixtures' exports (`golden/`), style configuration, the vcpkg manifest for Windows and macOS (`vcpkg.json`, with overlay ports in `vcpkg-ports/`), the installers' build files (`packaging/`: the Flatpak manifest and the macOS vcpkg triplet), the icon set, About-box logo, Linux `.desktop` file and AppStream metainfo (`resources/`), and check, corpus, benchmark, export, BBC Micro emulator golden-data, icon and encoder-quality diagnostic scripts (`scripts/`, with the diagnostics' option sets in `scripts/sweeps/`) |
| [`docs/`](docs/index.md) | The published documentation site: the application's user documentation, the reference datasheet, and how the emulator and encoder work |
| [`assets/`](assets/README.md) | Artwork: the logo, the application icon and a screenshot of the application |
| `flake.nix` | The Nix development shells, the application's package and its pre-commit hooks |
| [`BUILD.md`](BUILD.md) | Development environment and build instructions |
| [`TESTING.md`](TESTING.md) | Testing vision, strategy and rules for the application |
| [`AGENTS.md`](AGENTS.md) | Contribution and coding standards. `CLAUDE.md`, `.github/copilot-instructions.md`, `.cody/` and `.vibe/` point AI coding assistants to it |
| `.github/` | Issue and pull request templates, the application's CI workflows, the release workflow that builds the installers, and the workflow that publishes the documentation site |

## The research

The research that produced the reference datasheet (the capture board and its firmware, the capture and analysis tools, the captures, the speech PROM inventory and the die analysis, with the pages documenting them) was removed from the repository after commit [`c537c1d`](https://github.com/simoninns/speak5220/tree/c537c1d), the last commit that contains it. It can be read there.

## Contributing

Read [`AGENTS.md`](AGENTS.md) for the coding standards and [`TESTING.md`](TESTING.md) for the testing rules. Issues and pull requests use the templates in `.github/`. Report security vulnerabilities privately, through GitHub's security advisories for this repository.

## Licence

GNU General Public License v3.0 or later. See [`LICENSE`](LICENSE).
