#!/usr/bin/env python3
"""
    edge_timing.py

    Measures the timing relationships between the TMS5220 output signals in a
    capture directory written by capture.py, ideally taken with a fast
    timebase (--timebase 3, 8 ns). All times are relative to the T11 rising
    edge that starts each sample period. Writes <capture>/edge_timing.json
    and, with --plot, edge_timing.png.

      T11     period, high time, rise and fall times, levels
      I/O     every edge, as bit slot boundary + offset, rising and falling
      DAC     when each step starts and settles
      ROMCLK  frequency, duty and phase (only if probe D is on TP1)

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os

import numpy as np

import tms5220_decode as td

PROBE_GAIN = {"a": 1, "b": 10, "c": 10, "d": 10}


def crossings(v, level, interval):
    """Interpolated crossing times (seconds) of level: (rising, falling)."""
    above = v > level
    idx = np.flatnonzero(above[1:] != above[:-1])
    frac = (level - v[idx]) / (v[idx + 1] - v[idx])
    t = (idx + frac) * interval
    rising = above[idx + 1]
    return t[rising], t[~rising]


def levels(v):
    """Low and high logic levels (5th and 95th percentile medians)."""
    lo, hi = np.percentile(v, [5, 95])
    mid = (lo + hi) / 2
    return float(np.median(v[v < mid])), float(np.median(v[v > mid]))


def transitionTimes(v, interval, lo, hi, rising):
    """10-90% transition times (seconds) of each edge."""
    l10, l90 = lo + 0.1 * (hi - lo), lo + 0.9 * (hi - lo)
    r10, f10 = crossings(v, l10, interval)
    r90, f90 = crossings(v, l90, interval)
    if rising:
        start, end = r10, r90
    else:
        start, end = f90, f10
    j = np.searchsorted(end, start)
    ok = j < len(end)
    dt = end[j[ok]] - start[ok]
    return dt[dt < 1e-6]


def stats(x, scale=1e9):
    x = np.asarray(x) * scale
    if len(x) == 0:
        return None
    return {"n": int(len(x)), "mean": float(x.mean()), "std": float(x.std()),
            "min": float(x.min()), "max": float(x.max())}


def relative(t, t11):
    """Times relative to the latest preceding T11 rising edge."""
    j = np.searchsorted(t11, t, side="right") - 1
    ok = (j >= 0) & (j < len(t11) - 1)
    return t[ok] - t11[j[ok]]


def slotOffsets(rel):
    """Split times into (nearest bit slot boundary, offset from it)."""
    slot = np.rint(rel / td.SLOT).astype(int)
    return slot, rel - slot * td.SLOT


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("capture", help="capture directory")
    parser.add_argument("--plot", action="store_true", help="also write edge_timing.png")
    args = parser.parse_args()

    with open(os.path.join(args.capture, "capture.json")) as f:
        meta = json.load(f)
    interval, volts = td.loadRaw(os.path.join(args.capture, "raw", "scope_raw.npz"))
    tip = {ch: volts[i] * PROBE_GAIN[ch] for i, ch in enumerate("abcd")}
    dIsRomclk = meta["channels"]["d"]["signal"].startswith("ROMCLK")
    result = {"capture": os.path.basename(os.path.normpath(args.capture)),
              "interval_ns": interval * 1e9}

    # T11
    lo, hi = levels(tip["b"])
    t11, t11Fall = crossings(tip["b"], (lo + hi) / 2, interval)
    t11Fall = t11Fall[t11Fall > t11[0]]
    n = min(len(t11), len(t11Fall))
    result["t11"] = {
        "low_v": lo, "high_v": hi,
        "period_us": stats(np.diff(t11), 1e6),
        "high_ns": stats(t11Fall[:n] - t11[:n]),
        "rise_10_90_ns": stats(transitionTimes(tip["b"], interval, lo, hi, True)),
        "fall_90_10_ns": stats(transitionTimes(tip["b"], interval, lo, hi, False)),
    }

    # I/O edges, by bit slot boundary
    lo, hi = levels(tip["c"])
    ioRise, ioFall = crossings(tip["c"], (lo + hi) / 2, interval)
    io = {"low_v": lo, "high_v": hi,
          "rise_10_90_ns": stats(transitionTimes(tip["c"], interval, lo, hi, True)),
          "fall_90_10_ns": stats(transitionTimes(tip["c"], interval, lo, hi, False))}
    for name, edges in (("rising", ioRise), ("falling", ioFall)):
        slot, offset = slotOffsets(relative(edges, t11))
        io[name] = {"slots_seen": sorted(set(slot.tolist())),
                    "offset_from_slot_boundary_ns": stats(offset)}
    result["io"] = io

    # DAC steps: average every large step (at least 20 codes), normalised
    # against the local levels just before and just after it, to get the
    # step response relative to the T11 rising edge
    d = td.decode(interval, volts)
    edges, code = d["edge"], d["code"]
    dac = tip["a"]
    pre, post = int(5e-6 / interval), int(20e-6 / interval)
    window = np.arange(-pre, post)
    tw = window * interval
    beforeMask = (tw > -4e-6) & (tw < -0.5e-6)
    afterMask = (tw > 12e-6) & (tw < 19e-6)
    steps = []
    for k in np.flatnonzero(np.abs(np.diff(code)) >= 20):
        e = edges[k + 1]
        if e - pre < 0 or e + post > len(dac):
            continue
        seg = dac[e + window]
        before, after = seg[beforeMask].mean(), seg[afterMask].mean()
        steps.append((seg - before) / (after - before))
    response = np.mean(steps, axis=0) if steps else None
    dacResult = {"steps_averaged": len(steps),
                 "max_code": int(code.max()), "min_code": int(code.min())}
    if response is not None:
        smooth = np.convolve(response, np.ones(9) / 9, mode="same")
        for frac in (0.1, 0.5, 0.9):
            i = np.flatnonzero((smooth > frac) & (tw > 0))
            dacResult[f"step_{int(frac * 100)}pct_after_t11_rise_ns"] = \
                float(tw[i[0]] * 1e9) if len(i) else None
    result["dac"] = dacResult
    np.save(os.path.join(args.capture, "dac_step_response.npy"),
            np.vstack([tw, response]) if response is not None else np.zeros((2, 0)))

    # ROMCLK
    if dIsRomclk:
        lo, hi = levels(tip["d"])
        rcRise, rcFall = crossings(tip["d"], (lo + hi) / 2, interval)
        period = np.diff(rcRise)
        n = min(len(rcRise), len(rcFall[rcFall > rcRise[0]]))
        result["romclk"] = {
            "low_v": lo, "high_v": hi,
            "frequency_hz": float(1 / period.mean()),
            "period_ns": stats(period),
            "high_ns": stats(rcFall[rcFall > rcRise[0]][:n] - rcRise[:n]),
            "rise_10_90_ns": stats(transitionTimes(tip["d"], interval, lo, hi, True)),
            "fall_90_10_ns": stats(transitionTimes(tip["d"], interval, lo, hi, False)),
        }
        # Each signal edge relative to the nearest ROMCLK rising and falling edge
        for label, t in (("t11_rising", t11), ("t11_falling", t11Fall),
                         ("io_rising", ioRise), ("io_falling", ioFall)):
            for rcName, rc in (("romclk_rising", rcRise), ("romclk_falling", rcFall)):
                j = np.clip(np.searchsorted(rc, t), 1, len(rc) - 1)
                nearest = np.where(np.abs(t - rc[j - 1]) < np.abs(t - rc[j]), rc[j - 1], rc[j])
                result["romclk"][f"{label}_minus_{rcName}_ns"] = stats(t - nearest)
        # Phase of ROMCLK within the sample period
        rel = relative(rcRise, t11)
        result["romclk"]["rising_after_t11_rise_mod_slot_ns"] = stats(rel % td.SLOT)

    out = os.path.join(args.capture, "edge_timing.json")
    with open(out, "w") as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))

    if args.plot:
        plot(args.capture, interval, tip, edges, code, dIsRomclk)


def plot(capture, interval, tip, edges, code, dIsRomclk):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # A sample period with several I/O transitions, preceded by a large DAC step
    busy = np.flatnonzero((np.abs(np.diff(code)) >= 20)[1:-2]) + 2
    k = int(busy[len(busy) // 2]) if len(busy) else len(edges) // 2
    e0 = edges[k] - int(10e-6 / interval)
    e1 = edges[k + 1] + int(10e-6 / interval)
    t = (np.arange(e0, e1) - edges[k]) * interval * 1e6
    names = [("a", "SPEAKER (TP2), V"), ("b", "T11, V"), ("c", "I/O, V"),
             ("d", "ROMCLK, V" if dIsRomclk else "SCOPE_TRIG, V")]
    fig, axes = plt.subplots(4, 1, sharex=True, figsize=(10, 7))
    for ax, (ch, label) in zip(axes, names):
        ax.plot(t, tip[ch][e0:e1], lw=0.8)
        ax.set_ylabel(label)
        for s in range(-1, 22):
            ax.axvline(s * td.SLOT * 1e6, color="0.85", lw=0.5, zorder=0)
    axes[2].set_title(f"code {code[k]} in slots 2-11 (LSB first)", fontsize=9)
    axes[-1].set_xlabel("time after T11 rising edge (us); grey lines = 6.25 us bit slots")
    fig.tight_layout()
    fig.savefig(os.path.join(capture, "edge_timing.png"), dpi=120)


if __name__ == "__main__":
    main()
