#!/usr/bin/env python3
"""
    protocol.py

    Host interface and VSM command tests (plan sections 9.1, 9.2 and 9.3,
    Phase 8). Each test drives the TMS5220 with raw commands and data
    bytes through the Pico (CMD, READ, WAIT) and records the VSM trace,
    which holds every host cycle (with READY and INT) and every VSM
    function, timed on the chip's crystal. No scope is needed.

      python tools/protocol.py all                 # every test
      python tools/protocol.py fifo_start starve   # some of them

    Each test writes captures/phase8/protocol/<test>/: raw/vsm_trace.bin
    (one per step, raw/<step>.bin, never modified), vsm_trace CSVs, and
    result.json with the measurements. analyse() rebuilds result.json from
    the raw traces, so the analysis can be rerun.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os
import random
import time

import numpy as np

import frameclock
import lpc
import pico as picolib
import prom as promlib

OUT_DIR = os.path.join(promlib.ROOT, "captures", "phase8", "protocol")
ACORN = 0x3CBB6         # Acorn PHROM A "ACORN": 27 frames
ACORN_B = 0x3D065       # "B": 16 frames

# A long Speak External stream: 40 identical voiced frames (50 bits each)
# and a stop code, so speech consumes 6.25 bytes per 25 ms frame
LONG_FRAME = {"energy": 10, "repeat": 0, "pitch": 32, "k": [23, 9, 8, 6, 7, 6, 6, 3, 3, 3]}
LONG_STREAM = lpc.encodeFrames([LONG_FRAME] * 40 + [{"energy": 15}])


def laNibbles(address, count=5):
    return [picolib.CMD_LOAD_ADDRESS | ((address >> (4 * i)) & 0xF) for i in range(count)]


def safeReset(pico, tries=5):
    """The reset sequence, retried: a command left pending by a test can
    hold READY for a while. Returns the errors seen."""
    errors = []
    for _ in range(tries):
        try:
            pico.reset()
            return errors
        except picolib.PicoError as error:
            errors.append(str(error))
            time.sleep(0.5)
    raise picolib.PicoError(f"reset failed {tries} times: {errors}")


def finishExternal(pico, timeout=1.5):
    """Bring a Speak External to its end before the next step. The reset
    sequence cannot do it: during Speak External its bytes are taken as FIFO
    data (Phase 8). Wait for Talk Status to clear; if it does not, or the
    stream never started, write stop codes (0xFF) until it does."""
    start = time.time()
    while time.time() - start < timeout:
        status = pico.read(1)[0]
        if not status & picolib.STATUS_TS and status & picolib.STATUS_BE:
            return
        if not status & picolib.STATUS_TS and time.time() - start > 0.1:
            pico.cmd(*[0xFF] * 9)       # start it, and stop it at the next frame
        time.sleep(0.03)
    raise picolib.PicoError("Speak External did not finish")


class Recorder:
    """Runs the steps of one test and keeps each step's raw trace."""

    def __init__(self, pico, name):
        self.pico, self.name = pico, name
        self.dir = os.path.join(OUT_DIR, name)
        os.makedirs(os.path.join(self.dir, "raw"), exist_ok=True)
        self.steps = {}

    def begin(self):
        self.pico.traceClear()

    def end(self, step, notes=None):
        raw, meta = self.pico.trace()
        with open(os.path.join(self.dir, "raw", f"{step}.bin"), "wb") as f:
            f.write(raw)
        self.steps[step] = dict(meta, notes=notes or {})
        return picolib.decodeTrace(raw, meta["clock_hz"])

    def save(self):
        with open(os.path.join(self.dir, "steps.json"), "w") as f:
            json.dump({"test": self.name, "time": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                       "firmware": self.pico.firmware.get("firmware"), "steps": self.steps},
                      f, indent=1)


# ---- trace helpers ----------------------------------------------------

def loadStep(test, step):
    with open(os.path.join(OUT_DIR, test, "raw", f"{step}.bin"), "rb") as f:
        return picolib.decodeTrace(f.read(), picolib.TRACE_CLOCK_HZ)


