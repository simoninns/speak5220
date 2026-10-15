# Investigation

Everything made to measure and document the TMS5220: the capture board (`kicad/`), its firmware (`firmware/`), the host tools (`tools/`), the captures (`captures/`) and the speech PROM manifests (`proms/`). The emulator written from the reference datasheet is the application's, in `app/src/core/emulator/`; the tools run it as `tms5220emu` ([BUILD.md](../BUILD.md)).

- Documentation: [docs/technical/](../docs/technical/index.md), published at <https://simoninns.github.io/speak5220/technical/>
- Building: [BUILD.md](../BUILD.md)

`local/` (git-ignored) holds third-party code and large working files that are not redistributed.
