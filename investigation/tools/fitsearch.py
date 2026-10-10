#!/usr/bin/env python3
"""
    fitsearch.py

    The first-departure search of docs/technical/die-analysis/lowest-order-arithmetic.md
    section 2.2, at scale. The model (tools/model.py) is exact up to a take's
    first departure from the chip, so the operands of every product before
    it are known. For each departure, every single correction of one
    product (+-1, +-2, in the departure's sample or the BACK samples before
    it) is tried, and the one that keeps the model matched to the chip for
    longest is applied; then the next departure is searched, and so on.

    It is fast because the filter's inputs (the energy, the excitation and
    the coefficients both chains see) do not depend on the filter's state.
    The model is run once per take to record them, and from then on only
    the filter is replayed: its products come from a table of the
    multiplier (datasheet section 8.8) for every K and M, and all the
    candidate corrections of a departure are replayed together as one
    numpy batch.

      python tools/fitsearch.py vectors      # every vector take, chips 1 and 4, and chip 1's repeat
      python tools/fitsearch.py corpus       # every PROM word: the takes' majority (takes 2-5)
      python tools/fitsearch.py captures/vectors/vectors_k   # one set
      python tools/fitsearch.py --check vectors    # check the replay against model.py only

    Writes <out>/<source>/: departures.jsonl (one line per departure: the
    correction chosen, the run it gives, and every correction that ties
    with it), and runs/<name>.npz per take or word, kept local: the
    filter's inputs, its state before every sample with the corrections
    applied, the chip's codes on the same index, and the corrections. The
    table fit reads those (tools/tablefit.py).

    Products are numbered as the model computes them: 0 is E x u11, 1-10
    are K10 x b10 ... K1 x b1 (giving u10 ... u1), 11-19 are K9 x u9 ...
    K1 x u1 (giving b10 ... b2).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import array
import glob
import json
import os
from multiprocessing import Pool

import numpy as np

import model as modellib
import regress
import departures
from regress import ROOT, STANDARD_FIRST_READ_SAMPLE

OUT = os.path.join(ROOT, "local", "fitsearch")
VECTOR_ROOTS = [os.path.join(ROOT, "captures", d) for d in ("vectors", "vectors_chip4", "vectors_chip1_repeat")]
NAMES = (["E*x"] + [f"K{i}*b{i}" for i in range(10, 0, -1)]
         + [f"K{i}*u{i}" for i in range(9, 0, -1)])
DELTAS = (-1, 1, -2, 2)
UNKNOWN = -9999         # a chip sample with no code to compare (or no majority)


def productTable():
    """The multiplier's product for every 10-bit K and 14-bit M, indexed
    [K & 1023, M & 16383] (Model.boothArray, vectorised)."""
    m = np.arange(16384, dtype=np.int64)
    m = np.where(m >= 8192, m - 16384, m)
    table = np.empty((1024, 16384), dtype=np.int32)
    for k in range(1024):
        digits = modellib.Model.boothDigits(k)
        d = digits[0]
        acc = (d * m if d >= 0 else ~(-d * m)) >> 2
        acc = acc + digits[1] * m
        for stage in (2, 3, 4):
            acc = (acc >> 2) + digits[stage] * m
        table[k] = (acc >> 1) | 1
    return table


TABLE = productTable()
FLAT = array.array("i", TABLE.tobytes())     # fast scalar lookups: FLAT[(k & 1023) << 14 | (m & 16383)]


# ---- recording a take ---------------------------------------------------

class InputModel(modellib.Model):
    """The model, recording the filter's inputs in every sample period:
    energy, excitation, K1-K10 as the u chain sees them, K1-K10 as the b
    chain sees them."""

    def lattice(self, excitation):
        self.inputs.append([self.view[0], excitation] + self.view[2:12] + self.view[14:24])
        if len(self.inputs) == 1:
            self.state0 = list(self.u) + list(self.x)
        return super().lattice(excitation)


def recordTake(takeDir, previous=None):
    """The model's run of a take: (inputs, state0, out, shift, chipFirst),
    with chip index = model index + shift. previous=None models the take
    alone, as model.compareTake does (the vector sets); a list of PROM
    addresses speaks them first, as departures.modelRun does (the corpus)."""
    capture, _, reads = regress.loadChip(takeDir)
    external = capture["path"] == "external"
    start = modellib.STANDARD_START["external" if external else "vsm"]
    chipFirst = reads[0] if reads and not external else STANDARD_FIRST_READ_SAMPLE
    m = InputModel(rng_at_speak=start["rng"])
    m.inputs = []
    if previous is not None:
        m.newIdx = [0] * 12
        manifest = regress.manifestFor(capture)
        for address in previous:
            m.speak(regress.framesAt(manifest, address))
            m.rng = start["rng"]
    m.inputs = []
    out, p0 = m.speak(modellib.loadFrames(takeDir))
    inputs = np.array(m.inputs, dtype=np.int32)
    if len(inputs) != len(out):
        raise RuntimeError(f"{takeDir}: the filter ran {len(inputs)} times in {len(out)} samples")
    return inputs, np.array(m.state0, dtype=np.int32), out, chipFirst + start["offset"] - p0, chipFirst


def alignChip(codes, n, shift):
    chip = np.full(n, UNKNOWN, dtype=np.int32)
    lo, hi = max(0, -shift), min(n, len(codes) - shift)
    if hi > lo:
        chip[lo:hi] = codes[lo + shift:hi + shift]
    return chip


# ---- replaying the filter -----------------------------------------------

def wrap14(v):
    return ((v + 8192) & 16383) - 8192


def replay(inputs, state, start, end, patches):
    """Replay the filter from `state` (u0-u10, x0-x9, before sample
    `start`) to `end`, with patches {(sample, product): delta}. Returns
    (out, states): the code of each sample and the state before it."""
    T = FLAT
    u = list(state[:11])
    x = list(state[11:])
    out = np.empty(end - start, dtype=np.int32)
    states = np.empty((end - start, 21), dtype=np.int32)
    for t in range(start, end):
        states[t - start, :11] = u
        states[t - start, 11:] = x
        row = inputs[t].tolist()
        e, exc, k, kx = row[0], row[1], row[2:12], row[12:22]
        pt = {j: d for (s, j), d in patches.items() if s == t} if patches else None
        p = T[((e & 1023) << 14) | ((exc << 6) & 16383)]
        if pt:
            p += pt.get(0, 0)
        u[10] = wrap14(p)
        for i in range(9, -1, -1):
            p = T[((k[i] & 1023) << 14) | (x[i] & 16383)]
            if pt:
                p += pt.get(10 - i, 0)
            u[i] = wrap14(u[i + 1] - p)
        for i in range(9, 0, -1):
            p = T[((kx[i - 1] & 1023) << 14) | (u[i - 1] & 16383)]
            if pt:
                p += pt.get(20 - i, 0)
            x[i] = wrap14(x[i - 1] + p)
        x[0] = u[0]
        out[t - start] = u[0] >> 4
    return out, states


def replayBatch(inputs, state, start, end, product, delta, chip):
    """Replay B candidates from the same state at `start`, candidate b
    adding delta[b] to product[b] in sample `start`. Stops each candidate
    at its first disagreement with the chip (UNKNOWN agrees). Returns the
    number of samples each candidate agreed for, from `start`."""
    B = len(product)
    u = np.repeat(state[None, :11].astype(np.int64), B, 0)
    x = np.repeat(state[None, 11:].astype(np.int64), B, 0)
    patch = np.zeros((B, 20), dtype=np.int64)
    patch[np.arange(B), product] = delta
    run = np.zeros(B, dtype=np.int64)
    alive = np.ones(B, dtype=bool)
    for t in range(start, end):
        e, exc = int(inputs[t, 0]), int(inputs[t, 1])
        k, kx = inputs[t, 2:12], inputs[t, 12:22]
        first = t == start
        p = int(TABLE[e & 1023, (exc << 6) & 16383])
        u[:, 10] = wrap14(p + patch[:, 0]) if first else wrap14(p)
        for i in range(9, -1, -1):
            p = TABLE[k[i] & 1023, x[:, i] & 16383]
            if first:
                p = p + patch[:, 10 - i]
            u[:, i] = wrap14(u[:, i + 1] - p)
        for i in range(9, 0, -1):
            p = TABLE[kx[i - 1] & 1023, u[:, i - 1] & 16383]
            if first:
                p = p + patch[:, 20 - i]
            x[:, i] = wrap14(x[:, i - 1] + p)
        x[:, 0] = u[:, 0]
        c = chip[t]
        if c != UNKNOWN:
            alive &= (u[:, 0] >> 4) == c
        if not alive.any():
            break
        run += alive
    return run


def allOps(inputs, states):
    """K and M of every product in every sample but the last, as two (n-1, 20)
    arrays, from the states before each sample (the b chain's products take
    their u from the state after it)."""
    n = len(states) - 1
    Kx = np.zeros((n, 20), np.int64)
    Mx = np.zeros((n, 20), np.int64)
    Kx[:, 0] = inputs[:n, 0]
    Mx[:, 0] = inputs[:n, 1].astype(np.int64) << 6
    for i in range(9, -1, -1):
        Kx[:, 10 - i] = inputs[:n, 2 + i]
        Mx[:, 10 - i] = states[:n, 11 + i]
    for i in range(9, 0, -1):
        Kx[:, 20 - i] = inputs[:n, 12 + i - 1]
        Mx[:, 20 - i] = states[1:n + 1, i - 1]
    return Kx, Mx


# ---- the search ---------------------------------------------------------

def search(inputs, state0, chip, back, horizon, maxDepartures, minGain):
    """The iterated first-departure search on one take. Returns
    (departures, patches, states, out)."""
    n = len(inputs)
    patches = {}
    out, states = replay(inputs, state0, 0, n, patches)
    found = []
    known = chip != UNKNOWN
    cand = [(j, d) for j in range(20) for d in DELTAS]
    while len(found) < maxDepartures:
        bad = np.flatnonzero(known & (out != chip))
        if not len(bad):
            break
        nd = int(bad[0])
        end = min(n, nd + horizon + 1)
        rows = []
        for s in range(max(0, nd - back), nd + 1):
            product = np.array([j for j, _ in cand])
            delta = np.array([d for _, d in cand])
            run = replayBatch(inputs, states[s], s, end, product, delta, chip)
            for (j, d), r in zip(cand, run.tolist()):
                rows.append((r - (nd - s), s, j, d))
        top = max(r[0] for r in rows)
        ties = [r for r in rows if r[0] == top]
        # prefer a +-1, then the latest sample
        ties.sort(key=lambda r: (abs(r[3]), -r[1], r[2]))
        gain, s, j, d = ties[0]
        rec = {"departure": nd, "chip": int(chip[nd]), "model": int(out[nd]), "gain": int(gain),
               "horizon": int(end - nd), "sample": s, "product": j, "name": NAMES[j], "delta": d,
               "best1": max((r[0] for r in rows if abs(r[3]) == 1), default=-1),
               "ties": [[r[1], r[2], r[3]] for r in ties]}
        found.append(rec)
        if gain < minGain:
            rec["applied"] = False      # no single correction explains it: stop the take here
            break
        rec["applied"] = True
        patches[(s, j)] = d
        o2, s2 = replay(inputs, states[s], s, n, patches)
        out[s:], states[s:] = o2, s2
    return found, patches, states, out


# ---- sources ------------------------------------------------------------

def vectorJobs(roots):
    jobs = []
    for root in roots:
        for idx in sorted(glob.glob(os.path.join(root, "**", "takes.csv"), recursive=True)):
            base = os.path.dirname(idx)
            for cap in sorted(glob.glob(os.path.join(base, "*", "*", "capture.json"))):
                take = os.path.dirname(cap)
                with open(cap) as f:
                    capture = json.load(f)
                if capture.get("procedure", {}).get("start") != "standard":
                    continue
                name = os.path.relpath(take, ROOT).replace("/", "__")
                jobs.append({"name": name, "takes": [take], "previous": None})
    return jobs


def corpusJobs(roots):
    return [{"name": name.replace("/", "__"), "takes": takes, "previous": previous}
            for name, takes, previous in departures.groups(roots)]


def chipCodes(job, n, shift, chipFirst):
    """The chip's codes on the model's index: one take's, or for a word
    the per-sample majority of its takes (UNKNOWN where there is none)."""
    if len(job["takes"]) == 1:
        _, codes, _ = regress.loadChip(job["takes"][0])
        return alignChip(codes, n, shift)
    data = []
    for t in job["takes"]:
        capture, codes, reads = regress.loadChip(t)
        external = capture["path"] == "external"
        data.append((codes, reads[0] if reads and not external else STANDARD_FIRST_READ_SAMPLE))
    m = min(len(c) - f for c, f in data)
    arr = np.array([c[f:f + m] for c, f in data])
    mode = np.full(m, UNKNOWN, dtype=np.int64)
    for j in range(m):
        vals, counts = np.unique(arr[:, j], return_counts=True)
        if counts.max() * 2 > len(arr):
            mode[j] = vals[counts.argmax()]
    full = np.full(chipFirst + m, UNKNOWN, dtype=np.int64)
    full[chipFirst:] = mode
    return alignChip(full, n, shift)


def runJob(job):
    o = job["opts"]
    inputs, state0, modelOut, shift, chipFirst = recordTake(job["takes"][0], job["previous"])
    chip = chipCodes(job, len(inputs), shift, chipFirst)
    if o["check"]:
        out, _ = replay(inputs, state0, 0, len(inputs), {})
        return job["name"], {"check": bool(np.array_equal(out, np.asarray(modelOut))),
                             "mismatched": int(np.count_nonzero((chip != UNKNOWN) & (out != chip)))}
    found, patches, states, out = search(inputs, state0, chip, o["back"], o["horizon"],
                                         o["max_departures"], o["min_gain"])
    np.savez_compressed(os.path.join(o["runs"], job["name"] + ".npz"), inputs=inputs, states=states,
                        chip=chip, out=out,
                        patches=np.array([[s, j, d] for (s, j), d in sorted(patches.items())], dtype=np.int32).reshape(-1, 3))
    left = int(np.count_nonzero((chip != UNKNOWN) & (out != chip)))
    return job["name"], {"departures": found, "left": left, "takes": job["takes"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("what", nargs="+", help="'vectors', 'corpus', or capture roots")
    parser.add_argument("--out", default=OUT)
    parser.add_argument("--back", type=int, default=3, help="samples before the departure to try")
    parser.add_argument("--horizon", type=int, default=60, help="samples after the departure to score")
    parser.add_argument("--max-departures", type=int, default=40, help="per take or word")
    parser.add_argument("--min-gain", type=int, default=10, help="a correction must match this long")
    parser.add_argument("--check", action="store_true", help="only check the replay against the model")
    parser.add_argument("--limit", type=int, help="only the first N takes or words")
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    args = parser.parse_args()
    for what in args.what:
        if what == "vectors":
            source, jobs = "vectors", vectorJobs(VECTOR_ROOTS)
        elif what == "corpus":
            roots = sorted(os.path.dirname(p) for p in glob.glob(os.path.join(departures.SOURCES["corpus"], "*", "takes.csv")))
            source, jobs = "corpus", corpusJobs(roots)
        else:
            source = os.path.basename(os.path.normpath(what))
            # the vector sets are standalone takes; anything else is spoken as PROM words
            vectorLike = os.path.basename(os.path.normpath(what)).startswith("vectors_")
            jobs = vectorJobs([what]) if vectorLike else corpusJobs([what])
        jobs = jobs[:args.limit] if args.limit else jobs
        out = os.path.join(args.out, source)
        runs = os.path.join(out, "runs")
        os.makedirs(runs, exist_ok=True)
        opts = {"back": args.back, "horizon": args.horizon, "max_departures": args.max_departures,
                "min_gain": args.min_gain, "check": args.check, "runs": runs}
        for j in jobs:
            j["opts"] = opts
        with Pool(args.jobs) as p:
            results = p.map(runJob, jobs, chunksize=1)
        if args.check:
            bad = [n for n, r in results if not r["check"]]
            print(f"{source}: {len(results)} takes, replay equals the model on {len(results) - len(bad)}; "
                  f"{sum(r['mismatched'] for _, r in results)} samples differ from the chip")
            for n in bad[:20]:
                print("  differs:", n)
            continue
        nDep = nApplied = nLeft = 0
        with open(os.path.join(out, "departures.jsonl"), "w") as f:
            for name, r in results:
                for d in r["departures"]:
                    f.write(json.dumps({"run": name, **d}) + "\n")
                    nDep += 1
                    nApplied += d["applied"]
                nLeft += r["left"]
        print(f"{source}: {len(results)} takes or words, {nDep} departures searched, "
              f"{nApplied} explained by one correction; {nLeft} samples still differ")


if __name__ == "__main__":
    main()
