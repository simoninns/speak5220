#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the encoder reference data in app/golden/encoder/.

The stages before level setting come from the reference Python encoder
(investigation/encoder/tms5220enc.py), which the C++ encoder re-implements
and matches but for its pitch anchor and onset rule (the pitch track and
voicing written are the Python's); the level setting and the frames come
from the C++ encoder itself, whose level setting, pitch anchor, onsets and
fades depart from the reference's (HLD §8 and §13).
On the phase 11 corpus (investigation/captures/phase11/inputs/) it writes
(application plan §1.4 and §3.2, Phase 2 task 1):

- inputs/NAME.wav: the WAV copies of a corpus subset (public-domain and CC0
  voices only), so the functional tests read WAV and never the FLAC
  originals.
- intermediates/NAME.json: for that subset with the reference's default
  options, encodeDetailed()'s intermediate values up to the plan (the
  filtered signals, the pitch track, the per-frame analysis, the plan of
  frame kinds, K and pitch), and in "encode" what the C++ encoder's level
  setting made of it (the faint-opening trim, the levels, the gains, the
  energies and the frames).
- frames/SET.tsv: the C++ encoder's frames of every corpus file for each
  option set in option_sets.tsv, packed. The C++ encoder gives the same
  frames whatever the thread count and the platform's rounding, so no
  frame is marked a near tie (near_ties "-").

The C++ part is a by-hand functional test
(EncoderReferenceWriter.DISABLED_WritesFramesAndLevelSetting), run on WAV
copies of the corpus (app/scripts/generate_corpus.py). Run by hand, in the
project's default Nix shell, from the repository root, with the functional
tests built:

    nix develop --command python3 app/scripts/golden/encoder.py
    nix develop --command python3 app/scripts/golden/encoder.py --check

