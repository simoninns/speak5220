import os
import sys, time, random
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import numpy as np, pico as P
random.seed(7)
n = 34
F = 3600000
with P.Pico() as p:
    p.traceClear()
    resets = []
    for i in range(n):
        time.sleep(random.uniform(0.02, 0.6))
        r = i % 2 == 1
        if r:
            p.reset()
            time.sleep(random.uniform(0.0, 0.03))
        resets.append(r)
        p.speak(0x3D065)
    raw, meta = p.trace()
open("grid2.bin", "wb").write(raw)
t = P.decodeTrace(raw, P.TRACE_CLOCK_HZ)
print(meta)
cyc = t["cycles"].astype(np.int64)
rd = np.flatnonzero(t["type"] == P.EV_READ)
speaks = [c for c in P.hostCycles(t) if c["kind"] == "write" and c["byte"] == 0x50]
ref = None
for r, s in zip(resets, speaks):
    sc = int(round(s["start"] * P.TRACE_CLOCK_HZ))
    fc = int(cyc[rd[cyc[rd] > sc][0]])
    ref = ref if ref is not None else fc
    ph = ((fc - ref + F // 2) % F) - F // 2
    print(f"{'R' if r else ' '} speak->first {(fc-sc)/144e3:7.3f} ms  grid offset {ph/144:9.2f} us  speak phase-in-frame {((sc-ref)%F)/144e3:7.3f} ms")
