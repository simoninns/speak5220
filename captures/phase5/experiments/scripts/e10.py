# Reset then a timed Speak: is the noise state independent of the frame index?
import sys, os, json, time, argparse, shutil
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import pico as P, run, prom as promlib, frameclock as fc
out = sys.argv[1]; word = sys.argv[2]
# entries: frameOffset:resetFramesBefore:extra   (extra 'B' = untimed Speak of B after the reset)
plan = [e.split(":") for e in sys.argv[3].split(",")]
manifest = promlib.loadManifest("acorn_phrom_a"); image = promlib.image(manifest)
bench = json.load(open(run.BENCH))
w = promlib.findWord(manifest, word); addr = promlib.address(w)
wordDir = os.path.join(out, manifest["name"], promlib.wordDir(w))
os.makedirs(wordDir, exist_ok=True)
shutil.copyfile(promlib.manifestPath("acorn_phrom_a"), os.path.join(out, manifest["name"], "prom.json"))
args = argparse.Namespace(reset=False, timeout_ms=10000, timebase=64, zip=False, note="phase5 e10",
                          board="measure", load_ohms=996.0, listen_filter="bbc", takes=len(plan), start="standard")
log = []
def at(p, us):
    run.waitUntil(p, us, 0.05)
    while p.time() < us:
        pass
with P.Pico() as p:
    clock = fc.FrameClock.locate(p)
    p.reset(); clock.calibrate(p, "vsm")
    n0 = clock.next(p, 25)
    for off, rb, extra in plan:
        n = n0 + int(off)
        entry = {"frame": n, "noise_phase": n % 8191, "reset_frames_before": rb, "extra": extra}
        if rb != "-":
            at(p, clock.gridUs + (n - float(rb)) * clock.frameUs)
            entry["reset_status"] = p.reset()
        if extra == "T":
            entry["extra_result"] = p.speak(0x3D065, 10000, clock.startUs(n - 280))
        if extra == "B":
            time.sleep(0.5 + (n % 7) * 0.0031)
            entry["extra_result"] = p.speak(0x3D065)
        takeDir, passed, end = run.captureTake(p, args, manifest, image, w, addr, "vsm", wordDir, bench, None, clock, n)
        entry.update({"take": os.path.basename(takeDir), "passed": passed})
        print(entry, flush=True); log.append(entry)
        json.dump(log, open(os.path.join(out, "e10.json"), "w"), indent=1)
