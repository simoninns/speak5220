#!/usr/bin/env python3
"""
    vecfit.py

    Measures the chip's parameter tables from the Phase 8 vector captures
    (plan section 9.1). For each table entry that a vector set exercises,
    the vector's takes are modelled (model.py) with the entry at its
    candidate value and at values around it. The entry is confirmed when
    its value fits the captures strictly better than every other value
    tried. The pitch periods are also measured directly, without the model.

      python tools/vecfit.py captures/vectors --out captures/phase8/analysis

    Writes tables.csv (one row per entry), pitch_periods.csv and
    vectors.csv (every take's comparison with the model) to --out.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import glob
import json
import multiprocessing
import os
import re

import numpy as np

import model as M

PERTURB = (-4, -2, -1, 1, 2, 4)
CHIRP_PERTURB = (-2, -1, 1, 2)


def takes(root, vectorSet, name, paths=("vsm01", "ext01")):
    found = []
    for d in glob.glob(os.path.join(root, f"vectors_{vectorSet}", f"*_{name}")):
        found += [os.path.join(d, p) for p in paths if os.path.exists(os.path.join(d, p, "capture.json"))]
    return sorted(found)


def tablesWith(table, index, value):
    t = {"energy": list(M.ENERGY), "pitch": list(M.PITCH), "k": [list(r) for r in M.K],
         "chirp": list(M.CHIRP)}
    if table == "energy" or table == "pitch":
        t[table][index] = value
    elif table == "chirp":
        t["chirp"][index] = value & 0xFF
    else:
        t["k"][int(table[1:]) - 1][index] = value
    return t


def baseValue(table, index):
    if table == "energy":
        return M.ENERGY[index]
    if table == "pitch":
        return M.PITCH[index]
    if table == "chirp":
        v = M.CHIRP[index]
        return v - 256 if v >= 128 else v
    return M.K[int(table[1:]) - 1][index]


def score(job):
    table, index, value, takeDirs = job
    tables = tablesWith(table, index, value)
    return sum(M.compareTake(t, tables=tables)["abs_sum"] for t in takeDirs)


def entryJobs(root):
    """(table, index, takes, perturbations) for every entry the sets exercise."""
    entries = []
    for e in range(1, 15):
        entries.append(("energy", e, takes(root, "energy", f"v_e{e:02d}") + takes(root, "energy", f"u_e{e:02d}"), PERTURB))
    for p in range(1, 64):
        entries.append(("pitch", p, takes(root, "pitch", f"p{p:02d}"), PERTURB))
    for j, size in enumerate(len(r) for r in M.K):
        for i in range(size):
            t = takes(root, "k", f"k{j + 1}_{i:02d}")
            if j < 4:
                t += takes(root, "k", f"uk{j + 1}_{i:02d}")
            entries.append((f"k{j + 1}", i, t, PERTURB))
    chirpTakes = [t for p in (20, 32, 45, 63) for t in takes(root, "pitch", f"p{p:02d}")]
    for i in range(52):
        entries.append(("chirp", i, chirpTakes, CHIRP_PERTURB))
    return [e for e in entries if e[2]]


def pitchPeriods(root):
    """The period of each pitch vector, from the chip's output alone: the
    smallest lag at which the steady part repeats best."""
    rows = []
    for p in range(1, 64):
        for take in takes(root, "pitch", f"p{p:02d}", ("vsm01",)):
            codes = M.loadCodes(take)
            first = M.loadFrames(take)[0]["read_sample"]
            seg = codes[first + 500:first + 1150].astype(int)
            scores = {lag: int(np.abs(seg[:300] - seg[lag:lag + 300]).sum()) for lag in range(10, 200)}
            best = min(scores.values())
            # The smallest lag that repeats about as well as the best:
            # multiples of the period repeat as well, and the filter's lowest
            # bits can make the waveform repeat exactly only every few periods
            tolerance = max(10, 0.02 * int(np.abs(seg[:300]).sum()))
            lag = min(lag for lag, s in scores.items() if s <= best + tolerance)
            rows.append({"index": p, "table": M.PITCH[p], "period_samples": lag,
                         "repeat_abs_sum": best, "take": take})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("root", help="the vector capture root (captures/vectors)")
    parser.add_argument("--out", default="captures/phase8/analysis")
    parser.add_argument("--jobs", type=int, default=14)
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)

    # Every take against the model
    allTakes = sorted(t for t in glob.glob(os.path.join(args.root, "vectors_*", "*", "*0*"))
                      if os.path.exists(os.path.join(t, "capture.json")))
    with multiprocessing.Pool(args.jobs) as pool:
        compared = pool.map(M.compareTake, allTakes)
    with open(os.path.join(args.out, "vectors.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["take", "path", "exact", "compared", "mismatched", "abs_sum", "max_abs", "first_mismatch"])
        for take, r in zip(allTakes, compared):
            w.writerow([os.path.relpath(take, args.root), M.takePath(take), int(r["exact"]),
                        r["compared"], r["mismatched"], r["abs_sum"], r["max_abs"], r["first_mismatch"]])
    sets = {}
    for take, r in zip(allTakes, compared):
        s = re.search(r"vectors_(\w+)", take).group(1)
        a = sets.setdefault(s, {"takes": 0, "exact": 0, "samples": 0, "mismatched": 0, "max_abs": 0})
        a["takes"] += 1
        a["exact"] += r["exact"]
        a["samples"] += r["compared"]
        a["mismatched"] += r["mismatched"]
        a["max_abs"] = max(a["max_abs"], r["max_abs"])
    print(json.dumps(sets, indent=1))

    # Table entries
    entries = entryJobs(args.root)
    jobs = []
    for table, index, t, perturb in entries:
        base = baseValue(table, index)
        for dv in (0,) + perturb:
            jobs.append((table, index, base + dv, t))
    with multiprocessing.Pool(args.jobs) as pool:
        scores = pool.map(score, jobs)
    byEntry = {}
    for (table, index, value, t), s in zip(jobs, scores):
        byEntry.setdefault((table, index), {"takes": len(t), "scores": {}})["scores"][value] = s
    rows = []
    for (table, index), e in byEntry.items():
        base = baseValue(table, index)
        s0 = e["scores"][base]
        others = {v: s for v, s in e["scores"].items() if v != base}
        bestOther = min(others.values())
        rows.append({"table": table, "index": index, "value": base, "takes": e["takes"],
                     "abs_sum": s0, "best_other_abs_sum": bestOther,
                     "best_other_value": min(others, key=others.get),
                     "confirmed": int(s0 < bestOther)})
    with open(os.path.join(args.out, "tables.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    periods = pitchPeriods(args.root)
    with open(os.path.join(args.out, "pitch_periods.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(periods[0]))
        w.writeheader()
        w.writerows({**r, "take": os.path.relpath(r["take"], args.root)} for r in periods)
    summary = {}
    for r in rows:
        s = summary.setdefault(r["table"], [0, 0])
        s[0] += r["confirmed"]
        s[1] += 1
    print("confirmed entries:", {k: f"{a}/{b}" for k, (a, b) in summary.items()})
    print("pitch period = table + 1:", sum(r["period_samples"] == r["table"] + 1 for r in periods),
          "of", len(periods))
    with open(os.path.join(args.out, "summary.json"), "w") as f:
        json.dump({"sets": sets, "confirmed": summary}, f, indent=1)


if __name__ == "__main__":
    main()
