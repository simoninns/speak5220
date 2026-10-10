#!/usr/bin/env python3
"""
    vectors.py

    Synthetic test vectors (plan sections 9.1 and 9.2, Phase 8). Each set
    is a list of frame sequences chosen to expose one part of the chip: a
    sweep of one parameter with the others held, interpolation jumps,
    special frames, the noise generator, or random data. A set is written
    as a synthetic PROM, so the capture runner treats it like any other:

      captures/phase8/vectors/<set>.json   a manifest (prom.py), whose word
                                           table lists the vectors
      captures/phase8/vectors/<set>.bin    the VSM image: the vectors one after
                                           another, each from a byte boundary
      captures/phase8/vectors/<set>.csv    every vector's frames, readable

    run.py then speaks each vector through the VSM and as a Speak External
    stream (the vector's bytes up to its stop code):

      python tools/vectors.py build all
      python tools/run.py captures/phase8/vectors/energy.json all --takes 2 \\
          --path both --out captures/vectors --zip-only

    Frames are dicts as lpc.parseFrames returns them; every field is a
    coded index, not a value (the tables are what the captures measure).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import hashlib
import json
import os
import random
import zlib

import lpc
import prom as promlib

OUT_DIR = os.path.join(promlib.ROOT, "captures", "phase8", "vectors")
FIRST_BANK = 0          # vector images start in bank 0 and may span up to 4 banks
MAX_BANKS = 4

# K indices whose values lie close to zero, so that the lattice filter
# passes the excitation almost unchanged (the values are MAME's, the
# hypothesis the sweeps test): K1 -1, K2 -18, K3 -9, K4 5, K5 -3, K6 10,
# K7 -21, K8 29, K9 -15, K10 14 (of 512)
NEUTRAL_K = [23, 9, 8, 6, 7, 6, 6, 3, 3, 3]
K_SIZES = [1 << n for n in lpc.K_BITS_VOICED]
HOLD = 6                # frames each setting is held for: long enough to settle


def V(energy, pitch, k=NEUTRAL_K):
    return {"energy": energy, "repeat": 0, "pitch": pitch, "k": list(k)}


def U(energy, k=NEUTRAL_K):
    return {"energy": energy, "repeat": 0, "pitch": 0, "k": list(k[:4])}


def R(energy, pitch):
    return {"energy": energy, "repeat": 1, "pitch": pitch}


SILENCE = {"energy": 0}
STOP = {"energy": 15}


def withK(j, index, k=NEUTRAL_K):
    k = list(k)
    k[j] = index
    return k


# ---- the sets ---------------------------------------------------------

def setEnergy():
    """Every energy index, voiced and unvoiced, with neutral K."""
    out = [(f"v_e{e:02d}", [V(e, 32)] * HOLD) for e in range(1, 15)]
    out += [(f"u_e{e:02d}", [U(e)] * HOLD) for e in range(1, 15)]
    return out


def setPitch():
    """Every non-zero pitch index, with neutral K."""
    return [(f"p{p:02d}", [V(10, p)] * HOLD) for p in range(1, 64)]


def setK():
    """Every index of every K, the other K neutral: voiced, pitch index 32.
    K1-K4 also unvoiced, since unvoiced frames use them too."""
    out = []
    for j in range(10):
        for i in range(K_SIZES[j]):
            out.append((f"k{j + 1}_{i:02d}", [V(10, 32, withK(j, i))] * HOLD))
    for j in range(4):
        for i in range(K_SIZES[j]):
            out.append((f"uk{j + 1}_{i:02d}", [U(10, withK(j, i))] * HOLD))
    return out


def setNoise():
    """Long unvoiced runs, loud and with neutral K, so that each sample's sign
    shows the noise generator's output; and the same at other energies."""
    return [("noise_e14", [U(14)] * 40), ("noise_e10", [U(10)] * 40),
            ("noise_e14_long", [U(14)] * 160)]


