#!/usr/bin/env python3
"""
    prom.py

    Speech PROM manifests (plan sections 6 and 8). A manifest,
    proms/<name>.json, names a VSM/PHROM image, gives its hash and
    provenance, and lists its word table. The runner (run.py) copies it into
    each capture set as <prom>/prom.json.

    Images that may not be redistributed are kept out of the repo, in
    proms/images/ (git-ignored); the manifest's hash identifies them. The
    Acorn PHROM A image is already in the repo, in firmware/src/
    romdata_acorn.h, and its manifest calls it "builtin".

      python tools/prom.py list acorn_phrom_a      # the word table
      python tools/prom.py acorn-manifest          # rebuild proms/acorn_phrom_a.json

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import hashlib
import json
import os
import re
import zlib

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
        with open(os.path.join(ROOT, source), "rb") as f:
            data = f.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != manifest["image"]["sha256"]:
        raise ValueError(f"{source}: SHA-256 {digest} does not match the manifest")
    return data


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


def addressDir(vsmAddress):
    """The capture directory name for an address that is not in the word table."""
    return f"addr_{vsmAddress:05x}"


# ---------------------------------------------------------------------
# The Acorn PHROM A manifest, from the word list in romdata_acorn.h

def acornManifest():
    with open(picolib.ROMDATA_ACORN) as f:
        text = f.read()
    table = text[text.index("PHROM Word list:"):text.index("*/", text.index("PHROM Word list:"))]
    bank = int(re.search(r"#define PHROM_BANK 0x([0-9A-F])", text).group(1), 16)
    words = []
    for line in table.splitlines():
        m = re.match(r"\s*(\d+)\s+([0-9A-F]+)\s+(.*\S)\s*$", line)
        if not m:
            continue
        name, note = m.group(3), None
        if "(unverified" in name:
            name, note = name[:name.index("(unverified")].strip(), \
                "start address unverified (romdata_acorn.h)"
        word = {"number": int(m.group(1)),
                "address": f"0x{(bank << 14) | int(m.group(2), 16):05X}", "name": name}
        if note:
            word["note"] = note
        words.append(word)
    data = picolib.acornImage()
    return {
        "name": "acorn_phrom_a",
        "title": "Acorn PHROM A (Kenneth Kendall voice)",
        "target_chip": "TMS5220",
        "image": {"source": "builtin", "file": "firmware/src/romdata_acorn.h",
                  "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                  "crc32": f"{zlib.crc32(data):08x}"},
        "bank": bank,
        "provenance": "TMS6100-Emulator (simoninns), "
                      "https://github.com/simoninns/TMS6100-Emulator/tree/master/Firmware/tms6100: "
                      "a dump of an Acorn PHROM A made with HxD. The word list was checked by "
                      "decoding the LPC data at each address (romdata_acorn.h).",
        "word_numbers": "the BBC Micro's word numbers (SOUND -1, <number>, 0, 0)",
        "words": words,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("list", help="print a PROM's word table")
    p.add_argument("prom")
    sub.add_parser("acorn-manifest", help="rebuild proms/acorn_phrom_a.json from romdata_acorn.h")
    args = parser.parse_args()

    if args.command == "list":
        manifest = loadManifest(args.prom)
        image(manifest)
        for word in manifest["words"]:
            print(f"{word['number']:5d}  {word['address']}  {wordDir(word):24s} {word['name']}")
    else:
        manifest = acornManifest()
        os.makedirs(PROMS_DIR, exist_ok=True)
        path = manifestPath(manifest["name"])
        with open(path, "w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        print(f"{len(manifest['words'])} words -> {path}")


if __name__ == "__main__":
    main()
