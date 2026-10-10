# Sweep the Speak time across a frame boundary: which boundary does the first read land on?
import os
import sys, time, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import numpy as np, pico as P
F = 25000.0
def rise(t):
    pins = t[(t["type"] == P.EV_PINS) | (t["type"] == P.EV_START)]
    i = np.flatnonzero((pins["trig"][1:] == 1) & (pins["trig"][:-1] == 0))[0] + 1
    return int(pins["cycles"][i])
def one(p, at=None, addr=0x3D065):
    p.traceClear()
    r = p.speak(addr, 10000, at)
    t = P.decodeTrace(p.trace()[0], P.TRACE_CLOCK_HZ)
    rc = rise(t)
    first = int(t["cycles"][t["type"] == P.EV_READ][0])
    sp = [c for c in P.hostCycles(t) if c["kind"] == "write" and c["byte"] == 0x50][0]
    spc = int(round(sp["start"] * P.TRACE_CLOCK_HZ))
    toUs = lambda c: r["trig_us"] + (c - rc) / 144.0
    return toUs(first), toUs(spc), r
offsets = [float(x) for x in sys.argv[1].split(",")]
rows = []
with P.Pico() as p:
    g, _, _ = one(p)
    for x in offsets:   # Speak command *trigger* this many us before a boundary
        now = p.time()
        n = int((now - g) // F) + 8
        at = g + n * F - x
        first, spk, r = one(p, round(at))
        k = (first - g) / F
        rows.append({"trig_before_boundary_us": x, "speak_write_before_boundary_us": g + n * F - spk,
                     "first_read_boundary": round(k) - n, "first_read_offset_us": (k - round(k)) * F,
                     "late_us": r["late_us"]})
        print(rows[-1])
json.dump(rows, open("edge.json", "w"), indent=1)