def intEdges(trace):
    """Times (s) at which INT was asserted (went low) and released."""
    pins = trace[(trace["type"] == picolib.EV_PINS) | (trace["type"] == picolib.EV_START)]
    v = pins["int"].astype(int)
    on = np.flatnonzero((v[1:] == 0) & (v[:-1] == 1)) + 1
    off = np.flatnonzero((v[1:] == 1) & (v[:-1] == 0)) + 1
    return pins["time_s"][on], pins["time_s"][off]


def vsmEvents(trace, kinds):
    return trace[np.isin(trace["type"], kinds)]


def frameStarts(trace):
    """First read of each frame burst (VSM path), in seconds."""
    reads = trace[trace["type"] == picolib.EV_READ]["time_s"]
    if not len(reads):
        return np.array([])
    gaps = np.diff(reads, prepend=reads[0] - 1)
    return reads[gaps > 1e-3]


def cycleList(trace):
    """Host cycles with their READY timing: busy_us is from the strobe to
    READY returning (active)."""
    out = []
    for c in picolib.hostCycles(trace):
        busy = (c["ready_low"] - c["start"]) * 1e6 if c["ready_low"] is not None else None
        out.append({"kind": c["kind"], "byte": c["byte"], "t": c["start"], "busy_us": busy})
    return out


def statusReads(trace):
    return [(c["t"], c["byte"]) for c in cycleList(trace) if c["kind"] == "read"]


# ---- tests --------------------------------------------------------------

def testFifoStart(pico):
    """Speak External, one byte at a time, with a status read after each:
    when Buffer Low clears, when Talk Status sets, and when speech starts
    (the first frame's bits leave the FIFO, which sets Buffer Low again and
    raises INT). Then the FIFO is left to run dry: Buffer Empty, Talk
    Status and INT at the end."""
    rec = Recorder(pico, "fifo_start")
    for count in (8, 9, 12, 16):
        pico.reset()
        rec.begin()
        statuses = []
        pico.cmd(picolib.CMD_SPEAK_EXTERNAL)
        statuses.append(pico.read(1)[0])
        for i in range(count):
            pico.cmd(LONG_STREAM[i])
            statuses.append(pico.read(1)[0])
        # Poll until Talk Status clears (or 0.5 s)
        t0 = time.time()
        while time.time() - t0 < 0.5:
            s = pico.read(1)[0]
            if not s & picolib.STATUS_TS and time.time() - t0 > 0.15:
                break
        rec.end(f"bytes{count:02d}", {"status_after_each_byte": statuses})
        finishExternal(pico)
    pico.reset()
    rec.save()


def testStarve(pico):
    """Speak External with N bytes written at once and no more: which frame
    runs the FIFO dry, and what the chip does then (plan 9.1: abnormal
    termination). The INT edges time each status change on the chip's
    clock."""
    rec = Recorder(pico, "starve")
    for count in (9, 10, 13, 14, 15, 16):
        pico.reset()
        rec.begin()
        pico.cmd(picolib.CMD_SPEAK_EXTERNAL, *LONG_STREAM[:count])
        t0 = time.time()
        while time.time() - t0 < 0.4:
            pico.read(1)
        rec.end(f"bytes{count:02d}")
        finishExternal(pico)
    pico.reset()
    rec.save()


def testExternalStart(pico, clock, trials=60):
    """When Speak External speech starts against the frame clock: the 9th
    byte is written at random times; the first frame's parse empties the
    FIFO below the half-full mark, so its INT times the start."""
    rec = Recorder(pico, "external_start")
    rng = random.Random(9)
    for n in range(trials):
        pico.reset()
        rec.begin()
        pico.cmd(picolib.CMD_SPEAK_EXTERNAL, *LONG_STREAM[:8])
        time.sleep(rng.uniform(0, 0.03))
        pico.cmd(LONG_STREAM[8])
        time.sleep(0.06)
        pico.read(1)
        rec.end(f"trial{n:02d}")
        finishExternal(pico)
    pico.reset()
    rec.steps["clock"] = clock.meta()
    rec.save()


