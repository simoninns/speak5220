---
name: Bug Report
about: Report a defect in the Speak5220 application or the repository's tools
labels: bug
title: "[Bug] "
---

# Bug Report

## Description

<!--
A clear and concise description of what the bug is.
-->

## Area

- [ ] Import (WAV, resampling)
- [ ] Encoder / Optimise / quality measures
- [ ] Emulator / playback / waveforms
- [ ] PHROM assignment and project size
- [ ] Project files (`.s5proj`, YAML)
- [ ] Export (PHROM image, TI indexed, C header, `.bin`, WAV)
- [ ] BBC Micro disc export (`.dsd`, PHROM player)
- [ ] GUI
- [ ] CLI (`speak5220-cli`)
- [ ] Build / packaging (Nix, Flatpak, MSI, DMG)
- [ ] Research tools (`investigation/`: firmware, capture tools, reference emulator)

## Steps to Reproduce

1. 
2. 
3. 

## Expected Behaviour

## Actual Behaviour

## Environment

- **OS:** [e.g., NixOS 26.05, Windows 11, macOS 15]
- **Installed from:** [Nix build / Flatpak / MSI / DMG / local build]
- **Version or commit SHA:**
- **Compiler and build type (local builds):** [e.g., GCC 14, Release]
- **Audio output device (playback issues):**
- **BBC Micro setup (BBC export issues):** [real machine or emulator; MOS version; Gotek or disc drive]

## Files

<!--
If you can, attach the smallest input that shows the problem:
- the source WAV, or the project (`.s5proj`);
- for export problems, the exported file.
Remove anything you do not want to share.
-->

## Logs / Output

```
<!-- Paste relevant console output or log files here (debug level if possible) -->
```

## Specifications or Sources Involved

<!--
If this relates to a documented behaviour, reference it:
- Reference datasheet: docs/technical/datasheet/ (e.g. §8.8, §12.8, §12.9)
- TI data manuals: docs/technical/sources/
- Acorn Speech System User Guide (Section 5, 6 or 7)
- The quality measures: docs/application/user-manual/encoder.md
-->

## Possible Solution

<!--
If you have suggestions for a fix, describe them here.
-->

## Additional Context
