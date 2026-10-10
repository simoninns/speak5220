#!/usr/bin/env python3
"""
    tablefit.py

    Fits single entries of the multiplier's product table to the chip, from
    the runs tools/fitsearch.py saved (docs/technical/die-analysis/lowest-order-arithmetic.md
    section 2.3). It asks whether the one-code error is a fixed function of
    the product's operands, K and M: if it were, changing the right entries
    would improve runs it was not fitted on.

    Each round: every fit run is replayed with the current table to its
    first departure from the chip, and the single corrections (K, M, delta)
    that keep it matched longest there are the candidates. A candidate is
    scored first on the runs it may fix (the gain in the first departure's
    index), then on every other run that uses the entry before its first
    departure, stopping once the loss exceeds the gain. The best few with
    disjoint runs are applied. The held-out runs are scored each round and
    never fitted.

      python tools/tablefit.py --fit vectors --hold corpus --rounds 15

    Writes <out>/tablefit_<fit>.json: each round's scores and the entries
    added. Prints the same.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import collections
import glob
import json
import os
import time
from multiprocessing import Pool

import numpy as np

import fitsearch as F

MINFIX = 4          # a candidate must be tied at this many first departures
PER_ROUND = 4       # entries applied per round
RUNS = []           # per run: path, inputs (list), inputs, state0, chip (list), chip
STATE = []          # per run, with the current table: first departure, states, entries used and where first, ties


def signed(key):
    k, m = key >> 14, key & 16383
    return k - 1024 if k >= 512 else k, m - 16384 if m >= 8192 else m


def firstMismatch(inp, state, start, ch):
    """Replay from `state` (before sample `start`) with the current table:
    the first sample that differs from the chip, or the run's length."""
    T = F.FLAT
    u, x = list(state[:11]), list(state[11:])
    w = F.wrap14
    for t in range(start, len(inp)):
        row = inp[t]
        u[10] = w(T[((row[0] & 1023) << 14) | ((row[1] << 6) & 16383)])
        for i in range(9, -1, -1):
            u[i] = w(u[i + 1] - T[((row[2 + i] & 1023) << 14) | (x[i] & 16383)])
        for i in range(9, 0, -1):
            x[i] = w(x[i - 1] + T[((row[11 + i] & 1023) << 14) | (u[i - 1] & 16383)])
        x[0] = u[0]
        c = ch[t]
        if c != F.UNKNOWN and (u[0] >> 4) != c:
            return t
    return len(inp)


def load(path):
    z = np.load(path)
    return path, z["inputs"].tolist(), z["inputs"], z["states"][0], z["chip"].tolist(), z["chip"]


def applyTable(patch):
    for k, v in patch.items():
        F.FLAT[k] = v
        F.TABLE[k >> 14, k & 16383] = v


