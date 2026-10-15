#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the emulator's runs in app/golden/emulator/.

Runs the emulator (tms5220emu, the application's emulator behind the
retired C emulator's command line, built with the tests into
build/app/tests/support/) on the committed inputs of app/golden/emulator/
and writes, for each run listed in runs.tsv, the output codes and the full
event log. The inputs are the scenario scripts (copied from
app/scripts/golden/scenarios/) with the images they load, and the phrases:
a subset of the synthetic vectors and the retired Python encoder's output
for a corpus subset, each with its frames as CSV.

The runs were first written by the retired C emulator (in git history at
commit 6abe9ed), which the application's emulator re-implements and
reproduces exactly. runs.tsv's synth_matches column and REFERENCE.tsv
record that origin: whether the Python encoder's synthesiser matched the
emulator on each phrase, and the C emulator's commit and the hashes of its
sources. They are kept as committed. app/golden/packaging/ (the Python
encoder's images and streams) and app/golden/encoder/ (written by
encoder.py) are not touched.

Run by hand, in the project's default Nix shell, from the repository root,
with the tests built (TMS5220EMU names another tms5220emu):

    nix develop --command python3 app/scripts/golden/generate_golden.py
    nix develop --command python3 app/scripts/golden/generate_golden.py --check

--check runs the emulator into a temporary directory and compares the runs
and the scenario scripts with the committed files; the functional test
EmulatorGoldenTest checks the same runs. The emulator's reference data
never changes unless the reference datasheet does (AGENTS.md §3.1): ask
before rewriting it.
"""

import argparse
import csv
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GOLDEN = ROOT / "app" / "golden" / "emulator"
SCENARIOS = Path(__file__).resolve().parent / "scenarios"
EMULATOR = Path(
    os.environ.get(
        "TMS5220EMU", ROOT / "build" / "app" / "tests" / "support" / "tms5220emu"
    )
)


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def write_codes(samples_text, path):
    """One code per line from tms5220emu's '<index> <slot> <code>' lines."""
    codes = []
    for n, line in enumerate(samples_text.splitlines()):
        index, slot, code = line.split()
        if int(index) != n or int(slot) != 20 * n:
            sys.exit("unexpected sample line: " + line)
        codes.append(code)
    path.write_text("\n".join(codes) + "\n")


def emulate(args, out_dir, name, cwd):
    samples = out_dir / (name + ".samples.tmp")
    events = out_dir / (name + ".events")
    run([str(EMULATOR), "-s", str(samples), "-e", str(events), *args], cwd=cwd)
    write_codes(samples.read_text(), out_dir / (name + ".codes"))
    samples.unlink()


def runs():
    """The rows of runs.tsv."""
    with open(GOLDEN / "runs.tsv", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def generate(out):
    """Runs the emulator for every run of runs.tsv into out."""
    if not EMULATOR.exists():
        sys.exit("build the tests first: tms5220emu is not at %s" % EMULATOR)
    out.mkdir(parents=True, exist_ok=True)
    for r in runs():
        if r["mode"] == "script":
            emulate([Path(r["input"]).name], out, r["name"], GOLDEN / "scenarios")
            continue
        args = ["--image", r["input"], "--addr", "0", "--phase", r["phase"]]
        if r["mode"] == "external":
            args.append("--external")
        emulate(args, out, r["name"], GOLDEN)


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