def setInterp():
    """Jumps between two settings of one parameter, held for HOLD frames
    each, so that every interpolation step is seen; and the transitions
    between voiced, unvoiced and silent frames that may inhibit it."""
    out = []
    low, high = [V(4, 20)], [V(13, 50)]
    out.append(("energy_jump", [V(4, 32)] * 3 + [V(13, 32)] * 3 + [V(4, 32)] * 3))
    out.append(("pitch_jump", [V(10, 10)] * 3 + [V(10, 60)] * 3 + [V(10, 10)] * 3))
    for j in range(10):
        lo, hi = withK(j, 0), withK(j, K_SIZES[j] - 1)
        out.append((f"k{j + 1}_jump", [V(10, 32, lo)] * 3 + [V(10, 32, hi)] * 3
                    + [V(10, 32, lo)] * 3))
    v, u = V(10, 32), U(10)
    out += [
        ("v_u_v", [v] * 3 + [u] * 3 + [v] * 3),
        ("u_v_u", [u] * 3 + [v] * 3 + [u] * 3),
        ("v_s_v", [v] * 3 + [SILENCE] * 3 + [v] * 3),
        ("u_s_u", [u] * 3 + [SILENCE] * 3 + [u] * 3),
        ("v_s1_v", [v] * 3 + [SILENCE] + [v] * 3),
        ("u_s1_u", [u] * 3 + [SILENCE] + [u] * 3),
        ("s_v", [SILENCE] * 3 + [v] * 3),
        ("s_u", [SILENCE] * 3 + [u] * 3),
        ("v_u1_v", [v] * 3 + [u] + [v] * 3),
        ("u_v1_u", [u] * 3 + [v] + [u] * 3),
        ("v_stop", [v] * 3),
        ("u_stop", [u] * 3),
        ("low_high", low * 3 + high * 3 + low * 3),
    ]
    return out


def setSpecial():
    """Repeat frames, silence and the stop code in unusual places, and the
    shortest utterances."""
    v, u = V(10, 32), U(10)
    k2 = withK(0, 28, withK(1, 20, NEUTRAL_K))
    return [
        ("stop_only", []),
        ("silence_only", [SILENCE]),
        ("one_voiced", [v]),
        ("one_unvoiced", [u]),
        ("repeat_voiced", [V(10, 32, k2)] + [R(13, 40)] * 3 + [R(6, 20)] * 3),
        ("repeat_to_unvoiced", [V(10, 32, k2)] * 2 + [R(10, 0)] * 3 + [V(10, 32, k2)] * 2),
        ("repeat_first", [R(10, 32)] * 3 + [v] * 2),
        ("repeat_after_unvoiced", [U(10, k2)] * 2 + [R(12, 30)] * 3),
        ("repeat_after_silence", [V(10, 32, k2)] * 2 + [SILENCE] * 2 + [R(10, 32)] * 3),
        ("unvoiced_then_voiced_k", [U(10, k2)] * 2 + [V(10, 32, k2)] * 3),
        ("silence_long", [v] * 2 + [SILENCE] * 12 + [v] * 2),
        ("silence_lead", [SILENCE] * 8 + [v] * 3),
        ("energy_one", [V(1, 32)] * 4),
    ]


def setRandom(count=60, frames=10, seed=5220):
    """Random frames with every field drawn uniformly (energy 1-14), so that
    parameter combinations no encoder would write are covered."""
    rng = random.Random(seed)
    out = []
    for n in range(count):
        seq = []
        for _ in range(frames):
            kind = rng.random()
            energy = rng.randint(1, 14)
            if kind < 0.08:
                seq.append(SILENCE)
            elif kind < 0.25:
                seq.append(R(energy, rng.randint(0, 63)))
            elif kind < 0.4:
                seq.append(U(energy, [rng.randrange(s) for s in K_SIZES]))
            else:
                seq.append(V(energy, rng.randint(1, 63), [rng.randrange(s) for s in K_SIZES]))
        out.append((f"r{n:02d}", seq))
    return out


