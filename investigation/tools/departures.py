#!/usr/bin/env python3
"""
    departures.py

    Where the chip departs from the model, and from itself
    (docs/technical/die-analysis/lowest-order-arithmetic.md). For each word and
    path it takes every standard take but the first, so that every take
    follows a take of the same word (the filter's state carries over,
    datasheet section 7), and lines them up with the model (tools/model.py,
    which the emulator equals on every take).

    Sample by sample, the takes that have agreed with the per-sample
    majority for the last 25 samples are "in step". Where at least 80 % of
    the takes are in step, and the model has also agreed with the majority
    for the last 25 samples, a sample is labelled:

      determ   a deterministic departure: every take in step agrees, and
               the model differs from them
      upset    an upset start: the model equals the majority, and some
               takes in step leave it (p: the share that leave)
      control  everything agrees (a sample of them, --control-rate)

    Each labelled sample is written with the model's state in that sample:
    the 15-bit value u1 before the shift to the I/O code (datasheet section
    8.4), its position within the output step, the excitation, energy and
    pitch, and for K1-K10 the value, its five Booth digits and the filter's
    b value it multiplies (section 8.8).

      python tools/departures.py corpus      # every PROM's words (takes 2-5)
      python tools/departures.py repeat      # the Phase 10 repeat test (takes 2-50)
      python tools/departures.py captures/corpus/starwars_speech   # one PROM

    Writes to <out>/<source>/: events.csv (one row per labelled sample;
    large, so kept local),
    words.csv (per word and path: the takes' agreement with each other and
    with the model, their loudness and the upsets' length, beside the
    frames' mean coded indices) and summary.json (the counts, the position
    of the model's value in the output step, the share of each K with a
    negative second Booth digit, and a logistic fit of each kind against
    the controls). It prints the summary.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import collections
import csv
import glob
import json
import os
import zlib
from multiprocessing import Pool

import numpy as np

import model as modellib
import regress
from regress import ROOT, STANDARD_FIRST_READ_SAMPLE

OUT = os.path.join(ROOT, "captures", "phase10", "departures")
SOURCES = {"corpus": os.path.join(ROOT, "captures", "corpus"),
           "repeat": os.path.join(ROOT, "captures", "phase10", "repeat")}
IN_STEP = 25            # samples of agreement before a take (or the model) is in step
QUORUM = 0.8            # share of the takes that must be in step for a sample to count
MERGE = 25              # differences closer than this belong to one upset


class TracedModel(modellib.Model):
    """The model, keeping its state at every run of the filter: one entry per
    sample period."""

    def lattice(self, excitation):
        value = super().lattice(excitation)
        self.trace.append((value, excitation, tuple(self.view[:12]), tuple(self.x)))
        return value


def modelRun(takeDir, previous, length, chipFirst):
    """The model's codes and state, aligned to the chip's sample indices as
    regress.modelAligned aligns them. Returns (codes, states), with None
    where the model did not run."""
    capture, _, _ = regress.loadChip(takeDir)
    external = capture["path"] == "external"
    manifest = regress.manifestFor(capture)
    start = modellib.STANDARD_START["external" if external else "vsm"]
    m = TracedModel(rng_at_speak=start["rng"])
    m.newIdx = [0] * 12             # the power-up state section 8.9 suggests
    m.trace = []
    for address in previous:
        m.speak(regress.framesAt(manifest, address))
        m.rng = start["rng"]
    m.trace = []
    out, p0 = m.speak(modellib.loadFrames(takeDir))
    if len(m.trace) != len(out):
        raise RuntimeError(f"{takeDir}: the filter ran {len(m.trace)} times in {len(out)} samples")
    shift = chipFirst + start["offset"] - p0          # chip index = model index + shift
    codes, states = [None] * length, [None] * length
    for i in range(max(0, shift), min(length, len(out) + shift)):
        codes[i], states[i] = out[i - shift], m.trace[i - shift]
    return codes, states


def stateRow(state):
    value, excitation, view, x = state
    row = {"u1": value, "step_pos": value & 15, "excitation": excitation,
           "energy": view[0], "pitch": view[1]}
    for j in range(10):
        k = view[2 + j]
        row[f"k{j + 1}"] = k
        row[f"k{j + 1}_booth"] = "".join(f"{d:+d}" for d in modellib.Model.boothDigits(k))
        row[f"k{j + 1}_d1neg"] = int(modellib.Model.boothDigits(k)[1] < 0)
        row[f"b{j + 1}"] = x[j]
    return row


def groups(roots):
    """(name, takes, previous) for every word and path with at least three
    standard takes: every take but the first, with the predecessors
    regress.takesOf gives the second."""
    out = []
    for root in roots:
        prevOf = dict(regress.takesOf(root, False))
        for wordDir in sorted(glob.glob(os.path.join(root, "*"))):
            if not os.path.isdir(wordDir) or os.path.basename(wordDir) == "sweep":
                continue
            for path in ("vsm", "ext"):
                takes = sorted(t for t in glob.glob(os.path.join(wordDir, path + "[0-9][0-9]")))
                takes = [t for t in takes[1:] if t in prevOf]
                if len(takes) >= 3:
                    name = os.path.relpath(wordDir, os.path.dirname(root)) + "/" + path
                    out.append((name, takes, prevOf[takes[0]]))
    return out


def upsetLengths(a, b):
    """Lengths of the stretches where two takes differ, merging differences
    less than MERGE samples apart."""
    runs = []
    for s in np.flatnonzero(a != b):
        if runs and s - runs[-1][1] < MERGE:
            runs[-1][1] = s
        else:
            runs.append([s, s])
    return [e - s + 1 for s, e in runs]


def sdr(power, noise):
    return min(99.0, 10 * np.log10(power / noise)) if noise else 99.0


def wordJob(job):
    name, takes, previous, controlRate = job
    data = []
    for t in takes:
        capture, codes, reads = regress.loadChip(t)
        external = capture["path"] == "external"
        data.append((codes, reads[0] if reads and not external else STANDARD_FIRST_READ_SAMPLE))
    n = min(len(c) - f for c, f in data)
    takesArr = np.array([c[f:f + n].astype(np.int64) for c, f in data])
    chipFirst = data[0][1]
    model, states = modelRun(takes[0], previous, len(data[0][0]), chipFirst)
    model, states = model[chipFirst:chipFirst + n], states[chipFirst:chipFirst + n]
    idle = takesArr[0, -1]
    modelArr = np.array([idle if c is None else c for c in model], dtype=np.int64)
    active = np.flatnonzero((takesArr != idle).any(0) | (modelArr != idle))
    end = min(n, active[-1] + 6) if len(active) else n

    mode = np.array([np.bincount(col - col.min()).argmax() + col.min() for col in takesArr.T])
    inStep = np.zeros(len(takes), int)
    modelInStep = 0
    seed = zlib.crc32(name.encode())
    events = []
    for j in range(end):
        ok = inStep >= IN_STEP
        if ok.sum() >= QUORUM * len(takes) and modelInStep >= IN_STEP and states[j] is not None:
            leave = ok & (takesArr[:, j] != mode[j])
            kind, p, direction = None, 0.0, 0
            if not leave.any() and model[j] != mode[j]:
                kind, direction = "determ", int(mode[j] - model[j])
            elif leave.any() and model[j] == mode[j]:
                kind, p = "upset", leave.sum() / ok.sum()
                direction = int(np.sign(np.mean(takesArr[leave, j]) - mode[j]))
            elif not leave.any() and zlib.crc32(j.to_bytes(4, "little"), seed) % 10000 < controlRate * 10000:
                kind = "control"
            if kind:
                events.append({"word": name, "sample": j, "kind": kind, "p": round(p, 3),
                               "in_step": int(ok.sum()), "dir": direction, **stateRow(states[j])})
        inStep = np.where(takesArr[:, j] == mode[j], inStep + 1, 0)
        modelInStep = modelInStep + 1 if (model[j] is not None and model[j] == mode[j]) else 0

    a, b = takesArr[0, :end], takesArr[1, :end]
    power = float(np.sum((a - idle).astype(float) ** 2))
    lengths = upsetLengths(a, b)
    with open(os.path.join(takes[0], "frames.csv")) as f:
        frames = list(csv.DictReader(f))
    voiced = [r for r in frames if r["kind"] == "voiced"]
    word = {"word": name, "takes": len(takes), "samples": int(end),
            "rms": round(float(np.sqrt(power / end)), 2) if end else 0.0,
            "take_sdr_db": round(sdr(power, float(np.sum((b - a).astype(float) ** 2))), 2),
            "model_sdr_db": round(sdr(power, float(np.sum((modelArr[:end] - a).astype(float) ** 2))), 2),
            "upsets": len(lengths), "upset_length_p50": float(np.median(lengths)) if lengths else "",
            "determ": sum(e["kind"] == "determ" for e in events),
            "upset_starts": sum(e["kind"] == "upset" for e in events),
            "frames": len(frames)}
    for kind in ("voiced", "unvoiced", "silence", "repeat"):
        word[f"{kind}_share"] = round(sum(r["kind"] == kind for r in frames) / max(1, len(frames)), 3)
    for field in ["energy", "pitch"] + [f"k{i}" for i in range(1, 11)]:
        word[f"mean_{field}"] = round(float(np.mean([int(r[field]) for r in voiced])), 2) if voiced else ""
    return events, word


def logistic(X, y, iterations=30, ridge=1e-3):
    """Newton's method; returns the weights and their standard errors."""
    w = np.zeros(X.shape[1])
    for _ in range(iterations):
        p = 1 / (1 + np.exp(-X @ w))
        H = (X * (p * (1 - p))[:, None]).T @ X + ridge * np.eye(len(w))
        w -= np.linalg.solve(H, X.T @ (p - y) + ridge * w)
    return w, np.sqrt(np.diag(np.linalg.inv(H)))


