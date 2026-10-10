#!/usr/bin/env python3
"""
    phase11.py

    Phase 11 (plan section 12): new speech data, made with the project's
    encoder (encoder/, written from the datasheet alone), predicted with
    the reference emulator before the chip is run, then captured and
    compared with the predictions (plan section 9.4, acceptance tests 2
    and 3).

      python tools/phase11.py roundtrip     # test 3: every PROM word parsed and re-packed
      python tools/phase11.py build         # encode the inputs, write the data sets
      python tools/phase11.py predict       # the emulator's predictions, per item and path
      python tools/phase11.py check         # the sets and predictions against their hashes
      python tools/run.py captures/phase11/data/<set>.json all --takes 3 --path both \\
          --out captures/phase11/takes --zip-only --resume --timeout-ms 30000
      python tools/phase11.py compare       # the chip against the predictions

    The data sets, in captures/phase11/data/, are synthetic PROMs as
    vectors.py writes them (a manifest, the image and every word's frames),
    so run.py captures them like any other:

      phrom   an Acorn-format PHROM (datasheet section 12.8) of the single
              words, numbers and computer phrases, in one voice each
      tts1..  every TTS input (captures/phase11/inputs/tts/)
      rec     every real recording (captures/phase11/inputs/recordings/)
      hand    hand-designed frame sequences that use every feature of the
              frame format, built by hand from the datasheet

    Inputs longer than SPLIT_S are encoded as several utterances, cut at
    pauses, because the Pico's trace holds about 16 s of VSM speech.

    Predictions (captures/phase11/predictions/): for every word and path,
    the emulator's I/O codes from 5 samples before the first frame read to
    the end of its output, and for the VSM path the bits it reads, the
    sample of each frame read and when Talk Status clears. Each utterance
    is predicted after the one that run.py speaks before it: the previous
    word, for a word's first take, or the same word. Where the two differ
    (only an utterance that starts with a repeat frame, datasheet section
    7), both are kept. The predictions are committed before any take is
    made; compare checks their hashes first.

    compare judges each set by the tolerance of plan section 1.5: the
    emulator's (the prediction's) median SDR against the chip may be at
    most 7 dB below chip 1's agreement with its own takes (takes 02 and 03
    of each word and path, which both follow the same word), counting
    agreement above 40 dB as 40 dB. It also checks the VSM timing (layer
    1) against the prediction, and the chip's VSM output against its
    Speak External output.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import glob
import hashlib
import json
import multiprocessing
import os
import subprocess
import sys
import tempfile
import zlib

import numpy as np

import prom as promlib
import regress
import variance

ROOT = promlib.ROOT
sys.path.insert(0, os.path.join(ROOT, "encoder"))
import tms5220enc as enc  # noqa: E402  (encoder/, written from the datasheet alone)
import ttsgen  # noqa: E402

P11 = os.path.join(ROOT, "captures", "phase11")
INPUTS = os.path.join(P11, "inputs")
DATA = os.path.join(P11, "data")
PRED = os.path.join(P11, "predictions")
TAKES = os.path.join(P11, "takes")
RESULTS = os.path.join(P11, "results")
ROUNDTRIP = os.path.join(P11, "roundtrip")

PROMS = ["acorn_phrom_a", "vm61002", "vm61004", "vm71003a", "vm61003_words",
         "vm61005_words", "ti99_speech", "starwars_speech"]
BANK = 0
MAX_BANKS = 4               # the Pico holds 4 banks at once
SET_BYTES = MAX_BANKS * promlib.BANK_SIZE
PHROM_BANK = 0
SPLIT_S = 8.0               # longer inputs become several utterances
STANDARD_SPEAK_AT = 1961    # slots before the frame boundary (datasheet section 7)
PHROM_CATEGORIES = ("word", "number", "retro")
K_SIZES = [32, 32, 16, 16, 16, 16, 16, 8, 8, 8]

# Table 8-6 of the datasheet (K values x 512), for the hand-designed set
K_TABLE = [
    [-501, -498, -497, -495, -493, -491, -488, -482, -478, -474, -469, -464, -459, -452, -445, -437,
     -412, -380, -339, -288, -227, -158, -81, -1, 80, 157, 226, 287, 337, 379, 411, 436],
    [-328, -303, -274, -244, -211, -175, -138, -99, -59, -18, 24, 64, 105, 143, 180, 215,
     248, 278, 306, 331, 354, 374, 392, 408, 422, 435, 445, 455, 463, 470, 476, 506],
    [-441, -387, -333, -279, -225, -171, -117, -63, -9, 45, 98, 152, 206, 260, 314, 368],
    [-328, -273, -217, -161, -106, -50, 5, 61, 116, 172, 228, 283, 339, 394, 450, 506],
    [-328, -282, -235, -189, -142, -96, -50, -3, 43, 90, 136, 182, 229, 275, 322, 368],
    [-256, -212, -168, -123, -79, -35, 10, 54, 98, 143, 187, 232, 276, 320, 365, 409],
    [-308, -260, -212, -164, -117, -69, -21, 27, 75, 122, 170, 218, 266, 314, 361, 409],
    [-256, -161, -66, 29, 124, 219, 314, 409],
    [-256, -176, -96, -15, 65, 146, 226, 307],
    [-205, -132, -59, 14, 87, 160, 234, 307],
]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def codesSha256(codes):
    return sha256(np.asarray(codes, dtype="<i2").tobytes())


# ---- acceptance test 3: the round trip ---------------------------------

def bitsOf(data):
    return np.unpackbits(np.frombuffer(bytes(data), dtype=np.uint8), bitorder="little")


def roundtrip():
    """Every word of every PROM: parsed with the documented frame format
    and packed again by the encoder, it must give back the original bits
    up to the end of its stop code."""
    os.makedirs(ROUNDTRIP, exist_ok=True)
    rows, summary = [], {}
    for name in PROMS:
        manifest = promlib.loadManifest(name)
        data = promlib.image(manifest)
        base = promlib.base(manifest)
        seen = set()
        n = same = padSame = 0
        for word in manifest["words"]:
            address = promlib.address(word)
            if address in seen:
                continue
            seen.add(address)
            original = data[address - base:]
            frames, used = enc.parse(original)
            if not frames or frames[-1].get("energy") != 15:
                raise ValueError(f"{name} word {word['number']}: no stop frame")
            packed = enc.pack(frames)
            ok = len(packed) * 8 >= used and np.array_equal(bitsOf(packed)[:used], bitsOf(original)[:used])
            stopBytes = (used + 7) // 8
            pad = packed[:stopBytes] == original[:stopBytes]
            n += 1
            same += ok
            padSame += pad
            rows.append({"prom": name, "number": word["number"], "name": word["name"],
                         "address": word["address"], "frames": len(frames), "bits": used,
                         "bits_identical": int(ok), "bytes_identical": int(pad)})
        summary[name] = {"words": n, "bits_identical": same, "bytes_identical": padSame}
        print(f"{name:16s} {same}/{n} words bit-identical to the stop code, "
              f"{padSame}/{n} including the padding of the stop code's byte")
    total = {k: sum(s[k] for s in summary.values()) for k in ("words", "bits_identical", "bytes_identical")}
    summary["all"] = total
    with open(os.path.join(ROUNDTRIP, "words.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    with open(os.path.join(ROUNDTRIP, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)
    print(f"all: {total['bits_identical']}/{total['words']} bit-identical")
    return total["bits_identical"] == total["words"]


# ---- the hand-designed frame sequences ---------------------------------

def nearestK(j, value):
    return int(np.argmin([abs(v - value) for v in K_TABLE[j]]))


NEUTRAL = [nearestK(j, 0) for j in range(10)]


def stepDown(a):
    """Reflection coefficients of A(z) = 1 + a1 z^-1 + ... (datasheet
    section 12.3: a_i of order i is k_i)."""
    a = list(a)
    k = [0.0] * len(a)
    for i in range(len(a), 0, -1):
        ki = a[i - 1]
        k[i - 1] = ki
        if i > 1:
            a = [(a[j] - ki * a[i - 2 - j]) / (1 - ki * ki) for j in range(i - 1)]
    return k


def formantK(formants, rate=8000):
    """K indices for an all-pole filter with poles at the given (frequency,
    bandwidth) pairs, in Hz."""
    poly = np.array([1.0])
    for f, bw in formants:
        r = np.exp(-np.pi * bw / rate)
        poly = np.convolve(poly, [1.0, -2 * r * np.cos(2 * np.pi * f / rate), r * r])
    a = list(poly[1:]) + [0.0] * (10 - (len(poly) - 1))
    return [nearestK(j, 512 * k) for j, k in enumerate(stepDown(a))]


# vowels: (F1, F2, F3, F4, F5) with bandwidths, an /a/, /i/ and /u/
VOWEL_A = formantK([(700, 90), (1200, 100), (2500, 140), (3300, 200), (3750, 250)])
VOWEL_I = formantK([(280, 70), (2250, 110), (2900, 150), (3400, 200), (3800, 250)])
VOWEL_U = formantK([(310, 70), (870, 90), (2250, 140), (3300, 200), (3750, 250)])
FRIC_S = formantK([(3500, 600), (2500, 900)])[:4]
FRIC_SH = formantK([(2200, 500), (3200, 700)])[:4]


def V(e, p, k):
    return {"energy": e, "repeat": 0, "pitch": p, "k": list(k)}


def U(e, k):
    return {"energy": e, "repeat": 0, "pitch": 0, "k": list(k[:4])}


def R(e, p):
    return {"energy": e, "repeat": 1, "pitch": p}


SIL = {"energy": 0}


def handSet():
    """(name, frames) pairs: every frame kind and transition of datasheet
    sections 8.1, 8.7 and 12.2, at speech levels. None starts with a
    silence frame, whose start rule is approximate (datasheet section 7);
    one starts with a repeat frame, which takes K1-K10 from the utterance
    before it."""
    out = []
    a, i, u = VOWEL_A, VOWEL_I, VOWEL_U
    # every kind of frame, in one utterance
    out.append(("kinds", [V(10, 40, a)] * 3 + [R(10, 38), R(9, 36)] + [U(6, FRIC_S)] * 2
                + [R(5, 0), R(4, 0)] + [V(10, 36, i)] * 2 + [SIL] * 2 + [R(10, 34), V(10, 34, u)] * 2
                + [R(8, 34)]))
    # energy: every index up and down, voiced and unvoiced
    out.append(("energy_ramp_v", [V(e, 40, a) for e in list(range(1, 15)) + list(range(13, 0, -1))]))
    out.append(("energy_ramp_u", [U(e, NEUTRAL) for e in list(range(1, 15)) + list(range(13, 0, -1))]))
    # pitch: the whole range, as a glide and as jumps
    out.append(("pitch_glide", [V(10, p, a) for p in list(range(1, 64, 3)) + list(range(63, 0, -4))]))
    out.append(("pitch_jumps", [V(10, p, i) for p in (1, 63, 1, 63, 20, 50, 5, 60)] * 1 + [R(10, 30)] * 2))
    # each K from one end of its table to the other and back, the rest neutral
    for j in range(10):
        lo, hi = list(NEUTRAL), list(NEUTRAL)
        lo[j], hi[j] = 0, K_SIZES[j] - 1
        e = 5 if j == 0 else 8      # K1 = -501 resonates strongly
        out.append((f"k{j + 1}_jump", [V(e, 30, lo)] * 2 + [V(e, 30, hi)] * 2 + [V(e, 30, lo)] * 2))
    # vowel to vowel: large jumps of every K at once
    out.append(("vowel_jumps", [V(10, 34, a)] * 2 + [V(10, 34, i)] * 2 + [V(10, 34, u)] * 2
                + [V(10, 34, a), V(10, 34, i), V(10, 34, u), V(10, 34, a)]))
    # voicing transitions: each is inhibited (datasheet section 8.7)
    out.append(("v_u_alternate", [V(11, 38, a), U(6, FRIC_S)] * 4 + [V(11, 38, a)]))
    out.append(("v_sil_v", [V(11, 30, a)] * 2 + [SIL] + [V(11, 46, i)] * 2 + [SIL] * 3 + [V(11, 20, u)] * 2))
    out.append(("u_sil_u", [U(7, FRIC_S)] * 2 + [SIL] + [U(7, FRIC_SH)] * 2 + [SIL] * 2 + [U(5, FRIC_S)] * 2))
    out.append(("sil_repeat", [V(11, 40, a)] * 2 + [SIL] * 2 + [R(11, 44)] * 2 + [SIL] + [R(9, 0)] * 2))
    # K5-K10 after an unvoiced frame: a voiced repeat frame keeps them at zero
    out.append(("u_then_vrepeat", [V(9, 36, i)] * 2 + [U(6, FRIC_SH)] + [R(9, 36)] * 3 + [V(9, 36, i)] * 2))
    # endings: unvoiced to stop (full level to the end), faded unvoiced, voiced
    out.append(("end_unvoiced", [V(11, 36, a)] * 2 + [U(7, FRIC_S)] * 3))
    out.append(("end_unvoiced_faded", [V(11, 36, a)] * 2 + [U(7, FRIC_S), R(5, 0), R(3, 0), R(1, 0)]))
    out.append(("end_voiced_sil", [V(11, 36, u)] * 3 + [SIL] * 2))
    # held: interpolation settled for a second, then the noise alone
    out.append(("held_vowel", [V(10, 44, a)] * 40))
    out.append(("held_noise", [U(6, FRIC_S)] * 40))
    # extremes at a safe level: every K at its first or last index
    out.append(("k_all_min", [V(3, 30, [0] * 10)] * 4))
    out.append(("k_all_max", [V(1, 30, [n - 1 for n in K_SIZES])] * 4))
    # a word under 9 bytes, lengthened with silence frames for Speak External
    out.append(("short_padded", None))
    # carried state: a first frame that is a repeat frame takes K1-K10 from
    # the utterance before (datasheet section 7)
    out.append(("first_repeat", [R(10, 40)] * 3 + [V(10, 40, u)] * 2))
    return out


# ---- encoding and packaging --------------------------------------------

def readIndex(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def splitPoints(x, rate=8000, limit=SPLIT_S):
    """Cut points (samples) so that each part is at most limit seconds,
    each at the quietest 50 ms of a pause in the second half of the part."""
    hop = rate // 100
    level = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2)) + 1e-9 for i in range(0, len(x) - hop + 1, hop)])
    db = 20 * np.log10(level)
    smooth = np.convolve(db, np.ones(5) / 5, mode="same")
    cuts, start = [], 0
    while (len(x) - start) / rate > limit:
        lo, hi = (start + int(0.5 * limit * rate)) // hop, (start + int(limit * rate)) // hop
        best = lo + int(np.argmin(smooth[lo:hi]))
        cuts.append(best * hop + hop // 2)
        start = cuts[-1]
    return cuts


def encodeInput(path):
    """[(part, frames, start, end)] for one input file."""
    rate, x = ttsgen.readFlac(path)
    x = x.astype(float) / 32768.0
    cuts = splitPoints(x, rate)
    bounds = [0] + cuts + [len(x)]
    out = []
    for n, (a, b) in enumerate(zip(bounds, bounds[1:]), 1):
        out.append((n if cuts else 0, enc.encode(x[a:b], rate=rate), a, b))
    return out


def encodeJob(job):
    return job, encodeInput(job)


def encodedItems(rows, root):
    """(name, frames, source row, part, start, end) for every input row."""
    files = [os.path.join(root, r["file"]) for r in rows]
    with multiprocessing.Pool() as pool:
        results = dict(pool.map(encodeJob, files))
    out = []
    for r, f in zip(rows, files):
        stem = os.path.splitext(r["file"])[0]
        for part, frames, a, b in results[f]:
            out.append((f"{stem}_{part}" if part else stem, frames, r, part, a, b))
    return out


def writeSet(name, title, provenance, items, image=None, table=None, fmt="phase11", bank=BANK):
    """Write a set like vectors.py: <name>.bin, <name>.json and <name>.csv,
    plus items.csv tying each word to its input."""
    if image is None:
        image, table = bytearray(), []
        for number, (label, frames, *_rest) in enumerate(items, 1):
            stream = bytes(enc.pack(frames))
            table.append({"number": number, "name": label, "offset": len(image), "bytes": len(stream),
                          "frames": len(frames)})
            image += stream
        image = bytes(image)
    if len(image) > SET_BYTES:
        raise ValueError(f"{name}: {len(image)} bytes does not fit in {MAX_BANKS} banks")
    base = bank << 14
    words = [{"number": t["number"], "address": f"0x{base + t['offset']:05X}", "name": t["name"],
              "frames": t["frames"], "bytes": t["bytes"]} for t in table]
    os.makedirs(DATA, exist_ok=True)
    imagePath = os.path.join(DATA, f"{name}.bin")
    with open(imagePath, "wb") as f:
        f.write(image)
    manifest = {
        "name": f"phase11_{name}",
        "title": title,
        "target_chip": "TMS5220",
        "image": {"source": os.path.relpath(imagePath, ROOT), "kind": "synthetic",
                  "size": len(image), "sha256": sha256(image), "crc32": f"{zlib.crc32(image):08x}"},
        "bank": bank,
        "format": fmt,
        "provenance": provenance,
        "words": words,
    }
    with open(os.path.join(DATA, f"{name}.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    byName = {i[0]: i for i in items}
    with open(os.path.join(DATA, f"{name}.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["word", "name", "frame", "kind", "energy", "repeat", "pitch"] + [f"k{i}" for i in range(1, 11)])
        for word in words:
            offset = int(word["address"], 16) - base
            frames, _ = enc.parse(image[offset:])
            for n, fr in enumerate(frames):
                k = fr.get("k", [])
                kind = ("stop" if fr["energy"] == 15 else "silence" if fr["energy"] == 0 else
                        "repeat" if fr["repeat"] else "voiced" if fr["pitch"] else "unvoiced")
                w.writerow([word["number"], word["name"], n, kind, fr["energy"], fr.get("repeat", ""),
                            fr.get("pitch", "")] + [k[i] if i < len(k) else "" for i in range(10)])
    with open(os.path.join(DATA, f"{name}_items.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["word", "name", "input", "text", "voice", "part", "start_sample", "end_sample",
                    "input_samples_sha256", "seconds"])
        for word in words:
            label, frames, row, part, a, b = byName[word["name"]]
            w.writerow([word["number"], word["name"], row.get("path", ""), row.get("text", ""),
                        row.get("voice", ""), part, a, b, row.get("samples_sha256", ""),
                        f"{word['frames'] * 0.025:.3f}"])
    print(f"{name}: {len(words)} words, {len(image)} bytes, "
          f"{sum(w['frames'] for w in words) * 0.025:.1f} s")
    return manifest


def build():
    tts = readIndex(os.path.join(INPUTS, "tts", "index.csv"))
    rec = readIndex(os.path.join(INPUTS, "recordings", "index.csv"))
    for r in tts:
        r["path"] = os.path.relpath(os.path.join(INPUTS, "tts", r["file"]), ROOT)
    for r in rec:
        r["path"] = os.path.relpath(os.path.join(INPUTS, "recordings", r["file"]), ROOT)
    ttsItems = encodedItems(tts, os.path.join(INPUTS, "tts"))
    recItems = encodedItems(rec, os.path.join(INPUTS, "recordings"))

    # the PHROM: each word, number and computer phrase in its first voice
    first = {}
    for item in ttsItems:
        row = item[2]
        if row["category"] in PHROM_CATEGORIES and item[3] == 0 and row["id"] not in first:
            first[row["id"]] = item
    phromItems = [(row_id, frames, r, part, a, b) for row_id, (label, frames, r, part, a, b) in first.items()]
    image, table = enc.acornPhrom([(label, frames) for label, frames, *_ in phromItems])
    writeSet("phrom", "Phase 11 PHROM: new words, numbers and phrases (Acorn format)",
             "tools/phase11.py: the single words, numbers and computer phrases of tools/prompts.csv, "
             "one voice each, encoded by encoder/ and laid out as an Acorn PHROM (datasheet section 12.8)",
             phromItems, image=image, table=table, fmt="acorn", bank=PHROM_BANK)

    # every TTS input, in as few sets as fit
    sets, current, size = [], [], 0
    for item in ttsItems:
        n = len(enc.pack(item[1]))
        if size + n > SET_BYTES:
            sets.append(current)
            current, size = [], 0
        current.append(item)
        size += n
    sets.append(current)
    for n, items in enumerate(sets, 1):
        writeSet(f"tts{n}", f"Phase 11 TTS speech, part {n}",
                 "tools/phase11.py: Piper TTS renders of tools/prompts.csv (captures/phase11/inputs/tts/), "
                 "encoded by encoder/", items)
    writeSet("rec", "Phase 11 recorded speech",
             "tools/phase11.py: real recordings (captures/phase11/inputs/recordings/), encoded by encoder/",
             recItems)

    # the hand-designed set
    hand = []
    for label, frames in handSet():
        if frames is None:
            frames = enc.parse(enc.speakExternalStream([V(10, 40, VOWEL_A), {"energy": 15}]))[0]
        elif frames[-1].get("energy") != 15:
            frames = frames + [{"energy": 15}]
        hand.append((label, frames, {"text": "", "voice": ""}, 0, "", ""))
    writeSet("hand", "Phase 11 hand-designed frame sequences",
             "tools/phase11.py handSet(): every frame kind and transition of datasheet sections 8.1, 8.7 "
             "and 12.2, at speech levels, built by hand", hand)


def setNames():
    return [os.path.splitext(os.path.basename(p))[0] for p in sorted(glob.glob(os.path.join(DATA, "*.json")))]


# ---- predictions --------------------------------------------------------

def emulate(imagePath, base, address, external, prior, tmp, tag):
    """The emulator's output for one utterance spoken with the standard
    procedure after the utterances at the addresses in prior."""
    samplesPath, eventsPath = os.path.join(tmp, tag + ".s"), os.path.join(tmp, tag + ".e")
    cmd = [regress.EMU, "-s", samplesPath, "-e", eventsPath, "--image", imagePath,
           "--load-at", f"{base:X}", "--addr", f"{address:X}", "--speak-at", str(STANDARD_SPEAK_AT), "--reset"]
    if external:
        cmd.append("--external")
    if prior:
        cmd += ["--prior", regress.priorScript(prior, os.path.join(tmp, tag + ".prior"))]
    subprocess.run(cmd, check=True)
    emu = regress.readSamples(samplesPath)
    events = regress.readEvents(eventsPath)
    mark = [s for s, e in events if e[0] == "MARK" and e[1].startswith("speak")][-1]
    ready = next(int(e[4]) for s, e in events if s == mark and e[0] == "HOST")
    first = (ready + STANDARD_SPEAK_AT) // 20
    codes = emu[first - 5:]
    # trim the idle tail, keeping 200 samples of it
    active = np.flatnonzero(codes != -1)
    end = min(len(codes), (active[-1] + 201) if len(active) else 10)
    out = {"codes": codes[:end].astype(np.int16)}
    if not external:
        after = [(s, e) for s, e in events if s > mark]
        reads = [s for s, e in after if e[:2] == ["VSM", "READ"]]
        out["bits"] = len(reads)
        out["frame_samples"] = [s // 20 - first for n, s in enumerate(reads)
                                if n == 0 or s - reads[n - 1] > 1000]
        ts = next((s for s, e in after if e[:3] == ["FLAG", "TS", "0"]), None)
        out["talk_us"] = round((ts - reads[0]) * regress.SLOT_US, 2) if ts else None
    return out


def predictJob(job):
    setName, imagePath, base, words, n, tmp = job
    word = words[n]
    address = int(word["address"], 16)
    priors = {"same": [address]}
    if n:
        priors["prev"] = [int(words[n - 1]["address"], 16)]
    out = {}
    for path in ("vsm", "external"):
        if path == "external" and word["bytes"] < 9:
            continue
        results = {v: emulate(imagePath, base, address, path == "external", p, tmp,
                              f"{setName}_{n}_{path}_{v}") for v, p in priors.items()}
        same = results["same"]
        if all(np.array_equal(r["codes"], same["codes"]) for r in results.values()):
            results = {"any": same}
        for v, r in results.items():
            out[f"{word['number']}_{path}_{v}"] = r
    return word, out


def predict():
    os.makedirs(PRED, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp, multiprocessing.Pool() as pool:
        for name in setNames():
            manifest = promlib.loadManifest(os.path.join(DATA, f"{name}.json"))
            imagePath = os.path.join(ROOT, manifest["image"]["source"])
            base = promlib.base(manifest)
            words = manifest["words"]
            jobs = [(name, imagePath, base, words, n, tmp) for n in range(len(words))]
            arrays, meta = {}, {}
            for word, results in pool.map(predictJob, jobs):
                for key, r in results.items():
                    arrays[key] = r["codes"]
                    meta[key] = {k: v for k, v in r.items() if k != "codes"}
                    meta[key].update({"word": word["name"], "samples": int(len(r["codes"])),
                                      "codes_sha256": codesSha256(r["codes"]),
                                      "peak": int(np.abs(r["codes"].astype(int)).max())})
            np.savez_compressed(os.path.join(PRED, f"{name}.npz"), **arrays)
            info = {"set": name, "image_sha256": manifest["image"]["sha256"],
                    "emulator": os.path.relpath(regress.EMU, ROOT),
                    "speak_at_slots": STANDARD_SPEAK_AT,
                    "codes_from": "5 samples before the first frame read (VSM), or where it would be "
                                  "(Speak External)",
                    "predictions": meta}
            with open(os.path.join(PRED, f"{name}.json"), "w") as f:
                json.dump(info, f, indent=1)
            peaks = [m["peak"] for m in meta.values()]
            print(f"{name}: {len(meta)} predictions, peak |code| {max(peaks)}, "
                  f"{sum(p > 127 for p in peaks)} over 127")


def loadPredictions(name):
    with open(os.path.join(PRED, f"{name}.json")) as f:
        info = json.load(f)
    arrays = np.load(os.path.join(PRED, f"{name}.npz"))
    bad = [k for k, m in info["predictions"].items() if codesSha256(arrays[k]) != m["codes_sha256"]]
    if bad:
        raise ValueError(f"{name}: predictions {bad[:5]} do not match their hashes")
    return info, arrays


def check():
    ok = True
    for name in setNames():
        manifest = promlib.loadManifest(os.path.join(DATA, f"{name}.json"))
        try:
            promlib.image(manifest)
            info, _ = loadPredictions(name)
            if info["image_sha256"] != manifest["image"]["sha256"]:
                raise ValueError("predicted from another image")
            print(f"{name}: image and {len(info['predictions'])} predictions OK")
        except (ValueError, FileNotFoundError) as e:
            print(f"{name}: {e}")
            ok = False
    return ok


# ---- comparison with the chip --------------------------------------------

def compareTake(job):
    take, previous, pred, setName = job
    capture, codes, reads = regress.loadChip(take)
    external = capture["path"] == "external"
    first = reads[0] if reads and not external else regress.STANDARD_FIRST_READ_SAMPLE
    address = int(capture["source"]["address"], 16)
    number = capture["source"]["word_number"]
    path = "external" if external else "vsm"
    variant = "any"
    if f"{number}_{path}_any" not in pred["meta"]:
        variant = "same" if previous and previous[-1] == address else "prev"
    key = f"{number}_{path}_{variant}"
    row = {"set": setName, "take": os.path.relpath(take, ROOT), "word": number,
           "name": capture["source"]["word"], "path": path, "prediction": key}
    if key not in pred["meta"]:
        row["note"] = "no prediction for this predecessor"
        return row
    p = pred["arrays"][key].astype(np.int64)
    row.update(variance.measure((codes, first), (p, 5)))
    if not external:
        m = pred["meta"][key]
        summary = capture["processed"]["summary"]
        row["bits_ok"] = int(summary.get("bits_consumed") == m["bits"])
        row["frames_ok"] = int([r - first for r in reads] == m["frame_samples"])
        talk = summary.get("talk_after_first_read_s")
        row["talk_us_diff"] = round(talk * 1e6 - m["talk_us"], 1) if talk and m["talk_us"] else ""
    return row


def pairJob(job):
    kind, setName, a, b = job
    row = {"kind": kind, "set": setName, "take": os.path.relpath(a, ROOT), "other": os.path.relpath(b, ROOT)}
    other = variance.chipCodes(b)
    if kind == "paths":
        # Speak External runs 2 samples ahead of the VSM Speak (datasheet section 8.5)
        other = (other[0], other[1] - 2)
    row.update(variance.measure(variance.chipCodes(a), other))
    return row


def unvoicedFree(manifest, number):
    data = promlib.image(manifest)
    word = promlib.findWord(manifest, number)
    frames, _ = enc.parse(data[promlib.address(word) - promlib.base(manifest):])
    return not any(f.get("pitch") == 0 and f["energy"] not in (0, 15) for f in frames)


def compare():
    os.makedirs(RESULTS, exist_ok=True)
    rows, pairs, summary = [], [], {}
    with multiprocessing.Pool() as pool:
        for name in setNames():
            root = os.path.join(TAKES, f"phase11_{name}")
            if not os.path.isdir(root):
                continue
            info, arrays = loadPredictions(name)
            pred = {"meta": info["predictions"], "arrays": {k: arrays[k] for k in arrays.files}}
            manifest = promlib.loadManifest(os.path.join(DATA, f"{name}.json"))
            takes = regress.takesOf(root, False)
            setRows = pool.map(compareTake, [(t, p, pred, name) for t, p in takes])
            # chip 1 against itself: takes 02 and 03, which both follow the same word;
            # and the VSM path against Speak External, on words with no unvoiced frame
            jobs = []
            for word in sorted(glob.glob(os.path.join(root, "*", ""))):
                for prefix in ("vsm", "ext"):
                    a, b = os.path.join(word, f"{prefix}02"), os.path.join(word, f"{prefix}03")
                    if os.path.isdir(a) and os.path.isdir(b) and variance.standard(a) and variance.standard(b):
                        jobs.append(("take", name, a, b))
                a, b = os.path.join(word, "vsm02"), os.path.join(word, "ext02")
                number = int(os.path.basename(word.rstrip("/")).split("_")[0])
                if (os.path.isdir(a) and os.path.isdir(b) and variance.standard(a) and variance.standard(b)
                        and unvoicedFree(manifest, number)):
                    jobs.append(("paths", name, a, b))
            setPairs = pool.map(pairJob, jobs)
            rows += setRows
            pairs += setPairs
            sdr = [r["sdr_db"] for r in setRows if "sdr_db" in r]
            ref = [p["sdr_db"] for p in setPairs if p["kind"] == "take"]
            paths = [p for p in setPairs if p["kind"] == "paths"]
            vsm = [r for r in setRows if r["path"] == "vsm" and "bits_ok" in r]
            talk = [abs(r["talk_us_diff"]) for r in vsm if r.get("talk_us_diff") != ""]
            emuMedian = float(np.median(sdr)) if sdr else None
            refMedian = float(np.median(ref)) if ref else None
            s = {
                "takes": len(setRows), "predicted": len(sdr),
                "unpredicted": sum("sdr_db" not in r for r in setRows),
                "emulator_sdr_db_p10_p50_p90": variance.percentiles(sdr),
                "emulator_samples_equal_pct_median": float(np.median([r["equal_pct"] for r in setRows if "equal_pct" in r])) if sdr else None,
                "emulator_max_abs_p90": float(np.percentile([r["max_abs"] for r in setRows if "max_abs" in r], 90)) if sdr else None,
                "reference_pairs": len(ref), "reference_sdr_db_p10_p50_p90": variance.percentiles(ref),
                "layer1_vsm_takes": len(vsm), "bits_ok": sum(r["bits_ok"] for r in vsm),
                "frames_ok": sum(r["frames_ok"] for r in vsm),
                "talk_us_max_abs_diff": max(talk) if talk else None,
                "vsm_vs_external_pairs": len(paths),
                "vsm_vs_external_exact": sum(p["max_abs"] == 0 for p in paths),
                "vsm_vs_external_sdr_db_p10_p50_p90": variance.percentiles([p["sdr_db"] for p in paths]),
            }
            if emuMedian is not None and refMedian is not None:
                agreement = min(refMedian, variance.AGREE_CAP_DB)
                s["tolerance"] = {"reference": "chip 1 takes", "reference_db": round(refMedian, 2),
                                  "emulator_db": round(emuMedian, 2), "gap_db": round(agreement - emuMedian, 2),
                                  "within": bool(emuMedian >= agreement - variance.MARGIN_DB)}
            summary[name] = s
            print(name, json.dumps(s))
    fields = ["set", "take", "word", "name", "path", "prediction", "compared", "equal_pct", "rms", "max_abs",
              "sdr_db", "first_diff", "bits_ok", "frames_ok", "talk_us_diff", "note"]
    with open(os.path.join(RESULTS, "takes.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fields, restval="")
        w.writeheader()
        w.writerows(rows)
    with open(os.path.join(RESULTS, "pairs.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, ["kind", "set", "take", "other", "compared", "equal_pct", "rms", "max_abs",
                               "sdr_db", "first_diff"])
        w.writeheader()
        w.writerows(pairs)
    summary["tolerance_rule"] = {"margin_db": variance.MARGIN_DB, "agreement_cap_db": variance.AGREE_CAP_DB,
                                 "note": "plan section 1.5; observed at the close of Phase 9, not derived"}
    with open(os.path.join(RESULTS, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["roundtrip", "build", "predict", "check", "compare"])
    args = parser.parse_args()
    if args.command == "roundtrip":
        sys.exit(0 if roundtrip() else 1)
    elif args.command == "build":
        build()
    elif args.command == "predict":
        predict()
    elif args.command == "check":
        sys.exit(0 if check() else 1)
    else:
        compare()


if __name__ == "__main__":
    main()
