# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reads Acorn DFS disc images, independently of the application's writer.

The functional tests read the BBC Micro disc back with this script, so that
the C++ writer is checked against a second reading of the catalogue layout
(Acorn DFS Manual) rather than against itself.

    dfs_catalogue.py IMAGE            prints each side's catalogue as JSON
    dfs_catalogue.py IMAGE SIDE NAME  writes that file's bytes to stdout

An image of 204800 bytes is double-sided (.dsd, tracks interleaved side 0,
side 1); any other size is one side (.ssd).
"""

import hashlib
import json
import sys

SECTOR = 256
TRACK = 10 * SECTOR
TRACKS = 40
DOUBLE_SIDED = 2 * TRACKS * TRACK


def sides_of(image):
    """Splits an image into its sides' bytes."""
    if len(image) != DOUBLE_SIDED:
        return [image]
    sides = [bytearray(), bytearray()]
    for track in range(TRACKS):
        for side in range(2):
            start = (2 * track + side) * TRACK
            sides[side] += image[start : start + TRACK]
    return [bytes(side) for side in sides]


def catalogue(side):
    """The catalogue of one side: title, boot option, sectors and files."""
    names, info = side[0:SECTOR], side[SECTOR : 2 * SECTOR]
    title = (names[0:8] + info[0:4]).decode("ascii").rstrip(" \0")
    files = []
    for entry in range(info[5] // 8):
        n = names[8 + 8 * entry : 16 + 8 * entry]
        i = info[8 + 8 * entry : 16 + 8 * entry]
        mixed = i[6]
        start = ((mixed & 0x03) << 8) | i[7]
        length = ((mixed >> 4 & 0x03) << 16) | i[4] | (i[5] << 8)
        data = side[start * SECTOR : start * SECTOR + length]
        files.append(
            {
                "name": n[0:7].decode("ascii").rstrip(" "),
                "directory": chr(n[7] & 0x7F),
                "locked": bool(n[7] & 0x80),
                "load": ((mixed >> 2 & 0x03) << 16) | i[0] | (i[1] << 8),
                "exec": ((mixed >> 6 & 0x03) << 16) | i[2] | (i[3] << 8),
                "length": length,
                "start_sector": start,
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )
    return {
        "title": title,
        "boot_option": info[6] >> 4 & 0x03,
        "sectors": ((info[6] & 0x03) << 8) | info[7],
        "files": files,
    }


def main(argv):
    with open(argv[1], "rb") as stream:
        sides = sides_of(stream.read())
    if len(argv) == 2:
        print(json.dumps({"sides": [catalogue(side) for side in sides]}, indent=1))
        return 0
    side = sides[int(argv[2])]
    for entry in catalogue(side)["files"]:
        if entry["name"] == argv[3]:
            start = entry["start_sector"] * SECTOR
            sys.stdout.buffer.write(side[start : start + entry["length"]])
            return 0
    print(f"no file {argv[3]} on side {argv[2]}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
