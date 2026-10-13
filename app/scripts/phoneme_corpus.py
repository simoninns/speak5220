#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Renders the phoneme test set: words and sentences that isolate S, SH, CH, K, T and F.

Every prompt of phoneme_prompts.csv (its consonant, and whether the
consonant starts or ends the word) is rendered with a text-to-speech
engine in each of its voices, resampled to 8 kHz and written as 16-bit WAV
with peaks at -3 dBFS, as investigation/tools/ttsgen.py renders the
encoder's test speech:

    OUTPUT/ENGINE/ID_VOICE.wav
    OUTPUT/ENGINE/index.tsv     file, prompt, voice, text, consonant,
                                position and the SHA-256 of the samples

The engines:

    piper   Piper (Linux; in the default Nix shell there) with four of
            ttsgen.py's voices, their models pinned by SHA-256 and kept in
            the gitignored local/piper/ ("fetch" downloads them). Piper's
            render is random, so a render is never repeated without
            --force: the files are the frozen inputs.
    say     macOS's say, with the voices the encoder-quality investigation
            used (docs-tech/encoder-quality-investigation.md §3.2).

The files go into the gitignored local/phoneme-corpus/ by default. "check"
confirms them against the index. phoneme-check.sh encodes them and reports
on each consonant (docs-tech/encoder-quality-plan.md, Phase 1).

    nix develop --command python3 app/scripts/phoneme_corpus.py render --engine say
    nix develop --command python3 app/scripts/phoneme_corpus.py check --engine say