--check regenerates everything into a temporary directory and compares it
with the committed files. Encoder reference data is regenerated only for an
intended improvement (AGENTS.md §3.1): ask before rewriting it.
"""

import argparse
import filecmp
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "investigation" / "encoder"
CORPUS = ROOT / "investigation" / "captures" / "phase11" / "inputs"
GOLDEN = ROOT / "app" / "golden" / "encoder"
SCRIPTS = ROOT / "app" / "scripts"
DEFAULT_BUILD = ROOT / "build" / "app"
ENCODER_H = ROOT / "app" / "src" / "core" / "encoder" / "encoder.h"
WRITER = "EncoderReferenceWriter.DISABLED_WritesFramesAndLevelSetting"

sys.path.insert(0, str(ENCODER))
import tms5220enc as enc  # noqa: E402

# Corpus subset: public-domain and CC0 voices only (see
# investigation/captures/phase11/inputs/tts/README.md), one or two of each
# category of prompt.
SUBSET = [
    "w07_joe",
    "w27_norman",
    "n01_kristin",
    "h01_joe",
    "h06_cori",
    "s01_norman",
    "s07_kristin",
    "s11_joe",
]

# The option sets every corpus file is encoded with, over the reference's
# defaults: the defaults, and three fixed combinations that between them
# change every option the reference and the application share. The
# reference's other options (targetPeak, peakLimit, repeats, iterations,
# fadeUnvoiced, compression) are fixed in the application.
OPTION_SETS = {
    "default": {},
    "lean": {
        "targetRms": 16.0,
        "repeatLambda": 30.0,
        "preEmphasis": 0.9,
        "voicingEvidence": 0.0,
    },
    "loud": {
        "targetRms": 40.0,
        "levelAgc": 4.0,
    },
    "plain": {
        "gateDb": 35.0,
        "window": 200,
        "lagWindowHz": 80.0,
        "voicingThreshold": 0.45,
    },
}

# Decimation of the long signals recorded in the intermediates.
SIGNAL_STEP = 16
# Hops whose CMNDF row and candidates are recorded: evenly spaced.
CMNDF_HOPS = 6


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def reference_commit():
    """The last commit that changed the encoder; refuses local edits."""
    paths = ["investigation/encoder/tms5220enc.py"]
    dirty = run(
        ["git", "status", "--porcelain", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if dirty:
        sys.exit("the reference encoder has local changes:\n" + dirty)
    return run(
        ["git", "log", "-1", "--format=%H", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()


def corpus_files():
    """{stem: path} of every corpus FLAC."""
    files = {p.stem: p for p in sorted(CORPUS.glob("*/*.flac"))}
    if len(files) != len(list(CORPUS.glob("*/*.flac"))):
        sys.exit("corpus file names are not unique")
    return files


def read_corpus(path):
    x, rate = sf.read(str(path), dtype="float64", always_2d=True)
    if rate != enc.RATE:
        sys.exit("%s is not 8 kHz" % path)
    return x.mean(axis=1)


def floats(values):
    return [float(v) for v in values]


def plan_json(plan):
    return [
        {
            "kind": p["kind"],
            "k": [int(v) for v in p.get("k", [])],
            "pitch": int(p.get("pitch", 0)),
        }
        for p in plan
    ]


def intermediates(stem, x_in):
    """encodeDetailed()'s intermediates up to the plan for one file, default
    options; the C++ encoder adds "encode"."""
    opts = dict(enc.DEFAULTS)
    an = enc.analyse(x_in, enc.RATE, **opts)
    x = an["x"]
    xl = enc._sosfiltfilt(enc._butter("lowpass", 1000.0), x)
    centres, dn, _ = enc._yin(xl)
    hops, periods = enc.trackPitch(x, opts)
    tilt, crossings = enc._tiltAndCrossings(x, hops)
    chosen = sorted(
        {round(i * (len(hops) - 1) / (CMNDF_HOPS - 1)) for i in range(CMNDF_HOPS)}
    )

    feats = an["feats"]
    # The first half of _kindsAndK: each frame's own K and prediction error
    own = []
    for f in feats:
        if f["gated"]:
            own.append(None)
            continue
        r = np.asarray(f["rv"] if f["voiced"] else f["ru"], dtype=float)
        order = 10 if f["voiced"] else 4
        refl = enc.levinson(r, order)
        kidx, err = enc.quantiseK(refl, r)
        own.append(
            {
                "reflection": floats(refl),
                "k": [int(v) for v in kidx],
                "error": float(err),
            }
        )
    plan = enc._kindsAndK(feats, opts)

    return {
        "name": stem,
        "sample_rate": enc.RATE,
        "samples": len(x_in),
        "highpass": {"step": SIGNAL_STEP, "values": floats(x[::SIGNAL_STEP])},
        "lowpass": {"step": SIGNAL_STEP, "values": floats(xl[::SIGNAL_STEP])},
        "pitch": {
            "hops": [int(h) for h in hops],
            "periods": floats(periods),
            "tilt": floats(tilt),
            "crossings": floats(crossings),
            "cmndf_hops": chosen,
            "cmndf": [floats(dn[h]) for h in chosen],
            "candidates": [[floats(c) for c in enc._candidates(dn[h])] for h in chosen],
        },
        "analysis": {
            "gate": float(an["gate"]),
            "loud": float(an["loud"]),
            "onset": int(an["onset"]),
            "offset": int(an["offset"]),
            "c0": int(an["c0"]),
            "frames": [
                {
                    "centre": int(f["centre"]),
                    "level": float(f["level"]),
                    "voiced": bool(f["voiced"]),
                    "period": float(f["period"]),
                    "gated": bool(f["gated"]),
                    "rv": floats(f["rv"]),
                    "ru": floats(f["ru"]),
                    "own": o,
                }
                for f, o in zip(feats, own)
            ],
        },
        "plan": plan_json(plan),
    }


def encoder_version():
    """The C++ encoder's kEncoderVersion."""
    m = re.search(r'kEncoderVersion = "([^"]+)"', ENCODER_H.read_text())
    if not m:
        sys.exit("no kEncoderVersion in %s" % ENCODER_H.relative_to(ROOT))
    return m.group(1)