def setLoud(count=24, seed=8):
    """Loud voiced frames with random K, which drive the filter towards and
    past its range: the output's overflow and clipping."""
    rng = random.Random(seed)
    out = []
    for n in range(count):
        k = [rng.randrange(s) for s in K_SIZES]
        k[0] = rng.choice([0, 1, 2, 30, 31])
        out.append((f"loud{n:02d}", [V(14, rng.randint(10, 63), k)] * 4))
    return out


def setRing():
    """Phase 9: the filter's lowest-order arithmetic. Voiced frames at the
    longest pitch (160 samples), so that each period ends in about 140
    samples of free ringing with zero excitation: the neutral K at every
    energy; one K at a time at its extremes; and the K2 = 506 resonance
    (radius about 0.99) at several frequencies set by K1, at a quiet and a
    loud energy."""
    out = [(f"n_e{e:02d}", [V(e, 63)] * HOLD) for e in range(1, 15)]
    for j in range(10):
        for i in (0, K_SIZES[j] - 1):
            out.append((f"k{j + 1}_{i:02d}", [V(10, 63, withK(j, i))] * HOLD))
    for e in (6, 14):
        for i in (0, 8, 16, 20, 23, 26, 31):
            out.append((f"res_e{e:02d}_k1_{i:02d}", [V(e, 63, withK(0, i, withK(1, 31)))] * HOLD))
    return out


def setLoc():
    """Phase 9: localising the lowest-order error
    (docs/technical/die-analysis/lowest-order-arithmetic.md sections 5.2 and 5.3).
    Unvoiced frames (no tails; K5-K10 zero) with two of K1-K4 large at
    once, so that an error in one product reaches the output by a
    different route from an error in another; K1 = -488 (index 6, the
    worst unvoiced K1) at energies with each lowest Booth digit; and loud
    frames followed by silence frames, which keep the K and let the filter
    ring down freely from a large amplitude to its limit cycle."""
    out = []
    # K1 = -488 with K2 at -328, 392, 408, 506 and neutral
    for i in (0, 22, 23, 31, NEUTRAL_K[1]):
        out.append((f"u_k1_06_k2_{i:02d}", [U(10, withK(0, 6, withK(1, i)))] * HOLD))
    # K2 = 392 with K1 at -501, -469, 436 and neutral
    for i in (0, 10, 31, NEUTRAL_K[0]):
        out.append((f"u_k2_22_k1_{i:02d}", [U(10, withK(1, 22, withK(0, i)))] * HOLD))
    # K1 = -488 with K3 = 152, and with K4 = -328
    out.append(("u_k1_06_k3_11", [U(10, withK(0, 6, withK(2, 11)))] * HOLD))
    out.append(("u_k1_06_k4_00", [U(10, withK(0, 6, withK(3, 0)))] * HOLD))
    # K1 = -488 at energies 1, 4, 6, 8, 11, 16, 33, 47, 85, 114 (indices)
    for e in (1, 4, 5, 6, 7, 8, 10, 11, 13, 14):
        out.append((f"u_k1_06_e{e:02d}", [U(e, withK(0, 6))] * HOLD))
    # free decay: loud voiced or unvoiced, then silence (K2 = 506 resonance)
    for i in (0, 6, 10, 20, 31):
        k = withK(0, i, withK(1, 31))
        out.append((f"dv_k1_{i:02d}", [V(14, 63, k)] * 2 + [SILENCE] * 7))
        out.append((f"du_k1_{i:02d}", [U(14, k)] * 2 + [SILENCE] * 7))
    return out


