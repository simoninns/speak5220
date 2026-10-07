#!/usr/bin/env python3
"""
    frameclock.py

    The TMS5220's frame clock, as seen from the Pico (plan section 12,
    Phase 5; datasheet section 7). The chip reads frames on a grid of
    16000 OSC cycles (25 ms at 640 kHz) that runs freely between words, and
    a Speak starts on the first grid point at least about 320 us after it.
    The Pico's microsecond timer runs from the same crystal as OSC, so one
    VSM Speak locates the grid on that timer, and a timed Speak
    (Pico.speak(..., atUs)) can then start on any later frame, always at the
    same point in it.

    Frame indices count from the first grid point after the Pico's timer
    started, so they hold for one clock session: until the Pico restarts or
    the clock is stopped or changed.

      python tools/frameclock.py          # locate the grid, calibrate both paths

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import time

import numpy as np

import pico as picolib

FRAME_US = 25000                # 200 samples at 640 kHz
FRAME_OSC_CYCLES = 16000        # 200 samples of 80 OSC cycles
CALIBRATION_ADDRESS = 0x3D065   # Acorn PHROM A "B" (bank F, built in): a short word

# The standard procedure has the chip accept the Speak (or Speak External)
# command, READY going low, this long before the boundary of the frame it
# starts on. The noise generator restarts from the command, counted in
# slots (datasheet section 7), so this must be the same in every take; it
# is the latch time of the bit-identical Phase 5 takes at 640 kHz.
LATCH_BEFORE_FRAME_US = 12256.1
LATCH_TOLERANCE_US = 2.0        # a slot is 6.25 us
CALIBRATION_STEP_US = 0.75
CALIBRATION_REPEATS = 3


def triggerRise(trace):
    """The trace cycle count at which SCOPE_TRIG rose."""
    pins = trace[(trace["type"] == picolib.EV_PINS) | (trace["type"] == picolib.EV_START)]
    rises = np.flatnonzero((pins["trig"][1:] == 1) & (pins["trig"][:-1] == 0)) + 1
    return int(pins["cycles"][rises[0]])


def toUs(trace, trigUs, cycles, clockHz=picolib.TRACE_CLOCK_HZ):
    """A trace cycle count on the Pico's microsecond timer, from the reply's trig_us."""
    return trigUs + (int(cycles) - triggerRise(trace)) * 1e6 / clockHz


def frameReads(trace):
    """The cycle counts of the first read of each frame of a Speak."""
    reads = trace["cycles"][trace["type"] == picolib.EV_READ].astype(np.int64)
    reads = reads[reads >= triggerRise(trace)]
    gaps = np.diff(reads, prepend=reads[0] - 10**9)
    return reads[gaps > picolib.TRACE_CLOCK_HZ // 1000]


def commandLatch(trace, command):
    """The cycle count at which the chip accepted the first write of command
    (an opcode, high nibble only) after SCOPE_TRIG rose: READY going low.
    None if the trace shows no READY low for that write. Later writes are
    not searched: a Speak External's stream bytes can look like the command
    (Phase 7), and the chip did not always drive READY low for the command."""
    rise = triggerRise(trace) / picolib.TRACE_CLOCK_HZ
    for c in picolib.hostCycles(trace):
        if c["kind"] == "write" and c["start"] >= rise and c["byte"] & 0xF0 == command:
            return None if c["ready_low"] is None else c["ready_low"] * picolib.TRACE_CLOCK_HZ
    return None


def frameUs(oscHz):
    return FRAME_OSC_CYCLES * 1e6 / oscHz


class FrameClock:
    def __init__(self, gridUs, bootTime, frameUs=FRAME_US):
        self.gridUs = gridUs        # a frame boundary (first read), in [0, frameUs)
        self.bootTime = bootTime    # host time (s) when the Pico's timer was zero
        self.frameUs = frameUs
        self.offsets = {}           # path: start offset found by calibrate()

    @classmethod
    def locate(cls, pico, address=CALIBRATION_ADDRESS):
        """Speak once (no scope) and find the grid from the trace. Trace
        events are timestamped up to 1.5 us late, so the earliest of the
        frame reads, each taken back to the first frame, is used."""
        pico.traceClear()
        result = pico.speak(address)
        trace = picolib.decodeTrace(pico.trace()[0], picolib.TRACE_CLOCK_HZ)
        period = frameUs(int(pico.info()["osc_hz"]))
        reads = [toUs(trace, result["trig_us"], c) for c in frameReads(trace)]
        first = min(t - k * period for k, t in enumerate(reads))
        bootTime = time.time() - pico.time() * 1e-6
        return cls(first % period, bootTime, period)

    def latchTarget(self):
        """LATCH_BEFORE_FRAME_US, moved by the same time as mid-frame when
        the clock is not 640 kHz."""
        return LATCH_BEFORE_FRAME_US + (self.frameUs - FRAME_US) / 2

    def calibrate(self, pico, path, address=CALIBRATION_ADDRESS, data=None, log=print):
        """Find the start offset that has the chip accept the command
        LATCH_BEFORE_FRAME_US before the frame boundary, every time, for one
        path ("vsm", or "external" with its stream in data). The host's write
        timing jitters by about 1.5 us against the chip's slots, so the
        offset is put in the middle of the range that always hits the
        target slot."""
        target = self.latchTarget()

        def latch(offset):
            frame = self.next(pico, 0.2)
            at = round(self.gridUs + (frame - 0.5) * self.frameUs + offset)
            pico.traceClear()
            if path == "vsm":
                result = pico.speak(address, 10000, at)
            else:
                result = pico.speakExternal(data, 10000, at)
            trace = picolib.decodeTrace(pico.trace()[0], picolib.TRACE_CLOCK_HZ)
            cycles = commandLatch(trace, picolib.CMD_SPEAK if path == "vsm"
                                  else picolib.CMD_SPEAK_EXTERNAL)
            if cycles is None:      # READY low not seen in the trace
                return None
            return self.gridUs + frame * self.frameUs - toUs(trace, result["trig_us"], cycles)

        first = next(v for v in (latch(0.0) for _ in range(5)) if v is not None)
        centre = first - target         # a later start moves the latch closer to the frame
        good = []
        for offset in np.arange(centre - 9.375, centre + 9.375, CALIBRATION_STEP_US):
            values = [latch(offset) for _ in range(CALIBRATION_REPEATS)]
            if all(v is not None and abs(v - target) <= LATCH_TOLERANCE_US for v in values):
                good.append(float(offset))
        if not good:
            raise RuntimeError(f"{path}: no start offset puts the latch on the target slot")
        # The longest run of consecutive good offsets
        runs, run = [], [good[0]]
        for offset in good[1:]:
            if offset - run[-1] < CALIBRATION_STEP_US * 1.5:
                run.append(offset)
            else:
                runs.append(run)
                run = [offset]
        runs.append(run)
        best = max(runs, key=len)
        self.offsets[path] = (best[0] + best[-1]) / 2
        log(f"{path}: start offset {self.offsets[path]:+.2f} us "
            f"(good from {best[0]:+.2f} to {best[-1]:+.2f} us)")
        return self.offsets[path]

    def meta(self, path=None):
        return {"frame_us": self.frameUs, "grid_us": self.gridUs,
                "start_offset_us": self.offsets.get(path),
                "latch_target_us": self.latchTarget() if path in self.offsets else None,
                "pico_boot": time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(self.bootTime)),
                "pico_boot_epoch": round(self.bootTime, 3)}

    def index(self, us):
        """The frame index of a time on the grid (a frame's first read)."""
        return int(round((us - self.gridUs) / self.frameUs))

    def offsetUs(self, us):
        """How far a time is from the nearest grid point."""
        return us - self.gridUs - self.index(us) * self.frameUs

    def startUs(self, frame, path=None):
        """The Speak start time (SCOPE_TRIG) that makes frame `frame` the
        first one read: half a frame before its boundary, plus the path's
        calibrated offset."""
        return self.gridUs + (frame - 0.5) * self.frameUs + self.offsets.get(path, 0.0)

    def next(self, pico, secondsAhead):
        """The first frame index at least secondsAhead from now."""
        return int((pico.time() + secondsAhead * 1e6 - self.gridUs) // self.frameUs) + 1


def main():
    with picolib.Pico() as pico:
        clock = FrameClock.locate(pico)
        now = pico.time()
        print(f"grid at {clock.gridUs:.2f} us (mod {clock.frameUs:.2f}); now frame "
              f"{clock.index(now)}; Pico booted {clock.meta()['pico_boot']}")
        pico.reset()
        clock.calibrate(pico, "vsm")
        clock.calibrate(pico, "external",
                        data=picolib.wordBytes(picolib.acornImage(), CALIBRATION_ADDRESS))


if __name__ == "__main__":
    main()