"""

import argparse
import csv
import hashlib
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from fractions import Fraction
from pathlib import Path

import numpy as np
import soundfile
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[2]
PROMPTS = Path(__file__).resolve().parent / "phoneme_prompts.csv"
DEFAULT_OUTPUT = ROOT / "local" / "phoneme-corpus"
MODELS = ROOT / "local" / "piper"
PIPER_BASE = "https://huggingface.co/rhasspy/piper-voices/resolve/main/"

RATE = 8000
PEAK_DBFS = -3.0
SENTENCE_SILENCE = 0.3  # Piper's pause between sentences, in seconds

# Four of ttsgen.py's voices, and the SHA-256 of their model files as
# downloaded on 2026-10-09 (ttsgen.py PINNED).
PIPER_VOICES = {
    "alba": "en/en_GB/alba/medium/en_GB-alba-medium",
    "nem": "en/en_GB/northern_english_male/medium/en_GB-northern_english_male-medium",
    "joe": "en/en_US/joe/medium/en_US-joe-medium",
    "kristin": "en/en_US/kristin/medium/en_US-kristin-medium",
}
PINNED = {
    "en_GB-alba-medium.onnx": "401369c4a81d09fdd86c32c5c864440811dbdcc66466cde2d64f7133a66ad03b",
    "en_GB-alba-medium.onnx.json": "aa965a2f02ecced632c2694e1fc72bbff6d65f265fab567ca945918c73dd89f4",
    "en_GB-northern_english_male-medium.onnx": "57a219ae8e638873db7d18893304be5069c42868f392bb95c3ff17f0690d0689",
    "en_GB-northern_english_male-medium.onnx.json": "69557ed3d974463453e9b0c09dd99a7ed0e52b8b87b64b357dbeeb2540a97d47",
    "en_US-joe-medium.onnx": "58afce0321b8d9c46d7cdf9c16500cc55a793b4220212dba6b70fb788b3baf06",
    "en_US-joe-medium.onnx.json": "3d6d5410b3795cb1950595247ef8f06190719e6fdbfa3a2356d8ec368e1aad33",
    "en_US-kristin-medium.onnx": "5849957f929cbf720c258f8458692d6103fff2f0e3d3b19c8259474bb06a18d4",
    "en_US-kristin-medium.onnx.json": "5681426d4aead22195de70531eeeeddb46493cfaffc5764b2ea3db73428b651c",
}

# macOS voices, by the key used in file names.
SAY_VOICES = {
    "daniel": "Daniel",
    "fred": "Fred",
    "eddy": "Eddy (English (UK))",
    "flo": "Flo (English (UK))",
}

INDEX_FIELDS = [
    "file",
    "prompt",
    "voice",
    "text",
    "consonant",
    "position",
    "samples_sha256",
]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def samples_sha256(x):
    return hashlib.sha256(x.astype("<i2").tobytes()).hexdigest()


def model_path(voice):
    return MODELS / (Path(PIPER_VOICES[voice]).name + ".onnx")


def fetch():
    MODELS.mkdir(parents=True, exist_ok=True)
    ok = True
    for model in PIPER_VOICES.values():
        for ext in (".onnx", ".onnx.json"):
            name = Path(model).name + ext
            path = MODELS / name
            if not path.exists():
                print(f"downloading {name}")
                urllib.request.urlretrieve(
                    PIPER_BASE + model + ext, str(path) + ".part"
                )
                Path(str(path) + ".part").replace(path)
            good = sha256_file(path) == PINNED[name]
            ok &= good
            print(f"  {name}: {'OK' if good else 'SHA-256 MISMATCH'}")
    return ok


def render_piper(voice, text, path):
    model = model_path(voice)
    if not model.exists():
        sys.exit("voice models missing: run phoneme_corpus.py fetch")
    subprocess.run(
        [
            shutil.which("piper") or "piper",
            "-m",
            str(model),
            "-f",
            str(path),
            "--sentence-silence",
            str(SENTENCE_SILENCE),
        ],
        input=text.encode(),
        check=True,
        stderr=subprocess.DEVNULL,
    )


def render_say(voice, text, path):
    subprocess.run(["say", "-v", SAY_VOICES[voice], "-o", str(path), text], check=True)


ENGINES = {
    "piper": (PIPER_VOICES, render_piper, ".wav"),
    "say": (SAY_VOICES, render_say, ".aiff"),
}


def to8k(x, rate):
    """Mono, resampled to 8 kHz (scipy's polyphase default) and scaled to
    PEAK_DBFS, as 16-bit integers."""
    if x.ndim > 1:
        x = x.mean(axis=1)
    r = Fraction(RATE, rate).limit_denominator(1000)
    y = resample_poly(x.astype(float), r.numerator, r.denominator)
    y *= 32767 * 10 ** (PEAK_DBFS / 20) / np.max(np.abs(y))
    return np.round(y).astype(np.int16)


def read_prompts():
    with open(PROMPTS, newline="") as f:
        return list(csv.DictReader(f))


def index_path(directory):
    return directory / "index.tsv"


def render(engine, output, force):
    voices, speak, native_ext = ENGINES[engine]
    directory = output / engine
    directory.mkdir(parents=True, exist_ok=True)
    rows = []
    with tempfile.TemporaryDirectory() as scratch:
        for prompt in read_prompts():
            for voice in voices:
                name = f"{prompt['id']}_{voice}.wav"
                path = directory / name
                if force or not path.exists():
                    native = Path(scratch) / ("native" + native_ext)
                    speak(voice, prompt["text"], native)
                    x, rate = soundfile.read(str(native), dtype="float64")
                    soundfile.write(str(path), to8k(x, rate), RATE, subtype="PCM_16")
                    print(f"rendered {name}")
                samples, _ = soundfile.read(str(path), dtype="int16")
                rows.append(
                    {
                        "file": name,
                        "prompt": prompt["id"],
                        "voice": voice,
                        "text": prompt["text"],
                        "consonant": prompt["consonant"],
                        "position": prompt["position"],
                        "samples_sha256": samples_sha256(samples),
                    }
                )
    with open(index_path(directory), "w", newline="") as f:
        w = csv.DictWriter(f, INDEX_FIELDS, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    print(f"{len(rows)} files in {index_path(directory)}")


def check(engine, output):
    directory = output / engine
    with open(index_path(directory), newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    bad = []
    for r in rows:
        path = directory / r["file"]
        if (
            not path.exists()
            or samples_sha256(soundfile.read(str(path), dtype="int16")[0])
            != r["samples_sha256"]
        ):
            bad.append(r["file"])
    expected = {
        f"{p['id']}_{v}.wav" for p in read_prompts() for v in ENGINES[engine][0]
    }
    absent = sorted(expected - {r["file"] for r in rows})
    for name in bad:
        print(f"MISMATCH {name}")
    for name in absent:
        print(f"not rendered {name}")
    print(
        f"{len(rows) - len(bad)} of {len(rows)} files OK, {len(absent)} prompts not rendered"
    )
    return not bad and not absent


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["fetch", "render", "check"])
    ap.add_argument(
        "--engine",
        choices=sorted(ENGINES),
        default="piper",
        help="the text-to-speech engine (default: %(default)s)",
    )
    ap.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="where the files go, in a directory per engine (default: %(default)s)",
    )
    ap.add_argument("--force", action="store_true", help="render existing files again")
    args = ap.parse_args()
    if args.command == "fetch":
        return 0 if fetch() else 1
    if args.command == "render":
        render(args.engine, args.output, args.force)
        return 0
    return 0 if check(args.engine, args.output) else 1


if __name__ == "__main__":
    sys.exit(main())
