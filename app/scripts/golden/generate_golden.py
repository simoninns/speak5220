#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the emulator's runs in app/golden/emulator/.

Runs the reference C emulator (investigation/emulator, built here into a
temporary directory) on the committed inputs of app/golden/emulator/ and
writes, for each run listed in runs.tsv, the output codes and the full event
log (application plan §1.4, Phase 1 tasks 4-6). The inputs are the scenario
scripts (copied from app/scripts/golden/scenarios/) with the images they
load, and the phrases: a subset of the synthetic vectors and the retired
Python encoder's output for a corpus subset, each with its frames as CSV.

runs.tsv's synth_matches column and REFERENCE.tsv record the reference data's
origin: whether the Python encoder's synthesiser matched the emulator on each
phrase, and the reference commit and the hashes of the sources. They are
kept as committed; the script refuses to run if the emulator's sources no
longer match their recorded hashes. app/golden/packaging/ (the Python
encoder's images and streams) and app/golden/encoder/ (written by encoder.py)
are not touched.

The application's build and tests read only the files written here; they
never run anything in investigation/.

Run by hand, in the project's default Nix shell, from the repository root:

    nix develop --command python3 app/scripts/golden/generate_golden.py
    nix develop --command python3 app/scripts/golden/generate_golden.py --check

--check runs the emulator into a temporary directory and compares the runs
and the scenario scripts with the committed files. The emulator's reference
data never changes unless the reference datasheet does (AGENTS.md §3.1): ask
before rewriting it.
"""

import argparse
import csv
import filecmp
import hashlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EMULATOR = ROOT / "investigation" / "emulator"
GOLDEN = ROOT / "app" / "golden" / "emulator"
SCENARIOS = Path(__file__).resolve().parent / "scenarios"


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def check_sources():
    """Refuses to run unless the emulator's sources are those recorded."""
    mismatched = []
    for line in (GOLDEN / "REFERENCE.tsv").read_text().splitlines():
        fields = line.split("\t")
        if fields[0] != "sha256" or not fields[2].startswith("investigation/emulator/"):
            continue
        path = ROOT / fields[2]
        if (
            not path.exists()
            or hashlib.sha256(path.read_bytes()).hexdigest() != fields[1]
        ):
            mismatched.append(fields[2])
    if mismatched:
        sys.exit(
            "the emulator differs from REFERENCE.tsv:\n  " + "\n  ".join(mismatched)
        )


def build_emulator(work):
    exe = work / "tms5220emu"
    run(
        [
            "cc",
            "-O2",
            "-std=c99",
            "-Wall",
            "-Wextra",
            "-o",
            str(exe),
            str(EMULATOR / "tms5220emu.c"),
            str(EMULATOR / "tms5220.c"),
            str(EMULATOR / "tms6100.c"),
        ]
    )
    return exe


def write_codes(samples_text, path):
    """One code per line from tms5220emu's '<index> <slot> <code>' lines."""
    codes = []
    for n, line in enumerate(samples_text.splitlines()):
        index, slot, code = line.split()
        if int(index) != n or int(slot) != 20 * n:
            sys.exit("unexpected sample line: " + line)
        codes.append(code)
    path.write_text("\n".join(codes) + "\n")


def emulate(exe, args, out_dir, name, cwd):
    samples = out_dir / (name + ".samples.tmp")
    events = out_dir / (name + ".events")
    run([str(exe), "-s", str(samples), "-e", str(events), *args], cwd=cwd)
    write_codes(samples.read_text(), out_dir / (name + ".codes"))
    samples.unlink()


def runs():
    """The rows of runs.tsv."""
    with open(GOLDEN / "runs.tsv", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def generate(out):
    """Runs the emulator for every run of runs.tsv into out."""
    check_sources()
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        exe = build_emulator(Path(tmp))
        for r in runs():
            if r["mode"] == "script":
                emulate(
                    exe, [Path(r["input"]).name], out, r["name"], GOLDEN / "scenarios"
                )
                continue
            args = ["--image", r["input"], "--addr", "0", "--phase", r["phase"]]
            if r["mode"] == "external":
                args.append("--external")
            emulate(exe, args, out, r["name"], GOLDEN)


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


def scenario_diff():
    """The scenario scripts whose committed copy differs from the source."""
    return [
        "scenarios/" + s.name
        for s in sorted(SCENARIOS.glob("*.txt"))
        if not filecmp.cmp(s, GOLDEN / "scenarios" / s.name, shallow=False)
    ]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--check",
        action="store_true",
        help="run into a temporary directory and compare",
    )
    args = ap.parse_args()
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp))
            diff = scenario_diff()
            diff += ["runs/" + d for d in compare(Path(tmp), GOLDEN / "runs")]
            if diff:
                print("differs from app/golden/emulator/:\n  " + "\n  ".join(diff))
                return 1
            print("app/golden/emulator/ is reproduced exactly")
    else:
        for script in sorted(SCENARIOS.glob("*.txt")):
            shutil.copyfile(script, GOLDEN / "scenarios" / script.name)
        shutil.rmtree(GOLDEN / "runs", ignore_errors=True)
        generate(GOLDEN / "runs")
        print("wrote", (GOLDEN / "runs").relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
