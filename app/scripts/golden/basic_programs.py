#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes BBC BASIC II's tokenisation of the reference programs.

Each program in app/golden/bbc/programs/NAME.bas is typed into BBC BASIC II
on MAME's BBC Micro (app/scripts/bbc/basic_in_mame.py), and the program
BASIC stored, the bytes SAVE writes, is written to NAME.bin beside it.
The application's tokeniser is compared with these files by its functional
tests.

MAME's bbcb ROM set is not part of the repository; it is read from --roms
(default local/mame/roms). REFERENCE.tsv records the SHA-256 of the BASIC
and MOS ROMs that tokenised the programs. MAME is taken from PATH, or else
from nixpkgs with "nix shell". Run by hand, from the repository root:

    nix develop --command python3 app/scripts/golden/basic_programs.py
    nix develop --command python3 app/scripts/golden/basic_programs.py --check

--check tokenises the programs again and compares the result with the
committed files. Ask before rewriting them (AGENTS.md §3.1).
"""

import argparse
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GOLDEN = ROOT / "app" / "golden" / "bbc"
sys.path.insert(0, str(ROOT / "app" / "scripts" / "bbc"))
import basic_in_mame  # noqa: E402

# The ROMs whose code tokenises the programs.
ROMS = ("bbcb/basic2.rom", "bbcb/os12.rom")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--roms", type=Path, default=ROOT / "local/mame/roms")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    sources = sorted((GOLDEN / "programs").glob("*.bas"))
    # DFS names have at most 7 characters: the programs go on the disc as
    # P1, P2, ...
    programs = {f"P{i + 1}": path.read_text() for i, path in enumerate(sources)}
    tokenised = basic_in_mame.tokenise(args.roms, programs)
    reference = "".join(
        f"sha256\t{hashlib.sha256((args.roms / rom).read_bytes()).hexdigest()}\t{rom}\n"
        for rom in ROMS
    )

    differences = 0
    for i, path in enumerate(sources):
        output = path.with_suffix(".bin")
        data = tokenised[f"P{i + 1}"]
        if args.check:
            if not output.exists() or output.read_bytes() != data:
                print(f"basic_programs: {output} differs")
                differences += 1
        else:
            output.write_bytes(data)
            print(f"basic_programs: wrote {output} ({len(data)} bytes)")
    if args.check:
        if (GOLDEN / "REFERENCE.tsv").read_text() != reference:
            print("basic_programs: the ROMs differ from REFERENCE.tsv")
            differences += 1
        print(f"basic_programs: {len(sources) - differences} of {len(sources)} match")
        return 1 if differences else 0
    (GOLDEN / "REFERENCE.tsv").write_text(reference)
    return 0


if __name__ == "__main__":
    sys.exit(main())
