#!/usr/bin/env python3
"""
    promimport.py

    The PROM inventory (docs/technical/capture/prom-corpus.md). Rebuilds every speech
    PROM image in the inventory from its pinned public sources, checks it
    against the reference dumps' hashes (MAME), reads its word table from
    the image itself, identifies the frame format it was written for, and
    writes its manifest, proms/<name>.json, and the inventory page,
    docs/technical/capture/prom-inventory.md.

    The images go in proms/images/ (git-ignored), and the source files are
    cached under proms/images/sources/. Only the manifests and the inventory
    page are committed. The Acorn PHROM A image is the one already in the repo,
    firmware/src/romdata_acorn.h; the other copies of it are compared with
    it.

      python tools/promimport.py              # fetch, rebuild, check, write
      python tools/promimport.py --offline    # use the cached sources only

    The inventory is frozen at INVENTORY_VERSION. Images found later go into
    a version 1.x.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import hashlib
import json
import os
import re
import urllib.request
import zlib

import lpc
import pico as picolib
import prom as promlib

INVENTORY_VERSION = "1.0"
IMAGES_DIR = os.path.join(promlib.PROMS_DIR, "images")
INVENTORY_PAGE = os.path.join(promlib.ROOT, "..", "docs", "technical", "capture", "prom-inventory.md")
SOURCES_DIR = os.path.join(IMAGES_DIR, "sources")

# Every source is pinned to a commit, so the images rebuild exactly
SOURCES = {
    "eradiofan": {"repo": "eRadiofan/TMS6100-Multi-PHROM-Emulator-for-the-BBC-Micro",
                  "commit": "b1a0bd83570755eb02c873b6e24e7b80b341eb59", "licence": "GPL-3.0"},
    "tms6100emu": {"repo": "simoninns/TMS6100-Emulator",
                   "commit": "0d78f97b20bee44786669abbe135318b30c696ce", "licence": "GPL-3.0"},
    "talkie": {"repo": "going-digital/Talkie",
               "commit": "7f57628bcfa78cad346508d0d43541cce50da1b3", "licence": "GPL-2.0"},
    "swaudio": {"repo": "nmikstas/star-wars-arcade-audio",
                "commit": "2e738970d94de46dc12491213827d797f3c588de", "licence": "none stated"},
}
ERAD = "Firmware/Common/Src/"

# The reference dumps, as MAME lists them (mamedev/mame at this commit)
MAME_COMMIT = "1ecdb5701405bd4cadba520fe1ff4d65f7c7e8aa"
MAME = {
    "cm62024": ("hash/bbc_vsm.xml", 0x4000, "98e1bf9e", "b369809275cb67dfd8a749265e91adb2d2558ae6"),
    "vm61002": ("hash/bbc_vsm.xml", 0x4000, "bf4b3b64", "66876702d1d95eecc034d20f25047f893a27cde5"),
    "vm61003": ("hash/bbc_vsm.xml", 0x4000, "2a4e6f17", "aa3bbb28fbdb848d609ec47ef8c250d4ff0d8e53"),
    "vm61004": ("hash/bbc_vsm.xml", 0x4000, "45ec7dcd", "24b88d0127a52eccc579d4ea90b39cb90da7fd39"),
    "vm61005": ("hash/bbc_vsm.xml", 0x4000, "09efc37f", "5cf785153dcce406e9a8f5d166af50e288b33126"),
    "vm71003a": ("hash/bbc_vsm.xml", 0x1000, "d1d5a1dc", "164dae26500d9ef01c591686f13078d51254c4e1"),
    "cd2325a": ("src/devices/bus/ti99/sidecar/speechsyn.cpp", 0x4000, "1f58b571",
                "0ef4f178716b575a1c0c970c56af8a8d97561ffe"),
    "cd2326a": ("src/devices/bus/ti99/sidecar/speechsyn.cpp", 0x4000, "65d00401",
                "a367242c2c96cebf0e2bf21862f3f6734b2b3020"),
    "cm62312": ("src/mame/ti/exelv.cpp", 0x4000, "93b817de", "03863087a071b8f22d36a52d18243f1c33e17ff7"),
}

# Original speech data known to exist but not held, and data left out of
# the inventory, for the inventory page
NOT_HELD = [
    ("VM61003 (complete image)", "TMS5220", "Listed as a BBC Micro speech PHROM, vm61003 (Weather), 16 KB, SHA-1 "
     f"`{MAME['vm61003'][3]}`. No public copy of the image was found; 116 of its words are held "
     "as `vm61003_words`."),
    ("VM61005 (complete image)", "TMS5220", "Listed as a BBC Micro speech PHROM, vm61005 (Avionic), 16 KB, SHA-1 "
     f"`{MAME['vm61005'][3]}`. No public copy of the image was found; 116 of its words are held "
     "as `vm61005_words`."),
    ("CM62312 (Exelvision EXL 100 system speech)", "TMS5220C", "Exelvision EXL 100, 16 KB, SHA-1 "
     f"`{MAME['cm62312'][3]}`. No public copy found. Written for the TMS5220C."),
    ("Speak External data in other hosts' program ROMs", "TMS5220", "Atari arcade games other "
     "than Star Wars (for example The Empire Strikes Back, Gauntlet, Indiana Jones and the "
     "Temple of Doom, Paperboy, 720°), and speech cards such as the Apple II Echo II. Their LPC "
     "data is embedded in program code, so each needs its own disassembly to find the phrase "
     "boundaries. Not collected."),
]
EXCLUDED = [
    ("VSMs for the TMS5100/TMS5110 family (TMC0281, CD2801/CD2802, TMS5110A)", "Speak & Spell, "
     "Speak & Math, Speak & Read, Language Translator, Spelling B, Tiger K28 and Bingo Bear, Eva "
     "", "Their frames "
     "have a 5-bit pitch field, so the TMS5220 misparses them from the first voiced frame. They "
     "test nothing the synthetic vectors do not test better."),
    ("M58817/M58819 data (Nintendo Radar Scope)", "Nintendo Radar Scope", "Mitsubishi's TMS5110 "
     "clone and its own speech ROM: the TMS5100 frame format."),
    ("Talkie `Vocab_Soundbites`", "going-digital/Talkie", "From a custom BBC Micro phrase ROM "
     "made by a hobbyist, not original data."),
]


# ---------------------------------------------------------------------
# Sources

def fetch(key, path, offline=False):
    """A source file's text, from the cache or from GitHub at the pinned commit."""
    source = SOURCES[key]
    cached = os.path.join(SOURCES_DIR, key, path)
    if not os.path.exists(cached):
        if offline:
            raise FileNotFoundError(f"{cached} is not cached (run without --offline)")
        url = f"https://raw.githubusercontent.com/{source['repo']}/{source['commit']}/{path}"
        print(f"fetching {url}")
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
        os.makedirs(os.path.dirname(cached), exist_ok=True)
        with open(cached, "wb") as f:
            f.write(data)
    with open(cached, encoding="utf-8", errors="replace") as f:
        return f.read()


