#!/usr/bin/env python3
"""
    ttsgen.py

    Test speech for the encoder (plan Phase 11). Every prompt in
    tools/prompts.csv is rendered with Piper, an open-source neural TTS,
    from a fixed set of voices whose models are pinned by SHA-256, then
    resampled to the chip's 8 kHz and written as 16-bit mono FLAC
    (lossless):

      captures/phase11/inputs/tts/<id>_<voice>.flac
      captures/phase11/inputs/tts/index.csv     text, voice, level, pitch
                                                 and SHA-256 of the samples
      captures/phase11/inputs/tts/README.md     the voices and their licences

    Piper's default render draws random noise inside the model, so the same
    text gives a different render on every run. The files are therefore the
    frozen inputs: render never overwrites one without --force, and check
    confirms every file against the index. The hash is of the decoded
    samples (16-bit little-endian), not of the FLAC file, so it does not
    depend on the FLAC encoder. The voice models (about 60 MB
    each) are kept in the gitignored local/piper/.

      python tools/ttsgen.py fetch      download and verify the voices
      python tools/ttsgen.py render     render every missing file
      python tools/ttsgen.py check      verify the files against the index

    A prompt's voices column is "all", a list of voice keys, or empty for
    one voice chosen in turn within its category.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import wave
from fractions import Fraction

import numpy as np
import soundfile
from scipy.signal import resample_poly

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROMPTS = os.path.join(ROOT, "tools", "prompts.csv")
OUT = os.path.join(ROOT, "captures", "phase11", "inputs", "tts")
MODELS = os.path.join(ROOT, "local", "piper")
NATIVE = os.path.join(MODELS, "renders")
BASE = "https://huggingface.co/rhasspy/piper-voices/resolve/main/"

RATE = 8000
PEAK_DBFS = -3.0           # every file is scaled to this peak
SENTENCE_SILENCE = 0.3     # seconds between sentences within a prompt

# key: (model, speaker, dataset licence, dataset URL). The speaker is the
# model's speaker id, for the multi-speaker VCTK model only
VOICES = {
    "alba":    ("en/en_GB/alba/medium/en_GB-alba-medium", None,
                "CC BY 4.0", "https://datashare.ed.ac.uk/handle/10283/3270"),
    "cori":    ("en/en_GB/cori/medium/en_GB-cori-medium", None,
                "Public domain (LibriVox)", "https://librivox.org"),
    "nem":     ("en/en_GB/northern_english_male/medium/en_GB-northern_english_male-medium", None,
                "CC BY-SA 4.0", "http://www.openslr.org/83/"),
    "sef":     ("en/en_GB/southern_english_female/low/en_GB-southern_english_female-low", None,
                "CC BY-SA 4.0", "http://www.openslr.org/83/"),
    "vctk_lo": ("en/en_GB/vctk/medium/en_GB-vctk-medium", "p245",
                "CC BY 4.0", "https://datashare.ed.ac.uk/handle/10283/3443"),
    "vctk_hi": ("en/en_GB/vctk/medium/en_GB-vctk-medium", "p307",
                "CC BY 4.0", "https://datashare.ed.ac.uk/handle/10283/3443"),
    "joe":     ("en/en_US/joe/medium/en_US-joe-medium", None,
                "CC0", "https://github.com/OHF-Voice/voice-datasets"),
    "kristin": ("en/en_US/kristin/medium/en_US-kristin-medium", None,
                "Public domain (LibriVox)", "https://librivox.org"),
    "norman":  ("en/en_US/norman/medium/en_US-norman-medium", None,
                "Public domain (LibriVox)", "https://librivox.org"),
}

# SHA-256 of every model file, as downloaded on 2026-10-09
PINNED = {
    "en_GB-alba-medium.onnx": "401369c4a81d09fdd86c32c5c864440811dbdcc66466cde2d64f7133a66ad03b",
    "en_GB-alba-medium.onnx.json": "aa965a2f02ecced632c2694e1fc72bbff6d65f265fab567ca945918c73dd89f4",
    "en_GB-cori-medium.onnx": "1899f98e5fb8310154f3c2973f4b8a929ba7245e722b3d3a85680b833d95f10d",
    "en_GB-cori-medium.onnx.json": "e262c16d7f192f69d4edd6b4ef8a5915379e67495fcc402f1ab15eeb33da3d36",
    "en_GB-northern_english_male-medium.onnx": "57a219ae8e638873db7d18893304be5069c42868f392bb95c3ff17f0690d0689",
    "en_GB-northern_english_male-medium.onnx.json": "69557ed3d974463453e9b0c09dd99a7ed0e52b8b87b64b357dbeeb2540a97d47",
    "en_GB-southern_english_female-low.onnx": "f2f37aed1b3a093476f719d1379ba0c0b1b1cf6f1ef99288e2ebf502971a07c3",
    "en_GB-southern_english_female-low.onnx.json": "a1af43310f8756161506e849a2863d342a98b0853af9e2437581d39e9922d0e5",
    "en_GB-vctk-medium.onnx": "4e9fc85ab9009385319fc6bae7f55577f8a2d7ee77fd9159a5500eb6531f41e6",
    "en_GB-vctk-medium.onnx.json": "7f85e6391ed0f7f46e4abd19345929a16be931a0c9945086f96692dce2087fa8",
    "en_US-joe-medium.onnx": "58afce0321b8d9c46d7cdf9c16500cc55a793b4220212dba6b70fb788b3baf06",
    "en_US-joe-medium.onnx.json": "3d6d5410b3795cb1950595247ef8f06190719e6fdbfa3a2356d8ec368e1aad33",
    "en_US-kristin-medium.onnx": "5849957f929cbf720c258f8458692d6103fff2f0e3d3b19c8259474bb06a18d4",
    "en_US-kristin-medium.onnx.json": "5681426d4aead22195de70531eeeeddb46493cfaffc5764b2ea3db73428b651c",
    "en_US-norman-medium.onnx": "b9739443232a80a59c7d18810dd856899bf16a7964725f5ab81ea49b1351cb71",
    "en_US-norman-medium.onnx.json": "6c2db7f558a4a8deb9fe822583c1c5105f6c4e834dd0f9de8ad17a888ee9fe1d",
}

INDEX_FIELDS = ["id", "category", "voice", "text", "file", "seconds", "rms_dbfs",
                "voiced_pct", "f0_p10", "f0_p50", "f0_p90", "samples_sha256"]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def modelFiles(voice):
    name = os.path.basename(VOICES[voice][0])
    return [os.path.join(MODELS, name + ".onnx"), os.path.join(MODELS, name + ".onnx.json")]


def fetch():
    os.makedirs(MODELS, exist_ok=True)
    ok = True
    for model in sorted({v[0] for v in VOICES.values()}):
        for ext in (".onnx", ".onnx.json"):
            name = os.path.basename(model) + ext
            path = os.path.join(MODELS, name)
            if not os.path.exists(path):
                print(f"downloading {name}")
                urllib.request.urlretrieve(BASE + model + ext, path + ".part")
                os.replace(path + ".part", path)
            good = sha256(path) == PINNED[name]
            ok &= good
            print(f"  {name}: {'OK' if good else 'SHA-256 MISMATCH'}")
    return ok


def readPrompts():
    with open(PROMPTS, newline="") as f:
        rows = list(csv.DictReader(f))
    keys = list(VOICES)
    turn = {}
    out = []
    for r in rows:
        if r["voices"] == "all":
            voices = keys
        elif r["voices"]:
            voices = r["voices"].split()
        else:   # one voice in turn within the category
            n = turn.get(r["category"], 0)
            turn[r["category"]] = n + 1
            voices = [keys[n % len(keys)]]
        for v in voices:
            if v not in VOICES:
                sys.exit(f"{r['id']}: unknown voice {v}")
            out.append((r, v))
    return out


def promWords():
    """Every word in the PROM word tables (proms/*.json), as upper-case tokens."""
    words = set()
    for path in glob.glob(os.path.join(ROOT, "proms", "*.json")):
        for w in json.load(open(path)).get("words", []):
            words.update(re.findall(r"[A-Z']+", w["name"].upper()))
    return words


def readWav(path):
    with wave.open(path) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2
        return w.getframerate(), np.frombuffer(w.readframes(w.getnframes()), "<i2")


def writeFlac(path, x):
    soundfile.write(path, x.astype(np.int16), RATE, subtype="PCM_16", format="FLAC")


def readFlac(path):
    x, rate = soundfile.read(path, dtype="int16")
    return rate, x


def samplesSha256(x):
    return hashlib.sha256(x.astype("<i2").tobytes()).hexdigest()


def to8k(rate, x):
    """Resample to 8 kHz (polyphase, scipy's default anti-alias filter) and
    scale to PEAK_DBFS."""
    r = Fraction(RATE, rate)
    y = resample_poly(x.astype(float), r.numerator, r.denominator)
    y *= 32767 * 10 ** (PEAK_DBFS / 20) / np.max(np.abs(y))
    return np.round(y).astype(np.int16)


def measure(x):
    """Active level and pitch of an 8 kHz signal: 40 ms frames every 10 ms;
    a frame is active within 40 dB of the loudest, and voiced when its
    unbiased normalised autocorrelation, past its first zero crossing,
    peaks above 0.5 at a period of 2-20 ms."""
    x = x.astype(float) / 32768
    n, hop = 320, 80
    frames = [x[i:i + n] for i in range(0, len(x) - n, hop)]
    rms = np.array([np.sqrt(np.mean(f * f)) for f in frames])
    active = rms > rms.max() * 10 ** (-40 / 20)
    f0 = []
    for f, a in zip(frames, active):
        if not a:
            continue
        f = f - f.mean()
        ac = np.correlate(f, f, "full")[n - 1:]
        if ac[0] <= 0:
            continue
        ac = ac / ac[0] * n / (n - np.arange(n))
        neg = np.flatnonzero(ac[:161] < 0)   # skip the lobe around lag 0
        if not len(neg):
            continue
        lo = max(16, int(neg[0]))
        lag = lo + int(np.argmax(ac[lo:161]))
        if ac[lag] > 0.5:
            f0.append(RATE / lag)
    level = 20 * np.log10(np.sqrt(np.mean(rms[active] ** 2)))
    p = np.percentile(f0, [10, 50, 90]) if f0 else [0, 0, 0]
    return {"rms_dbfs": f"{level:.1f}", "voiced_pct": f"{100 * len(f0) / active.sum():.0f}",
            "f0_p10": f"{p[0]:.0f}", "f0_p50": f"{p[1]:.0f}", "f0_p90": f"{p[2]:.0f}"}


def piper(voice, text, path):
    model, speaker = modelFiles(voice)[0], VOICES[voice][1]
    cmd = [shutil.which("piper") or "piper", "-m", model, "-f", path,
           "--sentence-silence", str(SENTENCE_SILENCE)]
    if speaker is not None:
        ids = json.load(open(model + ".json"))["speaker_id_map"]
        cmd += ["-s", str(ids[speaker])]
    subprocess.run(cmd, input=text.encode(), check=True, stderr=subprocess.DEVNULL)


def indexRow(r, voice):
    name = f"{r['id']}_{voice}.flac"
    rate, x = readFlac(os.path.join(OUT, name))
    row = {"id": r["id"], "category": r["category"], "voice": voice, "text": r["text"],
           "file": name, "seconds": f"{len(x) / rate:.2f}", "samples_sha256": samplesSha256(x)}
    row.update(measure(x))
    return row


def readme():
    lines = ["# Encoder test speech (Phase 11)", "",
             "Rendered by `tools/ttsgen.py` from the prompts in `tools/prompts.csv` with "
             "[Piper](https://github.com/OHF-Voice/piper1-gpl) 1.4.2 and the voices below "
             "([rhasspy/piper-voices](https://huggingface.co/rhasspy/piper-voices)), then "
             f"resampled to {RATE} Hz, 16-bit mono FLAC (lossless), with peaks at {PEAK_DBFS:g} dBFS. "
             "Piper's render is random, so these files are the frozen inputs; "
             "`index.csv` gives the SHA-256 of each file's decoded samples "
             "(16-bit little-endian).", "",
             "Each voice is trained on the dataset shown, under that dataset's licence; "
             "the speech in these files is attributed to it.", "",
             "| Voice | Model | Speaker | Dataset licence | Dataset |", "|---|---|---|---|---|"]
    for k, (model, speaker, lic, url) in VOICES.items():
        lines.append(f"| `{k}` | `{os.path.basename(model)}` | {speaker or ''} | {lic} | {url} |")
    return "\n".join(lines) + "\n"


def render(force, only):
    missing = [f for v in VOICES for f in modelFiles(v) if not os.path.exists(f)]
    if missing:
        sys.exit("voice models missing: run python tools/ttsgen.py fetch")
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(NATIVE, exist_ok=True)
    clash = promWords()
    rows = []
    for r, voice in readPrompts():
        if r["category"] == "word" and r["text"].upper() in clash:
            print(f"warning: {r['id']} '{r['text']}' is a PROM word")
        path = os.path.join(OUT, f"{r['id']}_{voice}.flac")
        if (force or not os.path.exists(path)) and (not only or r["id"] in only):
            native = os.path.join(NATIVE, f"{r['id']}_{voice}.wav")
            piper(voice, r["text"], native)
            writeFlac(path, to8k(*readWav(native)))
            print(f"rendered {os.path.basename(path)}")
        if os.path.exists(path):
            rows.append(indexRow(r, voice))
    with open(os.path.join(OUT, "index.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, INDEX_FIELDS)
        w.writeheader()
        w.writerows(rows)
    with open(os.path.join(OUT, "README.md"), "w") as f:
        f.write(readme())
    print(f"{len(rows)} files in {os.path.relpath(OUT, ROOT)}/index.csv")


def check():
    with open(os.path.join(OUT, "index.csv"), newline="") as f:
        rows = list(csv.DictReader(f))
    bad = [r["file"] for r in rows
           if not os.path.exists(os.path.join(OUT, r["file"]))
           or samplesSha256(readFlac(os.path.join(OUT, r["file"]))[1]) != r["samples_sha256"]]
    expected = {f"{r['id']}_{v}.flac" for r, v in readPrompts()}
    absent = sorted(expected - {r["file"] for r in rows})
    for name in bad:
        print(f"MISMATCH {name}")
    for name in absent:
        print(f"not rendered {name}")
    print(f"{len(rows) - len(bad)} of {len(rows)} files OK, {len(absent)} prompts not rendered")
    return not bad and not absent


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("command", choices=["fetch", "render", "check"])
    p.add_argument("ids", nargs="*", help="render only these prompt ids")
    p.add_argument("--force", action="store_true", help="re-render existing files")
    a = p.parse_args()
    if a.command == "fetch":
        sys.exit(0 if fetch() else 1)
    if a.command == "render":
        render(a.force, set(a.ids))
    else:
        sys.exit(0 if check() else 1)


if __name__ == "__main__":
    main()
