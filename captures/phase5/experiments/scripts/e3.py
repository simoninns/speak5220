import sys, os, json, time, argparse, shutil
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import numpy as np
import pico as P, run, prom as promlib, compare
FRAME_US = 25000
out = sys.argv[1]
offsets = [int(x) for x in sys.argv[2].split(",")]
wordName = sys.argv[3] if len(sys.argv) > 3 else "ACORN"
args = argparse.Namespace(reset=False, timeout_ms=10000, timebase=64, zip=False, note="phase5 e3",
                          board="measure", load_ohms=996.0, listen_filter="bbc", takes=1)
manifest = promlib.loadManifest("acorn_phrom_a"); image = promlib.image(manifest)
word = promlib.findWord(manifest, wordName); addr = promlib.address(word)
bench = json.load(open(run.BENCH))
wordDir = os.path.join(out, manifest["name"], promlib.wordDir(word))
os.makedirs(wordDir, exist_ok=True)
shutil.copyfile(promlib.manifestPath("acorn_phrom_a"), os.path.join(out, manifest["name"], "prom.json"))

def gridUs(p):
    p.traceClear()
    r = p.speak(0x3D065)
    t = P.decodeTrace(p.trace()[0], P.TRACE_CLOCK_HZ)
    pins = t[(t["type"] == P.EV_PINS)]
    rise = pins["cycles"][np.flatnonzero((pins["trig"][1:] == 1) & (pins["trig"][:-1] == 0))[0] + 1] if pins["trig"][0] == 0 else pins["cycles"][0]
    first = t["cycles"][t["type"] == P.EV_READ][0]
    return r["trig_us"] + (int(first) - int(rise)) / 144.0

with P.Pico() as p:
    g = gridUs(p)
    now = p.time()
    n0 = int((now - g) // FRAME_US) + 400      # 10 s ahead
    print("grid", g, "n0", n0)
    log = []
    for off in offsets:
        n = n0 + off
        at = g + n * FRAME_US - 12500
        takeDir, passed, end = run.captureTake(p, args, manifest, image, word, addr, "vsm", wordDir, bench, None, atUs=int(round(at)))
        m = json.load(open(os.path.join(takeDir, "capture.json")))
        fr = m["processed"]["summary"].get("first_frame_read_us")
        got = (fr - g) / FRAME_US
        log.append({"take": takeDir, "offset": off, "target_frame": n, "frame": got, "late_us": m["pico"]["result"].get("late_us"), "passed": passed})
        print(log[-1])
json.dump({"grid_us": g, "n0": n0, "takes": log}, open(os.path.join(wordDir, "e3.json"), "w"), indent=1)
