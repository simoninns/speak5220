#!/usr/bin/env python3
"""
    replay.py

    Replays the Phase 8 host-interface and VSM command tests (protocol.py,
    p8-19 to p8-25) through the emulator (tms5220emu, the command line of
    the application's emulator, built with its tests), and compares what the
    emulator does with what the chip did (plan section 12, Phase 9).

      python tools/replay.py                    # every test
      python tools/replay.py fifo_start starve  # some of them

    Each step's recorded host cycles (protocol.py, raw/<step>.bin) become
    an emulator scenario: the reset sequence, then every write and read at
    the slot of its recorded strobe. The step's traces carry time only from
    their own start, so the frame clock's position is not known: it is
    fitted, one value per step, by searching every slot position of the
    frame (and four positions of the strobes within a slot) for the best
    agreement. Everything else is predicted. Compared:

      status     the value of every status read
      ready      READY latency of every host cycle (strobe to READY active)
      int        every INT edge (asserted and released), in time
      vsm        every Load Address, Dummy Read and Read and Branch, in time
      frames     the number of bits read, and the time of each frame's first read

    Writes captures/phase9/replay/<test>.json and summary.json.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import multiprocessing
import os
import subprocess
import tempfile

import numpy as np

import pico as picolib
import prom as promlib
import protocol

ROOT = promlib.ROOT
EMULATOR = os.environ.get("TMS5220EMU", os.path.join(ROOT, "..", "build", "app", "tests", "support", "tms5220emu"))
OUT_DIR = os.path.join(ROOT, "captures", "phase9", "replay")
SLOT_US = 6.25
ORIGIN = 8000                   # emulator slot of the step's time 0, before the frame shift
FRAME_SLOTS = 4000
PHASES = (0.125, 0.375, 0.625, 0.875)   # where in a slot a recorded strobe falls
MATCH_US = 60.0                 # an edge or function further than this is unmatched
CAP_US = 1000.0                 # cap on one event's contribution to the fit


def measured(test, step):
    """The recorded step: host cycles, INT edges, VSM functions, frame reads."""
    trace = protocol.loadStep(test, step)
    cycles = [{"kind": c["kind"], "byte": c["byte"], "t": c["start"] * 1e6,
               "ready": None if c["ready_low"] is None else c["ready_low"] * 1e6}
              for c in picolib.hostCycles(trace)]
    on, off = protocol.intEdges(trace)
    vsm = protocol.vsmEvents(trace, [picolib.EV_LOAD_ADDRESS, picolib.EV_DUMMY_READ,
                                     picolib.EV_READ_BRANCH])
    names = {picolib.EV_LOAD_ADDRESS: "LOAD", picolib.EV_DUMMY_READ: "DUMMY",
             picolib.EV_READ_BRANCH: "BRANCH"}
    return {"cycles": cycles, "int_on": list(on * 1e6), "int_off": list(off * 1e6),
            "vsm": [(names[int(e["type"])], float(e["time_s"]) * 1e6) for e in vsm],
            "frames": list(protocol.frameStarts(trace) * 1e6),
            "reads": list(trace[trace["type"] == picolib.EV_READ]["time_s"] * 1e6),
            "end": float(trace["time_s"][-1]) * 1e6}


def script(m, image, shift, frac):
    """The scenario for one alignment. shift moves the step against the
    frame clock in slots (0-3999); frac is where in a slot the strobes fall.
    Returns the text and the emulator slot of the step's time 0."""
    sample, slot = divmod(shift, 20)
    lines = [f"load {image} 3C000", f"phase {sample}", "wait 1000",
             "writes FF FF FF FF FF FF FF FF FF 70"]
    origin = ORIGIN + slot
    for c in m["cycles"]:
        lines.append(f"at {origin + int(c['t'] / SLOT_US + frac)}")
        lines.append(f"write {c['byte']:02X}" if c["kind"] == "write" else "status")
    end = max([c["t"] for c in m["cycles"]] + m["int_on"] + m["int_off"] + [0.0])
    lines.append(f"at {origin + int(end / SLOT_US) + 400}")
    return "\n".join(lines) + "\n", origin


