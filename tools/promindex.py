#!/usr/bin/env python3
"""
    promindex.py

    Writes index.md for a PROM's capture set (plan section 6): every word in
    its prom.json, with its address, the frames and bits it parsed to, its
    length, its takes and their checks, how the repeats compare, and links
    to the audio. Words not captured yet are listed too. The address sweep
    (sweep/) is summarised there, and listed address by address in
    sweep.csv.

    takes.csv lists every take: its checks, the hash of its output
    (talk_sha256, process.py) and the hashes of its raw files. With the
    index, it is what the repo holds of a corpus kept local (plan section 6).

      python tools/promindex.py captures/corpus/acorn_phrom_a

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import collections
import csv
import json
import os

import compare as comparelib
import prom as promlib


def loadJson(path):
    try:
        with open(path) as f:
            return json.load(f)
    except OSError:
        return None


def repeatsText(result):
    if not result:
        return ""
    groups = result.get("standard_variants") or []
    if groups:
        # Standard takes: how many share the reference output, per path
        return "; ".join(f"{g['path']}: {g['takes_matching_majority']} of {g['takes']} match"
                         + (f", {g['samples_without_majority']} samples undecided"
                            if g["samples_without_majority"] else "")
                         for g in groups)
    pairs = [p for p in result["pairs"] if "error" not in p]
    if result["all_identical"]:
        return f"{len(pairs) + 1} identical"
    worst = max((p["rms_difference"] or 0 for p in pairs), default=0)
    same = sum(p["identical"] for p in pairs) + 1
    return f"{same} of {len(pairs) + 1} identical, worst rms {worst:.2f}"


def loadTakes(entryDir):
    """[(take directory, capture.json)] of an entry, in capture order."""
    if not os.path.isdir(entryDir):
        return []
    return [(t, loadJson(os.path.join(t, "capture.json"))) for t in comparelib.takeDirs(entryDir)]


def entrySummary(takes):
    """The parse of the first take that has one, the failed checks of all
    takes, and the take counts per path."""
    processed = [m.get("processed", {}) for _, m in takes]
    summary = next((p["summary"] for p in processed if p.get("summary", {}).get("frames")),
                   processed[0].get("summary", {}) if processed else {})
    failed = sorted({k for p in processed for k, v in p.get("checks", {}).items() if v is False})
    paths = collections.Counter(p.get("summary", {}).get("path") or "?" for p in processed)
    return summary, failed, paths


def wordRow(promDir, word, name, number, address, takes):
    cells = [str(number), word, address]
    if not takes:
        return cells + ["", "", "", "not captured", "", "", ""]
    summary, failed, paths = entrySummary(takes)
    first = os.path.relpath(takes[0][0], promDir)
    ms = summary.get("talk_samples", 0) / 8
    bits = summary.get("bits_consumed", summary.get("stream_bits_to_stop", ""))
    return cells + [
        str(summary.get("frames", "")), str(bits), f"{ms:.0f}",
        ", ".join(f"{n} {path}" for path, n in sorted(paths.items())),
        checksText(takes, failed),
        repeatsText(loadJson(os.path.join(promDir, name, "compare.json"))),
        f"[samples]({first}/samples.wav) · [listen]({first}/listen.wav) · "
        f"[frames]({first}/frames.csv)",
    ]


def checksText(takes, failed):
    """'pass', or how many takes failed which checks: failed takes are kept,
    and a take that failed a check of the capture procedure is retaken."""
    if not failed:
        return "pass"
    bad = sum(not m.get("processed", {}).get("passed") for _, m in takes)
    return f"{len(takes) - bad} pass, {bad} FAIL: " + ", ".join(failed)


TAKE_COLUMNS = ["entry", "address", "take", "path", "captured", "frame", "passed",
                "failed_checks", "frames", "bits", "talk_samples", "code_min", "code_max",
                "talk_sha256", "scope_raw_npz_sha256", "vsm_trace_bin_sha256"]
SWEEP_COLUMNS = ["address", "image_offset", "frames", "bits", "talk_samples", "voiced",
                 "unvoiced", "repeat", "silence", "stop", "code_min", "code_max", "takes",
                 "passed", "failed_checks", "talk_sha256", "outputs"]


def takeRow(promDir, entry, address, takeDir, meta):
    processed = meta.get("processed", {})
    summary = processed.get("summary", {})
    raw = meta.get("raw_sha256", {})
    codeRange = summary.get("code_range") or ["", ""]
    return [entry, address, os.path.relpath(takeDir, promDir), summary.get("path", ""),
            meta.get("captured", ""), (meta.get("timing") or {}).get("frame", ""),
            int(bool(processed.get("passed"))),
            " ".join(sorted(k for k, v in processed.get("checks", {}).items() if v is False)),
            summary.get("frames", ""),
            summary.get("bits_consumed", summary.get("stream_bits_to_stop", "")),
            summary.get("talk_samples", ""), codeRange[0], codeRange[1],
            summary.get("talk_sha256", ""), raw.get("scope_raw.npz", ""),
            raw.get("vsm_trace.bin", "")]


def sweepRow(address, base, takes):
    summary, failed, _ = entrySummary(takes)
    kinds = summary.get("frame_kinds", {})
    codeRange = summary.get("code_range") or ["", ""]
    hashes = [m.get("processed", {}).get("summary", {}).get("talk_sha256") for _, m in takes]
    return [f"0x{address:05X}", f"0x{address - base:04X}", summary.get("frames", ""),
            summary.get("bits_consumed", ""), summary.get("talk_samples", ""),
            *[kinds.get(k, "") for k in ("voiced", "unvoiced", "repeat", "silence", "stop")],
            codeRange[0], codeRange[1], len(takes),
            sum(bool(m.get("processed", {}).get("passed")) for _, m in takes),
            " ".join(failed), hashes[0] or "", len(set(hashes))]


def sweepLines(manifest, sweepRows):
    """The sweep's summary in index.md."""
    captured = [r for r in sweepRows if r[12]]
    if not captured:
        return []
    try:
        points, runOff = promlib.sweepPoints(manifest, promlib.image(manifest))
    except (OSError, ValueError):
        points, runOff = None, None
    runOffRows = {f"0x{a:05X}" for a in runOff or []}
    takes = sum(r[12] for r in captured)
    passedTakes = sum(r[13] for r in captured)
    noPass = [r[0] for r in captured if not r[13]]
    failures = collections.Counter(c for r in captured for c in r[14].split())
    stops = sum(r[9] == 1 for r in captured)
    frames = sorted(r[2] for r in captured if r[2] != "")
    lines = [
        "",
        "## Address sweep",
        "",
        f"- **Captured:** {len(captured)} of the "
        + (f"{len(points)} " if points is not None else "")
        + "sweep addresses, each spoken through the VSM (`sweep/addr_<address>/`). "
        "[`sweep.csv`](sweep.csv) lists them all.",
        "- **Covered:** every byte address of the image that is not a word start (words are "
        "listed above), except "
        + (f"the {len(runOff)} " if runOff is not None else "those ")
        + "whose data reaches the end of the image without a stop code. Those run on into the "
        "next bank, which the Pico serves as empty, so what the chip reads there depends on the "
        "board, not on the PROM"
        + (f" ({len(runOffRows & {r[0] for r in captured})} of them were captured anyway)"
           if runOffRows & {r[0] for r in captured} else "") + ".",
        f"- **Checks:** {passedTakes} of {takes} takes passed every check"
        + (". Failed takes are kept; addresses with a failed check: "
           + ", ".join(f"{k} ({n})" for k, n in failures.most_common()) if failures else "")
        + (f". {len(noPass)} addresses have no take that passed: {', '.join(noPass[:20])}"
           if noPass else ". Every address has a take that passed every check") + ".",
        f"- **Parsed:** {stops} ended with a stop code"
        + (f", after a median of {frames[len(frames) // 2]} frames (at most {frames[-1]})"
           if frames else "") + ".",
    ]
    repeated = [r for r in captured if r[12] > 1]
    if repeated:
        lines.append(f"- **Repeats:** {len(repeated)} addresses have more than one take; "
                     f"{sum(r[16] == 1 for r in repeated)} of them gave the same output "
                     "every time (`outputs` in `sweep.csv` counts the distinct outputs).")
    return lines


