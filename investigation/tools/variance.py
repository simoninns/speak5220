#!/usr/bin/env python3
"""
    variance.py

    How far apart two outputs of "the same utterance" are, measured the same
    way whatever produced them (Phase 9 close-out, plan section 12). Five
    kinds of pair:

      take   chip 1 against itself, two takes of the same session
      day    chip 1 against itself, takes two days apart (the chip refitted)
      chip   chip 4 against chip 1 (vector sets, and PROM words where
             chip 4 has captured them)
      model  the model (= the emulator, which matches it on every take)
             against chip 1
      model4 the model against chip 4

    For each pair, over the speech window (from 5 samples before the first
    frame read to 5 samples after the last sample where either output
    differs from its idle value): the share of samples equal, the RMS and
    largest difference in I/O codes, and the signal-to-difference ratio
    (the reference chip's code power over the difference's power, in dB).

      python tools/variance.py                       # every kind, writes captures/phase9/variance/

    Writes pairs.csv (one row per pair) and summary.json (per kind and set:
    the number of pairs, the share exact, and the 10th, 50th and 90th
    percentiles of each measure).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import glob
import json
import os
import re
from multiprocessing import Pool

import numpy as np

import regress
from regress import ROOT, STANDARD_FIRST_READ_SAMPLE

OUT = os.path.join(ROOT, "captures", "phase9", "variance")
CHIP1 = os.path.join(ROOT, "captures", "vectors")
CHIP1_REPEAT = os.path.join(ROOT, "captures", "vectors_chip1_repeat")
CHIP4 = os.path.join(ROOT, "captures", "vectors_chip4")
CORPUS = os.path.join(ROOT, "captures", "corpus")     # one directory per PROM
CORPUS4 = os.path.join(ROOT, "captures", "corpus_chip4")
SDR_CAP = 99.0          # dB, reported for identical outputs
# The tolerance of acceptance test 1 (plan section 1.5), as observed at the
# close of Phase 9 and not derived from the chip: per set, the emulator's
# median SDR against chip 1 may fall at most MARGIN_DB below the reference
# agreement (a second chip against chip 1 where captured, else chip 1's
# own takes), with agreement above AGREE_CAP_DB counted as AGREE_CAP_DB
MARGIN_DB = 7.0
AGREE_CAP_DB = 40.0


def chipCodes(takeDir):
    """A take's I/O codes, indexed from its first frame read (VSM) or where
    that read would be (Speak External)."""
    capture, codes, reads = regress.loadChip(takeDir)
    external = capture["path"] == "external"
    first = reads[0] if reads and not external else STANDARD_FIRST_READ_SAMPLE
    return codes, first


def measure(ref, other):
    """ref, other: (codes, first). Compares them from first - 5 to the end of
    speech in either."""
    (a, fa), (b, fb) = ref, other
    lo = -min(5, fa, fb)
    n = min(len(a) - fa, len(b) - fb)
    a, b = a[fa + lo:fa + n], b[fb + lo:fb + n]
    active = np.flatnonzero((a != a[-1]) | (b != b[-1]))
    end = min(len(a), active[-1] + 6) if len(active) else len(a)
    a, b = a[:end], b[:end]
    d = a - b
    nz = np.flatnonzero(d)
    power = float(np.sum(a.astype(float) ** 2))
    noise = float(np.sum(d.astype(float) ** 2))
    return {"compared": int(len(d)), "equal_pct": 100.0 * (1 - len(nz) / len(d)) if len(d) else 100.0,
            "rms": float(np.sqrt(noise / len(d))) if len(d) else 0.0,
            "max_abs": int(np.abs(d).max()) if len(d) else 0,
            "sdr_db": min(SDR_CAP, 10 * np.log10(power / noise)) if noise else SDR_CAP,
            "first_diff": int(nz[0] + lo) if len(nz) else ""}


def setOf(takeDir):
    parts = os.path.relpath(takeDir, ROOT).split(os.sep)
    return parts[2] if parts[1] in ("corpus", "corpus_chip4") else next(p for p in parts[2:] if p.startswith("vectors_"))


def pairJob(job):
    kind, refDir, otherDir = job
    row = {"kind": kind, "set": setOf(refDir), "take": os.path.relpath(refDir, ROOT),
           "other": os.path.relpath(otherDir, ROOT)}
    row.update(measure(chipCodes(refDir), chipCodes(otherDir)))
    return row


def modelJob(job):
    kind, takeDir, previous = job
    capture, codes, reads = regress.loadChip(takeDir)
    external = capture["path"] == "external"
    first = reads[0] if reads and not external else STANDARD_FIRST_READ_SAMPLE
    manifest = regress.manifestFor(capture)
    model = regress.modelAligned(takeDir, manifest, previous, external, len(codes), first)
    keep = model != 10**6
    model = np.where(keep, model, codes)        # outside the model's run: not compared
    row = {"kind": kind, "set": setOf(takeDir), "take": os.path.relpath(takeDir, ROOT), "other": "model"}
    row.update(measure((codes, first), (model, first)))
    return row


def standard(takeDir):
    with open(os.path.join(takeDir, "capture.json")) as f:
        capture = json.load(f)
    failed = {k for k, v in capture["processed"].get("checks", {}).items() if v is False}
    return capture.get("procedure", {}).get("start") == "standard" and failed <= {"layer2_vs_layer3"}


def vectorTakes(root):
    return sorted(t for t in glob.glob(os.path.join(root, "vectors_*", "*", "*0[0-9]"))
                  if os.path.isdir(t) and standard(t))


def jobs():
    out = []
    # take to take, chip 1: consecutive takes of a word and path. The filter's
    # state carries over from the utterance before (datasheet section 7), so a
    # word's first take, which follows another word, is paired only in the
    # vector sets, whose long held frames settle it: 01/02 there; 02/03 and
    # 04/05 in the corpus, where both takes follow the same word
    for t in vectorTakes(CHIP1) + sorted(glob.glob(os.path.join(CORPUS, "*", "*", "*0[0-9]"))):
        m = re.match(r"(vsm|ext)(\d\d)$", os.path.basename(t))
        if m and int(m.group(2)) % 2 == (0 if t.startswith(CORPUS) else 1):
            partner = os.path.join(os.path.dirname(t), f"{m.group(1)}{int(m.group(2)) + 1:02d}")
            if os.path.isdir(partner) and standard(t) and standard(partner):
                out.append((pairJob, ("take", t, partner)))
    # day to day, chip 1; chip 4 against chip 1
    for kind, root in (("day", CHIP1_REPEAT), ("chip", CHIP4)):
        for t in vectorTakes(root):
            ref = os.path.join(CHIP1, os.path.relpath(t, root))
            if os.path.isdir(ref) and standard(ref):
                out.append((pairJob, (kind, ref, t)))
    # chip 4 against chip 1 on PROM words: takes 02-05, which follow a take
    # of the same word on both chips
    for t in sorted(glob.glob(os.path.join(CORPUS4, "*", "*", "*0[2-5]"))):
        ref = os.path.join(CORPUS, os.path.relpath(t, CORPUS4))
        if os.path.isdir(ref) and standard(ref) and standard(t):
            out.append((pairJob, ("chip", ref, t)))
    # the model against each chip, with the takes spoken before each
    for kind, root in (("model", CHIP1), ("model", CORPUS), ("model4", CHIP4), ("model4", CORPUS4)):
        for take, previous in regress.takesOf(root, False):
            if os.sep + "clk" in take or "experiments" in take or "/sweep/" in take:
                continue
            out.append((modelJob, (kind, take, previous)))
    return out


def run(job):
    f, args = job
    return f(args)


def percentiles(values):
    return [round(float(v), 3) for v in np.percentile(values, [10, 50, 90])] if values else None


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("--out", default=OUT)
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    args = parser.parse_args()
    todo = jobs()
    with Pool(args.jobs) as p:
        rows = p.map(run, todo, chunksize=4)
    os.makedirs(args.out, exist_ok=True)
    fields = ["kind", "set", "take", "other", "compared", "equal_pct", "rms", "max_abs", "sdr_db", "first_diff"]
    with open(os.path.join(args.out, "pairs.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in sorted(rows, key=lambda r: (r["kind"], r["take"])):
            w.writerow({k: (round(v, 3) if isinstance(v, float) else v) for k, v in r.items()})
    summary = {}
    for r in rows:
        summary.setdefault(r["kind"], {}).setdefault(r["set"], []).append(r)
    out = {}
    for kind, sets in sorted(summary.items()):
        for name, rs in sorted(sets.items()):
            out.setdefault(kind, {})[name] = {
                "pairs": len(rs),
                "exact_pct": round(100.0 * sum(r["equal_pct"] == 100.0 for r in rs) / len(rs), 1),
                "equal_pct_p10_p50_p90": percentiles([r["equal_pct"] for r in rs]),
                "rms_p10_p50_p90": percentiles([r["rms"] for r in rs]),
                "max_abs_p10_p50_p90": percentiles([r["max_abs"] for r in rs]),
                "sdr_db_p10_p50_p90": percentiles([r["sdr_db"] for r in rs]),
            }
    tolerance = {}
    for name, s in out.get("model", {}).items():
        ref = out.get("chip", {}).get(name) or out.get("take", {}).get(name)
        if not ref:
            continue
        agree = min(AGREE_CAP_DB, ref["sdr_db_p10_p50_p90"][1])
        emu = min(AGREE_CAP_DB, s["sdr_db_p10_p50_p90"][1])
        tolerance[name] = {"reference": "chip 4" if name in out.get("chip", {}) else "chip 1 takes",
                           "reference_sdr_db": round(agree, 1), "emulator_sdr_db": round(emu, 1),
                           "gap_db": round(agree - emu, 1), "within": agree - emu <= MARGIN_DB}
    out["tolerance"] = {"margin_db": MARGIN_DB, "agreement_cap_db": AGREE_CAP_DB,
                        "note": "observed at the close of Phase 9, not derived", "sets": tolerance}
    with open(os.path.join(args.out, "summary.json"), "w") as f:
        json.dump(out, f, indent=1)
    print(f"{'kind':7s} {'set':16s} {'pairs':>5s} {'exact%':>6s}  {'equal% p10/p50/p90':22s} {'SDR dB p10/p50/p90':22s} {'max p50/p90':10s}")
    for kind, sets in out.items():
        if kind == "tolerance":
            continue
        for name, s in sets.items():
            e, d, m = s["equal_pct_p10_p50_p90"], s["sdr_db_p10_p50_p90"], s["max_abs_p10_p50_p90"]
            print(f"{kind:7s} {name:16s} {s['pairs']:5d} {s['exact_pct']:6.1f}  "
                  f"{e[0]:6.1f} {e[1]:6.1f} {e[2]:6.1f}    {d[0]:5.1f} {d[1]:5.1f} {d[2]:5.1f}     {m[1]:4.0f} {m[2]:4.0f}")
    print(f"\ntolerance (observed, not derived): emulator median SDR within {MARGIN_DB:g} dB of the reference, capped at {AGREE_CAP_DB:g} dB")
    for name, t in tolerance.items():
        print(f"  {name:16s} reference {t['reference']:12s} {t['reference_sdr_db']:5.1f} dB, emulator {t['emulator_sdr_db']:5.1f} dB, "
              f"gap {t['gap_db']:5.1f} dB: {'within' if t['within'] else 'OUTSIDE'}")


if __name__ == "__main__":
    main()
