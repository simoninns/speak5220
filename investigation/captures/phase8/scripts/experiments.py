# Phase 8 bench experiments that need the standard procedure with a twist.
# Run from the repository root, with the vector sets built (tools/vectors.py).
#
#   python captures/phase8/scripts/experiments.py latch_offset OUT
#       Takes of the loud unvoiced vector u_e14 (energy set) through both
#       paths, with the Speak timed 0, +-1, +-2 and +3 slots (6.25 us) from
#       the standard latch time. If the noise generator steps once per slot
#       from the command, its state at the first sample moves by one step
#       per slot of latch shift.
#
#   python captures/phase8/scripts/experiments.py previous OUT
#       The previous-utterance effect (Phase 7) with vectors: standard takes
#       of a loud vector (loud set), each preceded by a Speak External of a
#       chosen stream (or nothing), optionally followed by an idle wait.
#
# Each take is an ordinary capture directory (run.captureTake); the twist
# is recorded in capture.json's "experiment" field.
import argparse
import json
import os
import sys
import time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))
import frameclock as fc  # noqa: E402
import pico as P  # noqa: E402
import prom as promlib  # noqa: E402
import run  # noqa: E402
from ps3000a import Scope  # noqa: E402

VECTORS = os.path.join(ROOT, "captures", "phase8", "vectors")


def takeArgs(note):
    return argparse.Namespace(reset=False, timeout_ms=10000, timebase=64, zip=True, note=note,
                              board="measure", load_ohms=996.0, listen_filter="bbc", takes=1,
                              start="standard", prom=None)


def loadSet(name):
    path = os.path.join(VECTORS, f"{name}.json")
    manifest = promlib.loadManifest(path)
    return path, manifest, promlib.image(manifest)


def prepare(pico, manifest, image):
    for bank in set(pico.banks()) - {b for b, _ in promlib.bankImages(manifest, image)}:
        pico.unload(bank)
    for bank, data in promlib.bankImages(manifest, image):
        pico.upload(bank, data)


def annotate(takeDir, experiment):
    path = os.path.join(takeDir, "capture.json")
    with open(path) as f:
        meta = json.load(f)
    meta["experiment"] = experiment
    with open(path, "w") as f:
        json.dump(meta, f, indent=2)


def latchOffset(out):
    path, manifest, image = loadSet("energy")
    word = promlib.findWord(manifest, "u_e14")
    address = promlib.address(word)
    bench = json.load(open(run.BENCH))
    args = takeArgs("phase8 latch offset")
    args.prom = path
    wordDir = os.path.join(out, "latch_offset")
    with P.Pico() as pico, Scope() as scope:
        prepare(pico, manifest, image)
        clock = fc.FrameClock.locate(pico, address)
        last = None
        for side in ("vsm", "external"):
            pico.reset()
            clock.calibrate(pico, side, address,
                            P.wordBytes(image, address, promlib.base(manifest)))
            base = clock.offsets[side]
            for slots in (0, 1, -1, 2, -2, 3, 0):
                clock.offsets[side] = base + slots * 6.25
                takeDir, passed, last = run.captureTake(pico, args, manifest, image, word, address,
                                                        side, wordDir, bench, last, clock,
                                                        scope=scope)
                annotate(takeDir, {"name": "latch_offset", "slots": slots,
                                   "start_offset_us": clock.offsets[side]})
                print(takeDir, slots, passed, flush=True)
            clock.offsets[side] = base


def previous(out, sequence, idle):
    path, manifest, image = loadSet("loud")
    target = promlib.findWord(manifest, "loud03")
    address = promlib.address(target)
    streams = {name: P.wordBytes(image, promlib.address(promlib.findWord(manifest, name)),
                                 promlib.base(manifest))
               for name in ("loud00", "loud10", "loud20")}
    bench = json.load(open(run.BENCH))
    args = takeArgs("phase8 previous utterance")
    args.prom = path
    wordDir = os.path.join(out, "previous_utterance")
    with P.Pico() as pico, Scope() as scope:
        prepare(pico, manifest, image)
        clock = fc.FrameClock.locate(pico, address)
        pico.reset()
        clock.calibrate(pico, "vsm", address)
        last = None
        for step in sequence.split(","):
            before, wait = step, 0.0
            if ":" in step:
                before, wait = step.split(":")
                wait = float(wait)
            if before != "-":
                pico.speakExternal(streams[before])
            if wait:
                time.sleep(wait)
            takeDir, passed, last = run.captureTake(pico, args, manifest, image, target, address,
                                                    "vsm", wordDir, bench, last, clock,
                                                    scope=scope)
            annotate(takeDir, {"name": "previous_utterance", "before": before,
                               "idle_after_before_s": wait})
            print(takeDir, step, passed, flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("experiment", choices=("latch_offset", "previous"))
    parser.add_argument("out")
    parser.add_argument("--sequence", default="-,-,loud00,-,loud10,-,loud00,loud00,loud20,-,"
                                              "loud00:1,loud10:1,loud00:5,loud10:5")
    a = parser.parse_args()
    if a.experiment == "latch_offset":
        latchOffset(a.out)
    else:
        previous(a.out, a.sequence, None)


if __name__ == "__main__":
    main()
