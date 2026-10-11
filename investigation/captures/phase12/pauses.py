#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the chip outputs during the silence frames of a pause.

Reads the decoded captures p12-01 to p12-08 (decode.py's
word0_samples.csv) and the Speak External streams they spoke, and prints,
for each pause of each take, the range of codes in every frame and the
AC level at TP2 across the pause's silent frames, against the loudest
frame of speech. Reference datasheet section 12.2, "Silence inside an
utterance rings".

    python investigation/tools/decode.py investigation/captures/phase12/evidence/p12-01
    python investigation/captures/phase12/pauses.py
"""

import csv
import math
from pathlib import Path

EVIDENCE = Path(__file__).resolve().parent / "evidence"
TAKES = {
    "p12-01": "plain.bin",
    "p12-02": "plain.bin",
    "p12-03": "neutral_first.bin",
    "p12-04": "neutral_first.bin",
    "p12-05": "neutral_second.bin",
    "p12-06": "neutral_second.bin",
    "p12-07": "a0026_plain.bin",
    "p12-08": "a0026_quiet.bin",
}
FRAME_SAMPLES = 200
IDLE_CODE = -1
# Datasheet section 8.1: K field widths of a voiced frame.
K_BITS = (5, 5, 4, 4, 4, 4, 4, 3, 3, 3)
UNVOICED_K = 4
# The neutral frame's K indices: each K's Table 8-6 entry nearest zero
# (K1 -1, K2 -18, K3 -9, K4 5, K5 -3, K6 10, K7 -21, K8 29, K9 -15,
# K10 14, in units of 1/512).
NEUTRAL_K = (23, 9, 8, 6, 7, 6, 6, 3, 3, 3)
# Samples left out at each end of a frame, for the timing error of
# placing frames from the first non-idle sample.
MARGIN = 20


def frameKinds(stream):
    """One letter per frame of a Speak External stream: '.' silence,
    'V' voiced ('N' for the neutral frame: energy 1, NEUTRAL_K), 'u'
    unvoiced, 'r' repeat, 'S' the stop.
    Bits are taken from each byte least significant first, and each field
    most significant bit first (section 8.1)."""
    bits = [(byte >> i) & 1 for byte in stream for i in range(8)]
    pos = 0

    def take(n):
        nonlocal pos
        value = 0
        for i in range(n):
            value = (value << 1) | bits[pos + i]
        pos += n
        return value

    kinds = []
    while True:
        energy = take(4)
        if energy == 0:
            kinds.append(".")
            continue
        if energy == 15:
            kinds.append("S")
            return "".join(kinds)
        repeat = take(1)
        pitch = take(6)
        if repeat:
            kinds.append("r")
        elif pitch == 0:
            take(sum(K_BITS[:UNVOICED_K]))
            kinds.append("u")
        else:
            k = tuple(take(n) for n in K_BITS)
            kinds.append("N" if energy == 1 and k == NEUTRAL_K else "V")


def readTake(directory):
    with open(directory / "word0_samples.csv", newline="") as f:
        rows = list(csv.DictReader(f))
    return [int(r["code"]) for r in rows], [float(r["dac_v"]) for r in rows]


def acRms(values):
    mean = sum(values) / len(values)
    return math.sqrt(sum((v - mean) ** 2 for v in values) / len(values))


def pauses(kinds):
    """(first, end) of every run of two or more frames that are silence,
    or the neutral frame inside one."""
    runs, j = [], 0
    while j < len(kinds):
        if kinds[j] != ".":
            j += 1
            continue
        end = j
        while end < len(kinds) and kinds[end] in ".N":
            end += 1
        if end - j >= 2:
            runs.append((j, end))
        j = end
    return runs


def main():
    for take, streamName in TAKES.items():
        kinds = frameKinds((EVIDENCE / "streams" / streamName).read_bytes())
        codes, volts = readTake(EVIDENCE / take)
        start = next(i for i, c in enumerate(codes) if c != IDLE_CODE)

        def frame(j, values):
            a = start + FRAME_SAMPLES * j
            return values[a + MARGIN : a + FRAME_SAMPLES - MARGIN]

        loudest = max(
            acRms(frame(j, volts))
            for j in range(len(kinds) - 1)
            if kinds[j] not in ".N"
        )
        print(f"{take} ({streamName}): {kinds}")
        for first, end in pauses(kinds):
            ranges = " ".join(
                f"{kinds[j]}{min(frame(j, codes))}..{max(frame(j, codes))}"
                for j in range(first, end)
            )
            # The pause's silent frames after its first (over which the
            # sound before fades) and after any neutral frame.
            neutral = kinds.find("N", first, end)
            after = (neutral + 1) if neutral >= 0 else first + 1
            quiet = [v for j in range(after, end) for v in frame(j, volts)]
            level = ""
            if quiet:
                rms = acRms(quiet)
                level = (
                    f", TP2 {rms * 1000:.2f} mV rms, "
                    f"{20 * math.log10(rms / loudest):.1f} dB re loudest frame"
                )
            print(f"  frames {first}-{end - 1}: {ranges}{level}")


if __name__ == "__main__":
    main()
