#!/usr/bin/env python3
"""
    dac_compare.py

    Compares the DAC tables (dac_table.csv from dac_transfer.py) measured
    with two different TP2 loads, to characterise the SPEAKER output as a
    current source. If the pin were ideal, both loads would give the same
    current for every code. Writes, to --out:

      dac_compare.json  current ratio between the loads, equivalent output
                        resistance, per-code currents
      dac_compare.png   (with --plot) current per code at both loads, and
                        their ratio

    A Norton source (current I0, output resistance Ro) gives a load current
    of I0 * Ro / (Ro + R), so the ratio of the two currents is the same for
    every code and Ro = (R1 - k R2) / (k - 1), where k = I(R2) / I(R1)
    and R2 < R1.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os

import numpy as np

MIN_COUNT = 20      # codes seen fewer times than this are left out of the fits


def loadTable(directory):
    with open(os.path.join(directory, "dac_transfer.json")) as f:
        meta = json.load(f)
    table = {}
    with open(os.path.join(directory, "dac_table.csv")) as f:
        for row in csv.DictReader(f):
            if int(row["count"]):
                table[int(row["code"])] = (int(row["count"]), float(row["mean_v"]))
    return meta["load_ohms"], table


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("high", help="dac_transfer.py output with the higher load resistance")
    parser.add_argument("low", help="dac_transfer.py output with the lower load resistance")
    parser.add_argument("--out", required=True, help="output directory")
    parser.add_argument("--plot", action="store_true", help="also write dac_compare.png")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)

    rHigh, high = loadTable(args.high)
    rLow, low = loadTable(args.low)
    codes = np.array(sorted(set(high) & set(low)))
    count = np.array([min(high[c][0], low[c][0]) for c in codes])
    iHigh = np.array([high[c][1] for c in codes]) / rHigh
    iLow = np.array([low[c][1] for c in codes]) / rLow

    # Current ratio per code, and the Norton output resistance it implies.
    # Codes near zero current are dominated by the offset uncertainty, so
    # the fit uses codes whose current is at least a quarter of full scale.
    ratio = iLow / iHigh
    use = (count >= MIN_COUNT) & (iHigh > 0.25 * iHigh.max())
    k = float(np.average(ratio[use], weights=count[use]))
    ro = (rHigh - k * rLow) / (k - 1)
    spread = float(ratio[use].max() - ratio[use].min())

    result = {
        "high": {"dir": os.path.normpath(args.high), "load_ohms": rHigh},
        "low": {"dir": os.path.normpath(args.low), "load_ohms": rLow},
        "codes_compared": int(use.sum()),
        "current_ratio_low_over_high": k,
        "current_ratio_range": [float(ratio[use].min()), float(ratio[use].max())],
        "current_ratio_spread": spread,
        "norton_output_resistance_ohms": float(ro),
        "codes": {int(c): {"count": int(n), "high_ua": float(a * 1e6), "low_ua": float(b * 1e6),
                           "ratio": float(r)}
                  for c, n, a, b, r in zip(codes, count, iHigh, iLow, ratio)},
    }
    with open(os.path.join(args.out, "dac_compare.json"), "w") as f:
        json.dump(result, f, indent=2)
    print(json.dumps({key: value for key, value in result.items() if key != "codes"}, indent=2))

    if args.plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, axes = plt.subplots(2, 1, figsize=(9, 7), sharex=True)
        axes[0].plot(codes, iHigh * 1e6, ".", ms=3, label=f"{rHigh:.0f} ohm load")
        axes[0].plot(codes, iLow * 1e6, ".", ms=3, label=f"{rLow:.0f} ohm load")
        axes[0].set_ylabel("SPEAKER current (uA)")
        axes[0].legend()
        well = count >= MIN_COUNT
        axes[1].plot(codes[~well], ratio[~well], ".", ms=3, color="0.7", label=f"seen < {MIN_COUNT} times")
        axes[1].plot(codes[well], ratio[well], ".", ms=3, label="seen often")
        axes[1].axhline(k, color="C1", lw=0.8, label=f"mean {k:.4f} (Ro = {ro / 1e3:.1f} k)")
        axes[1].set_ylim(k - 0.05, k + 0.05)
        axes[1].set_ylabel(f"I({rLow:.0f}) / I({rHigh:.0f})")
        axes[1].set_xlabel("DAC code")
        axes[1].legend()
        fig.tight_layout()
        fig.savefig(os.path.join(args.out, "dac_compare.png"), dpi=120)


if __name__ == "__main__":
    main()