def sourceRef(key, path, symbol=None):
    source = SOURCES[key]
    ref = {"repo": f"https://github.com/{source['repo']}", "commit": source["commit"],
           "path": path, "licence": source["licence"]}
    if symbol:
        ref["symbol"] = symbol
    return ref


def stripComments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def cArray(text, name):
    """The bytes of a C array initialiser, with eRadiofan's toLE() macro expanded."""
    body = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*(?:PROGMEM\s*)?=\s*\{(.*?)\};",
                     stripComments(text), re.S).group(1)
    body = re.sub(r"toLE\(\s*(\w+)\s*\)",
                  lambda m: f"{int(m.group(1), 0) % 256},{int(m.group(1), 0) // 256}", body)
    values = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]
    if not all(0 <= v < 256 for v in values):
        raise ValueError(f"{name}: a value is not a byte")
    return bytes(values)


def listing(text, pattern):
    """Word listings in a source's header comment: [(groups...)] for every
    line matching pattern, up to the end of the first comment holding one."""
    rows = []
    for line in text.splitlines():
        m = re.match(pattern, line)
        if m:
            rows.append(m.groups())
        elif rows and "*/" in line:
            break
    return rows


def hashes(data):
    return {"size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "sha1": hashlib.sha1(data).hexdigest(), "crc32": f"{zlib.crc32(data):08x}"}


def padded(data, size):
    if len(data) > size:
        raise ValueError(f"{len(data)} bytes do not fit in {size}")
    return data + bytes(size - len(data))


def reference(name, data):
    """The MAME entry for a dump, and whether data matches it."""
    where, size, crc, sha1 = MAME[name]
    return {"mame": name, "listed_in": f"https://github.com/mamedev/mame/blob/{MAME_COMMIT}/{where}",
            "size": size, "crc32": crc, "sha1": sha1,
            "matches": len(data) == size and hashlib.sha1(data).hexdigest() == sha1}


def reverseBits(value):
    return picolib.reverseBits(value)


# ---------------------------------------------------------------------
# Word tables. Each returns {offset in the image: {"numbers": [...], "names": [...]}}

def acornTable(image):
    """Acorn's format (CM62024): a header read through Read Byte (so bit
    reversed) with copyright strings and counts, then from byte 64 a table
    of 16-bit pointers; word number N's pointer is at byte 2N. The first
    'ASCII associated' entries (numbers 32-126) repeat words that have their
    own number from 127 on. Each word's name is stored in the bytes before
    its data, backwards (its first character next to the data), one
    character per byte, bit reversed and shifted left by one."""
    text = bytes(reverseBits(b) for b in image[:64])
    strings = [s.decode("ascii") for s in text[2:54].split(b"\0") if s]
    asciiWords = text[56] | text[57] << 8
    pointers = text[58] | text[59] << 8
    if image[64 + 2 * pointers] != 0xFF:
        raise ValueError("Acorn pointer table does not end with 0xFF")
    table = {}
    for k in range(pointers):
        offset = image[64 + 2 * k] | image[65 + 2 * k] << 8
        table.setdefault(offset, {"numbers": [], "aliases": [], "names": []})
        (table[offset]["aliases"] if k < asciiWords else table[offset]["numbers"]).append(32 + k)
    # A name fills the bytes between the previous word's stop code (or the
    # end of the pointer table) and its word's data
    previous = 64 + 2 * pointers + 1
    for offset in sorted(table):
        stored = image[previous:offset][::-1]
        table[offset]["stored_name"] = "".join(chr(reverseBits(b) >> 1) for b in stored)
        previous = wordEnd(image, offset)
    return table, {"strings": strings, "ascii_words": asciiWords, "pointers": pointers}


def tiIndexedTable(image):
    """TI's indexed format: byte 0 is 0, byte 1 the word count, then a
    little-endian 16-bit pointer per word, word n's at byte 2n."""
    if image[0] != 0:
        raise ValueError("not TI's indexed format")
    table = {}
    for n in range(1, image[1] + 1):
        offset = image[2 * n] | image[2 * n + 1] << 8
        table.setdefault(offset, {"numbers": [], "names": []})["numbers"].append(n)
    return table


def ti99Table(image):
    """The TI-99/4A speech VSMs (CD2325A + CD2326A): byte 0 is 0x55, then
    373 index records, read bit reversed (through Read Byte): a length byte, the phrase's name,
    left and right pointers of a binary search tree, a zero, the phrase's
    address (big-endian) and its length in bytes."""
    data = bytes(reverseBits(b) for b in image)
    if image[0] != 0x55:
        raise ValueError("not the TI-99/4A index format")
    table, at, number = {}, 1, 0
    while at < len(data) and data[at] and number < 373:
        length = data[at]
        name = data[at + 1:at + 1 + length].decode("ascii")
        record = data[at + 1 + length:at + 9 + length]
        if record[4] != 0:
            raise ValueError(f"TI-99 index record {number} ({name}) is malformed")
        offset = record[5] << 8 | record[6]
        entry = table.setdefault(offset, {"numbers": [], "names": [], "length": record[7]})
        entry["numbers"].append(number)
        entry["names"].append(name)
        at += 9 + length
        number += 1
    return table


def wordEnd(image, offset, pitchBits=6):
    """The offset just past the byte holding the word's stop code, or None
    if the frames run off the image without one."""
    frames, bits = lpc.parseFrames(lpc.bitsLsbFirst(image[offset:]), pitchBits=pitchBits)
    if not frames or frames[-1]["kind"] != "stop":
        return None
    return offset + (bits + 7) // 8


def frameFormat(image, offsets):
    """Each word parsed in the TMS5200/TMS5220's format (6-bit pitch) and in
    the TMS5100's (5-bit pitch): how many reach a stop code, and how many
    of those run past the start of the next word (or the end of the image).
    Data written for one format misparses in the other from its first
    voiced frame, and a word cannot overlap the next, so a single overrun
    rules a format out."""
    starts = sorted(set(offsets))
    ends = starts[1:] + [len(image)]
    result = {}
    for label, pitchBits in (("tms5200_tms5220", 6), ("tms5100", 5)):
        stops = overruns = 0
        for start, nextStart in zip(starts, ends):
            end = wordEnd(image, start, pitchBits)
            stops += end is not None
            overruns += end is not None and end > nextStart
        result[label] = {"words": len(starts), "stop_code": stops, "overruns": overruns}
    return result


# ---------------------------------------------------------------------
# The inventory

def words(table, names=None, numberBase=None):
    """Manifest words from a table, in address order. names maps an offset to
    its name (otherwise the table's names are used)."""
    out = []
    for offset in sorted(table):
        entry = table[offset]
        number = min(entry["numbers"])
        word = {"number": number, "address": f"0x{numberBase + offset:05X}",
                "name": names[offset] if names else ", ".join(entry["names"])}
        others = sorted(set(entry["numbers"]) - {number}) + entry.get("aliases", [])
        if others:
            word["aliases"] = sorted(others)
        out.append(word)
    return sorted(out, key=lambda w: w["number"])


def checkListing(table, rows, label):
    """A source's published word list against the table read from the image."""
    listed = {offset: name for offset, name in rows}
    problems = [f"{label}: listed address 0x{o:04X} ({n}) is not in the image's index"
                for o, n in listed.items() if o not in table]
    problems += [f"{label}: indexed address 0x{o:04X} is not listed"
                 for o in table if o not in listed]
    return listed, problems


# Names in romdata_acorn.h's word list that contradict the names Acorn
# stored in the image itself; the stored names are used
ACORN_NAME_FIXES = {135: "-TEEN", 137: "-TY", 138: "-Z", 230: "O"}


def acornPhromA(offline):
    data = picolib.acornImage()
    table, header = acornTable(data)
    text = open(picolib.ROMDATA_ACORN).read()
    body = text[text.index("PHROM Word list:"):text.index("*/", text.index("PHROM Word list:"))]
    rows = [(int(m.group(1)), int(m.group(2), 16), m.group(3)) for line in body.splitlines()
            for m in [re.match(r"\s*(\d+)\s+([0-9A-F]+)\s+(.*\S)\s*$", line)] if m]
    names, problems = {}, []
    for number, offset, name in rows:
        name = name.split("(unverified")[0].strip()
        if offset not in table or number not in table[offset]["numbers"]:
            problems.append(f"romdata_acorn.h: word {number} at 0x{offset:04X} is not word "
                            f"{number} in the image's pointer table")
        names[offset] = name
    copies = [
        dict(sourceRef("tms6100emu", "Firmware/tms6100/romdata_acorn.h", "phromData"),
             **hashes(cArray(fetch("tms6100emu", "Firmware/tms6100/romdata_acorn.h", offline),
                             "phromData"))),
        dict(sourceRef("eradiofan", ERAD + "ROMdata_Acorn.c", "phromData_Acorn"),
             **hashes(cArray(fetch("eradiofan", ERAD + "ROMdata_Acorn.c", offline),
                             "phromData_Acorn"))),
    ]
    out = words(table, names, 0xF << 14)
    stored = {f"0x{(0xF << 14) + o:05X}": e["stored_name"] for o, e in table.items()}
    for word in out:
        word["stored_name"] = stored[word["address"]]
        fix = ACORN_NAME_FIXES.get(word["number"])
        if fix:
            if fix != word["stored_name"]:
                problems.append(f"word {word['number']}: the fix {fix!r} is not the stored name")
            word["listed_as"], word["name"] = word["name"], fix
    return {
        "name": "acorn_phrom_a",
        "title": "Acorn PHROM A (Kenneth Kendall voice)",
        "part": "CM62024",
        "target_chip": "TMS5220",
        "target_chip_basis": "Acorn's BBC Micro speech system, which used the TMS5220.",
        "image": dict({"source": "builtin", "file": "firmware/src/romdata_acorn.h",
                       "kind": "dump"}, **hashes(data)),
        "reference": reference("cm62024", data),
        "copies": copies,
        "bank": 0xF,
        "format": "acorn",
        "header": header,
        "provenance": "TMS6100-Emulator (simoninns): a dump of an Acorn PHROM A made with HxD, "
                      "held in this repo as firmware/src/romdata_acorn.h. The word names are "
                      "from its word list, whose addresses and numbers all match the image's "
                      "pointer table. Four of its names contradict the names stored in the "
                      "image (stored_name) and are corrected from them (listed_as keeps the "
                      "list's).",
        "word_numbers": "the BBC Micro's word numbers (SOUND -1, <number>, 0, 0): word N's "
                        "pointer is at byte 2N. Numbers 32-126 (aliases) are the 'ASCII "
                        "associated' words, which repeat words numbered from 127 on.",
        "words": out,
    }, data, problems


def vm61002(offline):
    path = "Firmware/tms6100/romdata_us.h"
    text = fetch("tms6100emu", path, offline)
    data = cArray(text, "phromData")
    table = tiIndexedTable(data)
    rows = listing(text, r"\s*(\d+)\s+([0-9A-F]{3,4})\s+(\S.*?)\s*$")
    listed, problems = checkListing(table, [(int(a, 16), n) for _, a, n in rows], "romdata_us.h")
    for number, a, _ in rows:
        if int(number) not in table.get(int(a, 16), {}).get("numbers", []):
            problems.append(f"romdata_us.h: word {number} is not at 0x{a} in the image's index")
    erad = cArray(fetch("eradiofan", ERAD + "ROMdata_VM61002.c", offline), "phromData_USA")
    return {
        "name": "vm61002",
        "title": "TI VM61002 (US male voice, industrial vocabulary)",
        "part": "VM61002",
        "target_chip": "TMS5220",
        "target_chip_basis": "Fitted to Acorn's US speech kits for the BBC Micro (TMS5220), "
                             "according to eRadiofan; listed by MAME as a BBC Micro speech "
                             "PHROM.",
        "image": dict({"source": "proms/images/vm61002.bin", "kind": "dump"}, **hashes(data)),
        "reference": reference("vm61002", data),
        "copies": [dict(sourceRef("tms6100emu", path, "phromData"), **hashes(data)),
                   dict(sourceRef("eradiofan", ERAD + "ROMdata_VM61002.c", "phromData_USA"),
                        **hashes(erad), note="truncated after the last word; identical to the "
                        "image once padded with zeros" if padded(erad, 0x4000) == data else
                        "DIFFERS from the image")],
        "bank": 0,
        "format": "ti_indexed",
        "provenance": "TMS6100-Emulator (simoninns), romdata_us.h: a dump made with HxD. The "
                      "word names are from its word list.",
        "word_numbers": "TI's indexed format: word n's pointer is at byte 2n (n from 1), so "
                        "on a BBC Micro with the PHROM in bank 0, SOUND -16, n, 0, 0 (assumed, "
                        "not tested).",
        "words": words(table, listed, 0),
    }, data, problems


def vm61004(offline):
    path = ERAD + "ROMdata_VM61004.c"
    text = fetch("eradiofan", path, offline)
    raw = cArray(text, "PHROMdata_Mil")
    data = padded(raw, 0x4000)
    rows = listing(text, r"\s+([0-9A-F]{1,4})\s{2}(\S.*?)\s*$")
    problems, table, names = [], {}, {}
    # Unindexed: each word follows the previous one's stop code after one
    # spare byte
    expected = 0
    for number, (a, name) in enumerate(rows, 1):
        offset = int(a, 16)
        if offset != expected:
            problems.append(f"VM61004 word {number} ({name}) at 0x{offset:04X}: the previous "
                            f"word's stop code puts it at 0x{expected:04X}")
        end = wordEnd(data, offset)
        if end is None:
            problems.append(f"VM61004 word {number} ({name}) has no stop code")
            break
        expected = end + 1
        table[offset] = {"numbers": [number], "names": [name]}
        names[offset] = name
    return {
        "name": "vm61004",
        "title": "TI VM61004 (US male voice, military vocabulary)",
        "part": "VM61004",
        "target_chip": "TMS5220",
        "target_chip_basis": "Listed by MAME as a BBC Micro speech PHROM (TMS5220), and by "
                             "TI's 1983 selection guide (whose part numbers do not match MAME's "
                             "for this series) as TMS5220A vocabulary.",
        "image": dict({"source": "proms/images/vm61004.bin", "kind": "dump"}, **hashes(data)),
        "reference": reference("vm61004", data),
        "copies": [dict(sourceRef("eradiofan", path, "PHROMdata_Mil"), **hashes(raw),
                        note="truncated after the last word; the image is this padded with "
                             "zeros to 16 KB")],
        "bank": 2,
        "format": "unindexed",
        "provenance": "TMS6100 Multi-PHROM Emulator (eRadiofan), ROMdata_VM61004.c, padded "
                      "with zeros to 16 KB; the result is MAME's dump. The word names are from "
                      "its word list; the addresses are checked by following each word's stop "
                      "code.",
        "word_numbers": "none in the image (it has no index): the words are numbered in "
                        "address order from 1.",
        "words": words(table, names, 2 << 14),
    }, data, problems


def vm71003a(offline):
    path = ERAD + "ROMdata_VM71003A.c"
    text = fetch("eradiofan", path, offline)
    raw = cArray(text, "PHROMdata_Clock")
    data = padded(raw, 0x1000)
    table = tiIndexedTable(data)
    rows = listing(text, r"(\d+)\s+([0-9A-F]{4})\s+(\S.*?)\s*$")
    listed, problems = checkListing(table, [(int(a, 16), n) for _, a, n in rows],
                                    "ROMdata_VM71003A.c")
    return {
        "name": "vm71003a",
        "title": "TI VM71003A (US female voice, talking clock)",
        "part": "VM71003A",
        "target_chip": "TMS5220",
        "target_chip_basis": "Listed by MAME as a BBC Micro speech PHROM (TMS5220); its data "
                             "parses only in the TMS5200/TMS5220 format.",
        "image": dict({"source": "proms/images/vm71003a.bin", "kind": "dump"}, **hashes(data)),
        "reference": reference("vm71003a", data),
        "copies": [dict(sourceRef("eradiofan", path, "PHROMdata_Clock"), **hashes(raw),
                        note="truncated after the last word; the image is this padded with "
                             "zeros to 4 KB")],
        "bank": 4,
        "format": "ti_indexed",
        "provenance": "TMS6100 Multi-PHROM Emulator (eRadiofan), ROMdata_VM71003A.c, padded "
                      "with zeros to 4 KB; the result is MAME's dump. The word names are from "
                      "its word list. A 4 KB VSM: the Pico serves it padded with zeros to "
                      "16 KB, so reads past 4 KB return zeros, where a real 4 KB part might "
                      "wrap.",
        "word_numbers": "TI's indexed format: word n's pointer is at byte 2n (n from 1).",
        "words": words(table, listed, 4 << 14),
    }, data, problems


def ti99(offline):
    path = ERAD + "ROMdata_TI99.c"
    raw = cArray(fetch("eradiofan", path, offline), "phromData_TI99")
    data = padded(raw, 0x8000)
    table = ti99Table(data)
    problems = []
    starts = sorted(table)
    for offset, nextOffset in zip(starts, starts[1:] + [None]):
        entry = table[offset]
        if nextOffset is not None and nextOffset - offset != entry["length"]:
            problems.append(f"TI-99 {entry['names'][0]}: index length {entry['length']}, "
                            f"next phrase {nextOffset - offset} bytes on")
    return {
        "name": "ti99_speech",
        "title": "TI-99/4A Solid State Speech Synthesizer VSMs (US male voice)",
        "part": "CD2325A (0x0000-0x3FFF) and CD2326A (0x4000-0x7FFF)",
        "target_chip": "TMS5200",
        "target_chip_basis": "The TI-99/4A Speech Synthesizer used the TMS5200 (CD2501E). The "
                             "frame format is the TMS5220's, but the TMS5200's parameter "
                             "tables differ, so it plays on the TMS5220 slightly wrong.",
        "image": dict({"source": "proms/images/ti99_speech.bin", "kind": "dump"},
                      **hashes(data)),
        "reference": [reference("cd2325a", data[:0x4000]), reference("cd2326a", data[0x4000:])],
        "copies": [dict(sourceRef("eradiofan", path, "phromData_TI99"), **hashes(raw),
                        note="both VSMs in one array, truncated after the last phrase; the "
                             "image is this padded with zeros to 32 KB")],
        "bank": 0,
        "format": "ti99",
        "provenance": "TMS6100 Multi-PHROM Emulator (eRadiofan), ROMdata_TI99.c, padded with "
                      "zeros to 32 KB; its two halves are MAME's dumps of the two VSMs. The "
                      "phrase names are the ones stored in the image's index.",
        "word_numbers": "the position of the phrase's record in the image's index, from 0 "
                        "(the first record is the search tree's root, MORE; the rest are in "
                        "alphabetical order). "
                        "Phrases with several names (for example BUY, BY, BYE) are one word "
                        "with aliases.",
        "words": words(table, None, 0),
    }, data, problems


def starWars(offline):
    path = ERAD + "ROMdata_StarWars.c"
    data = cArray(fetch("eradiofan", path, offline), "phromData_StarWars")
    table = tiIndexedTable(data)
    # The game's own copy, from the disassembly of sound ROM 0
    asm = fetch("swaudio", "Star_Wars_Sound_ROM.asm", offline)
    blocks, current, endTrim = [], None, {}
    for line in asm.splitlines():
        m = re.match(r"SpchDat(\d+)Strt\s*;\s*(.*?)\.?\s*$", line)
        if m:
            current = [m.group(2).strip('"').rstrip('.').strip('"'), bytearray()]
            blocks.append(current)
            continue
        if re.match(r"SpchDat\d+End", line):
            current = None
        elif current is not None:
            m = re.match(r"L[0-9A-F]{4}\s+FCB\s+(.*?)(;|$)", line)
            if m:
                current[1] += bytes(int(x, 16) for x in re.findall(r"\$([0-9A-F]{2})", m.group(1)))
    for m in re.finditer(r"SpchDat(\d+)Strt,\s*SpchDat\d+End-(\d+)", asm):
        endTrim[int(m.group(1))] = int(m.group(2)) - 1
    problems, names, streams = [], {}, {}
    for (name, block), offset in zip(blocks, sorted(table)):
        if data[offset:offset + len(block)] != block:
            problems.append(f"Star Wars {name}: differs from the disassembly")
        names[offset] = name
        streams[offset] = len(block) - endTrim.get(len(names) - 1, 0)
    if len(blocks) != len(table):
        problems.append(f"Star Wars: {len(blocks)} blocks in the disassembly, {len(table)} in "
                        "the image")
    out = words(table, names, 0xA << 14)
    for word in out:
        word["stream_bytes"] = streams[int(word["address"], 16) - (0xA << 14)]
    return {
        "name": "starwars_speech",
        "title": "Atari Star Wars (1983) arcade speech",
        "part": "speech data in sound ROM 0, 136021-107 (MAME `starwars`)",
        "target_chip": "TMS5220",
        "target_chip_basis": "The game's sound board drives a TMS5220 with Speak External.",
        "image": dict({"source": "proms/images/starwars_speech.bin", "kind": "assembled"},
                      **hashes(data)),
        "reference": None,
        "copies": [dict(sourceRef("eradiofan", path, "phromData_StarWars"), **hashes(data),
                        note="the image"),
                   dict(sourceRef("swaudio", "Star_Wars_Sound_ROM.asm"),
                        note="the game's 23 speech blocks (SpchDat0-22), compared byte for "
                             "byte with the image")],
        "bank": 0xA,
        "format": "ti_indexed",
        "provenance": "The game sends these blocks to the TMS5220 with Speak External; it has "
                      "no VSM. eRadiofan assembled them into a VSM image in TI's indexed "
                      "format (ROMdata_StarWars.c, credited to nmikstas's disassembly of the "
                      "sound ROM). Every block is checked against that disassembly. The image "
                      "itself is not original, so only the Speak External path replays the "
                      "game exactly. The sound ROM dump itself was not checked (MAME lists it "
                      "as 136021-107.1jk, SHA-1 c38661b2b846fe93487eef09ca3cda19c44f08a0).",
        "word_numbers": "the block number in the game (SpchDat<n>) plus 1, which is also the "
                        "word's index in the assembled image.",
        "words": out,
        "word_fields": {"stream_bytes": "how many bytes of the block the game sends with "
                                        "Speak External, which includes bytes after the stop "
                                        "code"},
    }, data, problems


def talkieWords(offline, rom, held):
    """A VSM image assembled from Talkie's words for one ROM, keeping only
    words that are not in an image already held (Talkie's other words all
    match the held images byte for byte)."""
    path = "Talkie/examples/Vocab_US_Large/Vocab_US_Large.ino"
    text = fetch("talkie", path, offline)
    section, entries = None, []
    for line in text.splitlines():
        m = re.match(r"\s*// ROM (\S+)", line)
        if m:
            section = m.group(1)
        m = re.search(r"sp(\w+)\[\]\s*PROGMEM\s*=\s*\{([^}]*)\}", line)
        if m and section == rom.upper():
            entries.append((m.group(1), bytes(int(x, 16) for x in
                                              re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2)))))
    kept = [(n, b) for n, b in entries if not any(b in image for image in held)]
    header = 2 + 2 * len(kept)
    data, offsets = bytearray([0, len(kept)]) + bytes(2 * len(kept)), []
    for k, (_, block) in enumerate(kept):
        offsets.append(len(data))
        data[2 + 2 * k:4 + 2 * k] = len(data).to_bytes(2, "little")
        data += block
    data = bytes(data)
    problems = [f"{rom} {n}: no stop code" for (n, _), o in zip(kept, offsets)
                if wordEnd(data, o) is None]
    if len(data) > 0x4000:
        problems.append(f"{rom}: {len(data)} bytes do not fit in 16 KB")
    bank = {"vm61003": 1, "vm61005": 3}[rom]
    table = tiIndexedTable(data)
    names = {o: n.replace("_", " ") for (n, _), o in zip(kept, offsets)}
    if header != offsets[0]:
        problems.append(f"{rom}: header length")
    return {
        "name": f"{rom}_words",
        "title": f"TI {rom.upper()} words (US male voice), from Talkie",
        "part": rom.upper(),
        "target_chip": "TMS5220",
        "target_chip_basis": "MAME lists the VM61003 and VM61005 as BBC Micro speech PHROMs "
                             "(TMS5220); the words parse only in the TMS5200/TMS5220 format.",
        "image": dict({"source": f"proms/images/{rom}_words.bin", "kind": "assembled"},
                      **hashes(data)),
        "reference": None,
        "copies": [dict(sourceRef("talkie", path), note=f"the {len(entries)} words under "
                        f"'ROM {rom.upper()}'; {len(entries) - len(kept)} of them are in images "
                        "already held, byte for byte, and are left out")],
        "bank": bank,
        "format": "ti_indexed",
        "provenance": f"Talkie (Peter Knight), Vocab_US_Large: words copied from the "
                      f"{rom.upper()}, which is not available as an image. Every Talkie word "
                      "from the PROMs that are held (VM61002, VM61004, VM71003A, CM62024, "
                      "TI-99) matches its image byte for byte, so these are taken to be "
                      "verbatim too. They are assembled here into a VSM image in TI's indexed "
                      "format, in Talkie's order; the layout is not the original's, so only "
                      "the words are original. The names are Talkie's.",
        "word_numbers": "the word's position in Talkie's list (from 1), which is also its "
                        "index in the assembled image. Not the original ROM's numbering.",
        "words": tails(words(table, names, bank << 14), data, bank << 14),
    }, data, problems