def writeIndex(promDir, log=print):
    manifest = loadJson(os.path.join(promDir, "prom.json"))
    base = promlib.base(manifest)
    rows, takeRows = [], []
    for word in manifest["words"]:
        name = promlib.wordDir(word)
        takes = loadTakes(os.path.join(promDir, name))
        rows.append(wordRow(promDir, word["name"], name, word["number"], word["address"], takes))
        takeRows += [takeRow(promDir, name, word["address"], t, m) for t, m in takes]
    sweepDir = os.path.join(promDir, promlib.SWEEP_DIR)
    sweepRows = []
    for name in sorted(os.listdir(sweepDir)) if os.path.isdir(sweepDir) else []:
        if not name.startswith("addr_"):
            continue
        address = int(name[5:], 16)
        takes = loadTakes(os.path.join(sweepDir, name))
        if takes:
            sweepRows.append(sweepRow(address, base, takes))
            takeRows += [takeRow(promDir, os.path.join(promlib.SWEEP_DIR, name),
                                 f"0x{address:05X}", t, m) for t, m in takes]
    captured = sum(r[6] != "not captured" for r in rows)
    image = manifest["image"]
    lines = [
        f"# {manifest.get('title', manifest['name'])}",
        "",
        f"- **Image:** {image['size']} bytes, SHA-256 `{image['sha256']}`, "
        f"bank {manifest['bank']:X}. Target chip: {manifest.get('target_chip', '?')}.",
        f"- **Provenance:** {manifest.get('provenance', '')}",
        f"- **Captured:** {captured} of {len(rows)} words. Each take directory follows "
        "`docs-tech/speech_capture_plan.md` §6; its `capture.json` holds the metadata and checks. "
        "[`takes.csv`](takes.csv) lists every take, with the SHA-256 of its output "
        "(`talk_sha256`: the I/O codes from sample 0 to the end of talk, as 16-bit "
        "little-endian integers) and of its raw files.",
        "- **Columns:** Frames and Bits are parsed from what the chip read (VSM) or was sent "
        "(Speak External), up to the stop code. ms is the talk time of the first take, "
        "from `SCOPE_TRIG` rising to Talk Status clearing. Repeats compares every take with "
        "the first, sample for sample, after aligning them. For takes made with the standard "
        "procedure, it gives how many match the per-sample majority of all of them exactly, and "
        "the samples where no majority exists (`compare.json`).",
        "",
        "| No. | Word | Address | Frames | Bits | ms | Takes | Checks | Repeats | Files |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    lines += ["| " + " | ".join(r) + " |" for r in rows]
    lines += sweepLines(manifest, sweepRows)
    path = os.path.join(promDir, "index.md")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(promDir, "takes.csv"), "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(TAKE_COLUMNS)
        writer.writerows(takeRows)
    if sweepRows:
        with open(os.path.join(promDir, "sweep.csv"), "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(SWEEP_COLUMNS)
            writer.writerows(sweepRows)
    log(f"{captured} of {len(rows)} words and {len(sweepRows)} sweep addresses captured, "
        f"{len(takeRows)} takes -> {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("prom_dir", help="a PROM's capture directory (holding prom.json)")
    writeIndex(parser.parse_args().prom_dir)


if __name__ == "__main__":
    main()
