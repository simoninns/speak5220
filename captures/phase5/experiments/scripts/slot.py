# Sweep the timed start across a slot: when does the Speak write land a slot later?
import os
import sys, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import numpy as np, pico as P, frameclock as fc
def speakTiming(p, at):
    p.traceClear()
    r = p.speak(0x3D065, 10000, at)
    tr = P.decodeTrace(p.trace()[0], P.TRACE_CLOCK_HZ)
    cyc = tr["cycles"].astype(np.int64)
    rise = fc.triggerRise(tr)
    w = [c for c in P.hostCycles(tr) if c["kind"] == "write" and c["start"] * 144e6 >= rise]
    first = int(cyc[(tr["type"] == P.EV_READ) & (cyc >= rise)][0])
    return (first - w[5]["start"] * 144e6) / 144, r["trig_us"] + (first - rise) / 144
offsets = np.arange(float(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]))
reps = int(sys.argv[4])
rows = []
with P.Pico() as p:
    clock = fc.FrameClock.locate(p)
    for off in offsets:
        vals = []
        for k in range(reps):
            n = clock.next(p, 0.2)
            v, firstUs = speakTiming(p, round(clock.startUs(n) + off))
            vals.append(round(v, 2))
        rows.append((float(off), vals))
        print(f"offset {off:+6.2f} us: WS->first read {vals}", flush=True)
json.dump(rows, open("slot.json", "w"))
