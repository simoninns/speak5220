#!/usr/bin/env python3
"""
    regress.py

    The Phase 9 regression harness (plan section 12): runs the reference
    emulator (emulator/tms5220emu, written from the datasheet alone) on
    every standard-procedure take of the data set and compares it with the
    chip, sample for sample (layer 2) and in its VSM timing (layer 1).

    Each take is rebuilt from what its capture.json records: the PROM
    image and address, the path (VSM Speak or Speak External), and the
    time the command was latched before the frame boundary. The takes
    captured before it are spoken first, back to the latest with a voiced
    frame, because the coded parameters carry over (datasheet section 7).

      python tools/regress.py vectors                 # every Phase 8/9 vector set
      python tools/regress.py corpus                  # the Acorn PHROM A words
      python tools/regress.py corpus --sweep          # ... and its address sweep
      python tools/regress.py captures/vectors/vectors_ring   # one set

    Writes <out>/<name>.csv (one row per take) and prints a summary per set.

    Layer 2: the emulator's I/O codes against samples.csv, from 5 samples
    before the first frame read to the end of the capture. Layer 1 (VSM
    path): the number of bits read, the sample period of every frame read,
    and when Talk Status cleared, relative to the first frame read.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import glob
import json
import multiprocessing
import os
import subprocess
import tempfile

import numpy as np

import lpc
import model as modellib
import prom as promlib

ROOT = promlib.ROOT
EMU = os.environ.get("TMS5220EMU", os.path.join(ROOT, "emulator", "tms5220emu"))
SLOT_US = 6.25
# Speak External leaves no read in the trace: the chip sample in which the
# first frame would have been read, for the standard procedure (as for VSM)
STANDARD_FIRST_READ_SAMPLE = 99
RESET_SEQUENCE = "writes FF FF FF FF FF FF FF FF FF 70"


def manifestFor(capture):
    source = capture["source"]
    path = source.get("manifest") or os.path.join("proms", f"{source['prom']}.json")
    with open(os.path.join(ROOT, path)) as f:
        return json.load(f)


_images = {}


def imageFile(manifest, tmp):
    """The PROM image as a file the emulator can load, and its VSM address."""
    name = manifest["name"]
    if name not in _images:
        path = os.path.join(tmp, f"{name}.bin")
        # Workers share tmp: write each copy under its own name, then rename
        part = f"{path}.{os.getpid()}"
        with open(part, "wb") as f:
            f.write(promlib.image(manifest))
        os.replace(part, path)
        _images[name] = (path, promlib.base(manifest))
    return _images[name]


def loadAddressWrites(address):
    nibbles = [(address >> (4 * i)) & 0xF for i in range(5)]
    return "writes " + " ".join(f"{0x40 | n:02X}" for n in nibbles)


def priorScript(previous, path):
    """A scenario that speaks the predecessor takes (oldest first) through
    the VSM, as the standard procedure did."""
    with open(path, "w") as f:
        for address in previous:
            f.write(f"{RESET_SEQUENCE}\n{loadAddressWrites(address)}\nboundary 1961\nwrite 50\nuntil_idle\n")
    return path


def readSamples(path):
    data = np.loadtxt(path, dtype=np.int64, ndmin=2)
    return data[:, 2]


def readEvents(path):
    events = []
    with open(path) as f:
        for line in f:
            parts = line.split()
            events.append((int(parts[0]), parts[1:]))
    events.sort(key=lambda e: e[0])
    return events


def loadChip(takeDir):
    with open(os.path.join(takeDir, "capture.json")) as f:
        capture = json.load(f)
    codes = np.loadtxt(os.path.join(takeDir, "samples.csv"), delimiter=",", skiprows=1,
                       usecols=1, dtype=np.int64, ndmin=1)
    reads = []
    with open(os.path.join(takeDir, "frames.csv")) as f:
        for row in csv.DictReader(f):
            if row.get("read_sample"):
                reads.append(int(row["read_sample"]))
    return capture, codes, reads


def framesAt(manifest, address):
    data = promlib.image(manifest)
    offset = address - promlib.base(manifest)
    frames, _ = lpc.parseFrames(lpc.bitsLsbFirst(data[offset:offset + 4096]))
    return frames


def modelAligned(takeDir, manifest, previous, external, length, chipFirst):
    """The model of datasheet section 8.9 (tools/model.py), aligned to the
    chip's sample indices as model.compareTake aligns it."""
    start = modellib.STANDARD_START["external" if external else "vsm"]
    m = modellib.Model(rng_at_speak=start["rng"])
    m.newIdx = [0] * 12             # the power-up state section 8.9 suggests
    for address in previous:
        m.speak(framesAt(manifest, address))
        m.rng = start["rng"]
    out, p0 = m.speak(modellib.loadFrames(takeDir))
    shift = chipFirst + start["offset"] - p0          # chip index = model index + shift
    aligned = np.full(length, 10**6, dtype=np.int64)
    lo, hi = max(0, shift), min(length, len(out) + shift)
    aligned[lo:hi] = out[lo - shift:hi - shift]
    return aligned


