#!/usr/bin/env python3
"""
    promstats.py

    How the original speech PROMs were encoded (plan section 8, for the
    authoring guide of Phase 11): per PROM, the share of each frame kind,
    the energy, pitch and K indices used, how words start and end, and the
    bit rate. Every word in the word table is parsed with the frame format
    of datasheet section 8.1, up to its stop code.

      python tools/promstats.py          # writes captures/phase11/promstats/

    Writes summary.json (per PROM and over all) and words.csv (one row per
    word).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import collections
import csv
import json
import os

import numpy as np

import lpc
import prom as promlib

ROOT = promlib.ROOT
OUT = os.path.join(ROOT, "captures", "phase11", "promstats")
PROMS = ["acorn_phrom_a", "vm61002", "vm61004", "vm71003a", "vm61003_words",
         "vm61005_words", "ti99_speech", "starwars_speech"]
KINDS = ["voiced", "unvoiced", "repeat", "silence", "stop"]


def wordFrames(manifest, data):
    base = promlib.base(manifest)
    seen = set()
    for word in manifest["words"]:
        address = promlib.address(word)
        if address in seen:
            continue
        seen.add(address)
        frames, used = lpc.parseFrames(lpc.bitsLsbFirst(data[address - base:]))
        yield word, frames, used


def hist(values, size):
    return np.bincount(np.asarray(values, dtype=int), minlength=size).tolist()


def pct(a, b):
    return round(100.0 * a / b, 1) if b else None


def stats(rows, frames):
    kinds = collections.Counter(f["kind"] for f in frames)
    n = len(frames)
    voiced = [f for f in frames if f["kind"] == "voiced"]
    unvoiced = [f for f in frames if f["kind"] == "unvoiced"]
    speech = [f for f in frames if f["kind"] in ("voiced", "unvoiced", "repeat")]
    # a repeat frame's voicing is its own pitch field
    pitched = [f for f in speech if f["pitch"]]
    seconds = sum(r["frames"] for r in rows) * 0.025
    out = {
        "words": len(rows),
        "frames": n,
        "seconds": round(seconds, 1),
        "frames_per_word_p10_p50_p90": np.percentile([r["frames"] for r in rows], [10, 50, 90]).tolist(),
        "kind_pct": {k: pct(kinds[k], n) for k in KINDS},
        "bits_per_second_mean": round(sum(r["bits"] for r in rows) / seconds, 0),
        "bits_per_second_p10_p50_p90": np.percentile([r["bits"] / (r["frames"] * 0.025) for r in rows],
                                                      [10, 50, 90]).round(0).tolist(),
        "energy_hist_voiced": hist([f["energy"] for f in speech if f["pitch"]], 16),
        "energy_hist_unvoiced": hist([f["energy"] for f in speech if not f["pitch"]], 16),
        "pitch_hist": hist([f["pitch"] for f in pitched], 64),
        "pitch_index_p5_p50_p95": np.percentile([f["pitch"] for f in pitched], [5, 50, 95]).tolist()
        if pitched else None,
        "k_index_range_voiced": [[min(f["k"][j] for f in voiced), max(f["k"][j] for f in voiced)]
                                 for j in range(10)] if voiced else None,
        "k_index_range_unvoiced": [[min(f["k"][j] for f in unvoiced), max(f["k"][j] for f in unvoiced)]
                                   for j in range(4)] if unvoiced else None,
        "first_frame": dict(collections.Counter(r["first"] for r in rows)),
        "last_before_stop": dict(collections.Counter(r["last"] for r in rows)),
        "leading_silence_frames": dict(sorted(collections.Counter(r["lead_silence"] for r in rows).items())),
        "trailing_silence_frames": dict(sorted(collections.Counter(r["trail_silence"] for r in rows).items())),
        "silence_frames_inside_words_pct": pct(sum(r["inner_silence"] for r in rows), n),
        "repeat_run_lengths": dict(sorted(collections.Counter(
            length for r in rows for length in r["repeat_runs"]).items())),
        "words_ending_unvoiced_pct": pct(sum(r["last"] == "unvoiced" or r["last_voicing"] == "unvoiced"
                                             for r in rows), len(rows)),
    }
    return out


def wordRow(prom, word, frames, used):
    kinds = [f["kind"] for f in frames]
    body = kinds[:-1] if kinds and kinds[-1] == "stop" else kinds
    lead = next((i for i, k in enumerate(body) if k != "silence"), len(body))
    trail = next((i for i, k in enumerate(reversed(body)) if k != "silence"), len(body))
    inner = body[lead:len(body) - trail]
    runs, run = [], 0
    for k in body + ["end"]:
        if k == "repeat":
            run += 1
        elif run:
            runs.append(run)
            run = 0
    lastSpeech = next((f for f in reversed(frames) if f["kind"] in ("voiced", "unvoiced", "repeat")), None)
    return {
        "prom": prom, "number": word["number"], "name": word["name"], "address": word["address"],
        "frames": len(frames), "bits": used, "first": kinds[0] if kinds else "",
        "last": body[-1] if body else "",
        "last_voicing": ("voiced" if lastSpeech and lastSpeech["pitch"] else "unvoiced") if lastSpeech else "",
        "lead_silence": lead, "trail_silence": trail if trail < len(body) else 0,
        "inner_silence": inner.count("silence"), "repeat_runs": runs,
        **{f"n_{k}": kinds.count(k) for k in KINDS},
    }


def main():
    os.makedirs(OUT, exist_ok=True)
    summary, allRows, allFrames = {}, [], []
    for name in PROMS:
        manifest = promlib.loadManifest(name)
        data = promlib.image(manifest)
        rows, frames = [], []
        for word, f, used in wordFrames(manifest, data):
            if not f or f[-1]["kind"] != "stop":
                continue
            rows.append(wordRow(name, word, f, used))
            frames += f
        summary[name] = stats(rows, frames)
        allRows += rows
        allFrames += frames
    summary["all"] = stats(allRows, allFrames)
    with open(os.path.join(OUT, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)
    fields = [k for k in allRows[0] if k != "repeat_runs"]
    with open(os.path.join(OUT, "words.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(allRows)
    for name, s in summary.items():
        print(f"{name:16s} words {s['words']:4d}  kinds {s['kind_pct']}  bps {s['bits_per_second_mean']:.0f}  "
              f"first {s['first_frame']}  last {s['last_before_stop']}")


if __name__ == "__main__":
    main()