def write_cpp(out, build, corpus):
    """Runs the C++ writer: frames/SET.tsv and each input's "encode"
    section, merged into its intermediates."""
    tests = build / "tests" / "speak5220_functional_tests"
    if not tests.exists():
        sys.exit(
            "%s not found; build it with: cmake --build %s "
            "--target speak5220_functional_tests" % (tests, build)
        )
    run([sys.executable, str(SCRIPTS / "generate_corpus.py"), "--output", str(corpus)])
    env = dict(
        os.environ,
        SPEAK5220_CORPUS_DIR=str(corpus),
        SPEAK5220_ENCODER_REFERENCE_DIR=str(out),
    )
    run(
        [str(tests), "--gtest_also_run_disabled_tests", "--gtest_filter=" + WRITER],
        env=env,
        stdout=subprocess.DEVNULL,
    )
    for stem in SUBSET:
        path = out / "intermediates" / (stem + ".json")
        data = json.loads(path.read_text())
        data["encode"] = json.loads((out / "encode" / (stem + ".json")).read_text())
        path.write_text(json.dumps(data, indent=1, sort_keys=True) + "\n")
    shutil.rmtree(out / "encode")


def generate(out, build, corpus):
    commit = reference_commit()
    files = corpus_files()
    missing = [s for s in SUBSET if s not in files]
    if missing:
        sys.exit("subset files missing: " + ", ".join(missing))
    for sub in ("inputs", "intermediates"):
        (out / sub).mkdir(parents=True, exist_ok=True)

    for stem in SUBSET:
        data, rate = sf.read(str(files[stem]), dtype="int16", always_2d=True)
        sf.write(str(out / "inputs" / (stem + ".wav")), data, rate, subtype="PCM_16")
        x = read_corpus(files[stem])
        text = json.dumps(intermediates(stem, x), indent=1, sort_keys=True)
        (out / "intermediates" / (stem + ".json")).write_text(text + "\n")

    sets = ["name\toptions"]
    sets += [
        "%s\t%s" % (k, json.dumps(v, sort_keys=True)) for k, v in OPTION_SETS.items()
    ]
    (out / "option_sets.tsv").write_text("\n".join(sets) + "\n")

    write_cpp(out, build, corpus)

    source = ENCODER / "tms5220enc.py"
    ref = [
        "reference_commit\t" + commit,
        "sha256\t%s\t%s"
        % (hashlib.sha256(source.read_bytes()).hexdigest(), source.relative_to(ROOT)),
        "numpy\t" + np.__version__,
    ]
    import scipy

    ref.append("scipy\t" + scipy.__version__)
    ref.append("encoder_version\t" + encoder_version())
    (out / "REFERENCE.tsv").write_text("\n".join(ref) + "\n")


def compare(a, b):
    """Lists the files that differ between two trees."""
    diff = []

    def walk(c):
        diff.extend(
            str(Path(c.left, f).relative_to(a))
            for f in c.diff_files + c.left_only + c.right_only
        )
        for sub in c.subdirs.values():
            walk(sub)

    walk(filecmp.dircmp(a, b))
    return diff


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--check",
        action="store_true",
        help="regenerate into a temporary directory and compare",
    )
    ap.add_argument(
        "--build",
        type=Path,
        default=DEFAULT_BUILD,
        help="the build directory, with the functional tests (default: %(default)s)",
    )
    args = ap.parse_args()
    corpus = args.build / "generated-corpus"
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp), args.build, corpus)
            diff = compare(Path(tmp), GOLDEN)
            if diff:
                print("differs from app/golden/encoder/:\n  " + "\n  ".join(diff))
                return 1
            print("app/golden/encoder/ is reproduced exactly")
    else:
        shutil.rmtree(GOLDEN, ignore_errors=True)
        generate(GOLDEN, args.build, corpus)
        print("wrote", GOLDEN.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