def tails(out, data, base):
    """Note the words that carry bytes after the one following their stop
    code (Talkie's copies of a few words run on)."""
    starts = sorted(int(w["address"], 16) - base for w in out) + [len(data)]
    for word in out:
        offset = int(word["address"], 16) - base
        extra = starts[starts.index(offset) + 1] - wordEnd(data, offset)
        if extra > 1:
            word["note"] = (f"Talkie's copy runs on for {extra} bytes after the byte holding "
                            "the stop code; the chip never reads them")
    return out


def build(offline):
    """Every PROM in the inventory: [(manifest, image, problems)]."""
    out = [acornPhromA(offline), vm61002(offline), vm61004(offline), vm71003a(offline),
           ti99(offline), starWars(offline)]
    held = [image for _, image, _ in out]
    out += [talkieWords(offline, "vm61003", held), talkieWords(offline, "vm61005", held)]
    for manifest, image, problems in out:
        offsets = [promlib.address(w) - promlib.base(manifest) for w in manifest["words"]]
        manifest["frame_format"] = frameFormat(image, offsets)
        refs = manifest["reference"]
        refs = refs if isinstance(refs, list) else [refs] if refs else []
        problems += [f"{manifest['name']}: does not match MAME's {r['mame']}"
                     for r in refs if not r["matches"]]
        problems += [f"{manifest['name']}: word {w['number']} ({w['name']}) has no stop code"
                     for w in manifest["words"]
                     if wordEnd(image, promlib.address(w) - promlib.base(manifest)) is None]
    return out


