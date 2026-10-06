#!/usr/bin/env python3
"""
    decode.py

    Decodes a capture directory written by capture.py. For each SCOPE_TRIG
    window (one per Speak) it writes:

      word<N>_samples.wav  I/O samples, 16-bit PCM at 8 kHz (value = 10-bit code)
      word<N>_samples.csv  sample index, code, mean DAC voltage, T11 time

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import os
import wave

import numpy as np

import tms5220_decode as td


def writeWav(path, codes, rate=8000):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(codes.astype("<i2").tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("capture", help="capture directory")
    args = parser.parse_args()

    interval, volts = td.loadRaw(os.path.join(args.capture, "raw", "scope_raw.npz"))
    d = td.decode(interval, volts)
    edges, code, dac = d["edge"], d["code"], d["dac"]

    period = np.diff(edges).mean() * interval
    print(f"{len(edges)} sample periods, T11 period {period * 1e6:.4f} us "
          f"({1 / period:.2f} Hz)")
    errors = td.unusedSlotErrors(d["bits"])
    if errors:
        print(f"Warning: {errors} samples have I/O activity outside slots 2-11")

    preTrigger = int(np.load(os.path.join(args.capture, "raw", "scope_raw.npz"))["preTrigger"])
    for n, (first, end) in enumerate(td.triggerWindows(interval, volts, edges)):
        words = code[first:end]
        active = np.flatnonzero(words != -1)
        span = f"samples {active[0]}-{active[-1]}" if len(active) else "silent"
        print(f"word {n}: {(end - first) * td.SAMPLE_PERIOD * 1e3:.1f} ms, {span}, "
              f"codes {words.min()}..{words.max()}")
        base = os.path.join(args.capture, f"word{n}_samples")
        writeWav(base + ".wav", words)
        t = (edges[first:end] - preTrigger) * interval
        np.savetxt(base + ".csv",
                   np.column_stack([np.arange(end - first), words, dac[first:end], t]),
                   fmt=["%d", "%d", "%.6f", "%.9f"], delimiter=",",
                   header="sample,code,dac_v,t11_time_s", comments="")


if __name__ == "__main__":
    main()
