#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the encoder reference data in app/golden/encoder/.

Runs the reference Python encoder (investigation/encoder/tms5220enc.py) on
the phase 11 corpus (investigation/captures/phase11/inputs/) and writes what
it produces (application plan §1.4 and §3.2, Phase 2 task 1):

- inputs/NAME.wav: the WAV copies of a corpus subset (public-domain and CC0
  voices only), so the functional tests read WAV and never the FLAC
  originals.
- intermediates/NAME.json: encodeDetailed()'s intermediate values for that
  subset with the default options: the filtered signals, the pitch track,
  the per-frame analysis, the plan of frame kinds, K and pitch, the gains
  and the energies.
- frames/SET.tsv: the frames of every corpus file for each option set in
  option_sets.tsv, packed, with the file's near-tie frames.

A near-tie frame is one whose coding changes when the input is disturbed at
the level of floating-point noise (a relative 1e-9): the encoder's decision
there rests on a margin smaller than the differences between two correct
implementations. Each file is encoded once as given and once per
disturbance in DISTURBANCES; a frame that differs in any of those runs is a
near-tie frame. A file whose frame count changes has every frame marked.

Run by hand, in the project's default Nix shell, from the repository root:

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
import multiprocessing
import os
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

# The option sets every corpus file is encoded with: the defaults, and
# three fixed combinations that between them change every option the
# application has. The reference's other options (targetPeak, peakLimit,
# repeats, iterations, fadeUnvoiced, compression) stay at their defaults,
# at which the application fixes them.
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

# Disturbances for the near-tie search: (seed, scale). The input is scaled
# by 1 + scale and given added noise of relative size 1e-9 from the seed
# (no noise for seed None).
DISTURBANCE = 1e-9
DISTURBANCES = [(None, DISTURBANCE), (1, 0.0), (2, 0.0)]

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


def disturbed(x, seed, scale):
    y = x * (1.0 + scale)
    if seed is not None:
        rms = float(np.sqrt(np.mean(x * x))) or 1.0
        y = y + np.random.default_rng(seed).standard_normal(len(x)) * rms * DISTURBANCE
    return y


def near_ties(frames, others):
    """Frame indices that differ between `frames` and any of `others`."""
    marked = set()
    for other in others:
        if len(other) != len(frames):
            return list(range(len(frames)))
        marked.update(i for i, (a, b) in enumerate(zip(frames, other)) if a != b)
    return sorted(marked)


def encode_job(job):
    """(set, stem, hex, frame count, near-tie frames) for one file and set."""
    set_name, stem, path = job
    opts = OPTION_SETS[set_name]
    x = read_corpus(path)
    frames = enc.encode(x, enc.RATE, **opts)
    others = [enc.encode(disturbed(x, s, k), enc.RATE, **opts) for s, k in DISTURBANCES]
    return (
        set_name,
        stem,
        enc.pack(frames).hex(),
        len(frames),
        near_ties(frames, others),
    )


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
    """encodeDetailed()'s intermediates for one file, default options."""
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

    # The energies _setLevels returns, captured on the way out
    calls = []
    set_levels = enc._setLevels

    def recording(*args, **kwargs):
        e = set_levels(*args, **kwargs)
        calls.append([int(v) for v in e])
        return e

    enc._setLevels = recording
    try:
        frames, info = enc.encodeDetailed(x_in, enc.RATE)
    finally:
        enc._setLevels = set_levels
    final_plan = info["plan"]
    lead = an["centres"].index(info["c0"])
    eff = enc._effectiveK(final_plan)
    gains = []
    for j, p in enumerate(final_plan):
        if p["kind"] == "silence":
            gains.append(0.0)
            continue
        per = enc.PITCH[p["pitch"]] + 1 if p["pitch"] else 0
        gains.append(float(enc._gainOf(eff[j], per)))

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
        "encode": {
            "lead": lead,
            "dropped": int(info["stats"]["droppedOpening"]),
            "plan": plan_json(final_plan),
            "levels": floats(f["level"] for f in info["feats"]),
            "gain": float(info["gain"]),
            "gains": gains,
            "energies": calls[-1],
            "silenced": int(info["stats"]["silenced"]),
            "limited": int(info["stats"]["limited"]),
            "sounding": int(info["stats"]["sounding"]),
            "frames_hex": enc.pack(frames).hex(),
            "frame_count": len(frames),
        },
    }


def generate(out, processes):
    commit = reference_commit()
    files = corpus_files()
    missing = [s for s in SUBSET if s not in files]
    if missing:
        sys.exit("subset files missing: " + ", ".join(missing))
    for sub in ("inputs", "intermediates", "frames"):
        (out / sub).mkdir(parents=True, exist_ok=True)

    for stem in SUBSET:
        data, rate = sf.read(str(files[stem]), dtype="int16", always_2d=True)
        sf.write(str(out / "inputs" / (stem + ".wav")), data, rate, subtype="PCM_16")
        x = read_corpus(files[stem])
        text = json.dumps(intermediates(stem, x), indent=1, sort_keys=True)
        (out / "intermediates" / (stem + ".json")).write_text(text + "\n")

    jobs = [(s, stem, path) for s in OPTION_SETS for stem, path in files.items()]
    with multiprocessing.Pool(processes) as pool:
        results = pool.map(encode_job, jobs, chunksize=1)
    for set_name in OPTION_SETS:
        lines = ["name\tframes\tnear_ties\tpacked"]
        for s, stem, packed, count, ties in results:
            if s == set_name:
                tie_text = ",".join(str(i) for i in ties) if ties else "-"
                lines.append("%s\t%d\t%s\t%s" % (stem, count, tie_text, packed))
        (out / "frames" / (set_name + ".tsv")).write_text("\n".join(lines) + "\n")

    sets = ["name\toptions"]
    sets += [
        "%s\t%s" % (k, json.dumps(v, sort_keys=True)) for k, v in OPTION_SETS.items()
    ]
    (out / "option_sets.tsv").write_text("\n".join(sets) + "\n")

    source = ENCODER / "tms5220enc.py"
    ref = [
        "reference_commit\t" + commit,
        "sha256\t%s\t%s"
        % (hashlib.sha256(source.read_bytes()).hexdigest(), source.relative_to(ROOT)),
        "numpy\t" + np.__version__,
    ]
    import scipy

    ref.append("scipy\t" + scipy.__version__)
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
        "--jobs",
        type=int,
        default=os.cpu_count() or 1,
        help="encoder processes to run in parallel (default: one per CPU)",
    )
    args = ap.parse_args()
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp), args.jobs)
            diff = compare(Path(tmp), GOLDEN)
            if diff:
                print("differs from app/golden/encoder/:\n  " + "\n  ".join(diff))
                return 1
            print("app/golden/encoder/ is reproduced exactly")
    else:
        shutil.rmtree(GOLDEN, ignore_errors=True)
        generate(GOLDEN, args.jobs)
        print("wrote", GOLDEN.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
