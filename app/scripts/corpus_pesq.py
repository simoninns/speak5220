#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks PESQ for the corpus encodings that differ from the reference.

Reads the report of the C++ corpus check (corpus-check.sh) and, for every
encoding that is not identical to the reference, scores both the reference
frames and the C++ frames with PESQ as investigation/tools/speechqual.py
measures it (P.862 narrowband as P.862.1 MOS-LQO, the frames spoken by the
reference emulator and aligned with the input). Plan §3.2: the two scores
must agree within 0.02.

Until the application measures PESQ itself (plan Phase 3), this uses the
retired reference tools as an oracle, so it needs the project's default Nix
shell and a built investigation/emulator/tms5220emu.

Usage: corpus_pesq.py REPORT.tsv CORPUS_DIR OUTPUT_DIR
Prints one line per differing encoding; exits 1 if any is out of tolerance.
"""

import argparse
import csv
import multiprocessing
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "investigation" / "tools"
GOLDEN = ROOT / "app" / "golden" / "encoder" / "frames"

# Plan §3.2: PESQ within this of the reference's, where frames differ.
TOLERANCE = 0.02


def reference_packed(set_name):
    with open(GOLDEN / (set_name + ".tsv"), newline="") as f:
        return {r["name"]: r["packed"] for r in csv.DictReader(f, delimiter="\t")}


def score(job):
    """PESQ of the packed frames `hex_bytes` against the input `wav`."""
    wav, hex_bytes, bin_path = job
    sys.path.insert(0, str(TOOLS))
    import speechqual

    Path(bin_path).write_bytes(bytes.fromhex(hex_bytes))
    row = speechqual.measure((str(wav), bin_path, {}, False, False, None, None))
    return float(row["pesq"])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("report", type=Path)
    ap.add_argument("corpus", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    if not (TOOLS / "speechqual.py").exists():
        print("PESQ not checked: %s is not available" % (TOOLS / "speechqual.py"))
        return 1
    with open(args.report, newline="") as f:
        rows = [
            r for r in csv.DictReader(f, delimiter="\t") if r["result"] != "identical"
        ]
    if not rows:
        print("PESQ: every encoding is identical to the reference; nothing to score")
        return 0
    args.output.mkdir(parents=True, exist_ok=True)
    refs = {}
    jobs = []
    for r in rows:
        if r["set"] not in refs:
            refs[r["set"]] = reference_packed(r["set"])
        stem = "%s_%s" % (r["set"], r["name"])
        wav = args.corpus / (r["name"] + ".wav")
        jobs.append(
            (wav, refs[r["set"]][r["name"]], str(args.output / (stem + "_ref.bin")))
        )
        jobs.append((wav, r["packed"], str(args.output / (stem + "_cpp.bin"))))
    with multiprocessing.Pool(os.cpu_count() or 1) as pool:
        scores = pool.map(score, jobs)
    failed = 0
    for i, r in enumerate(rows):
        ref, cpp = scores[2 * i], scores[2 * i + 1]
        ok = abs(cpp - ref) <= TOLERANCE
        failed += 0 if ok else 1
        print(
            "%-8s %-24s PESQ reference %.3f, C++ %.3f, difference %+.3f %s"
            % (
                r["set"],
                r["name"],
                ref,
                cpp,
                cpp - ref,
                "ok" if ok else "OUT OF TOLERANCE",
            )
        )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
