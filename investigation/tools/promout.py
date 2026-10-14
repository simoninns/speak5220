#!/usr/bin/env python3
"""
    promout.py

    Reads the TMS5220's parameter ROM through PROM OUT (pin 10, TP10, on
    the PicoScope's logic input D0). The patent (US 4,335,277, Fig. 8f)
    taps the serial output of the parameter output register (201) to pin
    10: for each parameter in turn it carries the ROM value addressed by
    the stored coded index (reference datasheet section 11.3).

      python tools/promout.py idle          # decode the stream while idle
      python tools/promout.py dump          # every table entry, against the datasheet

    The stream, as measured: one bit per T period, LSB first, in the same
    bit slots as I/O (slot 0 starts at the T11 rising edge). It repeats
    every 25 sample periods, PC = 0-11 with an A and a B cycle each and
    PC = 12 with an A cycle only. Each parameter is a 10-bit two's
    complement value followed by copies of its sign bit. Energy starts in
    slot 0 of its sample period, pitch 40 slots later, and K1-K10 39 slots
    apart after that, each one slot earlier than the last. Pitch is the
    table value times 4, unsigned.

    PROM OUT is an open-drain output: it pulls up to about +4.5 V and
    otherwise floats, to about +1 V on the logic input, so D0's threshold
    is +3 V.

    dump speaks, through Speak External, 64 runs of 25 identical frames
    (run i: energy i % 14 + 1, pitch i, every K index i modulo its table
    size; run 0 is unvoiced) and reads the fields mid-speech. Two more
    runs read energy indices 0 (a silent frame) and 15 (idle after the
    stop code). First, while idle, it records the fraction of time D0
    reads high at thresholds from -4.5 V to +4.5 V (D1-D7, left open, for
    comparison) and where D0's edges fall within the slot. It writes
    captures/promout/chip<N>/{promout.json,rom.csv}.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os
import sys
import time

import numpy as np

import model
import pico as picolib
from lpc import encodeFrames
from process import ROOT, gitHash
from ps3000a import DIGITAL_PORT0, INFO_SERIAL, INFO_VARIANT, Scope

THRESHOLD_V = 3.0
SLOT_S = 6.25e-6           # one T period (ROMCLK) at 640 kHz
SLOTS = 20                 # T periods per sample period
CYCLE = 25                 # sample periods per parameter counter cycle
SAMPLE_AT = 0.75           # where in a slot to read: the edges fall at 0.1-0.4
NAMES = ["energy", "pitch"] + [f"k{n}" for n in range(1, 11)]
# Where each field starts, in slots after the start of energy's
FIELD_START = [0, 40] + [79 + 39 * n for n in range(10)]
FIELD_BITS = 10
CHANNELS = {1: "T11 (TP3)", 2: "I/O (TP4)", 3: "SCOPE_TRIG (TP5)"}
MID_SPEECH = 1000          # read the cycle starting at this sample period (125 ms)
SPEECH_S = 0.42
SILENT_AT = 2000           # the silent-frame run reads 250 ms in
LEVEL_THRESHOLDS_V = [-4.5, -3, -1.5, -0.5, 0.5, 1.5, 3, 4.5]


def tables():
    """What the datasheet says each field holds, per index."""
    out = {"energy": model.ENERGY, "pitch": [4 * p for p in model.PITCH]}
    out.update({f"k{n + 1}": t for n, t in enumerate(model.K)})
    return out


def slots(interval, t11, dig):
    """The D0 bit in each slot of each sample period: a list of 20-char strings."""
    t11 = t11 > 0
    d = dig & 1
    per = SLOT_S / interval
    rises = np.flatnonzero(np.diff(t11.astype(np.int8)) == 1) + 1
    return ["".join(str(int(d[int(r + (k + SAMPLE_AT) * per)])) for k in range(SLOTS))
            for r in rises if r + SLOTS * per < len(d)]


def cycleAt(words, start):
    """One 500-slot cycle starting at sample period start, checked against the next."""
    a, b = words[start:start + CYCLE], words[start + CYCLE:start + 2 * CYCLE]
    if len(b) < CYCLE or a != b:
        raise RuntimeError(f"the PROM OUT stream does not repeat at sample period {start}")
    return "".join(a)


def field(cycle, at, signed=True):
    s = cycle * 2
    v = sum(int(c) << k for k, c in enumerate(s[at % len(cycle):][:FIELD_BITS]))
    return v - (1 << FIELD_BITS) if signed and v & (1 << (FIELD_BITS - 1)) else v


def fields(cycle, rotation):
    """The 12 fields, with energy starting at sample period rotation."""
    base = rotation * SLOTS
    return {n: field(cycle, base + FIELD_START[j], signed=(n != "pitch"))
            for j, n in enumerate(NAMES)}


def bestRotation(cycle, score):
    """The rotation with the highest score, and whether it is unique."""
    scores = [score(fields(cycle, r)) for r in range(CYCLE)]
    best = max(scores)
    return scores.index(best), best, scores.count(best) == 1


def inTables(values):
    t = tables()
    return sum(values[n] in t[n] for n in NAMES)


class Bench:
    def __init__(self):
        self.pico = picolib.Pico(timeout=30)
        self.scope = Scope()
        for ch in CHANNELS:
            self.scope.setChannel(ch, 1, 0.0)
        self.scope.setDigitalPort(DIGITAL_PORT0, THRESHOLD_V)

    def close(self):
        self.scope.close()
        self.pico.close()

    def idle(self, seconds=0.02, timebase=15):
        self.scope.setTrigger(1, 0.15, autoMs=100)
        interval, _ = self.scope.timebase(timebase, 1000)
        _, raw, _ = self.scope.runBlock(timebase, int(seconds / interval))
        return slots(interval, raw[1], raw[DIGITAL_PORT0])

    def levels(self, thresholds=LEVEL_THRESHOLDS_V, timebase=15):
        """While idle: the fraction of time D0 (PROM OUT) and D1-D7 (left
        open, for comparison) read high at each threshold, and where D0's
        edges fall within the slot (in tenths of a slot, at +3 V)."""
        out = {"threshold_v": list(thresholds), "d0_high": [], "open_inputs_high": []}
        interval, _ = self.scope.timebase(timebase, 1000)
        for v in thresholds:
            self.scope.setDigitalPort(DIGITAL_PORT0, v)
            self.scope.setTrigger(1, 0.15, autoMs=100)
            _, raw, _ = self.scope.runBlock(timebase, int(0.01 / interval))
            d = raw[DIGITAL_PORT0]
            out["d0_high"].append(round(float(np.mean(d & 1)), 4))
            out["open_inputs_high"].append(round(float(np.mean((d >> 1) & 1)), 4))
        self.scope.setDigitalPort(DIGITAL_PORT0, THRESHOLD_V)
        self.scope.setTrigger(1, 0.15, autoMs=100)
        _, raw, _ = self.scope.runBlock(timebase, int(0.02 / interval))
        t11, d = raw[1] > 0, (raw[DIGITAL_PORT0] & 1).astype(np.int8)
        rises = np.flatnonzero(np.diff(t11.astype(np.int8)) == 1) + 1
        edges = np.flatnonzero(np.diff(d)) + 1
        edges = edges[edges >= rises[0]]
        since = edges - rises[np.searchsorted(rises, edges, side="right") - 1]
        phase = np.mod(since * interval / SLOT_S, 1.0)
        out["edge_phase_tenths"] = np.histogram(phase, bins=10, range=(0, 1))[0].tolist()
        return out

    def speak(self, frames, seconds=SPEECH_S, timebase=64):
        """Speak External the frames (and a stop code), capturing from SCOPE_TRIG."""
        data = encodeFrames(list(frames) + [{"energy": 15}])
        data += bytes(max(0, 9 - len(data)))
        self.pico.reset()
        self.scope.setTrigger(3, 0.15)
        interval, _ = self.scope.timebase(timebase, 1000)
        result = {}

        def armed():
            time.sleep(0.2)
            result.update(self.pico.speakExternal(data))
        _, raw, _ = self.scope.runBlock(timebase, int(seconds / interval), armed=armed)
        return slots(interval, raw[1], raw[DIGITAL_PORT0]), result


def runFrame(i):
    voiced = i > 0
    return {"energy": i % 14 + 1, "repeat": 0, "pitch": i,
            "k": [i % len(t) for t in model.K][:10 if voiced else 4]}


def expectedIndex(frame):
    out = {"energy": frame["energy"], "pitch": frame["pitch"]}
    out.update({f"k{n + 1}": v for n, v in enumerate(frame["k"])})
    return out


def dump(args):
    t = tables()
    bench = Bench()
    runs = []
    try:
        levels = bench.levels()
        for i in range(64):
            frame = runFrame(i)
            words, result = bench.speak([frame] * 25)
            runs.append({"run": i, "frame": frame, "talk_us": result.get("us"),
                         "cycle": cycleAt(words, MID_SPEECH)})
            print(f"run {i}: talked {result.get('us')} us", flush=True)
        # energy index 0: a silent frame after voiced ones; 15: idle after the stop code
        voiced = runFrame(10)
        words, result = bench.speak([voiced] * 5 + [{"energy": 0}] * 20, seconds=0.6)
        runs.append({"run": "silent", "frame": {**voiced, "energy": 0},
                     "talk_us": result.get("us"), "cycle": cycleAt(words, SILENT_AT)})
        runs.append({"run": "idle", "frame": {**voiced, "energy": 15},
                     "cycle": cycleAt(bench.idle(0.03), 0)})
        scopeInfo = {"variant": bench.scope.info(INFO_VARIANT),
                     "serial": bench.scope.info(INFO_SERIAL)}
        firmware = bench.pico.firmware.get("firmware")
    finally:
        bench.close()

    read = {n: {} for n in NAMES}
    for r in runs:
        want = expectedIndex(r["frame"])
        live = [n for n in NAMES if n in want]
        if r["run"] == 0:
            live = live[:6]          # unvoiced: the frame sets only K1-K4
        rot, score, unique = bestRotation(
            r["cycle"], lambda v: sum(v[n] == t[n][want[n]] for n in live))
        r["rotation"], r["unique"] = rot, unique
        r["fields"] = fields(r["cycle"], rot)
        for n in live:
            read[n].setdefault(want[n], set()).add(r["fields"][n])

    out = args.out or os.path.normpath(os.path.join(ROOT, "captures", "promout", f"chip{chipNumber()}"))
    os.makedirs(out, exist_ok=True)
    rows, mismatches = [], 0
    for n in NAMES:
        for index in range(len(t[n])):
            got = sorted(read[n].get(index, ()))
            match = got == [t[n][index]]
            mismatches += bool(got) and not match
            rows.append({"parameter": n, "index": index,
                         "read": " ".join(map(str, got)), "datasheet": t[n][index],
                         "match": "" if not got else "yes" if match else "no"})
    with open(os.path.join(out, "rom.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    with open(os.path.join(BENCH_JSON)) as f:
        bench = json.load(f)
    meta = {"captured": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "host_git": gitHash(),
            "chip": bench["chip"], "scope": scopeInfo, "firmware": firmware,
            "threshold_v": THRESHOLD_V, "levels": levels, "field_start_slots": dict(zip(NAMES, FIELD_START)),
            "entries_read": sum(r["read"] != "" for r in rows), "entries": len(rows),
            "mismatches": mismatches, "runs": runs}
    with open(os.path.join(out, "promout.json"), "w") as f:
        json.dump(meta, f, indent=1)
    for n in NAMES:
        got = [r for r in rows if r["parameter"] == n]
        print(f"{n:6s} {sum(r['match'] != '' for r in got):2d} of {len(got):2d} read, "
              f"{sum(r['match'] == 'no' for r in got)} differ from the datasheet")
    print(f"{meta['entries_read']} of {meta['entries']} entries read, "
          f"{mismatches} differ. Wrote {out}")
    return 1 if mismatches else 0


BENCH_JSON = os.path.join(ROOT, "tools", "bench.json")


def chipNumber():
    with open(BENCH_JSON) as f:
        return json.load(f)["chip"]["number"]


def idle(args):
    bench = Bench()
    try:
        words = bench.idle()
    finally:
        bench.close()
    cycle = cycleAt(words, 0)
    rot, score, unique = bestRotation(cycle, inTables)
    values = fields(cycle, rot)
    print(f"{score} of 12 fields are table values{'' if unique else ' (rotation ambiguous)'}")
    t = tables()
    for n in NAMES:
        index = [i for i, v in enumerate(t[n]) if v == values[n]]
        print(f"  {n:6s} {values[n]:5d}   index {' or '.join(map(str, index)) or '?'}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("idle", help="decode the stream while the chip is idle")
    p = sub.add_parser("dump", help="read every table entry and compare with the datasheet")
    p.add_argument("--out", help="output directory (default captures/promout/chip<N>)")
    args = parser.parse_args()
    return {"idle": idle, "dump": dump}[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
