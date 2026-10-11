#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the quality reference data in app/golden/quality/.

Measures the reference encodings of the phase 11 corpus (the packed frames
in app/golden/encoder/frames/SET.tsv) as the retired quality tools measure
them (application plan §3.3, Phase 3 task 1):

- scores/SET.tsv, one row per corpus file for each option set:
  - lag: the samples by which the emulator's output is delayed to line up
    with the input (investigation/tools/speechqual.py align() and
    refine());
  - samples: the length of that output, from its first non-idle code to
    its last;
  - pesq: PESQ as speechqual.py measures it: ITU-T P.862 narrowband as
    P.862.1 MOS-LQO, the input after the 60 Hz high-pass against the
    emulator's output, aligned and level-matched;
  - level_err, level_bias, level_p90, spec_dist: the level error and the
    mel spectral distance of investigation/encoder/evaluate.py measure()
    (lvMeanAbs, lvBias, lvP90 and specDist), in dB.
- anchor.tsv, one row per corpus file: the PESQ of speechqual.py's anchor,
  a floating-point LPC-10 vocoder of the same input (speechqual.py
  --anchor), with its noise from seed 1.

A score the PESQ package cannot compute is written as nan.

Run by hand, in the project's default Nix shell, from the repository root,
after building the reference emulator (make -C investigation/emulator):

    nix develop --command python3 app/scripts/golden/quality.py
    nix develop --command python3 app/scripts/golden/quality.py --check

--check regenerates everything into a temporary directory and compares it
with the committed files. Ask before rewriting the reference data
(AGENTS.md §3.1).
"""

import argparse
import filecmp
import hashlib
import importlib.metadata
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / "investigation" / "tools"
ENCODER = ROOT / "investigation" / "encoder"
EMULATOR = ROOT / "investigation" / "emulator"
CORPUS = ROOT / "investigation" / "captures" / "phase11" / "inputs"
FRAMES = ROOT / "app" / "golden" / "encoder" / "frames"
OPTION_SETS = ROOT / "app" / "golden" / "encoder" / "option_sets.tsv"
GOLDEN = ROOT / "app" / "golden" / "quality"

sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ENCODER))
import evaluate  # noqa: E402
import speechqual as sq  # noqa: E402

# The reference sources the data depends on.
SOURCES = [
    TOOLS / "speechqual.py",
    TOOLS / "model.py",
    ENCODER / "evaluate.py",
    ENCODER / "tms5220enc.py",
    EMULATOR / "tms5220.c",
    EMULATOR / "tms6100.c",
    EMULATOR / "tms5220emu.c",
]

# evaluate.py measure()'s columns, and the names they are written under.
EVALUATE_COLUMNS = [
    ("lvMeanAbs", "level_err"),
    ("lvBias", "level_bias"),
    ("lvP90", "level_p90"),
    ("specDist", "spec_dist"),
]

ANCHOR_SEED = 1


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def reference_commit():
    """The last commit that changed the references; refuses local edits."""
    paths = [str(p.relative_to(ROOT)) for p in SOURCES]
    dirty = run(
        ["git", "status", "--porcelain", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if dirty:
        sys.exit("the reference tools have local changes:\n" + dirty)
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


def option_sets():
    """{set name: options} from the encoder's option_sets.tsv."""
    import json

    rows = OPTION_SETS.read_text().splitlines()[1:]
    return {r.split("\t")[0]: json.loads(r.split("\t")[1]) for r in rows}


def packed_frames(set_name):
    """{stem: packed bytes} of one option set's reference encodings."""
    rows = (FRAMES / (set_name + ".tsv")).read_text().splitlines()[1:]
    return {r.split("\t")[0]: bytes.fromhex(r.split("\t")[3]) for r in rows}


def number(v):
    """A float as the shortest text that reads back exactly."""
    return repr(float(v))


def reference_input(path):
    """The input as speechqual.py measure() reads it, after the 60 Hz
    high-pass."""
    import scipy.signal

    x = sq.readAudio(str(path))
    sos = scipy.signal.butter(2, 60, "highpass", fs=sq.RATE, output="sos")
    return x, scipy.signal.sosfiltfilt(sos, x)


def pesq_of(ref, deg):
    import pesq

    try:
        return float(pesq.pesq(sq.RATE, ref, deg, "nb"))
    except Exception:
        return float("nan")


def score_job(job):
    """One encoding's row: lag, samples, PESQ and evaluate.py's measures."""
    set_name, stem, path, data, opts = job
    _, ref = reference_input(path)
    with tempfile.TemporaryDirectory() as tmp:
        codes = sq.emulate(data, tmp)
    out = codes.astype(float) + 1  # idle (code -1) at 0, as measure() does
    lag = sq.refine(ref, out, sq.align(ref, out))
    deg = sq.matchLevel(ref, sq.placed(out, lag, len(ref)))
    m = evaluate.measure(str(path), {}, **opts)
    row = [stem, str(lag), str(len(codes)), number(pesq_of(ref, deg))]
    row += [number(m[k]) for k, _ in EVALUATE_COLUMNS]
    return set_name, "\t".join(row)


def anchor_job(job):
    stem, path = job
    _, ref = reference_input(path)
    voc = sq.matchLevel(ref, sq.lpcVocoder(ref, seed=ANCHOR_SEED))
    return "%s\t%s" % (stem, number(pesq_of(ref, voc)))


def generate(out, processes):
    commit = reference_commit()
    if not (EMULATOR / "tms5220emu").exists():
        sys.exit("build the reference emulator first: make -C investigation/emulator")
    files = corpus_files()
    sets = option_sets()
    (out / "scores").mkdir(parents=True, exist_ok=True)

    jobs = []
    for set_name, opts in sets.items():
        packed = packed_frames(set_name)
        if set(packed) != set(files):
            sys.exit("frames/%s.tsv does not cover the corpus" % set_name)
        jobs += [(set_name, s, files[s], packed[s], opts) for s in files]
    with multiprocessing.Pool(processes) as pool:
        scores = pool.map(score_job, jobs, chunksize=1)
        anchors = pool.map(anchor_job, list(files.items()), chunksize=1)

    header = "name\tlag\tsamples\tpesq\t" + "\t".join(n for _, n in EVALUATE_COLUMNS)
    for set_name in sets:
        lines = [header] + [r for s, r in scores if s == set_name]
        (out / "scores" / (set_name + ".tsv")).write_text("\n".join(lines) + "\n")
    (out / "anchor.tsv").write_text("\n".join(["name\tpesq"] + anchors) + "\n")

    ref = ["reference_commit\t" + commit]
    ref += [
        "sha256\t%s\t%s"
        % (hashlib.sha256(p.read_bytes()).hexdigest(), p.relative_to(ROOT))
        for p in SOURCES
    ]
    import scipy

    ref += [
        "numpy\t" + np.__version__,
        "scipy\t" + scipy.__version__,
        "pesq\t" + importlib.metadata.version("pesq"),
        "anchor_seed\t%d" % ANCHOR_SEED,
    ]
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

    walk(filecmp.dircmp(a, b, shallow=False))
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
        help="processes to run in parallel (default: one per CPU)",
    )
    args = ap.parse_args()
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp), args.jobs)
            diff = compare(Path(tmp), GOLDEN)
            if diff:
                print("differs from app/golden/quality/:\n  " + "\n  ".join(diff))
                return 1
            print("app/golden/quality/ is reproduced exactly")
    else:
        shutil.rmtree(GOLDEN, ignore_errors=True)
        generate(GOLDEN, args.jobs)
        print("wrote", GOLDEN.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
