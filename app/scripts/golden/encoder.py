#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the encoder reference data in app/golden/encoder/.

The frames and the level setting come from the C++ encoder; the stages
before level setting, in the intermediates, are the retired Python
encoder's, kept as committed. On the speech corpus
(investigation/captures/phase11/inputs/) it writes:

- inputs/NAME.wav: the WAV copies of a corpus subset (public-domain and CC0
  voices only), so the functional tests read WAV and never the FLAC
  originals.
- intermediates/NAME.json: for that subset, the Python encoder's
  intermediate values up to the plan with its default options (the
  filtered signals, the pitch track, the per-frame analysis, the plan of
  frame kinds, K and pitch), as committed, and in "encode" what the C++
  encoder's level setting made of them (the faint-opening trim, the levels,
  the gains, the energies and the frames).
- frames/SET.tsv: the C++ encoder's frames of every corpus file for each
  option set in option_sets.tsv, packed. The C++ encoder gives the same
  frames whatever the thread count and the platform's rounding, so no
  frame is marked a near tie (near_ties "-").
- REFERENCE.tsv: the Python encoder's commit, source hash and NumPy and
  SciPy versions, as committed, and the C++ encoder's version.

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
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import soundfile as sf

ROOT = Path(__file__).resolve().parents[3]
CORPUS = ROOT / "investigation" / "captures" / "phase11" / "inputs"
GOLDEN = ROOT / "app" / "golden" / "encoder"
SCRIPTS = ROOT / "app" / "scripts"
DEFAULT_BUILD = ROOT / "build" / "app"
ENCODER_H = ROOT / "app" / "src" / "core" / "encoder" / "encoder.h"
WRITER = "EncoderReferenceWriter.DISABLED_WritesFramesAndLevelSetting"

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

# The option sets every corpus file is encoded with, over the defaults: the
# defaults, and three fixed combinations that between them change every
# option the Python encoder and the application share, under the Python
# encoder's names (EncoderOptionInfo::key).
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


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def corpus_files():
    """{stem: path} of every corpus FLAC."""
    files = {p.stem: p for p in sorted(CORPUS.glob("*/*.flac"))}
    if len(files) != len(list(CORPUS.glob("*/*.flac"))):
        sys.exit("corpus file names are not unique")
    return files


def python_stages(stem):
    """The committed intermediates of one file without the C++ encoder's
    "encode": the Python encoder's stages up to the plan."""
    data = json.loads((GOLDEN / "intermediates" / (stem + ".json")).read_text())
    data.pop("encode", None)
    return data


def python_reference():
    """The committed REFERENCE.tsv lines on the Python encoder."""
    lines = (GOLDEN / "REFERENCE.tsv").read_text().splitlines()
    return [x for x in lines if not x.startswith("encoder_version\t")]


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
    files = corpus_files()
    missing = [s for s in SUBSET if s not in files]
    if missing:
        sys.exit("subset files missing: " + ", ".join(missing))
    for sub in ("inputs", "intermediates"):
        (out / sub).mkdir(parents=True, exist_ok=True)

    for stem in SUBSET:
        data, rate = sf.read(str(files[stem]), dtype="int16", always_2d=True)
        sf.write(str(out / "inputs" / (stem + ".wav")), data, rate, subtype="PCM_16")
        text = json.dumps(python_stages(stem), indent=1, sort_keys=True)
        (out / "intermediates" / (stem + ".json")).write_text(text + "\n")

    sets = ["name\toptions"]
    sets += [
        "%s\t%s" % (k, json.dumps(v, sort_keys=True)) for k, v in OPTION_SETS.items()
    ]
    (out / "option_sets.tsv").write_text("\n".join(sets) + "\n")

    ref = python_reference() + ["encoder_version\t" + encoder_version()]

    write_cpp(out, build, corpus)
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
        # The committed intermediates are read while writing the new ones.
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp) / "encoder", args.build, corpus)
            shutil.rmtree(GOLDEN)
            shutil.copytree(Path(tmp) / "encoder", GOLDEN)
        print("wrote", GOLDEN.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
