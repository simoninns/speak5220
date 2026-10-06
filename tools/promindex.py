#!/usr/bin/env python3
"""
    promindex.py

    Writes index.md for a PROM's capture set (plan section 6): every word in
    its prom.json, with its address, the frames and bits it parsed to, its
    length, its takes and their checks, how the repeats compare, and links
    to the audio. Words not captured yet are listed too.

      python tools/promindex.py captures/corpus/acorn_phrom_a

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os

import compare as comparelib
import prom as promlib


def loadJson(path):
    try:
        with open(path) as f:
            return json.load(f)
    except OSError:
        return None


def repeatsText(result):
    if not result:
        return ""
    pairs = [p for p in result["pairs"] if "error" not in p]
    if result["all_identical"]:
        return f"{len(pairs) + 1} identical"
    worst = max((p["rms_difference"] or 0 for p in pairs), default=0)
    same = sum(p["identical"] for p in pairs) + 1
    return f"{same} of {len(pairs) + 1} identical, worst rms {worst:.2f}"


def wordRow(promDir, word, name, number, address):
    wordDir = os.path.join(promDir, name)
    takes = comparelib.takeDirs(wordDir) if os.path.isdir(wordDir) else []
    cells = [str(number), word, address]
    if not takes:
        return cells + ["", "", "", "not captured", "", "", ""]
    processed = [loadJson(os.path.join(t, "capture.json")).get("processed", {}) for t in takes]
    summary = next((p["summary"] for p in processed if p.get("summary", {}).get("frames")), {})
    failed = sorted({k for p in processed for k, v in p.get("checks", {}).items() if v is False})
    paths = {}
    for p in processed:
        path = p.get("summary", {}).get("path") or "?"
        paths[path] = paths.get(path, 0) + 1
    first = os.path.relpath(takes[0], promDir)
    ms = summary.get("talk_samples", 0) / 8
    bits = summary.get("bits_consumed", summary.get("stream_bits_to_stop", ""))
    return cells + [
        str(summary.get("frames", "")), str(bits), f"{ms:.0f}",
        ", ".join(f"{n} {path}" for path, n in sorted(paths.items())),
        "pass" if not failed else "FAIL: " + ", ".join(failed),
        repeatsText(loadJson(os.path.join(wordDir, "compare.json"))),
        f"[samples]({first}/samples.wav) · [listen]({first}/listen.wav) · "
        f"[frames]({first}/frames.csv)",
    ]


def writeIndex(promDir, log=print):
    manifest = loadJson(os.path.join(promDir, "prom.json"))
    rows, known = [], set()
    for word in manifest["words"]:
        name = promlib.wordDir(word)
        known.add(name)
        rows.append(wordRow(promDir, word["name"], name, word["number"], word["address"]))
    for name in sorted(os.listdir(promDir)):
        if name.startswith("addr_") and os.path.isdir(os.path.join(promDir, name)):
            rows.append(wordRow(promDir, "(address sweep)", name, "", "0x" + name[5:].upper()))
            known.add(name)
    captured = sum(r[6] != "not captured" for r in rows)
    image = manifest["image"]
    lines = [
        f"# {manifest.get('title', manifest['name'])}",
        "",
        f"- **Image:** {image['size']} bytes, SHA-256 `{image['sha256']}`, "
        f"bank {manifest['bank']:X}. Target chip: {manifest.get('target_chip', '?')}.",
        f"- **Provenance:** {manifest.get('provenance', '')}",
        f"- **Captured:** {captured} of {len(rows)} entries. Each take directory follows "
        "`docs-tech/speech_capture_plan.md` §6; its `capture.json` holds the metadata and checks.",
        "- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent "
        "(Speak External), up to the stop code. ms is the talk time of the first take, "
        "from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with "
        "the first, sample for sample, after aligning them (`compare.json`).",
        "",
        "| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    lines += ["| " + " | ".join(r) + " |" for r in rows]
    path = os.path.join(promDir, "index.md")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    log(f"{captured} of {len(rows)} entries captured -> {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("prom_dir", help="a PROM's capture directory (holding prom.json)")
    writeIndex(parser.parse_args().prom_dir)


if __name__ == "__main__":
    main()