def testCommandsDuringSpeech(pico):
    """Commands written while a VSM Speak is talking: Load Address, Read
    Byte, Read and Branch, Speak, Speak External and Reset. The trace shows
    whether, and when, each reaches the VSM."""
    rec = Recorder(pico, "commands_during_speech")
    other = ACORN_B
    cases = {
        "load_address": laNibbles(other),
        "read_byte": [picolib.CMD_READ_BYTE],
        "read_branch": [0x30],
        "speak": [picolib.CMD_SPEAK],
        "reset": [picolib.CMD_RESET],
        "nop": [0x00],
    }
    for name, commands in cases.items():
        resetNotes = safeReset(pico)
        rec.begin()
        pico.cmd(*laNibbles(ACORN), picolib.CMD_SPEAK)
        time.sleep(0.2)
        notes = {"commands": [f"0x{c:02x}" for c in commands]}
        try:
            pico.cmd(*commands)
            if name == "read_byte":
                notes["read"] = pico.read(1)
            result = pico.wait(3000)
            notes["wait"] = result
        except picolib.PicoError as error:
            notes["error"] = str(error)
        time.sleep(0.05)
        try:
            notes["status_after"] = pico.read(1)
        except picolib.PicoError as error:
            notes["status_error"] = str(error)
        notes["reset_before"] = resetNotes
        rec.end(name, notes)
        rec.save()
    safeReset(pico)
    rec.save()


def testResetInExternal(pico):
    """Bytes that decode as Reset (0x70, 0xFF), written during Speak
    External: are they taken as data (plan 9.1)?"""
    rec = Recorder(pico, "reset_in_external")
    for value in (0x70, 0xFF):
        pico.reset()
        rec.begin()
        pico.cmd(picolib.CMD_SPEAK_EXTERNAL, *LONG_STREAM[:16])
        time.sleep(0.05)
        notes = {}
        try:
            pico.cmd(value)
        except picolib.PicoError as error:
            notes["error"] = str(error)
        t0 = time.time()
        while time.time() - t0 < 0.4:
            pico.read(1)
        rec.end(f"byte_{value:02x}", notes)
        finishExternal(pico)
    pico.reset()
    rec.save()


def testVsmCommands(pico, image):
    """VSM commands with Talk Status clear: Load Address with 1-6
    nibbles; Read Byte repeated (the dummy read); Read and Branch; and a
    Speak after Read and Branch, as the BBC Micro speaks a word."""
    rec = Recorder(pico, "vsm_commands")
    for count in range(1, 7):
        pico.reset()
        rec.begin()
        # A known address first, then only `count` nibbles of another (the
        # sixth nibble, 5, is beyond the 20-bit address)
        pico.cmd(*laNibbles(ACORN))
        pico.cmd(*laNibbles(0x53D065, count), picolib.CMD_READ_BYTE)
        values = pico.read(1)
        rec.end(f"la_{count}_nibbles", {"read": values})
    pico.reset()
    rec.begin()
    pico.cmd(*laNibbles(ACORN), picolib.CMD_READ_BYTE)
    values = pico.read(1)
    for _ in range(3):
        pico.cmd(picolib.CMD_READ_BYTE)
        values += pico.read(1)
    rec.end("read_byte_repeat", {"read": values,
                                 "image": [image[(ACORN & 0x3FFF) + i] for i in range(4)]})
    # Read and Branch at the pointer for word 160 (ACORN, pointer at 2 x 160)
    pointer = 0x3C000 + 2 * 160
    pico.reset()
    rec.begin()
    pico.cmd(*laNibbles(pointer), 0x30, picolib.CMD_READ_BYTE)
    values = pico.read(1)
    rec.end("read_branch", {"pointer": f"0x{pointer:05x}", "read": values})
    pico.reset()
    rec.begin()
    pico.cmd(*laNibbles(pointer), 0x30, picolib.CMD_SPEAK)
    result = pico.wait(3000)
    rec.end("read_branch_speak", {"wait": result})
    pico.reset()
    rec.save()


