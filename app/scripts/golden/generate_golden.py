#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the emulator and packaging reference data in app/golden/.

Runs the reference C emulator (investigation/emulator, built here into a
temporary directory) and the reference Python encoder
(investigation/encoder/tms5220enc.py) on chosen inputs, and writes what they
produce (application plan §1.4, Phase 1 tasks 4-6):

- app/golden/emulator/: the emulator's self-test scenarios, a subset of the
  synthetic vectors and encoder output for a corpus subset, spoken through
  the VSM and Speak External paths. For each run, the output codes and the
  full event log.
- app/golden/packaging/: Acorn PHROM and TI indexed images and Speak
  External streams of those phrases.

The application's build and tests read only the files written here; they
never run anything in investigation/.

Run by hand, in the project's default Nix shell, from the repository root:

    nix develop --command python3 app/scripts/golden/generate_golden.py
    nix develop --command python3 app/scripts/golden/generate_golden.py --check

--check regenerates everything into a temporary directory and compares it
with the committed files. The emulator's reference data never changes unless
the reference datasheet does (AGENTS.md §3.1): ask before rewriting it.
"""

import argparse
import filecmp
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EMULATOR = ROOT / "investigation" / "emulator"
ENCODER = ROOT / "investigation" / "encoder"
VECTORS = ROOT / "investigation" / "captures" / "phase8" / "vectors"
CORPUS = ROOT / "investigation" / "captures" / "phase11" / "inputs" / "tts"
GOLDEN = ROOT / "app" / "golden"
SCENARIOS = Path(__file__).resolve().parent / "scenarios"

sys.path.insert(0, str(ENCODER))
import tms5220enc as enc  # noqa: E402

# Words taken from each synthetic vector set: evenly spaced, up to this many.
# The "special" set (unusual frame sequences) is taken whole.
WORDS_PER_SET = 4
WHOLE_SETS = {"special"}
VECTOR_SETS = [
    "energy",
    "interp",
    "k",
    "loc",
    "loud",
    "noise",
    "onset",
    "pitch",
    "random",
    "ring",
    "special",
]

# Corpus subset: TTS renders by public-domain and CC0 voices only (see
# investigation/captures/phase11/inputs/tts/README.md), encoded with the
# encoder's defaults.
CORPUS_INPUTS = [
    "h01_joe",
    "h01_kristin",
    "h06_norman",
    "h06_cori",
    "h09_joe",
    "h10_kristin",
]

# Bulk runs repeated at other power-up phases, to cover the frame clock.
EXTRA_PHASES = {"special_repeat_voiced": [77, 150], "corpus_h01_joe": [123]}

# Header strings of the Acorn PHROM, as AcornPhromOptions' defaults.
ACORN_STRINGS = ("(C)2026 speak5220", "Speak5220 PHROM", "1.00")


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def reference_commit():
    """The last commit that changed the references; refuses local edits."""
    paths = ["investigation/emulator", "investigation/encoder"]
    dirty = run(
        ["git", "status", "--porcelain", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if dirty:
        sys.exit("the references have local changes:\n" + dirty)
    return run(
        ["git", "log", "-1", "--format=%H", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()


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


# -------------------------------------------------------------------------
# Inputs


def voiced(e, p, k=(20, 10, 7, 8, 8, 8, 8, 4, 4, 4)):
    return {"energy": e, "repeat": 0, "pitch": p, "k": list(k)}


def unvoiced(e, k=(20, 10, 7, 8)):
    return {"energy": e, "repeat": 0, "pitch": 0, "k": list(k)}


def scenario_images():
    """The images the scenario scripts load, as test_tms5220.c builds them."""
    stop = {"energy": 15}
    frames = [
        voiced(10, 30),
        {"energy": 10, "repeat": 1, "pitch": 30},
        unvoiced(8),
        {"energy": 0},
        stop,
    ]
    return {
        # A frame of each length, then a byte that must not be read
        "frames.bin": enc.pack(frames) + b"\xff",
        # A stop code as the first frame
        "stop_first.bin": bytes([0x0F, 0xFF]),
        # Three voiced frames and a stop: 20 bytes, for Speak External
        "external.bin": enc.pack([voiced(10, 30)] * 3 + [stop]),
        # Read Byte: the §6 example bytes
        "read_byte.bin": bytes([0x23, 0x9B, 0x35, 0x85]),
        # Two voiced frames, a stop, then 0x5A to be read by a held Read Byte
        "held.bin": enc.pack([voiced(10, 30)] * 2 + [stop]) + b"\x5a",
        # Read and Branch: a pointer, and the byte it points to
        "branch_pointer.bin": bytes([0xB6, 0x0B]),
        "branch_target.bin": bytes([0x23]),
        # Speech for the reset scenarios
        "phrase.bin": enc.pack([voiced(10, 30)] * 6 + [unvoiced(8)] * 2 + [stop]),
    }


def vector_words():
    """(name, bytes, frames) for the chosen synthetic vector words."""
    out = []
    for name in VECTOR_SETS:
        meta = json.loads((VECTORS / (name + ".json")).read_text())
        image = (VECTORS / (name + ".bin")).read_bytes()
        words = meta["words"]
        if name in WHOLE_SETS or len(words) <= WORDS_PER_SET:
            chosen = words
        else:
            step = (len(words) - 1) / (WORDS_PER_SET - 1)
            chosen = [words[round(i * step)] for i in range(WORDS_PER_SET)]
        for w in chosen:
            start = int(w["address"], 16)
            data = image[start : start + w["bytes"]]
            frames, used = enc.parse(data)
            if (used + 7) // 8 != len(data) or frames[-1] != {"energy": 15}:
                sys.exit("vector word %s does not parse whole" % w["name"])
            out.append(("%s_%s" % (name, w["name"]), data, frames))
    return out


def corpus_words():
    import soundfile as sf

    out = []
    for name in CORPUS_INPUTS:
        x, rate = sf.read(
            str(CORPUS / (name + ".flac")), dtype="float64", always_2d=True
        )
        frames = enc.encode(x.mean(axis=1), rate)
        frames = frames if frames[-1]["energy"] == 15 else frames + [{"energy": 15}]
        out.append(("corpus_" + name, enc.pack(frames), frames))
    return out


# -------------------------------------------------------------------------
# Runs


def write_codes(samples_text, path):
    """One code per line from tms5220emu's '<index> <slot> <code>' lines."""
    codes = []
    for n, line in enumerate(samples_text.splitlines()):
        index, slot, code = line.split()
        if int(index) != n or int(slot) != 20 * n:
            sys.exit("unexpected sample line: " + line)
        codes.append(code)
    path.write_text("\n".join(codes) + "\n")
    return [int(c) for c in codes]


