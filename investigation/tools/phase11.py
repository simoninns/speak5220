#!/usr/bin/env python3
"""
    phase11.py

    Phase 11 (plan section 12): new speech data, predicted with the
    reference emulator before the chip is run, then captured and compared
    with the predictions (plan section 9.4, acceptance test 2).

      python tools/phase11.py predict       # the emulator's predictions, per item and path
      python tools/phase11.py check         # the sets and predictions against their hashes
      python tools/run.py captures/phase11/data/<set>.json all --takes 3 --path both \\
          --out captures/phase11/takes --zip-only --resume --timeout-ms 30000
      python tools/phase11.py compare       # the chip against the predictions

    The data sets, in captures/phase11/data/, are synthetic PROMs as
    vectors.py writes them (a manifest, the image and every word's frames),
    so run.py captures them like any other:

      phrom   an Acorn-format PHROM (datasheet section 12.8) of the single
              words, numbers and computer phrases, in one voice each
      tts1..  every TTS input (captures/phase11/inputs/tts/)
      rec     every real recording (captures/phase11/inputs/recordings/)
      hand    hand-designed frame sequences that use every feature of the
              frame format, built by hand from the datasheet

    The speech in the sets was encoded by the project's Python research
    encoder. The sets are committed, with each word's input in
    <set>_items.csv.

    Predictions (captures/phase11/predictions/): for every word and path,
    the emulator's I/O codes from 5 samples before the first frame read to
    the end of its output, and for the VSM path the bits it reads, the
    sample of each frame read and when Talk Status clears. Each utterance
    is predicted after the one that run.py speaks before it: the previous
    word, for a word's first take, or the same word. Where the two differ
    (only an utterance that starts with a repeat frame, datasheet section
    7), both are kept. The predictions are committed before any take is
    made; compare checks their hashes first.

    compare judges each set by the tolerance of plan section 1.5: the
    emulator's (the prediction's) median SDR against the chip may be at
    most 7 dB below chip 1's agreement with its own takes (takes 02 and 03
    of each word and path, which both follow the same word), counting
    agreement above 40 dB as 40 dB. It also checks the VSM timing (layer
    1) against the prediction, and the chip's VSM output against its
    Speak External output.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import glob
import hashlib
import json
import multiprocessing
import os
import subprocess
import sys
import tempfile

import numpy as np

import lpc
import prom as promlib
import regress
import variance

ROOT = promlib.ROOT

P11 = os.path.join(ROOT, "captures", "phase11")
DATA = os.path.join(P11, "data")
PRED = os.path.join(P11, "predictions")
TAKES = os.path.join(P11, "takes")
RESULTS = os.path.join(P11, "results")

STANDARD_SPEAK_AT = 1961    # slots before the frame boundary (datasheet section 7)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def codesSha256(codes):
    return sha256(np.asarray(codes, dtype="<i2").tobytes())


def setNames():
    return [os.path.splitext(os.path.basename(p))[0] for p in sorted(glob.glob(os.path.join(DATA, "*.json")))]


# ---- predictions --------------------------------------------------------

def emulate(imagePath, base, address, external, prior, tmp, tag):
    """The emulator's output for one utterance spoken with the standard
    procedure after the utterances at the addresses in prior."""
    samplesPath, eventsPath = os.path.join(tmp, tag + ".s"), os.path.join(tmp, tag + ".e")
    cmd = [regress.EMU, "-s", samplesPath, "-e", eventsPath, "--image", imagePath,
           "--load-at", f"{base:X}", "--addr", f"{address:X}", "--speak-at", str(STANDARD_SPEAK_AT), "--reset"]
    if external:
        cmd.append("--external")
    if prior:
        cmd += ["--prior", regress.priorScript(prior, os.path.join(tmp, tag + ".prior"))]
    subprocess.run(cmd, check=True)
    emu = regress.readSamples(samplesPath)
    events = regress.readEvents(eventsPath)
    mark = [s for s, e in events if e[0] == "MARK" and e[1].startswith("speak")][-1]
    ready = next(int(e[4]) for s, e in events if s == mark and e[0] == "HOST")
    first = (ready + STANDARD_SPEAK_AT) // 20
    codes = emu[first - 5:]
    # trim the idle tail, keeping 200 samples of it
    active = np.flatnonzero(codes != -1)
    end = min(len(codes), (active[-1] + 201) if len(active) else 10)
    out = {"codes": codes[:end].astype(np.int16)}
    if not external:
        after = [(s, e) for s, e in events if s > mark]
        reads = [s for s, e in after if e[:2] == ["VSM", "READ"]]
        out["bits"] = len(reads)
        out["frame_samples"] = [s // 20 - first for n, s in enumerate(reads)
                                if n == 0 or s - reads[n - 1] > 1000]
        ts = next((s for s, e in after if e[:3] == ["FLAG", "TS", "0"]), None)
        out["talk_us"] = round((ts - reads[0]) * regress.SLOT_US, 2) if ts else None
    return out


def predictJob(job):
    setName, imagePath, base, words, n, tmp = job
    word = words[n]
    address = int(word["address"], 16)
    priors = {"same": [address]}
    if n:
        priors["prev"] = [int(words[n - 1]["address"], 16)]
    out = {}
    for path in ("vsm", "external"):
        if path == "external" and word["bytes"] < 9:
            continue
        results = {v: emulate(imagePath, base, address, path == "external", p, tmp,
                              f"{setName}_{n}_{path}_{v}") for v, p in priors.items()}
        same = results["same"]
        if all(np.array_equal(r["codes"], same["codes"]) for r in results.values()):
            results = {"any": same}
        for v, r in results.items():
            out[f"{word['number']}_{path}_{v}"] = r
    return word, out


def predict():
    os.makedirs(PRED, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp, multiprocessing.Pool() as pool:
        for name in setNames():
            manifest = promlib.loadManifest(os.path.join(DATA, f"{name}.json"))
            imagePath = os.path.join(ROOT, manifest["image"]["source"])
            base = promlib.base(manifest)
            words = manifest["words"]
            jobs = [(name, imagePath, base, words, n, tmp) for n in range(len(words))]
            arrays, meta = {}, {}
            for word, results in pool.map(predictJob, jobs):
                for key, r in results.items():
                    arrays[key] = r["codes"]
                    meta[key] = {k: v for k, v in r.items() if k != "codes"}
                    meta[key].update({"word": word["name"], "samples": int(len(r["codes"])),
                                      "codes_sha256": codesSha256(r["codes"]),
                                      "peak": int(np.abs(r["codes"].astype(int)).max())})
            np.savez_compressed(os.path.join(PRED, f"{name}.npz"), **arrays)
            info = {"set": name, "image_sha256": manifest["image"]["sha256"],
                    "emulator": os.path.relpath(regress.EMU, ROOT),
                    "speak_at_slots": STANDARD_SPEAK_AT,
                    "codes_from": "5 samples before the first frame read (VSM), or where it would be "
                                  "(Speak External)",
                    "predictions": meta}
            with open(os.path.join(PRED, f"{name}.json"), "w") as f:
                json.dump(info, f, indent=1)
            peaks = [m["peak"] for m in meta.values()]
            print(f"{name}: {len(meta)} predictions, peak |code| {max(peaks)}, "
                  f"{sum(p > 127 for p in peaks)} over 127")


def loadPredictions(name):
    with open(os.path.join(PRED, f"{name}.json")) as f:
        info = json.load(f)
    arrays = np.load(os.path.join(PRED, f"{name}.npz"))
    bad = [k for k, m in info["predictions"].items() if codesSha256(arrays[k]) != m["codes_sha256"]]
    if bad:
        raise ValueError(f"{name}: predictions {bad[:5]} do not match their hashes")
    return info, arrays


def check():
    ok = True
    for name in setNames():
        manifest = promlib.loadManifest(os.path.join(DATA, f"{name}.json"))
        try:
            promlib.image(manifest)
            info, _ = loadPredictions(name)
            if info["image_sha256"] != manifest["image"]["sha256"]:
                raise ValueError("predicted from another image")
            print(f"{name}: image and {len(info['predictions'])} predictions OK")
        except (ValueError, FileNotFoundError) as e:
            print(f"{name}: {e}")
            ok = False
    return ok


# ---- comparison with the chip --------------------------------------------

def compareTake(job):
    take, previous, pred, setName = job
    capture, codes, reads = regress.loadChip(take)
    external = capture["path"] == "external"
    first = reads[0] if reads and not external else regress.STANDARD_FIRST_READ_SAMPLE
    address = int(capture["source"]["address"], 16)
    number = capture["source"]["word_number"]
    path = "external" if external else "vsm"
    variant = "any"
    if f"{number}_{path}_any" not in pred["meta"]:
        variant = "same" if previous and previous[-1] == address else "prev"
    key = f"{number}_{path}_{variant}"
    row = {"set": setName, "take": os.path.relpath(take, ROOT), "word": number,
           "name": capture["source"]["word"], "path": path, "prediction": key}
    if key not in pred["meta"]:
        row["note"] = "no prediction for this predecessor"
        return row
    p = pred["arrays"][key].astype(np.int64)
    row.update(variance.measure((codes, first), (p, 5)))
    if not external:
        m = pred["meta"][key]
        summary = capture["processed"]["summary"]
        row["bits_ok"] = int(summary.get("bits_consumed") == m["bits"])
        row["frames_ok"] = int([r - first for r in reads] == m["frame_samples"])
        talk = summary.get("talk_after_first_read_s")
        row["talk_us_diff"] = round(talk * 1e6 - m["talk_us"], 1) if talk and m["talk_us"] else ""
    return row


def pairJob(job):
    kind, setName, a, b = job
    row = {"kind": kind, "set": setName, "take": os.path.relpath(a, ROOT), "other": os.path.relpath(b, ROOT)}
    other = variance.chipCodes(b)
    if kind == "paths":
        # Speak External runs 2 samples ahead of the VSM Speak (datasheet section 8.5)
        other = (other[0], other[1] - 2)
    row.update(variance.measure(variance.chipCodes(a), other))
    return row


def unvoicedFree(manifest, number):
    data = promlib.image(manifest)
    word = promlib.findWord(manifest, number)
    frames, _ = lpc.parseFrames(lpc.bitsLsbFirst(data[promlib.address(word) - promlib.base(manifest):]))
    return not any(f.get("pitch") == 0 and f["energy"] not in (0, 15) for f in frames)


def compare():
    os.makedirs(RESULTS, exist_ok=True)
    rows, pairs, summary = [], [], {}
    with multiprocessing.Pool() as pool:
        for name in setNames():
            root = os.path.join(TAKES, f"phase11_{name}")
            if not os.path.isdir(root):
                continue
            info, arrays = loadPredictions(name)
            pred = {"meta": info["predictions"], "arrays": {k: arrays[k] for k in arrays.files}}
            manifest = promlib.loadManifest(os.path.join(DATA, f"{name}.json"))
            takes = regress.takesOf(root, False)
            setRows = pool.map(compareTake, [(t, p, pred, name) for t, p in takes])
            # chip 1 against itself: takes 02 and 03, which both follow the same word;
            # and the VSM path against Speak External, on words with no unvoiced frame
            jobs = []
            for word in sorted(glob.glob(os.path.join(root, "*", ""))):
                for prefix in ("vsm", "ext"):
                    a, b = os.path.join(word, f"{prefix}02"), os.path.join(word, f"{prefix}03")
                    if os.path.isdir(a) and os.path.isdir(b) and variance.standard(a) and variance.standard(b):
                        jobs.append(("take", name, a, b))
                a, b = os.path.join(word, "vsm02"), os.path.join(word, "ext02")
                number = int(os.path.basename(word.rstrip("/")).split("_")[0])
                if (os.path.isdir(a) and os.path.isdir(b) and variance.standard(a) and variance.standard(b)
                        and unvoicedFree(manifest, number)):
                    jobs.append(("paths", name, a, b))
            setPairs = pool.map(pairJob, jobs)
            rows += setRows
            pairs += setPairs
            sdr = [r["sdr_db"] for r in setRows if "sdr_db" in r]
            ref = [p["sdr_db"] for p in setPairs if p["kind"] == "take"]
            paths = [p for p in setPairs if p["kind"] == "paths"]
            vsm = [r for r in setRows if r["path"] == "vsm" and "bits_ok" in r]
            talk = [abs(r["talk_us_diff"]) for r in vsm if r.get("talk_us_diff") != ""]
            emuMedian = float(np.median(sdr)) if sdr else None
            refMedian = float(np.median(ref)) if ref else None
            s = {
                "takes": len(setRows), "predicted": len(sdr),
                "unpredicted": sum("sdr_db" not in r for r in setRows),
                "emulator_sdr_db_p10_p50_p90": variance.percentiles(sdr),
                "emulator_samples_equal_pct_median": float(np.median([r["equal_pct"] for r in setRows if "equal_pct" in r])) if sdr else None,
                "emulator_max_abs_p90": float(np.percentile([r["max_abs"] for r in setRows if "max_abs" in r], 90)) if sdr else None,
                "reference_pairs": len(ref), "reference_sdr_db_p10_p50_p90": variance.percentiles(ref),
                "layer1_vsm_takes": len(vsm), "bits_ok": sum(r["bits_ok"] for r in vsm),
                "frames_ok": sum(r["frames_ok"] for r in vsm),
                "talk_us_max_abs_diff": max(talk) if talk else None,
                "vsm_vs_external_pairs": len(paths),
                "vsm_vs_external_exact": sum(p["max_abs"] == 0 for p in paths),
                "vsm_vs_external_sdr_db_p10_p50_p90": variance.percentiles([p["sdr_db"] for p in paths]),
            }
            if emuMedian is not None and refMedian is not None:
                agreement = min(refMedian, variance.AGREE_CAP_DB)
                s["tolerance"] = {"reference": "chip 1 takes", "reference_db": round(refMedian, 2),
                                  "emulator_db": round(emuMedian, 2), "gap_db": round(agreement - emuMedian, 2),
                                  "within": bool(emuMedian >= agreement - variance.MARGIN_DB)}
            summary[name] = s
            print(name, json.dumps(s))
    fields = ["set", "take", "word", "name", "path", "prediction", "compared", "equal_pct", "rms", "max_abs",
              "sdr_db", "first_diff", "bits_ok", "frames_ok", "talk_us_diff", "note"]
    with open(os.path.join(RESULTS, "takes.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fields, restval="")
        w.writeheader()
        w.writerows(rows)
    with open(os.path.join(RESULTS, "pairs.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, ["kind", "set", "take", "other", "compared", "equal_pct", "rms", "max_abs",
                               "sdr_db", "first_diff"])
        w.writeheader()
        w.writerows(pairs)
    summary["tolerance_rule"] = {"margin_db": variance.MARGIN_DB, "agreement_cap_db": variance.AGREE_CAP_DB,
                                 "note": "plan section 1.5; observed at the close of Phase 9, not derived"}
    with open(os.path.join(RESULTS, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["predict", "check", "compare"])
    args = parser.parse_args()
    if args.command == "predict":
        predict()
    elif args.command == "check":
        sys.exit(0 if check() else 1)
    else:
        compare()


if __name__ == "__main__":
    main()