# ---- analysis -----------------------------------------------------------

def stepSummary(trace):
    """The host cycles (with READY busy time), INT edges and VSM functions
    of one step, times in us from its first event."""
    t0 = trace["time_s"][0]
    us = lambda t: round((t - t0) * 1e6, 2)
    on, off = intEdges(trace)
    vsm = vsmEvents(trace, [picolib.EV_LOAD_ADDRESS, picolib.EV_DUMMY_READ, picolib.EV_READ_BRANCH])
    return {
        "cycles": [{"kind": c["kind"], "byte": f"0x{c['byte']:02x}" if c["byte"] is not None else None,
                    "t_us": us(c["t"]), "busy_us": None if c["busy_us"] is None else round(c["busy_us"], 2)}
                   for c in cycleList(trace)],
        "int_on_us": [us(t) for t in on], "int_off_us": [us(t) for t in off],
        "vsm": [{"type": picolib.EVENT_NAMES[int(e["type"])], "address": f"0x{int(e['address']):05x}",
                 "t_us": us(e["time_s"])} for e in vsm],
        "frame_reads_us": [us(t) for t in frameStarts(trace)],
        "reads": int((trace["type"] == picolib.EV_READ).sum()),
    }


def analyse(name):
    """Rebuild result.json for one test from its raw traces."""
    directory = os.path.join(OUT_DIR, name)
    with open(os.path.join(directory, "steps.json")) as f:
        steps = json.load(f)["steps"]
    result = {}
    for step, meta in steps.items():
        if step == "clock":
            continue
        trace = loadStep(name, step)
        result[step] = dict(stepSummary(trace), notes=meta.get("notes", {}))
    with open(os.path.join(directory, "result.json"), "w") as f:
        json.dump(result, f, indent=1)
    return result


def testHeldCommand(pico):
    """Two Load Address writes during a VSM Speak, then the trace is kept
    running to the end of speech: when the chip takes the first (held)
    command, and what READY does for the second."""
    rec = Recorder(pico, "held_command")
    safeReset(pico)
    rec.begin()
    pico.cmd(*laNibbles(ACORN), picolib.CMD_SPEAK)
    time.sleep(0.2)
    notes = {}
    for i, value in enumerate((0x45, 0x46)):
        try:
            pico.cmd(value)
        except picolib.PicoError as error:
            notes[f"write{i}"] = str(error)
    time.sleep(0.8)
    rec.end("two_load_address", notes)
    safeReset(pico)
    rec.save()


TESTS = ["fifo_start", "starve", "external_start", "commands_during_speech",
         "reset_in_external", "vsm_commands", "held_command"]


def run(names):
    image = picolib.acornImage()
    with picolib.Pico() as pico:
        if pico.banks().get(picolib.ACORN_BANK) != __import__("zlib").crc32(image):
            pico.upload(picolib.ACORN_BANK, image)
        pico.reset()
        clock = frameclock.FrameClock.locate(pico, ACORN_B)
        for name in names:
            print(f"running {name}", flush=True)
            if name == "fifo_start":
                testFifoStart(pico)
            elif name == "starve":
                testStarve(pico)
            elif name == "external_start":
                testExternalStart(pico, clock)
            elif name == "commands_during_speech":
                testCommandsDuringSpeech(pico)
            elif name == "reset_in_external":
                testResetInExternal(pico)
            elif name == "vsm_commands":
                testVsmCommands(pico, image)
            elif name == "held_command":
                testHeldCommand(pico)
            analyse(name)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("tests", nargs="+", help=f"{', '.join(TESTS)} or 'all'")
    parser.add_argument("--analyse", action="store_true", help="only rebuild result.json")
    args = parser.parse_args()
    names = TESTS if args.tests == ["all"] else args.tests
    if args.analyse:
        for name in names:
            analyse(name)
    else:
        run(names)


if __name__ == "__main__":
    main()