# ---------------------------------------------------------------------
# The inventory page, docs/technical/capture/prom-inventory.md

# The image layouts found, as the manifests' "format" names them
FORMATS = [
    ("acorn", "Acorn's PHROM format (CM62024). Bytes 0-63 are a header, read through Read "
     "Byte so bit reversed: a format byte (0) and a data flag, then null-terminated strings "
     "(`(C)1982 Acorn`, `Word PHROM A`, `1.00`), then 16-bit counts at bytes 56-59: 95 "
     "'ASCII associated' words and 260 pointers. From byte 64 comes the pointer table, "
     "little-endian, ended by 0xFF; word number N's pointer is at byte 2N, so numbers 32-126 "
     "are the ASCII aliases and 127-291 the words. Each word's name sits in the bytes between "
     "the previous word's stop code and its own data, stored backwards, one character per byte, "
     "bit reversed and shifted left by one."),
    ("ti_indexed", "TI's indexed format (VM61002, VM71003A). Byte 0 is 0, byte 1 the word "
     "count, then a little-endian 16-bit pointer per word, word n's at byte 2n. Every word "
     "ends with one spare byte after the byte holding its stop code. The assembled images "
     "(Star Wars, the Talkie words) use the same index, with the words as their sources "
     "give them."),
    ("unindexed", "No index (VM61004). The first word starts at byte 0, and each later word "
     "starts one spare byte after the byte holding the previous word's stop code."),
    ("ti99", "The TI-99/4A speech VSMs. Byte 0 is 0x55, then 373 index records, read bit "
     "reversed: a length byte, the phrase's name in ASCII, the left and right pointers of a "
     "binary search tree, a zero byte, the phrase's address (big-endian, so the byte order is "
     "the reverse of Read and Branch's) and its length in bytes. The first record is the "
     "tree's root. 34 records share their data with another name."),
]


