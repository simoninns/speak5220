#!/usr/bin/env python3
"""
    prom.py

    Speech PROM manifests (plan sections 6 and 8). A manifest,
    proms/<name>.json, names a VSM/PHROM image, gives its hash and
    provenance, and lists its word table. The runner (run.py) copies it into
    each capture set as <prom>/prom.json.

    Images that may not be redistributed are kept out of the repo, in
    proms/images/ (git-ignored); the manifest's hash identifies them, and
    promimport.py rebuilds them, and the manifests, from their pinned public
    sources. The Acorn PHROM A image is already in the repo, in
    firmware/src/romdata_acorn.h, and its manifest calls it "builtin".

    An image starts at the first VSM address of its bank and may span
    several 16K banks (a 32K image in banks 0 and 1, for example); word
    addresses are full VSM addresses.

      python tools/prom.py list acorn_phrom_a      # the word table

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import hashlib
import json
import os
import re

import pico as picolib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PROMS_DIR = os.path.join(ROOT, "proms")
BANK_SIZE = 0x4000


def manifestPath(name):
    return name if name.endswith(".json") else os.path.join(PROMS_DIR, name + ".json")


def loadManifest(name):
    with open(manifestPath(name)) as f:
        return json.load(f)


def image(manifest):
    """The image bytes, checked against the manifest's SHA-256."""
    source = manifest["image"]["source"]
    if source == "builtin":
        data = picolib.acornImage()
    else:
        path = os.path.join(ROOT, source)
        if not os.path.exists(path):
            raise FileNotFoundError(f"{source} is missing: run tools/promimport.py to rebuild it")
        with open(path, "rb") as f:
            data = f.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != manifest["image"]["sha256"]:
        raise ValueError(f"{source}: SHA-256 {digest} does not match the manifest")
    return data


def base(manifest):
    """The VSM address of the image's first byte."""
    return manifest["bank"] << 14


def bankImages(manifest, data):
    """[(bank, 16K image)] for uploading the image, one entry per bank it spans."""
    return [(manifest["bank"] + i, data[offset:offset + BANK_SIZE])
            for i, offset in enumerate(range(0, len(data), BANK_SIZE))]


def address(word):
    return int(word["address"], 16)


def findWord(manifest, key):
    """A word by its number (decimal), or by one of its names (any case)."""
    for word in manifest["words"]:
        if str(word["number"]) == str(key):
            return word
    for word in manifest["words"]:
        names = [n.strip().upper() for n in word["name"].split(",")]
        names += [re.sub(r"\s*\(.*?\)", "", n).strip() for n in names]
        if str(key).upper() in names:
            return word
    raise KeyError(f"{manifest['name']} has no word {key!r}")


def wordAt(manifest, vsmAddress):
    """The word that starts at a VSM address, or None."""
    for word in manifest["words"]:
        if address(word) == vsmAddress:
            return word
    return None


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_") or "x"


def wordDir(word):
    """The capture directory name of a word: its number, then its first name."""
    first = word["name"].split(",")[0]
    name = re.sub(r"\(.*?\)", "", first)
    return f"{word['number']}_{slug(name if slug(name) != 'x' else first)}"


SWEEP_DIR = "sweep"


def sweepPoints(manifest, data):
    """The address sweep (plan section 8): ([VSM addresses], [left out]).
    It covers every byte address of the image that is not a word start,
    except those whose data reaches the end of the image without a stop
    code. Those run into the next bank, which the Pico serves as empty, so
    what the chip reads there depends on the board, not on the PROM."""
    import lpc
    first = base(manifest)
    starts = {address(w) for w in manifest["words"]}
    bits = lpc.bitsLsbFirst(data)
    points, runOff = [], []
    for offset in range(len(data)):
        if first + offset in starts:
            continue
        frames, _ = lpc.parseFrames(bits[offset * 8:])
        (points if frames and frames[-1]["kind"] == "stop" else runOff).append(first + offset)
    return points, runOff


def addressDir(vsmAddress):
    """The capture directory, relative to the PROM's, for an address that is
    not in the word table: a point of the address sweep (plan section 8)."""
    return os.path.join(SWEEP_DIR, f"addr_{vsmAddress:05x}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("list", help="print a PROM's word table")
    p.add_argument("prom")
    args = parser.parse_args()

    manifest = loadManifest(args.prom)
    image(manifest)
    for word in manifest["words"]:
        print(f"{word['number']:5d}  {word['address']}  {wordDir(word):24s} {word['name']}")


if __name__ == "__main__":
    main()