def emulate(text):
    with tempfile.NamedTemporaryFile("w", suffix=".scn", delete=False) as f:
        f.write(text)
    try:
        out = subprocess.run([EMULATOR, "-s", "none", "-e", "-", f.name], capture_output=True,
                             text=True, check=True).stdout
    finally:
        os.unlink(f.name)
    return out


def parse(events, origin, frac):
    """Emulator events on the step's time axis (us; a slot's start)."""
    us = lambda slot: (slot - origin - frac) * SLOT_US
    cycles, on, off, vsm, reads, frames, last_read = [], [], [], [], [], [], None
    for line in events.splitlines():
        f = line.split()
        slot, kind = int(f[0]), f[1]       # a HOST line's slot is its strobe
        if slot < origin - 1:
            continue                       # the reset sequence before the step
        if kind == "HOST":
            cycles.append({"kind": "write" if f[2].startswith("WRITE") else "read",
                           "byte": int(f[3], 16), "t": us(slot), "ready": us(int(f[5]))})
        elif kind == "FLAG" and f[2] == "INT":
            (on if f[3] == "1" else off).append(us(slot))
        elif kind == "VSM":
            if f[2] == "READ":
                reads.append(us(slot))
                if last_read is None or slot - last_read > 160:
                    frames.append(us(slot))
                last_read = slot
            else:
                vsm.append((f[2], us(slot)))
    return {"cycles": cycles, "int_on": on, "int_off": off, "vsm": vsm,
            "frames": frames, "reads": reads}


def pairTimes(a, b):
    """Greedy nearest pairing of two time lists: the differences (b - a)
    of pairs within MATCH_US, and the counts left unmatched in each."""
    a, b = sorted(a), list(sorted(b))
    diffs, unmatched = [], 0
    for t in a:
        if b:
            j = int(np.argmin([abs(x - t) for x in b]))
            if abs(b[j] - t) <= MATCH_US:
                diffs.append(b.pop(j) - t)
                continue
        unmatched += 1
    return diffs, unmatched, len(b)


def cut(m, e):
    """Both records up to the end of the recording, or to the first host
    cycle the chip never released (the Pico gave up on READY; the emulator
    cannot, so it would run on)."""
    stop = next((i for i, c in enumerate(m["cycles"]) if c["ready"] is None), None)
    t = m["end"] if stop is None else m["cycles"][stop]["t"]
    stop = len(m["cycles"]) if stop is None else stop
    # the recording's last event is kept; the emulator may put it a little later
    keep = lambda r, n, end: dict({k: [x for x in r[k] if x <= end] for k in ("int_on", "int_off", "frames", "reads")},
                                  vsm=[v for v in r["vsm"] if v[1] <= end], cycles=r["cycles"][:n])
    timeout = stop < len(m["cycles"])
    if timeout:
        return keep(m, stop, t - 1e-3), keep(e, stop, t - 1e-3), round(t, 1)
    return keep(m, stop, t), keep(e, stop, t + MATCH_US), None


