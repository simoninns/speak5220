#!/usr/bin/env python3
"""
    capture.py

    Triggered PicoScope 3405D capture of the TMS5220 probe points. Waits for
    SCOPE_TRIG (Ch D) and writes <out>/raw/scope_raw.npz and <out>/capture.json.

    Expected probes: A x1 on TP2 (DAC), B/C/D x10 on TP3/TP4/TP5.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os
import subprocess
import time

import numpy as np

from ps3000a import INFO_SERIAL, INFO_VARIANT, Scope

# (range V, offset V) per channel, at the scope input
CHANNEL_SETUP = {
    0: (1, -0.5),   # A: DAC, 0-1.1 V with the probe at x1
    1: (1, 0.0),    # B: T11, +/-5 V with the probe at x10
    2: (1, 0.0),    # C: I/O
    3: (1, 0.0),    # D: SCOPE_TRIG, 0-3.3 V
}
TRIGGER_V = 0.15    # 1.5 V at the probe tip


def gitHash():
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"],
                                       text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("out", help="capture directory")
    parser.add_argument("--seconds", type=float, default=1.0, help="time after the trigger")
    parser.add_argument("--pre-ms", type=float, default=10.0, help="time before the trigger")
    parser.add_argument("--timebase", type=int, default=64,
                        help="ps3000a timebase, (n - 2) x 8 ns (64 = 2.016 MS/s, 15 = 9.6 MS/s)")
    parser.add_argument("--note", default="", help="free text stored in capture.json")
    args = parser.parse_args()

    with Scope() as scope:
        for channel, (rangeV, offsetV) in CHANNEL_SETUP.items():
            scope.setChannel(channel, rangeV, offsetV)
        scope.setTrigger(3, TRIGGER_V)
        interval, _ = scope.timebase(args.timebase, 1000)
        pre = int(args.pre_ms * 1e-3 / interval)
        samples = pre + int(args.seconds / interval)
        print(f"Capturing {samples} samples per channel at {1e-6 / interval:.3f} MS/s, "
              "waiting for SCOPE_TRIG...")
        interval, raw, overflow = scope.runBlock(args.timebase, samples, pre, timeoutS=60)
        variant, serial = scope.info(INFO_VARIANT), scope.info(INFO_SERIAL)
        maxAdc = scope.maxAdc

    if overflow:
        print(f"Warning: over-range on channels mask 0x{overflow:x}")

    os.makedirs(os.path.join(args.out, "raw"), exist_ok=True)
    np.savez(os.path.join(args.out, "raw", "scope_raw.npz"),
             interval=interval, preTrigger=pre, maxAdc=maxAdc,
             range=[CHANNEL_SETUP[ch][0] for ch in range(4)],
             offset=[CHANNEL_SETUP[ch][1] for ch in range(4)],
             **{"abcd"[ch]: raw[ch] for ch in range(4)})

    meta = {
        "captured": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "scope": {"variant": variant, "serial": serial, "timebase": args.timebase,
                  "interval_s": interval, "pre_trigger_samples": pre,
                  "samples": int(len(raw[0])), "overflow_mask": overflow},
        "channels": {
            "a": {"signal": "SPEAKER (TP2)", "probe": "x1"},
            "b": {"signal": "T11 (TP3)", "probe": "x10"},
            "c": {"signal": "I/O (TP4)", "probe": "x10"},
            "d": {"signal": "SCOPE_TRIG (TP5)", "probe": "x10"},
        },
        "host_git": gitHash(),
        "note": args.note,
    }
    with open(os.path.join(args.out, "capture.json"), "w") as f:
        json.dump(meta, f, indent=2)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
