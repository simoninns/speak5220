#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes every word of a PHROM image as frames and as the chip's output.

The PHROM round trip (docs-tech/encoder-quality-investigation.md §3.1):
each word of the image named by a PROM manifest (investigation/proms/;
by default Acorn PHROM A) is parsed with the frame format of speak5220
datasheet §8.1, packed again and checked against the image's bytes
(§12.11), and written into --output as

    NAME.frames.yaml   the frames, in a phrase's frames.yaml format
    NAME.bin           the word's bytes in the image, up to its stop code
    NAME.wav           the chip's output: speak5220-cli synth, 8 kHz,
                       the I/O codes x 64

with index.tsv listing the words. The output is the application's
emulator, spoken from power-up; its agreement with the chip is the
emulator's reference data (app/golden/emulator/), not checked here.

The files are mechanically derived, so they are generated, not committed:
by default into build/app/phrom-roundtrip/. phrom-check.sh runs this, then
encodes the WAV files and compares the encodings with the frames. Needs a
built speak5220-cli and a Python 3 with soundfile, as in the project's
default Nix shell:

    nix develop --command python3 app/scripts/phrom_roundtrip.py [--output DIR]
"""

import argparse
import csv
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INVESTIGATION = ROOT / "investigation"
DEFAULT_MANIFEST = INVESTIGATION / "proms" / "acorn_phrom_a.json"
DEFAULT_OUTPUT = ROOT / "build" / "app" / "phrom-roundtrip"
DEFAULT_CLI = ROOT / "build" / "app" / "src" / "cli" / "speak5220-cli"

# speak5220 datasheet §8.1: field widths, and the energy codes of silence
# and the stop.
ENERGY_BITS, REPEAT_BITS, PITCH_BITS = 4, 1, 6
K_BITS = (5, 5, 4, 4, 4, 4, 4, 3, 3, 3)
UNVOICED_K = 4
SILENCE, STOP = 0, 15

# Acorn layout (datasheet §12.8): a PHROM sits in a 16 KB bank, and the
# manifest's word addresses are 18-bit, bank in the top four bits.
BANK_SHIFT = 14

# A speech word, as the investigation counted them: more than this many
# frames, and not one of the tones.
MIN_SPEECH_FRAMES = 9


def read_image(manifest):
    image = manifest["image"]
    path = INVESTIGATION / image["file"]
    if path.suffix == ".h":
        text = path.read_text()
        data = bytes(
            int(v, 16)
            for v in re.findall(r"0x([0-9A-Fa-f]{2})", text[text.index("{") :])
        )
    else:
        data = path.read_bytes()
    if (
        len(data) != image["size"]
        or hashlib.sha256(data).hexdigest() != image["sha256"]
    ):
        sys.exit(f"{path}: not the image the manifest describes")
    return data


def stream_bits(data):
    """The bit stream: bytes read from bit 0, as the chip reads a VSM byte."""
    for byte in data:
        for i in range(8):
            yield (byte >> i) & 1


def parse(data):
    """The frames up to and including the stop, and the bits they use."""
    bits = stream_bits(data)
    used = 0

    def take(width):
        nonlocal used
        value = 0
        for _ in range(width):
            value = (value << 1) | next(bits)
        used += width
        return value

    frames = []
    while True:
        energy = take(ENERGY_BITS)
        if energy in (SILENCE, STOP):
            frames.append({"energy": energy})
            if energy == STOP:
                return frames, used
            continue
        repeat = take(REPEAT_BITS)
        pitch = take(PITCH_BITS)
        frame = {"energy": energy, "repeat": repeat, "pitch": pitch}
        if not repeat:
            count = len(K_BITS) if pitch else UNVOICED_K
            frame["k"] = [take(w) for w in K_BITS[:count]]
        frames.append(frame)


def pack(frames):
    """The frames as bytes, the last byte padded with zeros."""
    bits = []

    def put(value, width):
        bits.extend((value >> (width - 1 - i)) & 1 for i in range(width))

    for f in frames:
        put(f["energy"], ENERGY_BITS)
        if f["energy"] in (SILENCE, STOP):
            continue
        put(f["repeat"], REPEAT_BITS)
        put(f["pitch"], PITCH_BITS)
        for value, width in zip(f.get("k", []), K_BITS):
            put(value, width)
    bits += [0] * (-len(bits) % 8)
    return bytes(
        sum(b << i for i, b in enumerate(bits[n : n + 8]))
        for n in range(0, len(bits), 8)
    )


def word_bytes(data, used):
    """The bytes holding `used` bits, the unused bits of the last cleared."""
    n = (used + 7) // 8
    out = bytearray(data[:n])
    if used % 8:
        out[-1] &= (1 << (used % 8)) - 1
    return bytes(out)


def frames_yaml(frames):
    lines = ["frames:"]
    for f in frames:
        items = [f"energy: {f['energy']}"]
        if "repeat" in f:
            items += [f"repeat: {f['repeat']}", f"pitch: {f['pitch']}"]
        if "k" in f:
            items.append("k: [" + ", ".join(str(v) for v in f["k"]) + "]")
        lines.append("  - {" + ", ".join(items) + "}")
    return "\n".join(lines) + "\n"


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help="the PROM manifest (default: %(default)s)",
    )
    ap.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="directory for the files (default: %(default)s)",
    )
    ap.add_argument(
        "--cli",
        type=Path,
        default=DEFAULT_CLI,
        help="speak5220-cli (default: %(default)s)",
    )
    args = ap.parse_args()

    manifest = json.loads(args.manifest.read_text())
    if manifest.get("format") != "acorn" or manifest.get("target_chip") != "TMS5220":
        sys.exit(f"{args.manifest}: only Acorn PHROMs for the TMS5220 are supported")
    if not args.cli.exists():
        sys.exit(f"{args.cli} not found: build speak5220-cli first")
    data = read_image(manifest)
    base = manifest["bank"] << BANK_SHIFT
    args.output.mkdir(parents=True, exist_ok=True)

    rows, seen, bad_packs = [], set(), []
    for word in manifest["words"]:
        address = int(word["address"], 16)
        if address in seen:
            continue
        seen.add(address)
        stem = f"{word['number']}_{slug(word['name'])}"
        image = data[address - base :]
        frames, used = parse(image)
        stored = word_bytes(image, used)
        if pack(frames) != stored:
            bad_packs.append(stem)
        (args.output / f"{stem}.frames.yaml").write_text(frames_yaml(frames))
        (args.output / f"{stem}.bin").write_bytes(stored)
        wav = args.output / f"{stem}.wav"
        run = subprocess.run(
            [
                str(args.cli),
                "synth",
                str(args.output / f"{stem}.frames.yaml"),
                "--output",
                str(wav),
            ],
            capture_output=True,
            text=True,
        )
        if run.returncode != 0:
            sys.exit(f"{stem}: speak5220-cli synth failed:\n{run.stderr}")

        unvoiced = sum(1 for f in frames if "k" in f and f["pitch"] == 0)
        speech = len(frames) >= MIN_SPEECH_FRAMES and "TONE" not in word["name"].upper()
        rows.append(
            {
                "stem": stem,
                "number": word["number"],
                "name": word["name"],
                "address": word["address"],
                "frames": len(frames),
                "bits": used,
                "unvoiced_frames": unvoiced,
                "speech": int(speech),
            }
        )

    with open(args.output / "index.tsv", "w", newline="") as f:
        w = csv.DictWriter(f, list(rows[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

    print(
        f"phrom_roundtrip: {len(rows)} words ({sum(r['speech'] for r in rows)} speech) "
        f"in {args.output}; {len(rows) - len(bad_packs)} re-pack to the image's bytes"
    )
    for stem in bad_packs:
        print(
            f"phrom_roundtrip: {stem}: the frames do not re-pack to the image's bytes",
            file=sys.stderr,
        )
    return 1 if bad_packs else 0


if __name__ == "__main__":
    sys.exit(main())