def compare(m, e):
    """Measured against emulated, for one alignment."""
    m, e, stop = cut(m, e)
    out = {"host_timeout_at_us": stop}
    n = min(len(m["cycles"]), len(e["cycles"]))
    reads = [(a["byte"], b["byte"]) for a, b in zip(m["cycles"][:n], e["cycles"][:n])
             if a["kind"] == "read"]
    out["status"] = {"reads": len(reads), "differ": sum(x != y for x, y in reads),
                     "first_differ": next((i for i, (x, y) in enumerate(reads) if x != y), None),
                     "cycles_measured": len(m["cycles"]), "cycles_emulated": len(e["cycles"])}
    lat = [((b["ready"] - b["t"]) - (a["ready"] - a["t"]), a["kind"])
           for a, b in zip(m["cycles"][:n], e["cycles"][:n]) if a["ready"] is not None]
    out["ready"] = {k: summary([d for d, kind in lat if kind == k]) for k in ("write", "read")}
    out["ready"]["no_ready_measured"] = sum(a["ready"] is None for a in m["cycles"])
    for k in ("int_on", "int_off", "frames"):
        d, um, ue = pairTimes(m[k], e[k])
        out[k] = dict(summary(d), measured=len(m[k]), emulated=len(e[k]),
                      unmatched_measured=um, unmatched_emulated=ue)
    vm, ve = [k for k, _ in m["vsm"]], [k for k, _ in e["vsm"]]
    d, um, ue = pairTimes([t for _, t in m["vsm"]], [t for _, t in e["vsm"]])
    out["vsm"] = dict(summary(d), measured=len(vm), emulated=len(ve), same_sequence=vm == ve,
                      unmatched_measured=um, unmatched_emulated=ue)
    out["reads"] = {"measured": len(m["reads"]), "emulated": len(e["reads"])}
    if m["reads"] and len(m["reads"]) == len(e["reads"]):
        # every bit read, in order: the frame read schedule and Read Byte timing
        d = np.subtract(e["reads"], m["reads"])
        out["reads"].update(summary(list(d)), late_or_early_over_3us=int((np.abs(d) > 3).sum()))
    return out


def summary(d):
    if not d:
        return {"n": 0}
    d = np.asarray(d)
    return {"n": int(len(d)), "mean_us": round(float(d.mean()), 2),
            "min_us": round(float(d.min()), 2), "max_us": round(float(d.max()), 2)}


def cost(m, e):
    """How badly one alignment fits: status mismatches dominate, then the
    timing of INT edges, VSM functions and frame reads."""
    m, e, _ = cut(m, e)
    c = 0.0
    n = min(len(m["cycles"]), len(e["cycles"]))
    c += 1e4 * sum(a["byte"] != b["byte"] for a, b in zip(m["cycles"][:n], e["cycles"][:n])
                   if a["kind"] == "read")
    for k in ("int_on", "int_off", "frames"):
        for t in m[k]:
            c += min([abs(x - t) for x in e[k]] + [CAP_US])
    for _, t in m["vsm"]:
        c += min([abs(x - t) for _, x in e["vsm"]] + [CAP_US])
    c += CAP_US * abs(len(m["reads"]) - len(e["reads"])) / 50
    return c


def run(m, image, shift, frac):
    text, origin = script(m, image, shift, frac)
    return parse(emulate(text), origin, frac)


def fit(job):
    """Search the frame position for one step, then compare at the best."""
    test, step, image = job
    m = measured(test, step)
    timed = bool(m["frames"] or m["int_on"] or m["int_off"])
    best = None
    coarse = range(0, FRAME_SLOTS, 20) if timed else [0]
    for shift in coarse:
        c = cost(m, run(m, image, shift, 0.5))
        if best is None or c < best[0]:
            best = (c, shift, 0.5)
    if timed:
        centre = best[1]
        for shift in range(centre - 40, centre + 41):
            for frac in PHASES:
                c = cost(m, run(m, image, shift % FRAME_SLOTS, frac))
                if c < best[0]:
                    best = (c, shift % FRAME_SLOTS, frac)
    else:
        for frac in PHASES:
            c = cost(m, run(m, image, 0, frac))
            if c < best[0]:
                best = (c, 0, frac)
    e = run(m, image, best[1], best[2])
    return test, step, {"shift_slots": best[1], "strobe_frac": best[2], "frame_fitted": timed,
                        "cost": round(best[0], 1), **compare(m, e)}


