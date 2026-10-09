#!/usr/bin/env python3
"""
    recimport.py

    Real recordings for the encoder's tests (plan Phase 11). The originals
    are kept in the gitignored local/recordings/, each listed in
    local/recordings/sources.csv:

      file      the recording, relative to local/recordings/; it may hold
                several prompts, one after another with a pause between
      prompts   a text file of its prompts, one per line ("1. text"), in
                the order spoken
      id_prefix the prompts' ids are id_prefix plus the prompt number,
                four digits (arctic_a0001 for CMU ARCTIC's prompt a0001)
      voice     a short key for the speaker, as in the TTS file names
      speaker, source, licence   credited in the output README

    Each recording is split into its prompts at the pauses between them:
    of the pauses of 150 ms or more, the split takes the set that best fits
    each segment's duration to its text length (a speaking rate common to
    the recording) while preferring long pauses. Each segment is padded by
    up to 150 ms of the pause on each side, resampled to 8 kHz and written
    as the TTS inputs are (ttsgen.py):

      captures/phase11/inputs/recordings/<id>_<voice>.flac
      captures/phase11/inputs/recordings/index.csv   as ttsgen.py's, with
                                         each segment's place in the original
      captures/phase11/inputs/recordings/README.md

      python tools/recimport.py          split and import every recording
      python tools/recimport.py check    verify the files against the index

    The split depends only on the original, so it is repeatable; check
    confirms the files against the index without the originals.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import os
import re
import sys

import numpy as np
import soundfile

import ttsgen

ROOT = ttsgen.ROOT
LOCAL = os.path.join(ROOT, "local", "recordings")
SOURCES = os.path.join(LOCAL, "sources.csv")
OUT = os.path.join(ROOT, "captures", "phase11", "inputs", "recordings")

HOP = 0.01          # seconds per level frame
ACTIVE_DB = -45.0   # a frame is speech above this level (dBFS)
MIN_PAUSE = 0.15    # seconds of quiet that may separate prompts
PAD = 0.15          # seconds of the pause kept either side of a prompt
PAUSE_WEIGHT = 1.5  # preference for long pauses against fit to text length

INDEX_FIELDS = ["id", "voice", "text", "file", "source_file", "start_s", "end_s"] + \
    ttsgen.INDEX_FIELDS[5:]


def readPrompts(path):
    out = []
    for line in open(path):
        m = re.match(r"\s*(\d+)\.\s*(.*\S)", line)
        if m:
            out.append((int(m.group(1)), m.group(2)))
    return out


def split(x, rate, texts):
    """Split x into len(texts) segments; returns (start, end) sample pairs
    of the speech in each, without padding."""
    hop = int(rate * HOP)
    level = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2)) for i in range(0, len(x) - hop, hop)])
    active = np.flatnonzero(20 * np.log10(level + 1e-9) > ACTIVE_DB)
    if not len(active):
        sys.exit("no speech found")
    minGap = MIN_PAUSE / HOP
    gaps = [(active[k], active[k + 1]) for k in range(len(active) - 1)
            if active[k + 1] - active[k] > minGap]
    # points[i] = (end of speech before, start of speech after)
    # the file's ends count as pauses twice their length, so the first and
    # last prompts are padded up to the whole of them
    points = [(-active[0], active[0])] + gaps + \
        [(active[-1] + 1, 2 * len(level) - active[-1] - 1)]
    letters = [len(re.sub(r"[^a-z]", "", t.lower())) or 1 for t in texts]
    rate_ = (points[-1][0] - points[0][1]) / sum(letters)   # frames per letter
    n, K = len(points), len(texts)
    if n - 1 < K:
        sys.exit(f"only {n - 1} segments found for {K} prompts")
    INF = float("inf")
    cost = np.full((K + 1, n), INF)
    back = np.zeros((K + 1, n), int)
    cost[0][0] = 0.0
    for k in range(1, K + 1):
        for j in range(1, n):
            for i in range(j):
                if cost[k - 1][i] == INF:
                    continue
                d = points[j][0] - points[i][1]
                if d <= 0:
                    continue
                c = cost[k - 1][i] + np.log(d / (rate_ * letters[k - 1])) ** 2
                if i > 0:
                    c -= PAUSE_WEIGHT * np.log((points[i][1] - points[i][0]) / minGap)
                if c < cost[k][j]:
                    cost[k][j], back[k][j] = c, i
    if cost[K][n - 1] == INF:
        sys.exit("no split found")
    segs, j = [], n - 1
    for k in range(K, 0, -1):
        i = back[k][j]
        segs.append((points[i], points[j]))
        j = i
    out = []
    for before, after in reversed(segs):
        # pad into the pause either side, at most half of it
        a = before[1] - min(PAD / HOP, (before[1] - before[0]) / 2)
        b = after[0] + min(PAD / HOP, (after[1] - after[0]) / 2)
        out.append((int(max(0, a) * hop), int(min(len(x), b * hop))))
    return out


def readmeText(sources):
    lines = ["# Encoder test recordings (Phase 11)", "",
             "Real speech, split from the recordings listed below by `tools/recimport.py`, "
             f"resampled to {ttsgen.RATE} Hz, 16-bit mono FLAC (lossless), with peaks at "
             f"{ttsgen.PEAK_DBFS:g} dBFS. `index.csv` gives each file's text, its place in the "
             "original, and the SHA-256 of its decoded samples (16-bit little-endian). "
             "The originals are kept locally.", "",
             "| Voice | Original | Speaker | Source | Licence |", "|---|---|---|---|---|"]
    for s in sources:
        lines.append(f"| `{s['voice']}` | `{os.path.basename(s['file'])}` | {s['speaker']} | "
                     f"{s['source']} | {s['licence']} |")
    return "\n".join(lines) + "\n"


def importAll():
    with open(SOURCES, newline="") as f:
        sources = list(csv.DictReader(f))
    os.makedirs(OUT, exist_ok=True)
    rows = []
    for s in sources:
        prompts = readPrompts(os.path.join(LOCAL, s["prompts"]))
        x, rate = soundfile.read(os.path.join(LOCAL, s["file"]), dtype="float64")
        if x.ndim > 1:
            x = x.mean(axis=1)
        segs = split(x, rate, [t for _, t in prompts])
        for (num, text), (a, b) in zip(prompts, segs):
            pid = f"{s['id_prefix']}{num:04d}"
            name = f"{pid}_{s['voice']}.flac"
            y = ttsgen.to8k(rate, x[a:b])
            ttsgen.writeFlac(os.path.join(OUT, name), y)
            row = {"id": pid, "voice": s["voice"], "text": text, "file": name,
                   "source_file": os.path.basename(s["file"]),
                   "start_s": f"{a / rate:.3f}", "end_s": f"{b / rate:.3f}",
                   "seconds": f"{len(y) / ttsgen.RATE:.2f}",
                   "samples_sha256": ttsgen.samplesSha256(y)}
            row.update(ttsgen.measure(y))
            rows.append(row)
        print(f"{s['file']}: {len(segs)} prompts")
    with open(os.path.join(OUT, "index.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, INDEX_FIELDS)
        w.writeheader()
        w.writerows(rows)
    with open(os.path.join(OUT, "README.md"), "w") as f:
        f.write(readmeText(sources))
    print(f"{len(rows)} files in {os.path.relpath(OUT, ROOT)}/index.csv")


def check():
    with open(os.path.join(OUT, "index.csv"), newline="") as f:
        rows = list(csv.DictReader(f))
    bad = [r["file"] for r in rows
           if not os.path.exists(os.path.join(OUT, r["file"]))
           or ttsgen.samplesSha256(ttsgen.readFlac(os.path.join(OUT, r["file"]))[1])
           != r["samples_sha256"]]
    for name in bad:
        print(f"MISMATCH {name}")
    print(f"{len(rows) - len(bad)} of {len(rows)} files OK")
    return not bad


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("command", nargs="?", choices=["import", "check"], default="import")
    a = p.parse_args()
    if a.command == "import":
        importAll()
    else:
        sys.exit(0 if check() else 1)


if __name__ == "__main__":
    main()
