# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tokenises BASIC programs with BBC BASIC II itself, on MAME's BBC Micro.

The programs' text is put on a single-sided DFS disc (written here, apart
from the application's own writer), and app/scripts/bbc/tokenise.lua types
each one in with *EXEC and reads back the program BASIC stored. The result
is what SAVE would write, so the application's tokeniser can be compared
with BASIC II byte for byte.
"""

import os
import pathlib
import shutil
import subprocess
import tempfile

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
SECTOR = 256
SECTORS = 400


def write_ssd(path, files):
    """A 40-track single-sided DFS image holding `files` ((name, bytes))
    in directory $, laid out from sector 2 (Acorn DFS Manual)."""
    disc = bytearray(SECTORS * SECTOR)
    entries = []
    sector = 2
    for name, data in files:
        disc[sector * SECTOR : sector * SECTOR + len(data)] = data
        entries.append((name, len(data), sector))
        sector += (len(data) + SECTOR - 1) // SECTOR
    disc[0:8] = b"BASIC   "
    disc[SECTOR : SECTOR + 4] = b"    "
    disc[SECTOR + 5] = 8 * len(entries)
    disc[SECTOR + 6] = SECTORS >> 8
    disc[SECTOR + 7] = SECTORS & 0xFF
    # The catalogue lists the last file laid out first.
    for i, (name, length, start) in enumerate(reversed(entries)):
        disc[8 + 8 * i : 15 + 8 * i] = name.ljust(7).encode("ascii")
        disc[15 + 8 * i] = ord("$")
        info = SECTOR + 8 + 8 * i
        disc[info + 4] = length & 0xFF
        disc[info + 5] = length >> 8 & 0xFF
        disc[info + 6] = (length >> 16 & 3) << 4 | (start >> 8 & 3)
        disc[info + 7] = start & 0xFF
    pathlib.Path(path).write_bytes(bytes(disc))


def mame_command():
    if shutil.which("mame"):
        return ["mame"]
    return ["nix", "shell", "nixpkgs#mame", "--command", "mame"]


def tokenise(roms, programs):
    """BASIC II's tokenisation of each program, by name. `programs` maps a
    DFS name (up to 7 characters) to the program's text."""
    with tempfile.TemporaryDirectory() as work:
        work = pathlib.Path(work)
        files = [
            (name, text.replace("\r\n", "\n").replace("\n", "\r").encode("ascii"))
            for name, text in programs.items()
        ]
        write_ssd(work / "programs.ssd", files)
        result = subprocess.run(
            mame_command()
            + ["bbcb", "-rompath", str(roms), "-flop1", str(work / "programs.ssd")]
            + ["-video", "none", "-sound", "none", "-nothrottle", "-skip_gameinfo"]
            + ["-autoboot_script", str(SCRIPT_DIR / "tokenise.lua")]
            + ["-cfg_directory", str(work / "cfg")]
            + ["-nvram_directory", str(work / "nvram")],
            env={**os.environ, "BBC_PROGRAMS": " ".join(programs)},
            capture_output=True,
            text=True,
            check=False,
        )
    tokenised = {}
    for line in result.stdout.splitlines():
        if line.startswith("PROGRAM "):
            _, name, data = (line.split(" ", 2) + [""])[:3]
            tokenised[name] = bytes.fromhex(data)
    missing = set(programs) - set(tokenised)
    if missing:
        raise RuntimeError(f"MAME did not tokenise {sorted(missing)}: {result.stderr}")
    return tokenised