def detail(test, step, limit=None):
    """Print one step's recorded and emulated timelines, at its fitted
    alignment, with the cycles that differ marked."""
    image = os.path.join(tempfile.gettempdir(), "replay_acorn.bin")
    with open(image, "wb") as f:
        f.write(picolib.acornImage())
    _, _, r = fit((test, step, image))
    m = measured(test, step)
    e = run(m, image, r["shift_slots"], r["strobe_frac"])
    print(f"{test}/{step}: shift {r['shift_slots']} slots, strobe at {r['strobe_frac']} of a slot")
    rows = []
    for i, (a, b) in enumerate(zip(m["cycles"], e["cycles"])):
        busyA = None if a["ready"] is None else a["ready"] - a["t"]
        busyB = b["ready"] - b["t"]
        flag = "  <-- value" if a["byte"] != b["byte"] else (
            "  <-- ready" if busyA is None or abs(busyB - busyA) > SLOT_US * 1.5 else "")
        chipByte = "--" if a["byte"] is None else f"{a['byte']:02x}"
        rows.append((a["t"], f"{a['t']:10.1f} {a['kind']:5s} chip {chipByte} busy "
                     f"{'-' if busyA is None else f'{busyA:7.1f}'}   emu {b['byte']:02x} busy {busyB:7.1f}{flag}"))
    for k, name in (("int_on", "INT on"), ("int_off", "INT off")):
        rows += [(t, f"{t:10.1f} chip {name}") for t in m[k]]
        rows += [(t, f"{t:10.1f}                                emu {name}") for t in e[k]]
    rows += [(t, f"{t:10.1f} chip VSM {k}") for k, t in m["vsm"]]
    rows += [(t, f"{t:10.1f}                                emu VSM {k}") for k, t in e["vsm"]]
    rows += [(t, f"{t:10.1f} chip frame read") for t in m["frames"]]
    rows += [(t, f"{t:10.1f}                                emu frame read") for t in e["frames"]]
    for _, text in sorted(rows)[:limit]:
        print(text)
    os.unlink(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("tests", nargs="*", default=protocol.TESTS)
    parser.add_argument("--out", default=OUT_DIR)
    parser.add_argument("--jobs", type=int, default=14)
    parser.add_argument("--detail", nargs=2, metavar=("TEST", "STEP"),
                        help="print one step's timelines instead")
    parser.add_argument("--limit", type=int, help="with --detail, the number of lines")
    args = parser.parse_args()
    if args.detail:
        detail(*args.detail, limit=args.limit)
        return
    os.makedirs(args.out, exist_ok=True)
    image = os.path.join(args.out, "acorn_phrom_a.bin")
    with open(image, "wb") as f:
        f.write(picolib.acornImage())
    jobs = []
    for test in args.tests:
        with open(os.path.join(protocol.OUT_DIR, test, "steps.json")) as f:
            steps = [s for s in json.load(f)["steps"] if s != "clock"]
        jobs += [(test, s, image) for s in steps]
    with multiprocessing.Pool(args.jobs) as pool:
        results = pool.map(fit, jobs)
    os.unlink(image)
    by_test = {}
    for test, step, r in results:
        by_test.setdefault(test, {})[step] = r
    summary_out = {}
    for test, steps in by_test.items():
        with open(os.path.join(args.out, f"{test}.json"), "w") as f:
            json.dump(steps, f, indent=1)
        s = {"steps": len(steps),
             "status_reads": sum(r["status"]["reads"] for r in steps.values()),
             "status_differ": sum(r["status"]["differ"] for r in steps.values()),
             "int_unmatched": sum(r[k]["unmatched_measured"] + r[k]["unmatched_emulated"]
                                  for r in steps.values() for k in ("int_on", "int_off")),
             "vsm_sequence_differs": sum(not r["vsm"]["same_sequence"] for r in steps.values()),
             "reads_differ": sum(r["reads"]["measured"] != r["reads"]["emulated"] for r in steps.values())}
        summary_out[test] = s
        print(test, json.dumps(s))
    with open(os.path.join(args.out, "summary.json"), "w") as f:
        json.dump(summary_out, f, indent=1)


if __name__ == "__main__":
    main()
