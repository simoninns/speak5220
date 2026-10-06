#!/usr/bin/env python3
"""
    dac_transfer.py

    Measures the static transfer function of the TMS5220 DAC from one or
    more capture directories written by capture.py, with TP2 loaded by the
    1K resistor alone (audio stage disconnected). Writes, to --out:

      dac_transfer.json  bit-weight model, linearity, history check
      dac_table.csv      per DAC code: count, mean and spread of TP2, the
                         current through the load, model
      dac_transfer.png   (with --plot) transfer curve, INL and history

    The DAC takes 8 bits. I/O carries the 10-bit code before clipping, so
    the DAC code is the I/O code clipped to -128..127, and the DAC input
    byte is that plus 128 (code 0 = 0x80, code -1 = 0x7F). The model fits
    TP2 as an offset plus one weight per input bit, which is how a
    binary-weighted current DAC behaves; a mismatched MSB shows up as a
    step between codes -1 and 0.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os

import numpy as np

import tms5220_decode as td

LAGS = 4    # previous samples tested for a history effect


def dacInput(code):
    """The DAC's 8-bit input byte for each I/O code (clipped)."""
    return np.clip(code, -128, 127) + 128


def bitMatrix(byte):
    """Design matrix: a constant column, then input bits 0-7."""
    bits = (byte[:, None] >> np.arange(8)[None, :]) & 1
    return np.column_stack([np.ones(len(byte)), bits])


def idleNoise(code, dac, settle=200):
    """Scatter of the per-sample DAC mean during long idle runs of -1, after
    `settle` samples, where the previous samples cannot matter. This is the
    measurement noise floor."""
    idle = code == -1
    edges = np.flatnonzero(np.diff(np.r_[0, idle.astype(np.int8), 0]))
    values = [dac[s + settle:e] for s, e in zip(edges[::2], edges[1::2]) if e - s > 2 * settle]
    values = np.concatenate(values) if values else np.zeros(0)
    return float(values.std()) if len(values) > 10 else None, int(len(values))