def runTake(job):
    takeDir, previous, tmp, withModel = job
    capture, codes, chipReads = loadChip(takeDir)
    summary = capture["processed"]["summary"]
    manifest = manifestFor(capture)
    image, base = imageFile(manifest, tmp)
    address = int(capture["source"]["address"], 16)
    external = capture["path"] == "external"
    latch = summary.get("latch_before_frame_us") or capture["timing"]["latch_target_us"]
    speakAt = int(round(latch / SLOT_US))
    tag = os.path.relpath(takeDir, ROOT).replace("/", "_")
    samplesPath = os.path.join(tmp, tag + ".s")
    eventsPath = os.path.join(tmp, tag + ".e")
    cmd = [EMU, "-s", samplesPath, "-e", eventsPath, "--image", image,
           "--load-at", f"{base:X}", "--addr", f"{address:X}", "--speak-at", str(speakAt), "--reset"]
    if external:
        cmd.append("--external")
    if previous:
        cmd += ["--prior", priorScript(previous, os.path.join(tmp, tag + ".prior"))]
    subprocess.run(cmd, check=True)
    emu = readSamples(samplesPath)
    events = readEvents(eventsPath)
    os.remove(samplesPath)
    os.remove(eventsPath)

    # The command of this take is the last Speak/Speak External written
    mark = [s for s, e in events if e[0] == "MARK" and e[1].startswith("speak")][-1]
    ready = next(int(e[4]) for s, e in events if s == mark and e[0] == "HOST")
    emuFirst = (ready + speakAt) // 20
    chipFirst = chipReads[0] if chipReads and not external else STANDARD_FIRST_READ_SAMPLE
    shift = emuFirst - chipFirst                 # emulator index = chip index + shift
    lo = max(0, chipFirst - 5)
    hi = min(len(codes), len(emu) - shift)
    diff = codes[lo:hi] - emu[lo + shift:hi + shift]
    nz = np.flatnonzero(diff)
    row = {"take": os.path.relpath(takeDir, ROOT), "path": capture["path"],
           "compared": int(hi - lo), "mismatched": int(len(nz)),
           "max_abs": int(np.abs(diff).max()) if len(diff) else 0,
           "first_mismatch": int(nz[0] + lo - chipFirst) if len(nz) else "",
           "exact": int(len(nz) == 0)}
    if withModel:
        model = modelAligned(takeDir, manifest, previous, external, len(codes), chipFirst)
        emuOnChip = np.full(len(codes), 10**6, dtype=np.int64)
        emuOnChip[lo:hi] = emu[lo + shift:hi + shift]
        both = model[lo:hi] != 10**6
        row["model_mismatched"] = int(np.count_nonzero(codes[lo:hi][both] != model[lo:hi][both]))
        row["emu_vs_model"] = int(np.count_nonzero(emuOnChip[lo:hi][both] != model[lo:hi][both]))

    # Layer 1, VSM path: bits read, frame read samples, Talk Status clear
    if not external:
        after = [(s, e) for s, e in events if s > mark]
        readSlots = [s for s, e in after if e[:2] == ["VSM", "READ"]]
        bits = len(readSlots)
        # A frame's parameters are read over 446 slots, with gaps of up to
        # 42 inside it (datasheet section 4); frames are 4000 slots apart
        frameSamples = [s // 20 - emuFirst for i, s in enumerate(readSlots)
                        if i == 0 or s - readSlots[i - 1] > 1000]
        chipFrames = [r - chipFirst for r in chipReads]
        tsClear = next((s for s, e in after if e[:3] == ["FLAG", "TS", "0"]), None)
        firstReadSlot = next(s for s, e in after if e[:2] == ["VSM", "READ"])
        emuTalk = (tsClear - firstReadSlot) * SLOT_US / 1e6 if tsClear else None
        chipTalk = summary.get("talk_after_first_read_s")
        row.update({
            "bits_ok": int(bits == summary.get("bits_consumed")),
            "frames_ok": int(frameSamples == chipFrames),
            # The trace times host cycles to about 1.5 us; status reads poll
            "talk_us_diff": (round((emuTalk - chipTalk) * 1e6, 1)
                             if emuTalk is not None and chipTalk is not None else ""),
        })
    return row


def hasVoiced(manifest, address, cache={}):
    key = (manifest["name"], address)
    if key not in cache:
        try:
            cache[key] = any(f.get("kind") == "voiced" for f in framesAt(manifest, address))
        except Exception:
            cache[key] = False
    return cache[key]


