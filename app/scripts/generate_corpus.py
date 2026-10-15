#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes WAV copies of the speech corpus for the corpus check.

The corpus (investigation/captures/phase11/inputs/, 8 kHz 16-bit FLAC) is
converted losslessly to 16-bit WAV, named by file stem, so the application's
code reads WAV only (TESTING.md). The copies are mechanically derived, so
they are generated, not committed: by default into build/app/generated-corpus/.

Needs a Python 3 with soundfile, as in the project's default Nix shell:

    nix develop --command python3 app/scripts/generate_corpus.py [--output DIR]
"""

import argparse
import sys
from pathlib import Path

import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "investigation" / "captures" / "phase11" / "inputs"
DEFAULT_OUTPUT = ROOT / "build" / "app" / "generated-corpus"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="directory for the WAV files (default: %(default)s)",
    )
    args = ap.parse_args()
    files = sorted(CORPUS.glob("*/*.flac"))
    if not files:
        sys.exit("no corpus files in %s" % CORPUS)
    if len({p.stem for p in files}) != len(files):
        sys.exit("corpus file names are not unique")
    args.output.mkdir(parents=True, exist_ok=True)
    for path in files:
        data, rate = sf.read(str(path), dtype="int16", always_2d=True)
        sf.write(str(args.output / (path.stem + ".wav")), data, rate, subtype="PCM_16")
    print("wrote %d files to %s" % (len(files), args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
