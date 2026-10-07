# Phase 8 evidence: each evidence take's source, settings, raw-file hashes
# and its comparison with the candidate model (tools/model.py). Run from the
# repository root; prints the Markdown rows of the evidence README.
import json
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))
import csv  # noqa: E402
import lpc  # noqa: E402
import model as M  # noqa: E402
import prom as promlib  # noqa: E402

VECTORS = os.path.join(ROOT, "captures", "vectors")


def step(x):
    return ((x << 1) | (((x >> 12) ^ (x >> 3) ^ (x >> 2) ^ x) & 1)) & 0x1FFF


def stateAfter(n):
    """The noise generator's state n steps after 0x1FFF."""
    x = 0x1FFF
    for _ in range(n % 8191):
        x = step(x)
    return x


STEPS = {"vsm": 1958, "external": 1923}     # steps at the first sample, standard procedure


def frames(setName, word):
    manifest = promlib.loadManifest(os.path.join(ROOT, "captures", "phase8", "vectors", f"{setName}.json"))
    image = promlib.image(manifest)
    address = promlib.address(promlib.findWord(manifest, word))
    return lpc.parseFrames(lpc.bitsLsbFirst(image[address - promlib.base(manifest):]))[0]


def predecessor(source):
    """The frames spoken just before a vector take (the take before it in its
    set's takes.csv), or None."""
    parts = source.split("/")
    if parts[1] != "vectors" or not parts[2].startswith("vectors_"):
        return None
    setDir = os.path.join(ROOT, *parts[:3])
    rows = sorted(csv.DictReader(open(os.path.join(setDir, "takes.csv"))),
                  key=lambda r: (r["captured"], int(r["frame"] or 0)))
    names = [r["take"] for r in rows]
    take = "/".join(parts[3:])
    i = names.index(take)
    if i == 0:
        return None
    word = rows[i - 1]["entry"].split("_", 1)[1]
    return frames(parts[2][len("vectors_"):], word)

EVIDENCE = os.path.join(ROOT, "captures", "phase8", "evidence")
for line in open(os.path.join(EVIDENCE, "sources.txt")):
    ident, source = line.split()
    take = os.path.join(EVIDENCE, ident)
    meta = json.load(open(os.path.join(take, "capture.json")))
    options, before = {}, []
    exp = meta.get("experiment")
    if exp and exp["name"] == "latch_offset":
        # A Speak accepted n slots later starts the generator n steps later
        options["rng_at_speak"] = stateAfter(STEPS[meta["path"]] - exp["slots"])
    elif exp and exp["name"] == "previous_utterance":
        before = [frames("loud", exp["before"] if exp["before"] != "-" else "loud03")]
    else:
        p = predecessor(source)
        before = [p] if p else []
    r = M.compareTake(take, options, before=before)
    name = (meta.get("source") or {}).get("word")
    what = name + (f", {exp['name']} {exp.get('slots', exp.get('before'))}" if exp else "")
    raw = meta.get("raw_sha256", {})
    summary = meta["processed"]["summary"]
    agree = "exact" if r["exact"] else f"{r['mismatched']} of {r['compared']} differ (max {r['max_abs']})"
    if summary.get("code_range") == [-1, -1] and not (meta["pico"].get("result") or {}).get("talked"):
        agree = "chip never spoke (stream under 9 bytes)"
    print(f"| {ident} | `{source.replace('captures/vectors/', '')}` | {what} | {meta['path']} | {agree} | "
          f"`{raw.get('scope_raw.npz', '')}` | `{raw.get('vsm_trace.bin', '')}` |")
