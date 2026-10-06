#!/usr/bin/env python3
"""
    capture.py

    Triggered PicoScope 3405D capture of the TMS5220 probe points. Waits for
    SCOPE_TRIG (Ch D) and writes <out>/raw/scope_raw.npz and <out>/capture.json.

    With --speak or --speakext, it also drives the Pico (pico.py): once the
    scope is armed it clears the VSM trace and has the TMS5220 speak, then
    saves the trace as <out>/raw/vsm_trace.bin and <out>/vsm_trace.csv.
    Without them, something else must raise SCOPE_TRIG (for example
    "pico.py speak" from another terminal).

    Expected probes: A x1 on TP2 (DAC), B/C/D x10 on TP3/TP4/TP5. With
    --d-signal romclk, probe D is on TP1 (ROMCLK) instead and the capture
    triggers on T11 (Ch B).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os
import time
import zlib

import numpy as np

import pico as picolib
from process import gitHash
from ps3000a import INFO_SERIAL, INFO_VARIANT, Scope

# (range V, offset V) per channel, at the scope input
CHANNEL_SETUP = {
    0: (1, -0.5),   # A: DAC, 0-1.1 V with the probe at x1
    1: (1, 0.0),    # B: T11, +/-5 V with the probe at x10
    2: (1, 0.0),    # C: I/O
    3: (1, 0.0),    # D: SCOPE_TRIG, 0-3.3 V
}
TRIGGER_V = 0.15    # 1.5 V at the probe tip

# After RunBlock the scope first fills its pre-trigger buffer and only then
# accepts a trigger, so the Pico waits this long (plus the pre-trigger time)
# before raising SCOPE_TRIG
ARM_DELAY_S = 0.2

# What probe D is connected to: (signal label, trigger channel)
D_SIGNALS = {
    "trig": ("SCOPE_TRIG (TP5)", 3),
    "romclk": ("ROMCLK (TP1)", 1),
}

# Board configurations (plan section 5). Every capture records which one it
# was taken with.
BOARD_CONFIGS = {
    "measure": "C1 disconnected: TP2 is loaded by R2 (1K) alone",
    "measure-half": "C1 disconnected: TP2 is loaded by R2 with a second 1K in parallel",
    "audio": "C1 connected: the audio stage (C1, R3 4K7) also loads TP2",
}


def scopeCapture(seconds, preMs, timebase, triggerChannel, armed=None):
    """Arm the scope with the standard channel setup, call armed() (which
    should make the trigger happen), and capture one block. Returns a dict
    with the raw ADC arrays and the settings."""
    with Scope() as scope:
        for channel, (rangeV, offsetV) in CHANNEL_SETUP.items():
            scope.setChannel(channel, rangeV, offsetV)
        scope.setTrigger(triggerChannel, TRIGGER_V)
        interval, _ = scope.timebase(timebase, 1000)
        pre = int(preMs * 1e-3 / interval)
        samples = pre + int(seconds / interval)
        print(f"Capturing {samples} samples per channel at {1e-6 / interval:.3f} MS/s, "
              f"waiting for a trigger on Ch {'ABCD'[triggerChannel]}...")
        interval, raw, overflow = scope.runBlock(timebase, samples, pre, timeoutS=60,
                                                 armed=armed)
        result = {"interval": interval, "pre": pre, "raw": raw, "overflow": overflow,
                  "timebase": timebase, "trigger_channel": triggerChannel,
                  "variant": scope.info(INFO_VARIANT), "serial": scope.info(INFO_SERIAL),
                  "maxAdc": scope.maxAdc}
    if overflow:
        print(f"Warning: over-range on channels mask 0x{overflow:x}")
    return result


def saveScope(out, scope):
    """Write raw/scope_raw.npz (never modified afterwards)."""
    os.makedirs(os.path.join(out, "raw"), exist_ok=True)
    raw = scope["raw"]
    np.savez(os.path.join(out, "raw", "scope_raw.npz"),
             interval=scope["interval"], preTrigger=scope["pre"], maxAdc=scope["maxAdc"],
             range=[CHANNEL_SETUP[ch][0] for ch in range(4)],
             offset=[CHANNEL_SETUP[ch][1] for ch in range(4)],
             **{"abcd"[ch]: raw[ch] for ch in range(4)})


def scopeMeta(scope):
    return {"variant": scope["variant"], "serial": scope["serial"],
            "timebase": scope["timebase"], "interval_s": scope["interval"],
            "pre_trigger_samples": scope["pre"], "samples": int(len(scope["raw"][0])),
            "overflow_mask": scope["overflow"],
            "trigger_channel": "abcd"[scope["trigger_channel"]]}


def channelMeta(dSignal="trig"):
    return {
        "a": {"signal": "SPEAKER (TP2)", "probe": "x1"},
        "b": {"signal": "T11 (TP3)", "probe": "x10"},
        "c": {"signal": "I/O (TP4)", "probe": "x10"},
        "d": {"signal": D_SIGNALS[dSignal][0], "probe": "x10"},
    }


def boardMeta(config, loadOhms):
    return {"config": config, "description": BOARD_CONFIGS[config], "r2_load_ohms": loadOhms}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("out", help="capture directory")
    parser.add_argument("--seconds", type=float, default=1.0, help="time after the trigger")
    parser.add_argument("--pre-ms", type=float, default=10.0, help="time before the trigger")
    parser.add_argument("--timebase", type=int, default=64,
                        help="ps3000a timebase, (n - 2) x 8 ns (64 = 2.016 MS/s, 15 = 9.6 MS/s)")
    parser.add_argument("--d-signal", choices=D_SIGNALS, default="trig",
                        help="what probe D is on (romclk triggers on T11 instead)")
    parser.add_argument("--board", choices=BOARD_CONFIGS, default="measure",
                        help="board configuration (measure = audio stage disconnected)")
    parser.add_argument("--load-ohms", type=float,
                        help="R2 as measured with a meter, recorded in capture.json")
    parser.add_argument("--note", default="", help="free text stored in capture.json")
    speech = parser.add_argument_group("speaking through the Pico")
    speech.add_argument("--speak", type=lambda s: int(s, 0), metavar="ADDRESS",
                        help="speak from this VSM address once the scope is armed")
    speech.add_argument("--speakext", metavar="SOURCE",
                        help="Speak External: a binary file, or an image ('builtin' or a "
                             "file) with --word")
    speech.add_argument("--word", type=lambda s: int(s, 0), metavar="ADDRESS",
                        help="with --speakext, send the word at this VSM address of the image")
    speech.add_argument("--reset", action="store_true",
                        help="send the reset sequence (nine 0xFF, Reset) before arming")
    speech.add_argument("--port", help="the Pico's serial port (default: found by USB ID)")
    args = parser.parse_args()
    if args.speak is not None and args.speakext:
        parser.error("use --speak or --speakext, not both")
    if args.word is not None and not args.speakext:
        parser.error("--word needs --speakext")

    pico, picoMeta, armed = None, None, None
    if args.speak is not None or args.speakext:
        pico = picolib.Pico(args.port)
        picoMeta = {"firmware": pico.firmware.get("firmware"),
                    "osc_hz": int(pico.firmware["osc_hz"]), "banks": pico.firmware.get("banks")}
        if args.reset:
            picoMeta["reset_status"] = pico.reset()
        if args.speak is not None:
            picoMeta["command"] = {"speak": f"0x{args.speak:05x}"}

            def armed():
                time.sleep(ARM_DELAY_S + args.pre_ms * 1e-3)
                pico.traceClear()
                picoMeta["result"] = pico.speak(args.speak)
        else:
            data = picolib.loadImage(args.speakext)
            if args.word is not None:
                data = picolib.wordBytes(data, args.word)
            picoMeta["command"] = {"speakext": args.speakext, "bytes": len(data),
                                   "crc32": f"{zlib.crc32(data):08x}"}
            if args.word is not None:
                picoMeta["command"]["word"] = f"0x{args.word:05x}"

            def armed():
                time.sleep(ARM_DELAY_S + args.pre_ms * 1e-3)
                pico.traceClear()
                picoMeta["result"] = pico.speakExternal(data)

    triggerChannel = D_SIGNALS[args.d_signal][1]
    scope = scopeCapture(args.seconds, args.pre_ms, args.timebase, triggerChannel, armed)
    if pico:
        traceRaw, traceMeta = pico.trace()
        pico.close()
        picolib.writeTrace(args.out, traceRaw, traceMeta)
        picoMeta["trace"] = traceMeta
        print(f"Pico: {picoMeta['result']}, trace {traceMeta['count']} events")
    saveScope(args.out, scope)

    meta = {
        "captured": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "scope": scopeMeta(scope),
        "channels": channelMeta(args.d_signal),
        "board": boardMeta(args.board, args.load_ohms),
        "pico": picoMeta,
        "host_git": gitHash(),
        "note": args.note,
    }
    with open(os.path.join(args.out, "capture.json"), "w") as f:
        json.dump(meta, f, indent=2)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
