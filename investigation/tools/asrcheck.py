#!/usr/bin/env python3
"""
    asrcheck.py

    An objective intelligibility check for Phase 11 (plan section 9.4,
    "Listen"): whisper.cpp (whisper-cli, in nix develop) transcribes, for
    every encoded item with a text,

      input       the input audio the item was encoded from
      prediction  the emulator's prediction (VSM path)
      chip        the chip's output, take vsm02 (I/O codes)

    and the word error rate (WER) of each against the prompt's text is
    reported per set. It does not replace listening: it shows whether the
    encoding loses words, and whether the chip says what the prediction
    says. Recognisers mishear clean speech too, and write numbers as
    digits, so the input's WER is the baseline, not zero.

      python tools/asrcheck.py                 # every set with takes
      python tools/asrcheck.py --no-chip       # inputs and predictions only

    The model, ggml-small.en.bin, is kept in local/whisper/ (SHA-256 below):
    https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin

    Writes captures/phase11/results/asr/{items.csv,summary.json}.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os
import re
import subprocess
import tempfile
from collections import defaultdict

import numpy as np
import scipy.signal
import soundfile

import phase11
import regress
import ttsgen

ROOT = phase11.ROOT
MODEL = os.path.join(ROOT, "local", "whisper", "ggml-small.en.bin")
MODEL_SHA256 = "c6138d6d58ecc8322097e0f987c32f1be8bb0a18532a3f88f734d1bbf9c41e5d"
OUT = os.path.join(phase11.RESULTS, "asr")
SOURCES = ("input", "prediction", "chip")


def words(text):
    text = text.lower().replace("-", " ")
    return re.sub(r"[^a-z0-9' ]", " ", text).split()


def wer(ref, hyp):
    r, h = words(ref), words(hyp)
    d = np.arange(len(h) + 1)
    for i in range(1, len(r) + 1):
        prev, d[0] = d.copy(), i
        for j in range(1, len(h) + 1):
            d[j] = min(prev[j] + 1, d[j - 1] + 1, prev[j - 1] + (r[i - 1] != h[j - 1]))
    return int(d[len(h)]), len(r)


def writeWav(path, x):
    """x: float, 8 kHz; written at 16 kHz for whisper."""
    y = scipy.signal.resample_poly(x, 2, 1)
    peak = np.max(np.abs(y)) or 1.0
    soundfile.write(path, (0.7 * y / peak).astype(np.float32), 16000, subtype="PCM_16")


def transcribe(paths):
    """whisper-cli on many files at once: {path: text}."""
    out = {}
    for n in range(0, len(paths), 64):
        batch = paths[n:n + 64]
        cmd = ["whisper-cli", "-m", MODEL, "-nt", "-np", "-otxt", "-l", "en"]
        for p in batch:
            cmd += ["-f", p]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for p in batch:
            with open(p + ".txt") as f:
                out[p] = " ".join(f.read().split())
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("--no-chip", action="store_true")
    args = parser.parse_args()
    with open(MODEL, "rb") as f:
        if phase11.sha256(f.read()) != MODEL_SHA256:
            raise SystemExit(f"{MODEL}: SHA-256 does not match")
    os.makedirs(OUT, exist_ok=True)
    items = []          # (set, utterance key, text, {source: wav path})
    with tempfile.TemporaryDirectory() as tmp:
        for name in phase11.setNames():
            if name == "hand":
                continue
            info, arrays = phase11.loadPredictions(name)
            with open(os.path.join(phase11.DATA, f"{name}_items.csv"), newline="") as f:
                rows = list(csv.DictReader(f))
            # the parts of a split input are joined again
            groups = defaultdict(list)
            for r in rows:
                groups[r["input"]].append(r)
            for inputPath, parts in groups.items():
                text = parts[0]["text"]
                if not text:
                    continue
                key = os.path.splitext(os.path.basename(inputPath))[0]
                wavs = {}
                rate, x = ttsgen.readFlac(os.path.join(ROOT, inputPath))
                wavs["input"] = os.path.join(tmp, f"{name}_{key}_input.wav")
                writeWav(wavs["input"], x.astype(float) / 32768)
                pred, chip = [], []
                for r in parts:
                    k = next(k for k in (f"{r['word']}_vsm_any", f"{r['word']}_vsm_same") if k in arrays.files)
                    pred.append(arrays[k].astype(float) / 512)
                    take = os.path.join(phase11.TAKES, f"phase11_{name}",
                                        phase11.promlib.wordDir({"number": r["word"], "name": r["name"]}), "vsm02")
                    if not args.no_chip and os.path.isdir(take):
                        chip.append(regress.loadChip(take)[1].astype(float) / 512)
                wavs["prediction"] = os.path.join(tmp, f"{name}_{key}_prediction.wav")
                writeWav(wavs["prediction"], np.concatenate(pred))
                if len(chip) == len(parts):
                    wavs["chip"] = os.path.join(tmp, f"{name}_{key}_chip.wav")
                    writeWav(wavs["chip"], np.concatenate(chip))
                items.append((name, key, text, wavs))
        texts = transcribe([p for *_, wavs in items for p in wavs.values()])
    rows, totals = [], defaultdict(lambda: defaultdict(lambda: [0, 0]))
    for name, key, text, wavs in items:
        row = {"set": name, "item": key, "text": text}
        for source in SOURCES:
            if source in wavs:
                hyp = texts[wavs[source]]
                errors, n = wer(text, hyp)
                row[source] = hyp
                row[f"{source}_wer"] = round(errors / n, 3) if n else ""
                for group in (name, "all"):
                    totals[group][source][0] += errors
                    totals[group][source][1] += n
        rows.append(row)
    fields = ["set", "item", "text"] + [c for s in SOURCES for c in (f"{s}_wer", s)]
    with open(os.path.join(OUT, "items.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fields, restval="")
        w.writeheader()
        w.writerows(rows)
    summary = {g: {s: {"wer": round(e / n, 3), "words": n} for s, (e, n) in d.items()}
               for g, d in totals.items()}
    summary["model"] = {"file": "ggml-small.en.bin", "sha256": MODEL_SHA256}
    with open(os.path.join(OUT, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)
    for g, d in summary.items():
        if g != "model":
            print(g, {s: v["wer"] for s, v in d.items()})


if __name__ == "__main__":
    main()
