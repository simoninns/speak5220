# Phase 7: the figures the datasheet cites from the Acorn PHROM A reference
# set (corpus acorn_phrom_a): the sweep's reads and Talk Status timing, the
# repeats of words and of the sweep's repeat sample, and how early takes
# depart from the others depending on the take before them.
#   python corpus_stats.py [captures/corpus/acorn_phrom_a]
import collections
import glob
import json
import os
import sys

import numpy as np

root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "corpus", "acorn_phrom_a")


def load(path):
    with open(path) as f:
        return json.load(f)


# The sweep: the first take of each start address
rows = [load(f)["processed"] for f in glob.glob(os.path.join(root, "sweep", "addr_*", "vsm01", "capture.json"))]
print(f"sweep starts: {len(rows)}")
for check in ("bits_vs_frames", "vsm_image", "frame_period", "duration", "stop_frame"):
    print(f"  {check}: {dict(collections.Counter(r['checks'].get(check) for r in rows))}")
s = [r["summary"] for r in rows]
print(f"  frames {sum(x['frames'] for x in s)}, bits {sum(x['bits_consumed'] for x in s)}")
kinds = collections.Counter()
for x in s:
    kinds.update(x["frame_kinds"])
print(f"  frame kinds {dict(kinds)}")
d = np.array([x["talk_after_first_read_s"] - x["frames"] * 0.025 for x in s if x["frames"] > 1]) * 1e3
print(f"  talk - frames x 25 ms, {len(d)} starts with more than one frame: {d.min():.3f} to {d.max():.3f} ms")
one = np.array([x["talk_after_first_read_s"] for x in s if x["frames"] == 1]) * 1e3
silent = sum(x["frames"] == 1 and x["speech_first_sample"] is None for x in s)
print(f"  stop code first: {len(one)} starts, talk {one.min():.3f} to {one.max():.3f} ms after the read, "
      f"{silent} with no output")


def groups(pattern):
    for f in glob.glob(os.path.join(root, pattern, "compare.json")):
        for g in load(f)["standard_variants"]:
            yield g


for name, pattern in (("words", "[0-9]*_*"), ("sweep repeat sample", os.path.join("sweep", "addr_*"))):
    gs = [g for g in groups(pattern) if g["takes"] >= 3]
    takes = sum(g["takes"] for g in gs)
    dev = collections.Counter(m["max_abs_deviation"] for g in gs for m in g["members"])
    print(f"{name}: {len(gs)} groups of 3 or more takes, {takes} takes")
    print(f"  identical groups {sum(g['variants'] == 1 for g in gs)}, "
          f"takes matching the majority {sum(g['takes_matching_majority'] for g in gs)}, "
          f"within 2 codes {sum(n for k, n in dev.items() if k <= 2)}, "
          f"largest departure {max(dev)}, groups with samples without a majority "
          f"{sum(g['samples_without_majority'] > 0 for g in gs)}")
    onsets = collections.Counter(len(m["upset_onsets"]) for g in gs for m in g["members"])
    print(f"  upset onsets per take {sorted(onsets.items())}")
    # Takes in capture order: does a take depart within 20 samples of the
    # start more often when it follows a different utterance?
    order = collections.Counter()
    for g in gs:
        for i, m in enumerate(sorted(g["members"], key=lambda m: m["take"])):
            order[(i, bool(m["upset_onsets"]) and m["upset_onsets"][0] < 20)] += 1
    for i in range(5):
        n = order[(i, True)] + order[(i, False)]
        if n:
            print(f"  take {i + 1}: {order[(i, True)]} of {n} depart within 20 samples of the start")