def emulate(exe, args, out_dir, name, cwd):
    samples = out_dir / (name + ".samples.tmp")
    events = out_dir / (name + ".events")
    run([str(exe), "-s", str(samples), "-e", str(events), *args], cwd=cwd)
    codes = write_codes(samples.read_text(), out_dir / (name + ".codes"))
    samples.unlink()
    return codes


def generate(out):
    commit = reference_commit()
    emu_dir = out / "emulator"
    pkg_dir = out / "packaging"
    for d in (emu_dir / "scenarios", emu_dir / "phrases", emu_dir / "runs", pkg_dir):
        d.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        exe = build_emulator(Path(tmp))

        rows = []
        # Scenario scripts and their images
        for fname, data in scenario_images().items():
            (emu_dir / "scenarios" / fname).write_bytes(data)
        for script in sorted(SCENARIOS.glob("*.txt")):
            shutil.copyfile(script, emu_dir / "scenarios" / script.name)
            name = "scenario_" + script.stem
            emulate(exe, [script.name], emu_dir / "runs", name, emu_dir / "scenarios")
            rows.append((name, "script", "scenarios/" + script.name, 0, "-"))

        # Phrases through both paths
        phrases = vector_words() + corpus_words()
        mismatches = []
        for name, data, frames in phrases:
            (emu_dir / "phrases" / (name + ".bin")).write_bytes(data)
            (emu_dir / "phrases" / (name + ".csv")).write_text(enc.framesCsv(frames))
            for phase in [0] + EXTRA_PHASES.get(name, []):
                for mode in ("vsm", "external"):
                    run_name = "%s_%s" % (name, mode)
                    if phase:
                        run_name += "_phase%d" % phase
                    args = [
                        "--image",
                        "phrases/%s.bin" % name,
                        "--addr",
                        "0",
                        "--phase",
                        str(phase),
                    ]
                    if mode == "external":
                        args.append("--external")
                    codes = emulate(exe, args, emu_dir / "runs", run_name, emu_dir)
                    match = "-"
                    if mode == "vsm" and phase == 0:
                        # The encoder's synthesiser against the emulator,
                        # from the first frame read (sample 200)
                        c = [int(v) for v in enc.synthesize(frames)]
                        match = "yes" if codes[200 : 200 + len(c)] == c else "no"
                        if match == "no":
                            mismatches.append(name)
                    rows.append((run_name, mode, "phrases/%s.bin" % name, phase, match))

        lines = ["name\tmode\tinput\tphase\tsynth_matches"]
        lines += ["%s\t%s\t%s\t%d\t%s" % r for r in rows]
        (emu_dir / "runs.tsv").write_text("\n".join(lines) + "\n")

    # Packaging: the corpus phrases, then the short "special" vectors
    words = [
        (name.upper().replace("_", " ")[:16], frames)
        for name, _, frames in phrases
        if name.startswith("corpus_")
    ]
    words += [
        (name.split("_", 1)[1].upper().replace("_", " ")[:16], frames)
        for name, _, frames in phrases
        if name.startswith("special_")
    ]
    ascii_map = {"J": words[0][0], 65: words[1][0], "Z": words[2][0]}
    acorn, acorn_table = enc.acornPhrom(
        words, strings=ACORN_STRINGS, asciiMap=ascii_map
    )
    ti, ti_table = enc.tiIndexed(words)
    (pkg_dir / "acorn.bin").write_bytes(acorn)
    (pkg_dir / "ti_indexed.bin").write_bytes(ti)
    word_lines = ["number\tname\tframes"]
    for i, (name, frames) in enumerate(words):
        stem = "word%02d" % (i + 1)
        (pkg_dir / (stem + ".csv")).write_text(enc.framesCsv(frames))
        (pkg_dir / (stem + "_external.bin")).write_bytes(
            enc.speakExternalStream(frames)
        )
        word_lines.append("%d\t%s\t%s.csv" % (i + 1, name, stem))
    (pkg_dir / "words.tsv").write_text("\n".join(word_lines) + "\n")
    asc = ["code\tname"] + [
        "%d\t%s" % (ord(k) if isinstance(k, str) else k, v)
        for k, v in ascii_map.items()
    ]
    (pkg_dir / "acorn_ascii.tsv").write_text("\n".join(asc) + "\n")
    for fname, table in (
        ("acorn_table.tsv", acorn_table),
        ("ti_indexed_table.tsv", ti_table),
    ):
        tl = ["number\tname\toffset\tbytes\tframes"]
        tl += [
            "%d\t%s\t%d\t%d\t%d"
            % (t["number"], t["name"], t["offset"], t["bytes"], t["frames"])
            for t in table
        ]
        (pkg_dir / fname).write_text("\n".join(tl) + "\n")

    sources = sorted(EMULATOR.glob("*.[ch]")) + [ENCODER / "tms5220enc.py"]
    ref = ["reference_commit\t" + commit]
    ref += [
        "sha256\t%s\t%s"
        % (hashlib.sha256(p.read_bytes()).hexdigest(), p.relative_to(ROOT))
        for p in sources
    ]
    for d in (emu_dir, pkg_dir):
        (d / "REFERENCE.tsv").write_text("\n".join(ref) + "\n")
    return mismatches


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
    args = ap.parse_args()
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            mismatches = generate(Path(tmp))
            diff = compare(Path(tmp), GOLDEN)
            diff = [d for d in diff if d != "README.md"]
            if diff:
                print("differs from app/golden/:\n  " + "\n  ".join(diff))
                return 1
            print("app/golden/ is reproduced exactly")
    else:
        for sub in ("emulator", "packaging"):
            shutil.rmtree(GOLDEN / sub, ignore_errors=True)
        mismatches = generate(GOLDEN)
        print("wrote", GOLDEN.relative_to(ROOT))
    if mismatches:
        print(
            "encoder synthesiser differs from the emulator on:", ", ".join(mismatches)
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
