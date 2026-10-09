#!/usr/bin/env python3
"""TMS5220 encoder evaluation.

Encodes test inputs, synthesises the frames with the reference algorithm and
measures the result against the input: level tracking per frame, a spectral
distance, voicing and pitch against the input index, clipping and wraps.

    python encoder/evaluate.py [--set tts|recordings|all] [--limit N] [--csv OUT]

This file is part of speak5220.
SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import math
import os
import sys
import time

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tms5220enc as enc  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
INPUTS = os.path.join(ROOT, "captures", "phase11", "inputs")


def melBank(nfft=256, bands=20, lo=100.0, hi=3800.0):
    """Triangular mel filters over an rfft of nfft points at 8 kHz."""
    def mel(f):
        return 2595 * np.log10(1 + f / 700.0)

    def imel(m):
        return 700 * (10 ** (m / 2595.0) - 1)
    edges = imel(np.linspace(mel(lo), mel(hi), bands + 2))
    freqs = np.arange(nfft // 2 + 1) * enc.RATE / nfft
    fb = np.zeros((bands, len(freqs)))
    for i in range(bands):
        a, c, b = edges[i], edges[i + 1], edges[i + 2]
        fb[i] = np.clip(np.minimum((freqs - a) / (c - a), (b - freqs) / (b - c)), 0, None)
    return fb


FB = melBank()
HAM = np.hamming(256)


def bandsDb(seg):
    if len(seg) < 256:
        seg = np.concatenate([seg, np.zeros(256 - len(seg))])
    p = np.abs(np.fft.rfft(seg * HAM)) ** 2
    return 10 * np.log10(FB @ p + 1e-9)


def measure(path, meta, **opts):
    x, rate = sf.read(path, dtype="float64")
    t0 = time.time()
    frames, info = enc.encodeDetailed(x, rate, **opts)
    tEnc = time.time() - t0
    st = {}
    codes = enc._synth(frames, st)
    xs = info["x"]
    gain = info["gain"]
    plan = info["plan"]
    cen = info["centres"]
    nfr = len(plan)
    c = codes.astype(float)
    # Level per frame, input and output, same 200-sample windows
    inDb, outDb, snd = [], [], []
    for j in range(nfr):
        a = enc._frameRms(xs, cen[j]) * gain
        o = enc._frameRms(c, enc.REACHED + enc.FRAME * j)
        inDb.append(20 * math.log10(max(a, 1e-3)))
        outDb.append(20 * math.log10(max(o, 1e-3)))
        snd.append(plan[j]["kind"] != "silence")
    inDb, outDb, snd = np.array(inDb), np.array(outDb), np.array(snd)
    loudIn = inDb[snd].max() if np.any(snd) else 0
    sel = snd & (inDb > loudIn - 30)
    lvErr = (outDb - inDb)[sel]
    # Spectral distance, level-normalised, on frames within 30 dB of the loudest
    sd = []
    for j in np.nonzero(sel)[0]:
        bi = bandsDb(xs[max(0, cen[j] - 128):cen[j] + 128] * gain)
        m = enc.REACHED + enc.FRAME * j
        bo = bandsDb(c[max(0, m - 128):m + 128])
        d = (bi - bi.mean()) - (bo - bo.mean())
        sd.append(math.sqrt(float(np.mean(d * d))))
    # Envelope alignment check: best lag of 5 ms log envelopes
    def env(v):
        n = len(v) // 40
        return 10 * np.log10(np.mean(v[:n * 40].reshape(n, 40) ** 2, axis=1) + 1e-6)
    ei, eo = env(xs * gain), env(c)
    expect = enc.REACHED - info["c0"]
    best, bestLag = None, 0
    for lag in range(-10, 11):
        sh = int(round(expect / 40.0)) + lag
        if sh >= 0:
            a, b = ei[:len(eo) - sh] if sh < len(eo) else ei[:0], eo[sh:]
        else:
            a, b = ei[-sh:], eo
        n = min(len(a), len(b))
        if n < 10:
            continue
        r = np.corrcoef(a[:n], b[:n])[0, 1]
        if best is None or r > best:
            best, bestLag = r, lag
    # Predecessor dependence: the same frames after two very different
    # parameter RAM states (all K indices 0, or all at their maximum)
    lo = enc._synth(frames, initIdx=[15, 1] + [0] * 10)
    hi = enc._synth(frames, initIdx=[15, 63] + [(1 << b) - 1 for b in enc.KBITS])
    n = min(len(lo), len(hi))
    predDep = int(np.max(np.abs(lo[:n] - hi[:n]))) if n else 0
    kinds = [enc.frameKind(f) for f in frames]
    nb = sum(enc.frameBits(f) for f in frames)
    # Pitch: voiced frames' f0 against the index
    f0 = []
    lastP = None
    jumps = 0
    for f in frames:
        k = enc.frameKind(f)
        if k in ("voiced", "repeat") and f["pitch"]:
            p = enc.PITCH[f["pitch"]] + 1
            f0.append(enc.RATE / p)
            if lastP and (p / lastP > 1.6 or p / lastP < 0.625):
                jumps += 1
            lastP = p
        else:
            lastP = None
    spk = codes[(codes != -1)]
    voicedFrames = sum(1 for p in plan if p["kind"] != "silence" and p["pitch"])
    soundFrames = sum(1 for p in plan if p["kind"] != "silence")
    return {
        "file": os.path.basename(path),
        "voice": meta.get("voice", ""),
        "frames": len(frames),
        "bitrate": nb / (len(frames) * 0.025),
        "voiced": kinds.count("voiced"), "unvoiced": kinds.count("unvoiced"),
        "repeat": kinds.count("repeat"), "silence": kinds.count("silence"),
        "voicedPct": 100.0 * voicedFrames / max(1, soundFrames),
        "idxVoicedPct": float(meta.get("voiced_pct") or "nan"),
        "f0med": float(np.median(f0)) if f0 else float("nan"),
        "idxF0": float(meta.get("f0_p50") or "nan"),
        "jumps": jumps,
        "lvMeanAbs": float(np.mean(np.abs(lvErr - np.median(lvErr)))) if len(lvErr) else 0.0,
        "lvBias": float(np.median(lvErr)) if len(lvErr) else 0.0,
        "lvP90": float(np.percentile(np.abs(lvErr - np.median(lvErr)), 90)) if len(lvErr) else 0.0,
        "specDist": float(np.median(sd)) if sd else float("nan"),
        "alignLag": bestLag * 40,
        "rms": float(np.sqrt(np.mean(spk.astype(float) ** 2))) if len(spk) else 0.0,
        "peak": int(np.max(np.abs(codes))),
        "clip": int(np.sum((codes > 127) | (codes < -128))),
        "wraps": st["wraps"],
        "predDep": predDep,
        "encSeconds": tEnc,
        "limited": info["stats"].get("limited", 0),
        "silenced": info["stats"].get("silenced", 0),
        "droppedOpening": info["stats"].get("droppedOpening", 0),
        "fillers": sum(1 for f in frames if f == {"energy": 1, "repeat": 1, "pitch": 0}),
        "soundingFrames": info["stats"].get("sounding", 0),
        "energyVoiced": " ".join(str(f["energy"]) for f in frames if enc.frameKind(f) == "voiced"),
        "energyUnvoiced": " ".join(str(f["energy"]) for f in frames if enc.frameKind(f) == "unvoiced"),
    }


def loadIndex(sub):
    with open(os.path.join(INPUTS, sub, "index.csv")) as fh:
        return [dict(r, path=os.path.join(INPUTS, sub, r["file"])) for r in csv.DictReader(fh)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", default="all", choices=["tts", "recordings", "all"])
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--match", default="")
    ap.add_argument("--csv")
    ap.add_argument("--opt", action="append", default=[], help="name=value encode option")
    args = ap.parse_args()
    opts = {}
    for o in args.opt:
        k, v = o.split("=")
        opts[k] = None if v == "None" else (v == "True" if v in ("True", "False") else
                                           (int(v) if v.isdigit() else float(v)))
    rows = []
    subs = ["tts", "recordings"] if args.set == "all" else [args.set]
    for sub in subs:
        idx = [r for r in loadIndex(sub) if args.match in r["file"]]
        if args.limit:
            idx = idx[::max(1, len(idx) // args.limit)][:args.limit]
        for r in idx:
            m = measure(r["path"], r, **opts)
            m["set"] = sub
            rows.append(m)
            print("%-24s %4d fr %5.0f b/s V%3.0f%%(%3.0f) f0 %5.1f(%5.1f) j%d lv %.2f/%.2f bias %+.1f sd %.2f lag %+d rms %.1f pk %d clip %d wr %d dep %d" % (
                m["file"], m["frames"], m["bitrate"], m["voicedPct"], m["idxVoicedPct"], m["f0med"],
                m["idxF0"], m["jumps"], m["lvMeanAbs"], m["lvP90"], m["lvBias"], m["specDist"],
                m["alignLag"], m["rms"], m["peak"], m["clip"], m["wraps"], m["predDep"]), flush=True)
    if not rows:
        return
    def med(k, rs):
        return float(np.nanmedian([r[k] for r in rs]))
    for name, rs in [("tts", [r for r in rows if r["set"] == "tts"]),
                     ("recordings", [r for r in rows if r["set"] == "recordings"]), ("all", rows)]:
        if not rs:
            continue
        ratio = np.array([r["f0med"] / r["idxF0"] for r in rs if r["idxF0"] == r["idxF0"] and r["f0med"] == r["f0med"]])
        print("%s: n=%d bitrate %.0f, level err |dB| median %.2f (p90 %.2f), bias %+.2f, spec dist %.2f dB, "
              "rms %.1f, peak max %d, clipped %d, wraps %d, predecessor-dependent %d (max %d codes), f0 ratio outside 0.8-1.25: %d, octave jumps %d, voiced%% diff %.1f" % (
                  name, len(rs), med("bitrate", rs), med("lvMeanAbs", rs), med("lvP90", rs), med("lvBias", rs),
                  med("specDist", rs), med("rms", rs), max(r["peak"] for r in rs), sum(r["clip"] for r in rs),
                  sum(r["wraps"] for r in rs), sum(1 for r in rs if r["predDep"]), max(r["predDep"] for r in rs), int(np.sum((ratio < 0.8) | (ratio > 1.25))),
                  sum(r["jumps"] for r in rs), med("voicedPct", rs) - med("idxVoicedPct", rs)))
    if args.csv:
        with open(args.csv, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)


if __name__ == "__main__":
    main()
