#!/usr/bin/env python3
"""
    compare.py

    Repeat comparator (plan sections 7 and 11): compares the takes of one
    word, sample for sample. Each take is compared with the first, after
    aligning their output by the shift that matches them best, because the
    delay from Speak to the first sample varies (plan section 2).

    Give it a word directory (it compares every take in it, and writes
    compare.json there) or any capture directories (--out to save).

      python tools/compare.py captures/corpus/acorn_phrom_a/160_acorn

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os

import numpy as np

MAX_SHIFT = 400         # samples (50 ms) searched either way
MARGIN = 10             # samples compared before the first and after the last change


def takeDirs(wordDir):
    """The take directories of a word, in the order they were captured."""
    def key(directory):
        with open(os.path.join(directory, "capture.json")) as f:
            return json.load(f).get("captured", ""), directory
    return sorted((os.path.join(wordDir, d) for d in os.listdir(wordDir)
                   if os.path.exists(os.path.join(wordDir, d, "capture.json"))), key=key)


def loadTake(directory):
    with open(os.path.join(directory, "capture.json")) as f:
        meta = json.load(f)
    data = np.genfromtxt(os.path.join(directory, "samples.csv"), delimiter=",", names=True)
    frames = []
    if os.path.exists(os.path.join(directory, "frames.csv")):
        with open(os.path.join(directory, "frames.csv")) as f:
            frames = [{k: v for k, v in row.items() if k not in ("read_us", "read_sample")}
                      for row in csv.DictReader(f)]
    summary = meta.get("processed", {}).get("summary", {})
    return {"dir": directory, "code": data["code"].astype(int), "summary": summary,
            "frames": frames, "path": summary.get("path")}


def bestShift(ref, other, start, end):
    """The shift s minimising the mean |ref[i] - other[i + s]| over ref[start:end]."""
    best = None
    for shift in range(-MAX_SHIFT, MAX_SHIFT + 1):
        a0, a1 = max(start, -shift), min(end, len(other) - shift)
        if a1 - a0 < (end - start) // 2:
            continue
        score = np.mean(np.abs(ref[a0:a1] - other[a0 + shift:a1 + shift]))
        if best is None or score < best[0]:
            best = (score, shift)
    return best[1]


def comparePair(ref, other):
    s = ref["summary"]
    if s.get("speech_first_sample") is None:
        return {"take": other["dir"], "error": "the reference take is silent"}
    start = max(0, s["speech_first_sample"] - MARGIN)
    end = min(len(ref["code"]), s["speech_last_sample"] + MARGIN + 1)
    shift = bestShift(ref["code"], other["code"], start, end)
    a0, a1 = max(start, -shift), min(end, len(other["code"]) - shift)
    a = ref["code"][a0:a1]
    b = other["code"][a0 + shift:a1 + shift]
    diff = b - a
    differing = np.flatnonzero(diff)
    o = other["summary"]
    result = {
        "take": other["dir"], "path": other["path"],
        "shift_samples": int(shift),
        "speech_first_sample": o.get("speech_first_sample"),
        "compared_samples": int(len(a)),
        "identical": bool(len(differing) == 0 and len(a) == end - start),
        "differing_samples": int(len(differing)),
        "first_difference": int(differing[0] + a0 - s["speech_first_sample"])
        if len(differing) else None,
        "max_abs_difference": int(np.abs(diff).max()) if len(diff) else None,
        "rms_difference": float(np.sqrt(np.mean(diff.astype(float) ** 2))) if len(diff) else None,
        "frames_identical": other["frames"] == ref["frames"] if ref["frames"] else None,
    }
    if s.get("first_frame_read_s") is not None and o.get("first_frame_read_s") is not None:
        result["first_frame_read_difference_s"] = o["first_frame_read_s"] - s["first_frame_read_s"]
    return result


def compare(directories, log=print):
    takes = [loadTake(d) for d in directories]
    ref = takes[0]
    pairs = [comparePair(ref, t) for t in takes[1:]]
    result = {
        "reference": ref["dir"], "reference_path": ref["path"],
        "reference_speech_first_sample": ref["summary"].get("speech_first_sample"),
        "takes": len(takes),
        "all_identical": all(p.get("identical") for p in pairs),
        "all_frames_identical": all(p.get("frames_identical") is not False for p in pairs),
        "pairs": pairs,
    }
    log(f"reference {ref['dir']} ({ref['path']}), speech from sample "
        f"{result['reference_speech_first_sample']}")
    for p in pairs:
        if "error" in p:
            log(f"  {p['take']}: {p['error']}")
            continue
        verdict = "identical" if p["identical"] else (
            f"{p['differing_samples']} of {p['compared_samples']} samples differ, "
            f"first at {p['first_difference']}, max {p['max_abs_difference']}, "
            f"rms {p['rms_difference']:.2f}")
        log(f"  {os.path.basename(p['take']):8s} ({p['path']}): shift {p['shift_samples']:+d}, "
            f"{verdict}, frames {'same' if p['frames_identical'] else 'DIFFER'}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("dirs", nargs="+", help="a word directory, or capture directories")
    parser.add_argument("--out", help="write the result to this JSON file")
    args = parser.parse_args()
    if len(args.dirs) == 1 and not os.path.exists(os.path.join(args.dirs[0], "capture.json")):
        directories = takeDirs(args.dirs[0])
        out = args.out or os.path.join(args.dirs[0], "compare.json")
    else:
        directories, out = args.dirs, args.out
    if len(directories) < 2:
        parser.error("need at least two takes")
    result = compare(directories)
    if out:
        with open(out, "w") as f:
            json.dump(result, f, indent=2)


if __name__ == "__main__":
    main()
