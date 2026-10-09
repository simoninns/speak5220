#!/usr/bin/env python3
"""
    model.py

    The candidate model of the TMS5220's synthesis path that the reference
    datasheet's sections 7 and 8.5-8.9 describe, measured against the
    captures (plan section 12, Phase 8). It is not the reference emulator
    (Phase 9), which is written from the datasheet alone; it is the tool the
    datasheet's synthesis sections were measured with. Its starting
    hypotheses came from US patents 4,209,844 and 4,335,277 and from MAME's
    tms5220.cpp (plan section 10). The defaults are the behaviour the
    captures support; the options select the alternatives that were tested
    and rejected, so that each claim can be checked again.

    The model runs sample by sample on the chip's counters: IP
    (interpolation period, 0-7) and PC (parameter count, 0-12), each PC two
    samples (A and B) except PC 12, one. A frame is read in PC 12 of IP 7,
    becomes the target 25 samples later (PC 12 of IP 0), and each parameter
    is computed in its PC's A sample and reaches the filter a fixed number
    of samples later (param_delay). The interpolator's registers (cur) and
    what the filter sees (view) are kept apart: a Speak hides the registers
    from the filter, and its zero targets clear them in IP 0. What carries
    from one utterance to the next is the coded indices (newIdx).

      python tools/model.py <take dir>...     # model each standard take and compare

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os

import numpy as np

ENERGY = [0, 1, 2, 3, 4, 6, 8, 11, 16, 23, 33, 47, 63, 85, 114, 0]
PITCH = [0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
         30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 44, 46, 48,
         50, 52, 53, 56, 58, 60, 62, 65, 68, 70, 72, 76, 78, 80, 84, 86,
         91, 94, 98, 101, 105, 109, 114, 118, 122, 127, 132, 137, 142, 148, 153, 159]
K = [
    [-501, -498, -497, -495, -493, -491, -488, -482, -478, -474, -469, -464, -459, -452, -445, -437,
     -412, -380, -339, -288, -227, -158, -81, -1, 80, 157, 226, 287, 337, 379, 411, 436],
    [-328, -303, -274, -244, -211, -175, -138, -99, -59, -18, 24, 64, 105, 143, 180, 215,
     248, 278, 306, 331, 354, 374, 392, 408, 422, 435, 445, 455, 463, 470, 476, 506],
    [-441, -387, -333, -279, -225, -171, -117, -63, -9, 45, 98, 152, 206, 260, 314, 368],
    [-328, -273, -217, -161, -106, -50, 5, 61, 116, 172, 228, 283, 339, 394, 450, 506],
    [-328, -282, -235, -189, -142, -96, -50, -3, 43, 90, 136, 182, 229, 275, 322, 368],
    [-256, -212, -168, -123, -79, -35, 10, 54, 98, 143, 187, 232, 276, 320, 365, 409],
    [-308, -260, -212, -164, -117, -69, -21, 27, 75, 122, 170, 218, 266, 314, 361, 409],
    [-256, -161, -66, 29, 124, 219, 314, 409],
    [-256, -176, -96, -15, 65, 146, 226, 307],
    [-205, -132, -59, 14, 87, 160, 234, 307],
]
CHIRP = [0x00, 0x03, 0x0f, 0x28, 0x4c, 0x6c, 0x71, 0x50, 0x25, 0x26, 0x4c, 0x44, 0x1a, 0x32, 0x3b, 0x13,
         0x37, 0x1a, 0x25, 0x1f, 0x1d] + [0] * 31
INTERP_SHIFT = [0, 3, 3, 3, 2, 2, 1, 1]     # by IP
SAMPLES_PER_IP = 25
FRAME_SAMPLES = 200


def wrap(value, bits):
    half = 1 << (bits - 1)
    return ((value + half) & ((1 << bits) - 1)) - half


class Model:
    """The chip's synthesis state. It persists from one utterance to the
    next, as on the chip, unless reset() is called."""

    def __init__(self, tables=None, **options):
        t = tables or {}
        self.energy = list(t.get("energy", ENERGY))
        self.pitch = list(t.get("pitch", PITCH))
        self.k = [list(row) for row in t.get("k", K)]
        self.chirp = list(t.get("chirp", CHIRP))
        self.interp = list(t.get("interp", INTERP_SHIFT))
        self.options = {"lfsr_steps": 20, "output_bits": 10, "out": "floor",
                        "round_e": "booth1", "round_u": "booth1", "round_x": "booth1",
                        "neg_u": False, "neg_x": False, "pitch_period_extra": 1,
                        "silence_zpar": False, "interp_round": "floor",
                        "carry_u": 0, "carry_x": 0, "b1_mask_mul": 0, "b1_mask_add": 0,
                        "energy_frac_bits": 0, "speak_zpar": True, "speak_idle_idx": False,
                        "param_delay": [2, 3] + [1] * 10, "param_delay_x": None,
                        "frac_bits": [0, 0], "rng_at_speak": None,
                        "stop_is_silence": True, "idle_lattice": True,
                        "idle_zero_all": "pc", "idle_zero_shift": -1,
                        "uv_zero_inhibited": True, "excitation_delay": 1,
                        "lfsr_unvoiced_only": False, "excitation_pipeline": False,
                        "pitch_zero": True, "pitch_hold": 3, "zpar_until_sound": False,
                        "energy_zero_delay": None, "idle_keep_registers": True,
                        "zpar_view_only": True, "uv_zero_view_only": False,
                        "zpar_parses": 2, "repeat_keeps_uv": True,
                        "datapath_bits": 14}
        self.options.update(options)
        self.reset()

    def reset(self):
        self.ip, self.pc, self.sub = 7, 12, 1    # the sample before a frame's IP 0
        self.cur = [0] * 12             # energy, pitch, K1-K10, as used
        # The values the filter and pitch counter see; 14-23 are K1-K10 as
        # the b (x) chain's products see them
        self.view = [0] * 24
        self.pending = []
        self.oldpHistory = []
        self.nextExcitation = 0
        self.parses = 0
        self.lastUv = False
        self.t = 0
        self.newIdx = [0, 0] + [0] * 4 + [15] * 3 + [7] * 3
        self.zpar = self.uvZpar = False
        self.olde = self.oldp = True
        self.inhibit = True
        self.pitchCount = 0
        self.pitchZero = False
        self.rng = 0x1FFF
        self.u = [0] * 11
        self.x = [0] * 10
        self.talk = self.talkd = self.spen = False

    # ---- frame parameters -------------------------------------------------

    def target(self, i):
        idx = self.newIdx[i]
        if i == 0:
            value = self.energy[idx]
        elif i == 1:
            value = self.pitch[idx]
        else:
            value = self.k[i - 2][idx]
        zero = self.zpar if i < 6 else self.uvZpar
        return 0 if zero else value << self.fracBits(i)

    def fracBits(self, i):
        """Extra low-order bits the interpolator keeps for energy and pitch."""
        return self.options["frac_bits"][i] if i < 2 else 0

    def newSilence(self):
        if self.options["stop_is_silence"]:
            return self.newIdx[0] in (0, 15)
        return self.newIdx[0] == 0

    def newUnvoiced(self):
        return self.newIdx[1] == 0

    def newStop(self):
        return self.newIdx[0] == 15

    def startSpeak(self):
        """Speak or Speak External: the chip starts at the next frame boundary."""
        self.spen = True
        self.parses = 0
        if self.options["rng_at_speak"] is not None:
            self.rng = self.options["rng_at_speak"]
        if self.options["speak_zpar"]:
            self.zpar = self.uvZpar = True
        self.olde = self.oldp = True
        if self.options["speak_idle_idx"]:
            self.newIdx = [0, 0] + [0] * 4 + [15] * 3 + [7] * 3

    def parse(self, frame):
        energy = frame["energy"]
        self.parses += 1
        keep = (energy in (0, 15) and self.options["zpar_until_sound"]) or \
            (energy == 0 and self.parses < self.options["zpar_parses"])
        if not keep:
            self.zpar = self.uvZpar = False
        self.newIdx[0] = energy
        if energy in (0, 15):
            if energy == 0 and self.options["silence_zpar"]:
                self.zpar = self.uvZpar = True
            return
        self.newIdx[1] = frame["pitch"]
        if frame.get("repeat") and self.options["repeat_keeps_uv"]:
            # A voiced repeat frame keeps the K5-K10 zeroing of the frame before
            self.uvZpar = frame["pitch"] == 0 or self.lastUv
            return
        self.uvZpar = frame["pitch"] == 0
        self.lastUv = self.uvZpar
        if frame.get("repeat"):
            return
        for i, value in enumerate(frame["k"]):
            self.newIdx[2 + i] = value

    # ---- one sample -------------------------------------------------------

    @staticmethod
    def boothDigits(k):
        """K (10-bit two's complement) recoded into five radix-4 Booth
        digits, -2..+2, least significant first."""
        k &= 0x3FF
        digits, previous = [], 0
        for i in range(0, 10, 2):
            low, high = (k >> i) & 1, (k >> (i + 1)) & 1
            digits.append(-2 * high + low + previous)
            previous = high
        return digits

    @classmethod
    def boothArray(cls, k, mr, onesStages=1, stage0Truncated=False):
        """The patent's array multiplier: five Booth stages, the partial sum
        shifted right two places after each; a negative digit adds the
        inverted multiplicand, and the stages in onesStages (a bit mask)
        drop the +1 that completes its negation. The last shift is one
        place, and the product's LSB is forced to 1."""
        def partial(digit, stage):
            if digit >= 0:
                return digit * mr
            value = -digit * mr
            return ~value if (onesStages >> stage) & 1 else -value
        digits = cls.boothDigits(k)
        if stage0Truncated:     # stage 0 sees only MR2-MR13
            d = digits[0]
            acc = d * (mr >> 2) if d >= 0 else (~(-d * (mr >> 2)) if onesStages & 1 else d * (mr >> 2))
        else:
            acc = partial(digits[0], 0) >> 2
        acc += partial(digits[1], 1)
        for stage in (2, 3, 4):
            acc = (acc >> 2) + partial(digits[stage], stage)
        return (acc >> 1) | 1

    ROUNDING = {
        "floor": lambda a, b: (a * b) >> 9,
        "floor_or1": lambda a, b: ((a * b) >> 9) | 1,
        "trunc": lambda a, b: int((a * b) / 512),
        "trunc_or1": lambda a, b: int((a * b) / 512) | 1,
        "round": lambda a, b: (a * b + 256) >> 9,
        "round_or1": lambda a, b: ((a * b + 256) >> 9) | 1,
    }

    def multiplier(self, name):
        if name.startswith("booth"):
            truncated = name.startswith("boothm")
            mask = int(name[6 if truncated else 5:] or 1)
            return lambda a, b: self.boothArray(a, b, mask, truncated)
        return self.ROUNDING[name]

    def lattice(self, excitation):
        """One sample of the lattice filter. Each product is a K (or the
        energy) times a 14-bit value, scaled by 2^-9 with the rounding
        given for its role; neg_u and neg_x negate K before the product
        instead of the product after it, which changes the rounding bias."""
        o = self.options
        k, u, x = self.view[2:], self.u, self.x
        rE, rU, rX = (self.multiplier(o[r]) for r in ("round_e", "round_u", "round_x"))
        cu, cx = o["carry_u"], o["carry_x"]
        fe = o["energy_frac_bits"]
        # The adder and delay stack are 14 bits wide (US 4,335,277: D0-D13),
        # and every sum wraps there; None leaves the sums unbounded
        bits = o["datapath_bits"]
        fit = (lambda v: wrap(v, bits)) if bits else (lambda v: v)
        u[10] = fit(rE(self.view[0], excitation << 6))
        # b1 is the previous output, from the Y latch; b1m is it as the
        # multiplier sees it, b1a as the adder does
        b1m = x[0] & ~o["b1_mask_mul"]
        b1a = x[0] & ~o["b1_mask_add"]
        for i in range(9, -1, -1):
            xi = b1m if i == 0 else x[i]
            u[i] = fit((u[i + 1] + rU(-k[i], xi) if o["neg_u"] else u[i + 1] - rU(k[i], xi)) - cu)
        kx = self.view[14:24]
        for i in range(9, 0, -1):
            xp = b1a if i == 1 else x[i - 1]
            x[i] = fit((xp - rX(-kx[i - 1], u[i - 1]) if o["neg_x"]
                        else xp + rX(kx[i - 1], u[i - 1])) - cx)
        x[0] = u[0]
        return u[0]

    def sample(self, frames):
        """Run one sample period. frames is an iterator of the frames still
        to parse. Returns the I/O code."""
        out = -1
        if self.pending:
            due = [p for p in self.pending if p[0] <= self.t]
            for _, i, value in sorted(due):
                self.view[i] = value
            self.pending = [p for p in self.pending if p[0] > self.t]
        if self.talkd:
            if self.ip == 0 and self.pc == 12 and self.sub == 1:
                frame = next(frames, None)
                if frame is None:       # out of data: read as a stop code
                    frame = {"energy": 15}
                self.parse(frame)
                if self.newStop():
                    self.talk = self.spen = False
                old_unvoiced, old_silence = self.oldp, self.olde
                self.inhibit = ((old_unvoiced != self.newUnvoiced())
                                or (old_silence and not self.newSilence())
                                or (old_unvoiced and self.newSilence()))
            elif self.sub == 1 and self.pc <= 11:
                # The interpolator computes each parameter's new value in
                # its PC; the filter sees it param_delay samples after the
                # PC's first (A) sample
                inhibit = self.inhibit and self.ip != 0
                i = self.pc
                if True:
                    target = self.target(i)
                    diff, shift = target - self.cur[i], self.interp[self.ip]
                    if inhibit:
                        delta = 0
                    elif self.options["interp_round"] == "trunc":
                        delta = -((-diff) >> shift) if diff < 0 else diff >> shift
                    else:
                        delta = diff >> shift
                    zero = self.zpar if i < 6 else self.uvZpar
                    if zero and i >= 6 and inhibit and self.options["uv_zero_inhibited"]:
                        zero = False    # held, like any inhibited update, until IP 0
                    viewOnly = zero and (self.options["zpar_view_only"] if self.zpar
                                         else self.options["uv_zero_view_only"])
                    if viewOnly:
                        # The filter sees zero; the interpolator keeps its value
                        self.cur[i] = self.cur[i] + delta
                        value = 0
                    else:
                        self.cur[i] = 0 if zero else self.cur[i] + delta
                        value = self.cur[i]
                    delay = self.options["param_delay"][i]
                    if i == 0 and self.cur[0] == 0 and self.options["energy_zero_delay"] is not None:
                        delay = self.options["energy_zero_delay"]
                    self.pending.append((self.t + delay, i, value >> self.fracBits(i)))
                    if i >= 2:
                        delayX = (self.options["param_delay_x"] or self.options["param_delay"])[i]
                        self.pending.append((self.t + delayX, 12 + i, value))
            # The pitch counter's hold ends in the pitch_hold-th sample of IP 0
            # (datasheet section 8.6: PC 0 A and B, and PC 1 A)
            if self.ip == 0 and 2 * self.pc + self.sub == self.options["pitch_hold"]:
                self.pitchZero = False
            self.oldpHistory = (self.oldpHistory + [self.oldp])[-8:]
            unvoiced = self.oldpHistory[-1 - self.options["excitation_delay"]] \
                if len(self.oldpHistory) > self.options["excitation_delay"] else self.oldp
            if unvoiced:
                excitation = -64 if self.rng & 1 else 64
            else:
                excitation = self.chirp[min(self.pitchCount, 51)]
                excitation = excitation - 256 if excitation >= 128 else excitation
            if self.options["excitation_pipeline"]:
                # The excitation is fetched one sample ahead of its use
                excitation, self.nextExcitation = self.nextExcitation, excitation
            steps = self.options["lfsr_steps"] if (unvoiced or not self.options["lfsr_unvoiced_only"]) else 0
            for _ in range(steps):
                bit = ((self.rng >> 12) ^ (self.rng >> 3) ^ (self.rng >> 2) ^ self.rng) & 1
                self.rng = ((self.rng << 1) | bit) & 0x1FFF
            value = wrap(self.lattice(excitation), 15)
            shifted = value >> 4 if self.options["out"] == "floor" else int(value / 16)
            out = wrap(shifted, self.options["output_bits"])
        elif self.options["idle_lattice"]:
            if self.options["idle_zero_all"] == "pc" and self.sub == 1 and self.pc <= 11:
                # The interpolator zeroes each parameter in its own PC
                i = self.pc
                shift = self.options["idle_zero_shift"]
                if not self.options["idle_keep_registers"]:
                    self.cur[i] = 0
                self.pending.append((self.t + self.options["param_delay"][i] + shift, i, 0))
                if i >= 2:
                    delayX = (self.options["param_delay_x"] or self.options["param_delay"])[i]
                    self.pending.append((self.t + delayX + shift, 12 + i, 0))
                if self.pending:
                    due = [p for p in self.pending if p[0] <= self.t]
                    for _, j, value in sorted(due):
                        self.view[j] = value
                    self.pending = [p for p in self.pending if p[0] > self.t]
            # Between utterances the filter keeps running; its parameters
            # were zeroed when speech ended
            if self.oldp:
                excitation = -64 if self.rng & 1 else 64
            else:
                excitation = self.chirp[min(self.pitchCount, 51)]
                excitation = excitation - 256 if excitation >= 128 else excitation
            value = wrap(self.lattice(excitation), 15)
            out = wrap(value >> 4, self.options["output_bits"])
        # counters
        self.sub += 1
        if self.sub == 2 and self.pc == 12:
            if self.ip == 7 and self.inhibit and self.talkd and self.options["pitch_zero"]:
                self.pitchZero = True
            if self.ip == 7:
                self.olde = self.newSilence()
                self.oldp = self.newUnvoiced()
                if self.talkd and not self.talk and self.options["idle_lattice"]:
                    zeroed = {True: range(12), False: [0], "pc": []}[self.options["idle_zero_all"]]
                    for i in zeroed:
                        self.cur[i] = 0
                        self.pending.append((self.t + self.options["param_delay"][i], i, 0))
                        if i >= 2:
                            delayX = (self.options["param_delay_x"] or self.options["param_delay"])[i]
                            self.pending.append((self.t + delayX, 12 + i, 0))
                self.talkd = self.talk
                if not self.talk and self.spen:
                    self.talk = True
            self.sub = 1
            self.pc = 0
            self.ip = (self.ip + 1) & 7
        elif self.sub == 3:
            self.sub = 1
            self.pc += 1
        if self.talkd:
            self.pitchCount += 1
            if self.pitchCount >= self.view[1] + self.options["pitch_period_extra"] \
                    or self.pitchZero:
                self.pitchCount = 0
            self.pitchCount &= 0x1FF
        self.t += 1
        return out

    def talkStatus(self):
        return self.spen or self.talkd

    def speak(self, frames, lead=0, tail=400):
        """Speak a list of frames (lpc.parseFrames dicts). The Speak is taken
        at a frame boundary (IP 7, PC 12) after `lead` samples of idle.
        Returns (codes, index of the sample in which frame 0 takes effect)."""
        frames = iter(frames)
        codes = []
        while not (self.ip == 7 and self.pc == 12 and self.sub == 1) or lead > 0:
            codes.append(self.sample(frames))
            lead -= 1
        self.startSpeak()
        parse0 = None
        quiet = 0
        while quiet < tail:
            if parse0 is None and self.talkd and self.ip == 0 and self.pc == 12 and self.sub == 1:
                parse0 = len(codes)
            codes.append(self.sample(frames))
            quiet = 0 if self.talkStatus() else quiet + 1
        return np.array(codes), parse0


def loadFrames(takeDir):
    """A take's frames.csv as parse dicts, with read_sample."""
    frames = []
    with open(os.path.join(takeDir, "frames.csv")) as f:
        for row in csv.DictReader(f):
            frame = {"energy": int(row["energy"]), "kind": row["kind"],
                     "read_sample": int(row["read_sample"]) if row.get("read_sample") else None}
            if row["repeat"] != "":
                frame["repeat"] = int(row["repeat"])
                frame["pitch"] = int(row["pitch"])
            k = [int(row[f"k{i}"]) for i in range(1, 11) if row[f"k{i}"] != ""]
            if k:
                frame["k"] = k
            frames.append(frame)
    return frames


def loadCodes(takeDir):
    data = np.genfromtxt(os.path.join(takeDir, "samples.csv"), delimiter=",", names=True)
    return data["code"].astype(int)


# How a standard-procedure take lines up with the model (datasheet
# section 7). The noise generator's state at the Speak, and the chip
# sample in which the model's first frame takes effect, counted from the
# sample in which the chip read the first frame's first bit (VSM), or from
# where that read would be (Speak External, which starts 2 samples
# earlier and leaves no read in the trace).
STANDARD_START = {"vsm": {"rng": 235, "offset": 25},
                  "external": {"rng": 175, "offset": 23}}
STANDARD_FIRST_READ_SAMPLE = 99


def takePath(takeDir):
    with open(os.path.join(takeDir, "capture.json")) as f:
        return json.load(f)["processed"]["summary"]["path"]


def compareTake(takeDir, options=None, tables=None, window=0, model=None, before=None):
    """Model a standard-procedure take and compare it with the chip's I/O
    codes, sample for sample, from the first frame's effect to the end of
    the capture. With window > 0, the alignment is also searched that many
    samples either way. Returns a dict: offset, compared, mismatched,
    abs_sum, max_abs, first_mismatch (samples after the first frame's
    effect) and exact."""
    path = takePath(takeDir)
    start = STANDARD_START[path]
    frames, codes = loadFrames(takeDir), loadCodes(takeDir)
    first = frames[0]["read_sample"]
    if first is None:
        first = STANDARD_FIRST_READ_SAMPLE
    opts = {"rng_at_speak": start["rng"]}
    opts.update(options or {})
    m = model or Model(tables=tables, **opts)
    for previous in before or []:       # utterances spoken before this one
        m.speak(previous)
        m.rng = opts["rng_at_speak"]
    out, p0 = m.speak(frames)
    best = None
    for offset in range(start["offset"] - window, start["offset"] + window + 1):
        shift = first + offset - p0             # chip index = model index + shift
        lo, hi = max(0, -shift), min(len(out), len(codes) - shift)
        diff = codes[lo + shift:hi + shift] - out[lo:hi]
        result = {"offset": offset, "compared": int(hi - lo),
                  "mismatched": int(np.count_nonzero(diff)),
                  "abs_sum": int(np.abs(diff).sum()), "max_abs": int(np.abs(diff).max()),
                  "first_mismatch": (int(np.flatnonzero(diff)[0] + lo - p0)
                                     if np.count_nonzero(diff) else None)}
        if best is None or result["abs_sum"] < best["abs_sum"]:
            best = result
    best["exact"] = best["mismatched"] == 0
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("takes", nargs="+", help="standard-procedure take directories")
    parser.add_argument("--window", type=int, default=0, help="also search the alignment")
    args = parser.parse_args()
    exact = 0
    for take in args.takes:
        r = compareTake(take, window=args.window)
        exact += r["exact"]
        print(f"{take}: {'exact' if r['exact'] else 'differs'}, {r['mismatched']} of "
              f"{r['compared']} samples, largest {r['max_abs']}, first at {r['first_mismatch']}")
    print(f"{exact} of {len(args.takes)} exact")


if __name__ == "__main__":
    main()
