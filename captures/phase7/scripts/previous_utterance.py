# Phase 7: does the previous utterance change the start of the next one?
# Each take of the target is a standard take (reset, timed Speak); before
# some of them, another address is spoken first (untimed, to completion).
#   python previous_utterance.py OUT 0x3f9d4 X,-,-,Y,-,-,X,-,Y,-,X,Y   (as run for p7-01 to p7-06)
# where X/Y are given by --x/--y and '-' means no extra utterance (the
# previous one is the target's own previous take).
import sys, os, json, argparse
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "tools"))
import pico as P, run, prom as promlib, frameclock as fc, compare as C
from ps3000a import Scope

ap = argparse.ArgumentParser()
ap.add_argument("out"); ap.add_argument("target", type=lambda s: int(s, 0)); ap.add_argument("sequence")
ap.add_argument("--x", type=lambda s: int(s, 0), default=0x3fd5e)
ap.add_argument("--y", type=lambda s: int(s, 0), default=0x3d065)
a = ap.parse_args()
manifest = promlib.loadManifest("acorn_phrom_a"); image = promlib.image(manifest)
bench = json.load(open(run.BENCH))
promDir = os.path.join(a.out, manifest["name"])
wordDir = os.path.join(promDir, promlib.addressDir(a.target))
os.makedirs(wordDir, exist_ok=True)
import shutil; shutil.copyfile(promlib.manifestPath("acorn_phrom_a"), os.path.join(promDir, "prom.json"))
args = argparse.Namespace(reset=False, timeout_ms=10000, timebase=64, zip=False, note="phase7 previous-utterance test",
                          board="measure", load_ohms=996.0, listen_filter="bbc", takes=1, start="standard")
log = []
with P.Pico() as p, Scope() as scope:
    for bank in set(p.banks()) - {0xF}:
        p.unload(bank)
    clock = fc.FrameClock.locate(p, a.target)
    p.reset(); clock.calibrate(p, "vsm", a.target)
    last = None
    for step in a.sequence.split(","):
        before = {"X": a.x, "Y": a.y}.get(step)
        if before is not None:
            p.speak(before)
        takeDir, passed, last = run.captureTake(p, args, manifest, image, None, a.target, "vsm", wordDir,
                                                bench, last, clock, scope=scope)
        meta = json.load(open(os.path.join(takeDir, "capture.json")))
        meta["note"] += f"; spoken just before: {('0x%05x' % before) if before is not None else 'the previous take'}"
        json.dump(meta, open(os.path.join(takeDir, "capture.json"), "w"), indent=2)
        log.append({"take": os.path.basename(takeDir), "before": step, "passed": passed})
        print(log[-1], flush=True)
json.dump(log, open(os.path.join(wordDir, "sequence.json"), "w"), indent=1)
takes = C.takeDirs(wordDir)
ref = C.loadTake(takes[0])
codes = {os.path.basename(t): C.loadTake(t)["code"] for t in takes}
import numpy as np
names = sorted(codes)
print("first differing sample (from speech start) between takes:")
start = ref["summary"]["speech_first_sample"]
for i, n1 in enumerate(names):
    row = []
    for n2 in names:
        x, y = codes[n1], codes[n2]; m = min(len(x), len(y))
        d = np.flatnonzero(x[:m] != y[:m])
        row.append("  same" if not len(d) else f"{d[0] - start:6d}")
    print(f"{n1} ({log[i]['before']}) " + " ".join(row))