def baseOf(r):
    """First departure, states, entry first-use, and the first departure's
    tied corrections, with the current table."""
    path, inp, inputs, s0, ch, chip = RUNS[r]
    out, st = F.replay(inputs, s0, 0, len(inputs), {})
    known = chip != F.UNKNOWN
    bad = np.flatnonzero(known & (out != chip))
    f = int(bad[0]) if len(bad) else len(chip)
    Kx, Mx = F.allOps(inputs, np.vstack([st, st[-1:]]))
    key = (((Kx & 1023) << 14) | (Mx & 16383))
    uk, ui = np.unique(key.ravel(), return_index=True)
    ties = []
    if f < len(chip):
        end = min(len(inputs), f + 61)
        cand = [(j, d) for j in range(20) for d in F.DELTAS]
        rows = []
        for s in range(max(0, f - 3), f + 1):
            run = F.replayBatch(inputs, st[s], s, end, np.array([j for j, _ in cand]),
                                np.array([d for _, d in cand]), chip)
            rows += [(rr - (f - s), s, j, d) for (j, d), rr in zip(cand, run.tolist())]
        top = max(x[0] for x in rows)
        if top >= 10:
            ties = sorted({(int(key[s, j]), d) for g, s, j, d in rows if g == top})
    return f, st, uk, (ui // 20).astype(np.int32), ties


def evalOn(r, k, d):
    """(first departure, first departure with entry k changed by d) for run r."""
    f, st, uk, ui, _ = STATE[r]
    i = np.searchsorted(uk, k)
    if i >= len(uk) or uk[i] != k or ui[i] > f:
        return f, f
    old = F.FLAT[k]
    F.FLAT[k] = old + d
    nf = firstMismatch(RUNS[r][1], st[ui[i]], int(ui[i]), RUNS[r][4])
    F.FLAT[k] = old
    return f, nf


def stage1(args):
    c, runs = args
    return c, sum(nf - f for f, nf in (evalOn(r, *c) for r in runs))


def stage2(args):
    c, gain, users, fixed = args
    loss = worse = 0
    for r in users:
        if r in fixed:
            continue
        f, nf = evalOn(r, *c)
        loss += f - nf
        worse += nf < f
        if loss >= gain:
            return c, gain, loss, worse, False
    return c, gain, loss, worse, True


def holdScore(r):
    path, inp, inputs, s0, ch, chip = RUNS[r]
    out, _ = F.replay(inputs, s0, 0, len(inputs), {})
    known = chip != F.UNKNOWN
    bad = np.flatnonzero(known & (out != chip))
    return (int(bad[0]) if len(bad) else len(chip)), len(bad)


def main():
    global RUNS, STATE
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("--fit", default="vectors", help="fitsearch source to fit")
    parser.add_argument("--hold", default="corpus", help="fitsearch source to score only")
    parser.add_argument("--rounds", type=int, default=15)
    parser.add_argument("--out", default=F.OUT)
    args = parser.parse_args()
    ROUNDS = args.rounds
    OUTF = os.path.join(args.out, f"tablefit_{args.fit}.json")
    t0 = time.time()
    fitPaths = sorted(glob.glob(os.path.join(args.out, args.fit, "runs", "*.npz")))
    holdPaths = sorted(glob.glob(os.path.join(args.out, args.hold, "runs", "*.npz")))
    with Pool(16) as p:
        RUNS = p.map(load, fitPaths + holdPaths, chunksize=8)
    nFit = len(fitPaths)
    patch = {}
    log = []
    for rnd in range(ROUNDS + 1):
        applyTable(patch)
        with Pool(16) as p:
            STATE = p.map(baseOf, range(nFit), chunksize=8)
            hold = p.map(holdScore, range(nFit, len(RUNS)), chunksize=8)
        fitFirst = sum(s[0] for s in STATE)
        fitExact = sum(s[0] == len(RUNS[i][5]) for i, s in enumerate(STATE))
        hf, hd = sum(h[0] for h in hold), sum(h[1] for h in hold)
        print(f"round {rnd}: {len(patch)} entries; fit first-sum {fitFirst} exact {fitExact}; "
              f"hold first-sum {hf} differ {hd}  ({round(time.time() - t0)} s)", flush=True)
        log.append({"round": rnd, "entries": len(patch), "fit_first": fitFirst, "fit_exact": int(fitExact),
                    "hold_first": hf, "hold_differ": hd})
        if rnd == ROUNDS:
            break
        fix = collections.defaultdict(set)
        for r, s in enumerate(STATE):
            for c in s[4]:
                fix[c].add(r)
        cands = [(c, sorted(rs)) for c, rs in fix.items() if len(rs) >= MINFIX]
        with Pool(16) as p:
            s1 = p.map(stage1, cands, chunksize=16)
        good = sorted([x for x in s1 if x[1] > 0], key=lambda x: -x[1])[:400]
        keys = np.array(sorted({c[0] for c, _ in good}))
        users = collections.defaultdict(list)
        for r, s in enumerate(STATE):
            uk, ui, f = s[2], s[3], s[0]
            m = np.isin(uk, keys)
            for k, i in zip(uk[m].tolist(), ui[m].tolist()):
                if i <= f:
                    users[k].append(r)
        fixedOf = dict(cands)
        with Pool(16) as p:
            s2 = p.map(stage2, [(c, g, users[c[0]], set(fixedOf[c])) for c, g in good], chunksize=1)
        ok = sorted([x for x in s2 if x[4]], key=lambda x: -(x[1] - x[2]))
        if not ok:
            print("no candidate left")
            break
        chosen, covered = [], set()
        for c, g, l, nl, _ in ok:
            if c[0] in patch or any(c[0] == x[0] for x, *_ in chosen):
                continue
            if covered & set(fixedOf[c]):
                continue
            chosen.append((c, g, l, nl))
            covered |= set(fixedOf[c])
            if len(chosen) >= PER_ROUND:
                break
        for (k, d), g, l, nl in chosen:
            K, M = signed(k)
            old = int(F.FLAT[k])
            patch[k] = old + d
            print(f"   K={K:5d} M={M:6d}: {old} -> {old + d}   net {g - l:+d} (worse on {nl})", flush=True)
            log[-1].setdefault("added", []).append([K, M, old, old + d, g - l])
        save(OUTF, log, patch)
    save(OUTF, log, patch)


def save(path, log, patch):
    with open(path, "w") as f:
        json.dump({"log": log, "entries": [[*signed(k), v] for k, v in patch.items()]}, f, indent=1)


if __name__ == "__main__":
    main()