def formatText(fmt):
    return "; ".join(f"{label}: {f['overruns']} overruns, {f['words'] - f['stop_code']} without "
                     "a stop code" for label, f in (("5200/5220", fmt["tms5200_tms5220"]),
                                                    ("5100", fmt["tms5100"])))


def readme(built):
    lines = [
        "# Speech PROM inventory",
        "",
        "The PROMs of the reference data set ([PROM corpus](prom-corpus.md)). "
        "This page is written by `tools/promimport.py`, which also rebuilds every image "
        "from its pinned public source and checks it.",
        "",
        "Each PROM has a manifest, `proms/<name>.json`: the image's hashes and source, the "
        "reference dump it matches, the other copies compared with it, the target chip and "
        "the evidence for it, and the word table read from the image itself. The images are "
        "not committed ([open questions](open-questions.md#licensing)); "
        "`python tools/promimport.py` rebuilds them in "
        "`proms/images/`, and `python tools/run.py <name> ...` captures from them.",
        "",
        "## Held",
        "",
        "| Manifest | Contents | Part | Target | Words | Image | Reference dump | "
        "Words parsed in each format |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for manifest, image, _ in built:
        refs = manifest["reference"]
        refs = refs if isinstance(refs, list) else [refs] if refs else []
        refText = ("; ".join(f"{r['mame']}: {'matches' if r['matches'] else 'DIFFERS'}"
                             for r in refs) or "none: assembled, see the manifest")
        aliases = sum(len(w.get("aliases", [])) for w in manifest["words"])
        lines.append(
            f"| `proms/{manifest['name']}.json` | {manifest['title']} "
            f"| {re.sub(r' [(]MAME `[^`]*`[)]', '', manifest['part'])} | {manifest['target_chip']} | {len(manifest['words'])}"
            + (f" (+{aliases} aliases)" if aliases else "")
            + f" | {len(image)} bytes, {manifest['image']['kind']}, bank "
            f"{manifest['bank']:X} | {refText} | {formatText(manifest['frame_format'])} |")
    lines += [
        "",
        "- **Words parsed in each format:** every word read with the TMS5200/TMS5220's 6-bit "
        "pitch field and with the TMS5100's 5-bit one: how many run past the start of the next "
        "word, and how many never reach a stop code. Data written for one format misparses in "
        "the other, and a word cannot overlap the next, so one overrun rules a format out. "
        "This identifies the format from the data alone. It cannot tell the TMS5200 from the "
        "TMS5220, which share a frame format but not their parameter tables; that comes from "
        "the product the PROM was made for.",
        "- **Image kind:** a *dump* is a copy of a real VSM, checked against its published reference hash. An "
        "*assembled* image holds original words in a layout made for this project or by its "
        "source, so only the words, and not the image's layout or read-ahead, are original.",
        "- **Copies compared:** every source copy of each image is hashed in its manifest "
        "(`copies`). The Acorn PHROM A's three copies are identical. The eRadiofan copies of "
        "VM61002, VM61004, VM71003A and the TI-99 VSMs stop after the last word; padded with "
        "zeros they are identical to the full dumps.",
        "",
        "## Image formats",
        "",
    ]
    lines += [f"- **`{name}`:** {text}" for name, text in FORMATS]
    lines += [
        "",
        "## Known, not held",
        "",
        "| Data | Target | Notes |",
        "|---|---|---|",
    ]
    lines += [f"| {a} | {b} | {c} |" for a, b, c in NOT_HELD]
    lines += [
        "",
        "## Left out",
        "",
        "| Data | Where | Why |",
        "|---|---|---|",
    ]
    lines += [f"| {a} | {b} | {c} |" for a, b, c in EXCLUDED]
    lines += [
        "",
        "## Sources",
        "",
    ]
    lines += [f"- `{key}`: <https://github.com/{s['repo']}> at `{s['commit'][:12]}` "
              f"(licence: {s['licence']})" for key, s in SOURCES.items()]
    lines += ["- Reference hashes: published ROM hash lists, at the commit pinned in "
              "`tools/promimport.py` (hashes only)",
              "- The Acorn PHROM A image is in this repo, `firmware/src/romdata_acorn.h`."]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("--offline", action="store_true",
                        help="use only the sources already cached in proms/images/sources/")
    args = parser.parse_args()

    built = build(args.offline)
    os.makedirs(IMAGES_DIR, exist_ok=True)
    failed = False
    for manifest, image, problems in built:
        if manifest["image"]["source"] != "builtin":
            with open(os.path.join(promlib.ROOT, manifest["image"]["source"]), "wb") as f:
                f.write(image)
        with open(promlib.manifestPath(manifest["name"]), "w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        promlib.image(manifest)     # reads it back and checks the hash
        print(f"{manifest['name']:16s} {len(manifest['words']):4d} words, {len(image):6d} bytes, "
              f"{formatText(manifest['frame_format'])}"
              + ("" if not problems else f", {len(problems)} PROBLEMS"))
        for problem in problems:
            print(f"  {problem}")
        failed |= bool(problems)
    with open(INVENTORY_PAGE, "w") as f:
        f.write(readme(built))
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
