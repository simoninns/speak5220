#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Encodes a directory of speech with each of a set of encoder options and compares them.

For each option set of a table (name, then options as
"repeat_lambda=300,voicing_threshold=0.5", empty for the defaults; the form
OptionsCorpusCheck and phrom-check.sh take), every WAV file of --input is
added to a new project with those default options (speak5220-cli new
--option), encoded and measured (speak5220-cli measure); with --phrom, the
PHROM round trip is run with the same options (phrom-check.sh's test). One
row per set goes to OUTPUT/summary.tsv and is printed: the median and mean
overall score, the median component scores, envelope distortion, GPE, VDE
and ESTOI, the total bytes, the median loudness figures and, with --phrom,
the round trip's voicing agreement and its breakdown, repeats and
fade-outs, energy biases and bits. --only takes a list of the files to
encode (sweeps/tts-subset.txt picks the 64 TTS files the encoder's
options were tuned on out of the phoneme set). Each set's project,
measures and round-trip report are kept in OUTPUT/SET/. Changes meant to
improve the encoder's output are judged with it.

It is run by hand from the repository root in the default Nix shell, with
speak5220-cli and the functional tests built:

    nix develop --command python3 app/scripts/option_sweep.py \\
        --input local/arctic-project/generated-corpus --phrom
"""

import argparse
import csv
import os
import re
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = Path(__file__).resolve().parent
DEFAULT_SETS = SCRIPTS / "sweeps" / "encoder-quality.tsv"
DEFAULT_BUILD = ROOT / "build" / "app"

# The measures summarised, from speak5220-cli measure's columns: (column,
# heading, how the files are pooled).
MEASURES = [
    ("score", "score_median", statistics.median),
    ("score", "score_mean", statistics.fmean),
    ("envelope_score", "envelope", statistics.median),
    ("pitch_score", "pitch", statistics.median),
    ("intelligibility_score", "intelligibility", statistics.median),
    ("lsd_db", "lsd_db", statistics.median),
    ("gross_pitch_error", "gpe_pct", statistics.median),
    ("voicing_error", "vde_pct", statistics.median),
    ("estoi", "estoi", statistics.median),
    ("bytes", "bytes", sum),
    ("sounding_rms", "sounding_rms", statistics.median),
    ("peak", "peak", statistics.median),
    ("clipped_percent", "clipped_pct", statistics.fmean),
    ("bbc_rms", "bbc_rms", statistics.median),
    ("a_weighted_rms", "a_weighted_rms", statistics.median),
]

# The round trip's figures, from the total row of its report.
ROUND_TRIP = [
    ("voicing_agree_pct", "phrom_agree_pct"),
    ("voiced_as_unvoiced_inside", "phrom_inside"),
    ("voiced_as_unvoiced_onset", "phrom_onset"),
    ("voiced_as_unvoiced_offset", "phrom_offset"),
    ("unvoiced_as_voiced", "phrom_unvoiced_as_voiced"),
    ("repeats_enc", "phrom_repeats"),
    ("fade_outs_enc", "phrom_fade_outs"),
    ("energy_diff_voiced", "phrom_energy_voiced"),
    ("energy_diff_unvoiced", "phrom_energy_unvoiced"),
    ("shape_db_voiced", "phrom_shape_db_voiced"),
    ("bits_enc", "phrom_bits"),
]


def run(command, env=None):
    result = subprocess.run(
        [str(c) for c in command], capture_output=True, text=True, env=env
    )
    if result.returncode != 0:
        sys.exit(
            f"option_sweep: {' '.join(str(c) for c in command)} failed:\n"
            f"{result.stdout}{result.stderr}"
        )
    return result.stdout


def read_sets(path):
    sets = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            name = row["name"]
            if not re.fullmatch(r"[A-Za-z0-9_.-]+", name):
                sys.exit(f"{path}: set name {name!r} is not usable as a directory name")
            options = (row.get("options") or "").strip()
            sets.append((name, [o for o in options.split(",") if o]))
    if not sets:
        sys.exit(f"{path}: no option sets")
    return sets


def read_names(path):
    """The names of a list file: one per line, # starts a comment."""
    names = set()
    with open(path) as f:
        for line in f:
            name = line.split("#", 1)[0].strip()
            if name:
                names.add(name)
    if not names:
        sys.exit(f"{path}: no names")
    return names


def listed(wav, names):
    """Whether `wav` is named in `names` by its stem, or by its stem less
    a last "_VOICE" part (a prompt id of phoneme_corpus.py)."""
    stem = wav.stem
    return stem in names or stem.rsplit("_", 1)[0] in names


def read_tsv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def summarise_measures(rows):
    out = {}
    for column, heading, pool in MEASURES:
        values = [float(r[column]) for r in rows if r[column] != ""]
        out[heading] = pool(values) if values else float("nan")
    out["bytes"] = int(out["bytes"])
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--input",
        type=Path,
        required=True,
        help="a directory of 8 kHz WAV files to encode",
    )
    ap.add_argument(
        "--sets",
        type=Path,
        default=DEFAULT_SETS,
        help="the option sets (default: %(default)s)",
    )
    ap.add_argument(
        "--build",
        type=Path,
        default=DEFAULT_BUILD,
        help="the build directory (default: %(default)s)",
    )
    ap.add_argument(
        "--output", type=Path, help="where the results go (default: BUILD/option-sweep)"
    )
    ap.add_argument(
        "--only",
        type=Path,
        help="a list of the files of --input to encode, by stem or prompt id "
        "(such as sweeps/tts-subset.txt); default: every file",
    )
    ap.add_argument(
        "--phrom",
        action="store_true",
        help="also run the PHROM round trip with each set",
    )
    args = ap.parse_args()

    cli = args.build / "src" / "cli" / "speak5220-cli"
    tests = args.build / "tests" / "speak5220_functional_tests"
    for needed in [cli] + ([tests] if args.phrom else []):
        if not needed.exists():
            sys.exit(
                f"option_sweep: {needed} not found; build it with: "
                f"cmake --build {args.build}"
            )
    wavs = sorted(args.input.glob("*.wav"))
    if args.only:
        names = read_names(args.only)
        wavs = [w for w in wavs if listed(w, names)]
    if not wavs:
        sys.exit(f"option_sweep: no WAV files in {args.input}")
    output = args.output or args.build / "option-sweep"
    sets = read_sets(args.sets)

    words = args.build / "phrom-roundtrip"
    if args.phrom:
        run(
            [
                sys.executable,
                SCRIPTS / "phrom_roundtrip.py",
                "--output",
                words,
                "--cli",
                cli,
            ]
        )

    rows = []
    for name, options in sets:
        directory = output / name
        if directory.exists():
            shutil.rmtree(directory)
        directory.mkdir(parents=True)
        project = directory / "project"
        option_args = [a for o in options for a in ("--option", o)]
        run([cli, "new", project] + option_args)
        run([cli, "add", project] + wavs)
        run([cli, "encode", project])
        run([cli, "measure", project, "--output", directory])
        row = {"set": name, "options": ",".join(options), "files": len(wavs)}
        row.update(summarise_measures(read_tsv(directory / "measures.tsv")))
        if args.phrom:
            report = directory / "phrom-roundtrip.tsv"
            env = dict(
                os.environ,
                SPEAK5220_PHROM_DIR=str(words),
                SPEAK5220_PHROM_REPORT=str(report),
                SPEAK5220_PHROM_OPTIONS=",".join(options),
            )
            run(
                [
                    tests,
                    "--gtest_also_run_disabled_tests",
                    "--gtest_filter=PhromRoundTripCheck.*",
                ],
                env=env,
            )
            total = [r for r in read_tsv(report) if r["name"] == "total"][0]
            row.update(
                {heading: float(total[column]) for column, heading in ROUND_TRIP}
            )
        rows.append(row)
        print(
            f"option_sweep: {name}: "
            + ", ".join(
                f"{k} {v:.4g}" if isinstance(v, float) else f"{k} {v}"
                for k, v in row.items()
                if k not in ("set", "options")
            ),
            flush=True,
        )

    with open(output / "summary.tsv", "w", newline="") as f:
        w = csv.DictWriter(f, list(rows[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        for row in rows:
            w.writerow(
                {k: f"{v:.6g}" if isinstance(v, float) else v for k, v in row.items()}
            )
    print(f"option_sweep: summary in {output / 'summary.tsv'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