FIT_FEATURES = (["voiced", "step_pos 1|15", "step_pos 3|13", "step_pos 5|11"]
                + [f"k{i}_d1neg" for i in range(1, 11)])


def fitRow(e):
    return ([1.0, float(e["excitation"] not in (64, -64)), float(e["step_pos"] in (1, 15)),
             float(e["step_pos"] in (3, 13)), float(e["step_pos"] in (5, 11))]
            + [float(e[f"k{i}_d1neg"]) for i in range(1, 11)])


def summarise(events, words):
    byKind = collections.defaultdict(list)
    for e in events:
        byKind[e["kind"]].append(e)
    out = {"words": len(words), "counts": {k: len(v) for k, v in sorted(byKind.items())}}
    # where the model's value lies within its output step, by kind and direction
    pos = {}
    for kind, es in sorted(byKind.items()):
        for d in sorted({e["dir"] for e in es}):
            sel = [e for e in es if e["dir"] == d]
            c = collections.Counter(e["step_pos"] for e in sel)
            pos[f"{kind} {d:+d}"] = {"n": len(sel), "pct": [round(100 * c[i] / len(sel), 1) for i in range(16)]}
    out["step_pos_pct"] = pos
    out["d1neg_share"] = {k: [round(float(np.mean([e[f"k{i}_d1neg"] for e in es])), 3) for i in range(1, 11)]
                          for k, es in sorted(byKind.items())}
    out["voiced_share"] = {k: round(float(np.mean([e["excitation"] not in (64, -64) for e in es])), 3)
                           for k, es in sorted(byKind.items())}
    ups = byKind.get("upset", [])
    if ups:
        out["upset_p_p10_p50_p90"] = [round(float(v), 3) for v in np.percentile([e["p"] for e in ups], [10, 50, 90])]
    fits = {}
    controls = byKind.get("control", [])
    for kind in ("determ", "upset"):
        pos_ = byKind.get(kind, [])
        if len(pos_) < 20 or len(controls) < 20:
            continue
        X = np.array([fitRow(e) for e in controls + pos_])
        y = np.array([0.0] * len(controls) + [1.0] * len(pos_))
        w, se = logistic(X, y)
        fits[kind] = {name: {"log_odds": round(float(wi), 2), "z": round(float(wi / s), 1)}
                      for name, wi, s in zip(["const"] + FIT_FEATURES, w, se)}
    out["logistic_vs_control"] = fits
    return out