def mv(x):
    return float(x * 1e3)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("captures", nargs="+", help="capture directories (pooled)")
    parser.add_argument("--out", help="output directory (default: the first capture)")
    parser.add_argument("--load-ohms", type=float,
                        help="TP2 load (R2) for currents (default: capture.json, else 1000)")
    parser.add_argument("--zero", metavar="CAPTURE",
                        help="a capture taken with probe A's tip on GND; its mean Ch A "
                             "reading is subtracted as the scope's zero offset")
    parser.add_argument("--plot", action="store_true", help="also write dac_transfer.png")
    args = parser.parse_args()
    out = args.out or args.captures[0]
    if args.load_ohms is None:
        with open(os.path.join(args.captures[0], "capture.json")) as f:
            board = json.load(f).get("board", {})
        args.load_ohms = board.get("r2_load_ohms") or 1000.0
    os.makedirs(out, exist_ok=True)
    scopeZero = 0.0
    if args.zero:
        _, volts = td.loadRaw(os.path.join(args.zero, "raw", "scope_raw.npz"))
        scopeZero = float(volts[0].mean())
        print(f"scope zero from {args.zero}: {scopeZero * 1e3:.2f} mV")

    # Pool every decoded sample period. The first LAGS samples of each
    # capture have no known predecessors, so they are dropped.
    codes, dacs, trigs, prevCodes = [], [], [], []
    for capture in args.captures:
        interval, volts = td.loadRaw(os.path.join(capture, "raw", "scope_raw.npz"))
        d = td.decode(interval, volts)
        n = len(d["code"])
        codes.append(d["code"][LAGS:])
        dacs.append(d["dac"][LAGS:] - scopeZero)
        trigs.append(d["trig"][LAGS:])
        prevCodes.append(np.column_stack([d["code"][LAGS - k:n - k] for k in range(1, LAGS + 1)]))
        print(f"{capture}: {n} sample periods")
    code, dac, trig, prevCode = (np.concatenate(x) for x in (codes, dacs, trigs, prevCodes))
    byte, prevByte = dacInput(code), dacInput(prevCode)

    # Bit-weight model
    coef, *_ = np.linalg.lstsq(bitMatrix(byte), dac, rcond=None)
    offset, weights = coef[0], coef[1:]
    model = bitMatrix(np.arange(256)) @ coef
    residual = dac - model[byte]

    # Per-code table
    count = np.bincount(byte, minlength=256)
    total = np.bincount(byte, dac, minlength=256)
    square = np.bincount(byte, dac ** 2, minlength=256)
    with np.errstate(invalid="ignore", divide="ignore"):
        mean = total / count
        std = np.sqrt(np.maximum(square / count - mean ** 2, 0))

    # Linearity against the straight line through the model's end points
    lsb = (model[255] - model[0]) / 255
    ideal = model[0] + lsb * np.arange(256)
    inl = (model - ideal) / lsb
    dnl = np.diff(model) / lsb - 1
    seen = count > 0
    measuredInl = np.where(seen, (mean - ideal) / lsb, np.nan)

    # History: does TP2 depend on earlier samples? Regress the residual from
    # the per-code table on the level of each of the previous LAGS samples
    # relative to this one. A clean measurement gives coefficients near zero
    # and no fall in the residual.
    tableResidual = dac - mean[byte]
    previousStep = mean[prevByte] - mean[byte][:, None]
    history = np.column_stack([previousStep, np.ones(len(dac))])
    lagCoef, *_ = np.linalg.lstsq(history, tableResidual, rcond=None)
    afterHistory = tableResidual - history @ lagCoef
    noise, noiseCount = idleNoise(code, dac)
    minusOne, zero = 127, 128
    speechMinusOne = (byte == minusOne) & trig

    def ua(x):
        return float(x / args.load_ohms * 1e6)

    result = {
        "captures": [os.path.normpath(c) for c in args.captures],
        "samples": int(len(code)),
        "load_ohms": args.load_ohms,
        "scope_zero_mv": mv(scopeZero),
        "scope_zero_capture": os.path.normpath(args.zero) if args.zero else None,
        "codes_seen": int(seen.sum()),
        "io_code_range": [int(code.min()), int(code.max())],
        "clipped_samples": int(((code < -128) | (code > 127)).sum()),
        "model": {
            "offset_mv": mv(offset),
            "bit_weights_mv": [mv(w) for w in weights],
            "bit_weights_ua": [ua(w) for w in weights],
            "bit_weights_lsb": [float(w / weights[0]) for w in weights],
            "full_scale_mv": [mv(model[0]), mv(model[255])],
            "full_scale_ua": [ua(model[0]), ua(model[255])],
            "lsb_mv": mv(lsb), "lsb_ua": ua(lsb),
            "max_abs_inl_lsb": float(np.abs(inl).max()),
            "max_abs_dnl_lsb": float(np.abs(dnl).max()),
            "step_minus1_to_0_lsb": float((model[zero] - model[minusOne]) / lsb),
        },
        "measured": {
            "minus1_mv": mv(mean[minusOne]), "zero_mv": mv(mean[zero]),
            "step_minus1_to_0_lsb": float((mean[zero] - mean[minusOne]) / lsb)
            if count[zero] and count[minusOne] else None,
        },
        "fit": {
            "model_residual_rms_mv": mv(residual.std()),
            "table_residual_rms_mv": mv(tableResidual.std()),
            "history_coefficients": [float(x) for x in lagCoef[:LAGS]],
            "table_residual_after_history_rms_mv": mv(afterHistory.std()),
            "idle_noise_rms_mv": mv(noise) if noise is not None else None,
            "idle_noise_samples": noiseCount,
            "speech_minus1_scatter_rms_mv": mv(tableResidual[speechMinusOne].std()),
            "speech_minus1_samples": int(speechMinusOne.sum()),
        },
    }

    with open(os.path.join(out, "dac_transfer.json"), "w") as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))

    rows = [(c - 128, count[c], mean[c] if seen[c] else np.nan, std[c] if seen[c] else np.nan,
             mean[c] / args.load_ohms * 1e6 if seen[c] else np.nan,
             model[c], measuredInl[c], inl[c]) for c in range(256)]
    np.savetxt(os.path.join(out, "dac_table.csv"), rows,
               fmt=["%d", "%d", "%.6f", "%.6f", "%.2f", "%.6f", "%.3f", "%.3f"], delimiter=",",
               header="code,count,mean_v,std_v,mean_ua,model_v,measured_inl_lsb,model_inl_lsb",
               comments="")

    if args.plot:
        moving = byte != prevByte[:, 0]
        plot(out, code, byte, mean, seen, model, measuredInl, inl,
             previousStep[:, 0], tableResidual, moving, lsb)


def plot(out, code, byte, mean, seen, model, measuredInl, inl, previousStep, residual, moving, lsb):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    codes = np.arange(256) - 128
    fig, axes = plt.subplots(3, 1, figsize=(9, 10))
    ax = axes[0]
    ax.plot(codes[seen], mean[seen] * 1e3, ".", ms=3, label="measured mean")
    ax.plot(codes, model * 1e3, lw=0.8, label="bit-weight model")
    ax.set_xlabel("DAC code (I/O code clipped to -128..127)")
    ax.set_ylabel("TP2 (mV)")
    ax.legend()
    ax = axes[1]
    ax.plot(codes[seen], measuredInl[seen], ".", ms=3, label="measured")
    ax.plot(codes, inl, lw=0.8, label="model")
    ax.set_xlabel("DAC code")
    ax.set_ylabel("INL (LSB, end-point line)")
    ax.legend()
    ax = axes[2]
    ax.plot(previousStep[moving] / lsb, residual[moving] / lsb, ",", alpha=0.3)
    ax.set_xlabel("previous sample level minus this one (LSB)")
    ax.set_ylabel("residual from table (LSB)")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "dac_transfer.png"), dpi=120)


if __name__ == "__main__":
    main()