def takesOf(root, sweep):
    """Standard takes that passed every check (the DAC model's apart), in
    the order captured, with
    their predecessors: the takes captured before each one, back to the
    latest that has a voiced frame. A voiced frame sets every coded index
    (datasheet sections 7 and 8.9), so nothing earlier can matter, except
    through the filter's state, which has decayed by then."""
    rows = []
    for idx in glob.glob(os.path.join(root, "**", "takes.csv"), recursive=True):
        base = os.path.dirname(idx)
        with open(idx) as f:
            for r in csv.DictReader(f):
                rows.append((r["captured"], os.path.join(base, r["take"])))
    rows.sort()
    out, history = [], []
    for _, take in rows:
        cap = os.path.join(take, "capture.json")
        if not os.path.exists(cap):
            continue
        with open(cap) as f:
            capture = json.load(f)
        address = int(capture["source"]["address"], 16)
        # The DAC model's check (layer 3) says nothing about the I/O codes
        failed = {k for k, v in capture["processed"].get("checks", {}).items() if v is False}
        keep = (capture.get("procedure", {}).get("start") == "standard"
                and failed <= {"layer2_vs_layer3"}
                and (sweep or "/sweep/" not in take))
        if keep:
            out.append((take, list(history)))
        manifest = manifestFor(capture)
        history = [address] if hasVoiced(manifest, address) else history[-8:] + [address]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("what", nargs="+", help="'vectors', 'corpus', or capture roots")
    parser.add_argument("--sweep", action="store_true", help="include the corpus address sweep")
    parser.add_argument("--out", default=os.path.join(ROOT, "captures", "phase9", "regress"))
    parser.add_argument("--jobs", type=int, default=14)
    parser.add_argument("--model", action="store_true",
                        help="also run tools/model.py and compare the emulator with it")
    args = parser.parse_args()
    roots = []
    for w in args.what:
        if w == "vectors":
            roots += sorted(glob.glob(os.path.join(ROOT, "captures", "vectors", "vectors_*")))
        elif w == "corpus":
            roots.append(os.path.join(ROOT, "captures", "corpus", "acorn_phrom_a"))
        else:
            roots.append(os.path.abspath(w))
    os.makedirs(args.out, exist_ok=True)
    totals = {}
    with tempfile.TemporaryDirectory() as tmp, multiprocessing.Pool(args.jobs) as pool:
        for root in roots:
            hasSweep = args.sweep and os.path.isdir(os.path.join(root, "sweep"))
            name = os.path.basename(root.rstrip("/")) + ("_sweep" if hasSweep else "")
            jobs = [(t, p, tmp, args.model) for t, p in takesOf(root, args.sweep)]
            rows = pool.map(runTake, jobs, chunksize=8)
            with open(os.path.join(args.out, f"{name}.csv"), "w", newline="") as f:
                fields = ["take", "path", "exact", "compared", "mismatched", "max_abs",
                          "first_mismatch", "bits_ok", "frames_ok", "talk_us_diff",
                          "model_mismatched", "emu_vs_model"]
                w = csv.DictWriter(f, fieldnames=fields, restval="")
                w.writeheader()
                w.writerows(rows)
            n = len(rows)
            samples = sum(r["compared"] for r in rows)
            mism = sum(r["mismatched"] for r in rows)
            vsm = [r for r in rows if r["path"] == "vsm"]
            talk = [abs(r["talk_us_diff"]) for r in vsm if r["talk_us_diff"] != ""]
            s = {"takes": n, "exact": sum(r["exact"] for r in rows),
                 "samples_equal": round(100 * (1 - mism / samples), 3) if samples else None,
                 "bits_ok": sum(r.get("bits_ok", 0) for r in vsm), "frames_ok": sum(r.get("frames_ok", 0) for r in vsm),
                 "vsm_takes": len(vsm), "talk_us_max_abs_diff": max(talk) if talk else None}
            if args.model:
                s["model_samples_equal"] = round(100 * (1 - sum(r["model_mismatched"] for r in rows) / samples), 3)
                s["emu_equals_model_takes"] = sum(r["emu_vs_model"] == 0 for r in rows)
                s["emu_vs_model_samples"] = sum(r["emu_vs_model"] for r in rows)
            totals[name] = s
            print(name, json.dumps(s), flush=True)
    # Merge with the sets of earlier runs
    path = os.path.join(args.out, "summary.json")
    if os.path.exists(path):
        with open(path) as f:
            totals = {**json.load(f), **totals}
    with open(path, "w") as f:
        json.dump(dict(sorted(totals.items())), f, indent=1)


if __name__ == "__main__":
    main()