def printSummary(source, s):
    print(f"{source}: {s['words']} word groups; " + ", ".join(f"{k} {v}" for k, v in s["counts"].items()))
    print("  the model's value within its output step (u1 & 15, odd positions), % by kind and direction (n >= 20):")
    for key, v in s["step_pos_pct"].items():
        if v["n"] < 20:
            continue
        print(f"    {key:10s} n={v['n']:6d}: " + " ".join(f"{v['pct'][i]:5.1f}" for i in range(1, 16, 2)))
    print("  share of samples with K1..K10's second Booth digit negative:")
    for k, v in s["d1neg_share"].items():
        print(f"    {k:8s} " + " ".join(f"{x:.2f}" for x in v))
    if "upset_p_p10_p50_p90" in s:
        print("  upset chance at its sample (p10/p50/p90):", s["upset_p_p10_p50_p90"])
    for kind, fit in s["logistic_vs_control"].items():
        print(f"  {kind} against control, log-odds (z): " +
              "  ".join(f"{n} {v['log_odds']:+.2f}({v['z']:+.0f})" for n, v in fit.items()))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("what", nargs="+", help="'corpus', 'repeat', or capture roots (one directory per PROM)")
    parser.add_argument("--out", default=OUT)
    parser.add_argument("--control-rate", type=float, default=0.03, help="share of agreeing samples kept as controls")
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    args = parser.parse_args()
    for what in args.what:
        if what in SOURCES:
            source = what
            roots = sorted(os.path.dirname(p) for p in glob.glob(os.path.join(SOURCES[what], "*", "takes.csv")))
        else:
            source = os.path.basename(os.path.normpath(what))
            roots = [what]
        todo = [(name, takes, previous, args.control_rate) for name, takes, previous in groups(roots)]
        with Pool(args.jobs) as p:
            results = p.map(wordJob, todo, chunksize=2)
        events = [e for es, _ in results for e in es]
        words = [w for _, w in results]
        out = os.path.join(args.out, source)
        os.makedirs(out, exist_ok=True)
        for fileName, rows in (("events.csv", events), ("words.csv", words)):
            with open(os.path.join(out, fileName), "w", newline="") as f:
                w = csv.DictWriter(f, fieldnames=list(rows[0]) if rows else ["word"])
                w.writeheader()
                w.writerows(rows)
        summary = summarise(events, words)
        with open(os.path.join(out, "summary.json"), "w") as f:
            json.dump(summary, f, indent=1)
        printSummary(source, summary)


if __name__ == "__main__":
    main()