def setOnset():
    """Phase 10: the first pitch pulse after silence. In the PROM words the
    chip's chirp starts one sample later than the model's after voiced
    speech and one or two silence frames, and on time after three or after
    unvoiced speech. Voiced, n silence frames, voiced: n from 1 to 6, at
    pitches before the silence that leave the pitch counter at different
    phases; other pitches after it; and the same after unvoiced speech,
    into a repeat frame, at a loud resonant setting, and with no silence."""
    k2 = withK(0, 28, withK(1, 20, NEUTRAL_K))
    out = []
    for n in (1, 2, 3, 4, 6):
        for p in (12, 20, 28, 36, 44, 52, 60):
            out.append((f"v_p{p:02d}_s{n}_v_p24", [V(10, p)] * 3 + [SILENCE] * n + [V(10, 24)] * 3))
    for q in (12, 44, 63):
        for n in (1, 3):
            out.append((f"v_p28_s{n}_v_p{q:02d}", [V(10, 28)] * 3 + [SILENCE] * n + [V(10, q)] * 3))
    for n in (1, 2, 3):
        out.append((f"u_s{n}_v_p24", [U(10)] * 3 + [SILENCE] * n + [V(10, 24)] * 3))
        out.append((f"v_p28_s{n}_r_p24", [V(10, 28)] * 3 + [SILENCE] * n + [R(10, 24)] * 3))
        out.append((f"loud_v_p28_s{n}_v_p24", [V(14, 28, k2)] * 3 + [SILENCE] * n + [V(14, 24, k2)] * 3))
    out.append(("v_p28_v_p24", [V(10, 28)] * 3 + [V(10, 24)] * 3))
    out.append(("v_p28_u_v_p24", [V(10, 28)] * 3 + [U(10)] + [V(10, 24)] * 3))
    return out


SETS = {"energy": setEnergy, "pitch": setPitch, "k": setK, "noise": setNoise,
        "interp": setInterp, "special": setSpecial, "random": setRandom, "loud": setLoud,
        "ring": setRing, "loc": setLoc, "onset": setOnset}


# ---- writing a set ----------------------------------------------------

def build(name):
    """Write a set's image, manifest and frame list. Every vector ends with
    a stop frame."""
    vectors = SETS[name]()
    image = bytearray()
    words = []
    base = FIRST_BANK << 14
    for number, (label, frames) in enumerate(vectors, 1):
        frames = list(frames) + [STOP]
        stream = lpc.encodeFrames(frames)
        words.append({"number": number, "address": f"0x{base + len(image):05X}", "name": label,
                      "frames": len(frames), "bytes": len(stream)})
        image += stream
    if len(image) > MAX_BANKS * promlib.BANK_SIZE:
        raise ValueError(f"{name}: {len(image)} bytes does not fit in {MAX_BANKS} banks")
    image = bytes(image)
    os.makedirs(OUT_DIR, exist_ok=True)
    imagePath = os.path.join(OUT_DIR, f"{name}.bin")
    with open(imagePath, "wb") as f:
        f.write(image)
    manifest = {
        "name": f"vectors_{name}",
        "title": f"Phase 8 test vectors: {name}",
        "target_chip": "TMS5220",
        "image": {"source": os.path.relpath(imagePath, promlib.ROOT), "kind": "synthetic",
                  "size": len(image), "sha256": hashlib.sha256(image).hexdigest(),
                  "crc32": f"{zlib.crc32(image):08x}"},
        "bank": FIRST_BANK,
        "format": "vectors",
        "provenance": f"tools/vectors.py set '{name}': {SETS[name].__doc__.strip()}",
        "words": words,
    }
    with open(os.path.join(OUT_DIR, f"{name}.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    with open(os.path.join(OUT_DIR, f"{name}.csv"), "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["vector", "name"] + lpc.FRAME_COLUMNS)
        for word, (label, frames) in zip(words, vectors):
            parsed, _ = lpc.parseFrames(lpc.bitsLsbFirst(lpc.encodeFrames(list(frames) + [STOP])))
            for i, frame in enumerate(parsed):
                writer.writerow([word["number"], label] + lpc.frameRow(i, frame))
    print(f"{name}: {len(words)} vectors, {len(image)} bytes")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("build", help="write a set's image, manifest and frame list")
    p.add_argument("sets", nargs="+", help=f"set names ({', '.join(SETS)}) or 'all'")
    args = parser.parse_args()
    for name in (list(SETS) if args.sets == ["all"] else args.sets):
        build(name)


if __name__ == "__main__":
    main()
