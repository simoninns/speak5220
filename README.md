# speak5220
Texas Instruments TMS5220 voice synthesis processor

## Cloning

The repository uses a git submodule for the [Ki-Lime Pi](https://github.com/recursivenomad/ki-lime-pi-to-go) Raspberry Pi Pico KiCad library, so clone it recursively:

```bash
git clone --recursive https://github.com/simoninns/speak5220.git
```

If you have already cloned without `--recursive`, fetch the submodule with:

```bash
git submodule update --init
```

## Development environment (Nix)

A Nix flake provides a development shell with KiCad 10, Python 3 and git:

```bash
nix develop
```

From inside the shell, start KiCad with `kicad`, or use `kicad-cli` for command-line tasks such as ERC:

```bash
kicad-cli sch erc kicad/tms5220_usb/tms5220_usb.kicad_sch
```

The flake requires Nix with flakes enabled (`experimental-features = nix-command flakes`).

## Repository layout

- `kicad/tms5220_usb/` – KiCad project, including the project symbol library for the TMS5220 and TMS6100
- `kicad/libraries/ki-lime-pi-pico/` – Ki-Lime Pi library submodule (Raspberry Pi Pico symbols, footprints and 3D models)
- `docs-tech/` – Datasheets and reference schematics
