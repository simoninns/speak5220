#!/usr/bin/env python3
"""TMS5220 encoder self-test.

Checks the encoder against statements in the reference datasheet: the frame
format and bit order (§8.1, §12.1), the multiplier examples (§8.8), the sign
convention of the lattice (§12.3), the start of speech (§8.5), the PHROM and
TI indexed layouts (§12.8), the 9-byte rule (§12.9), and that encode() gives
a valid, deterministic frame list for a test input.

    nix develop -c python3 encoder/test_tms5220enc.py

This file is part of speak5220.
SPDX-License-Identifier: GPL-3.0-or-later
"""

import os
import random
import subprocess
import sys
import tempfile
import unittest

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, HERE)
import tms5220enc as enc  # noqa: E402


def randomFrame(rng, kind=None):
    kind = kind or rng.choice(["voiced", "unvoiced", "repeat", "silence"])
    if kind == "silence":
        return {"energy": 0}
    if kind == "stop":
        return {"energy": 15}
    e = rng.randint(1, 14)
    if kind == "repeat":
        return {"energy": e, "repeat": 1, "pitch": rng.randint(0, 63)}
    if kind == "unvoiced":
        return {"energy": e, "repeat": 0, "pitch": 0,
                "k": [rng.randint(0, (1 << enc.KBITS[i]) - 1) for i in range(4)]}
    return {"energy": e, "repeat": 0, "pitch": rng.randint(1, 63),
            "k": [rng.randint(0, (1 << enc.KBITS[i]) - 1) for i in range(10)]}


def speechLike(rng, n=30):
    """Random frames in the range of speech, starting voiced, ending in a stop."""
    fr = [{"energy": 8, "repeat": 0, "pitch": 30, "k": [15, 15, 8, 8, 8, 8, 8, 4, 4, 4]}]
    for _ in range(n):
        r = rng.random()
        if r < 0.5:
            fr.append({"energy": rng.randint(6, 13), "repeat": 0, "pitch": rng.randint(20, 60),
                       "k": [rng.randint(8, 24), rng.randint(8, 24)] + [rng.randint(4, 11) for _ in range(5)]
                       + [rng.randint(2, 5) for _ in range(3)]})
        elif r < 0.7:
            fr.append({"energy": rng.randint(3, 8), "repeat": 0, "pitch": 0,
                       "k": [rng.randint(10, 24), rng.randint(8, 24), rng.randint(4, 11), rng.randint(4, 11)]})
        elif r < 0.9:
            fr.append({"energy": rng.randint(3, 12), "repeat": 1, "pitch": rng.choice([0, rng.randint(20, 60)])})
        else:
            fr.append({"energy": 0})
    fr.append({"energy": 15})
    return fr


class PackParse(unittest.TestCase):
    def testFrameLengths(self):
        rng = random.Random(1)
        for kind, bits in [("voiced", 50), ("unvoiced", 29), ("repeat", 11), ("silence", 4), ("stop", 4)]:
            f = randomFrame(rng, kind)
            self.assertEqual(enc.frameBits(f), bits)
            self.assertEqual(enc.frameKind(f), kind)
            _, used = enc.parse(enc.pack([f, {"energy": 15}]))
            self.assertEqual(used, bits + (0 if kind == "stop" else 4))

    def testRoundTripRandom(self):
        rng = random.Random(2)
        for trial in range(300):
            frames = [randomFrame(rng) for _ in range(rng.randint(0, 40))] + [{"energy": 15}]
            data = enc.pack(frames)
            back, used = enc.parse(data)
            self.assertEqual(back, frames)
            self.assertEqual(used, sum(enc.frameBits(f) for f in frames))
            self.assertEqual(len(data), (used + 7) // 8)
            # Padding after the stop code is zero
            if used % 8:
                self.assertEqual(data[-1] >> (used % 8), 0)

    def testRoundTripHandMade(self):
        frames = [
            {"energy": 14, "repeat": 0, "pitch": 63, "k": [31, 31, 15, 15, 15, 15, 15, 7, 7, 7]},
            {"energy": 1, "repeat": 0, "pitch": 1, "k": [0] * 10},
            {"energy": 7, "repeat": 1, "pitch": 40},
            {"energy": 0},
            {"energy": 5, "repeat": 0, "pitch": 0, "k": [3, 30, 1, 14]},
            {"energy": 3, "repeat": 1, "pitch": 0},
            {"energy": 15},
        ]
        back, used = enc.parse(enc.pack(frames))
        self.assertEqual(back, frames)
        self.assertEqual(used, 50 + 50 + 11 + 4 + 29 + 11 + 4)

    def testPackStopsAtStop(self):
        frames = [{"energy": 0}, {"energy": 15}, {"energy": 0}]
        self.assertEqual(enc.pack(frames), enc.pack(frames[:2]))

    def testTruncated(self):
        frames = [randomFrame(random.Random(3), "voiced") for _ in range(3)] + [{"energy": 15}]
        data = enc.pack(frames)
        back, used = enc.parse(data[:12])          # 96 bits: one whole frame, then part of one
        self.assertEqual(back, frames[:1])
        self.assertEqual(used, 50)
        back, used = enc.parse(data, maxFrames=2)
        self.assertEqual(back, frames[:2])

    def testInvalid(self):
        for f in [{"energy": 16}, {"energy": 5, "repeat": 0, "pitch": 0, "k": [0] * 10},
                  {"energy": 5, "repeat": 0, "pitch": 3, "k": [32] + [0] * 9}]:
            with self.assertRaises(ValueError):
                enc.pack([f])


class BitOrder(unittest.TestCase):
    def testStopByte(self):
        # 1111 read LSB first from the byte
        self.assertEqual(enc.pack([{"energy": 15}]), b"\x0f")

    def testFieldsMsbFirst(self):
        # energy 1 = 0001, repeat 1, pitch 1 = 000001, then stop 1111:
        # stream bits 0001 1 000001 1111 fill bytes from bit 0
        data = enc.pack([{"energy": 1, "repeat": 1, "pitch": 1}, {"energy": 15}])
        self.assertEqual(data, bytes([0x18, 0x7C]))
        # energy 8 (1000) then silence: bit 0 of the first byte is the energy's MSB
        self.assertEqual(enc.pack([{"energy": 8, "repeat": 1, "pitch": 0}, {"energy": 0}, {"energy": 15}])[0] & 1, 1)

    def testReadByteReversal(self):
        # §6: image bytes 0x23, 0x9B, 0x35, 0x85 read back as 0xC4, 0xD9, 0xAC, 0xA1
        for a, b in [(0x23, 0xC4), (0x9B, 0xD9), (0x35, 0xAC), (0x85, 0xA1)]:
            self.assertEqual(enc.bitReverse(a), b)


class Arithmetic(unittest.TestCase):
    EXAMPLES = [  # §8.8: K, M, digits, product
        (0, 12345, [0, 0, 0, 0, 0], 1),
        (33, 192, [1, 0, -2, 1, 0], 13),
        (114, 4096, [-2, 1, -1, 2, 0], 911),
        (-18, 3, [-2, 0, -1, 0, 0], -1),
        (-18, -3, [-2, 0, -1, 0, 0], 1),
        (506, 1000, [-2, -1, 0, 0, 2], 989),
        (-501, -777, [-1, -1, 1, 0, -2], 761),
        (409, -8192, [1, -2, 2, -2, 2], -6543),
        (-256, 8191, [0, 0, 0, 0, -1], -4095),
    ]

    def testBoothExamples(self):
        for k, m, digits, product in self.EXAMPLES:
            self.assertEqual(enc.boothDigits(k), digits, (k, m))
            self.assertEqual(enc.booth(k, m), product, (k, m))
            self.assertEqual(enc.boothFast(k, m), product, (k, m))

    def testBoothFastMatches(self):
        rng = random.Random(4)
        for _ in range(20000):
            k, m = rng.randint(-512, 511), rng.randint(-8192, 8191)
            self.assertEqual(enc.booth(k, m), enc.boothFast(k, m))

    def testWrap(self):
        self.assertEqual(enc.wrap14(8191), 8191)
        self.assertEqual(enc.wrap14(8192), -8192)
        self.assertEqual(enc.wrap14(-8193), 8191)

    def testTables(self):
        self.assertEqual(sum(c * c for c in enc.CHIRP), 65546)        # §12.3
        self.assertEqual(len(enc.CHIRP), 52)
        self.assertEqual([len(t) for t in enc.KTABLES], [1 << b for b in enc.KBITS])
        for t in enc.KTABLES:
            self.assertEqual(t, sorted(t))
        self.assertEqual(enc.PITCH[13] + 1, 28)
        self.assertEqual(enc.PITCH[63] + 1, 160)


class SignConvention(unittest.TestCase):
    def testOneStage(self):
        # §12.3: u1 = u2 - K1*b1 with b1 the previous output is y(n) = x(n) - k1 y(n-1),
        # 1 / (1 + k1 z^-1); Levinson-Durbin in the e(n) = s(n) + sum a s(n-j)
        # convention gives the same k1.
        K1 = -459                       # K1 index 12
        k1 = K1 / 512.0
        rng = np.random.default_rng(5)
        x = rng.integers(-300, 300, 4000)
        y = np.zeros(len(x), dtype=np.int64)
        b1 = 0
        for n in range(len(x)):
            u1 = enc.wrap14(int(x[n]) - enc.boothFast(K1, b1))
            y[n] = u1
            b1 = u1
        yf = np.zeros(len(x))
        for n in range(len(x)):
            yf[n] = x[n] - k1 * (yf[n - 1] if n else 0.0)
        self.assertLess(np.max(np.abs(y - yf)), 12)
        r = np.array([np.dot(yf[:len(yf) - i], yf[i:]) for i in range(3)])
        ks = enc.levinson(r, 2)
        self.assertAlmostEqual(ks[0], k1, delta=0.03)
        self.assertTrue(np.allclose(enc.stepUp([k1]), [1.0, k1]))

    def testNegativeK1IsLowPass(self):
        # In the chip's synthesis, a large negative K1 gives a low-frequency output
        def lag1(kidx):
            f = {"energy": 8, "repeat": 0, "pitch": 0, "k": [kidx, 15, 8, 6]}
            c = enc.synthesize([f] * 6 + [{"energy": 15}]).astype(float)[400:1200]
            c -= c.mean()
            return np.dot(c[:-1], c[1:]) / np.dot(c, c)
        self.assertGreater(lag1(5), 0.8)       # K1 = -491
        self.assertLess(lag1(30), -0.3)        # K1 = +411


class Synthesis(unittest.TestCase):
    def testStartAndEnd(self):
        f = {"energy": 10, "repeat": 0, "pitch": 40, "k": [20, 15, 8, 8, 8, 8, 8, 4, 4, 4]}
        c = enc.synthesize([f] * 4 + [{"energy": 15}])
        nz = np.nonzero(c != -1)[0]
        self.assertIn(nz[0], range(205, 210))   # §7: 206-207 for PROM words
        # Unvoiced: the energy is computed in PC 0 A of IP 0 (sample 201) and
        # reaches the filter two samples later (§8.5), when the noise shows at once
        u = {"energy": 10, "repeat": 0, "pitch": 0, "k": [20, 15, 8, 8]}
        cu = enc.synthesize([u] * 4 + [{"energy": 15}])
        self.assertEqual(np.nonzero(cu != -1)[0][0], 203)
        self.assertTrue(np.all(c[-25:] == -1))
        # Talking for 5 frames (stop included) and then a short decay
        self.assertLess(nz[-1], 5 * 200 + 60)
        self.assertGreater(nz[-1], 5 * 200)

    def testIdleStop(self):
        c = enc.synthesize([{"energy": 15}])
        self.assertTrue(np.all(c == -1))

    @unittest.skipUnless(os.path.exists(os.path.join(ROOT, "emulator", "tms5220emu")),
                         "reference emulator not built")
    def testAgainstReferenceEmulator(self):
        # The project's emulator as a black box: same codes, sample for sample
        rng = random.Random(6)
        with tempfile.TemporaryDirectory() as d:
            for trial in range(3):
                frames = speechLike(rng)
                img = os.path.join(d, "v.bin")
                out = os.path.join(d, "v.txt")
                with open(img, "wb") as fh:
                    fh.write(enc.pack(frames))
                subprocess.run([os.path.join(ROOT, "emulator", "tms5220emu"), "-s", out, "-e", "none",
                                "--image", img, "--addr", "0"], check=True, capture_output=True)
                e = np.loadtxt(out, dtype=int)[:, 2]
                c = enc.synthesize(frames)
                # With the standard procedure the first frame is read at sample 200
                self.assertTrue(np.array_equal(e[200:200 + len(c)], c))


class Packaging(unittest.TestCase):
    def words(self):
        rng = random.Random(7)
        return [("HELLO", speechLike(rng, 10)), ("A", speechLike(rng, 5)),
                ("WORLD", speechLike(rng, 20))]

    def testSpeakExternalMinimum(self):
        for frames in ([{"energy": 15}], [{"energy": 3, "repeat": 0, "pitch": 0, "k": [1, 2, 3, 4]}],
                       [randomFrame(random.Random(8), "voiced")]):
            data = enc.speakExternalStream(frames)
            self.assertGreaterEqual(len(data), 9)
            back, used = enc.parse(data)
            self.assertEqual(len(data), (used + 7) // 8)
            self.assertEqual(back[-1], {"energy": 15})
            body = [f for f in frames if f["energy"] != 15]
            self.assertEqual(back[:len(body)], body)
            self.assertTrue(all(f == {"energy": 0} for f in back[len(body):-1]))
            # No more silence than needed
            if len(back) > len(body) + 1:
                self.assertLess(len(enc.pack(back[:-2] + [{"energy": 15}])), 9)
        long = speechLike(random.Random(9), 10)
        self.assertEqual(enc.speakExternalStream(long), enc.pack(long))

    def testAcornPhrom(self):
        words = self.words()
        amap = {"H": "HELLO", 65: "A", "W": "WORLD"}
        img, table = enc.acornPhrom(words, asciiMap=amap)
        self.assertEqual(len(img), 16384)
        host = bytes(enc.bitReverse(b) for b in img[:64])      # as Read Byte returns it
        self.assertEqual(host[0:2], b"\x00\xff")
        strs = host[2:56].split(b"\x00")
        self.assertEqual(strs[:3], [b"(C)2026 speak5220", b"Phase 11 PHROM", b"1.00"])
        self.assertEqual(host[56] | host[57] << 8, 95)
        npt = host[58] | host[59] << 8
        self.assertEqual(npt, 95 + len(words))
        self.assertEqual(host[60:64], bytes([0, 2, 0, 2]))
        last = 32 + npt - 1
        self.assertEqual(img[2 * last + 2], 0xFF)
        # The first word's name follows the pointer table's 0xFF
        self.assertEqual(table[0]["offset"] - len(words[0][0]), 2 * last + 3)

        def ptr(n):
            return img[2 * n] | img[2 * n + 1] << 8
        for i, (name, frames) in enumerate(words):
            n = 127 + i
            off = ptr(n)
            self.assertEqual(table[i]["number"], n)
            self.assertEqual(table[i]["offset"], off)
            self.assertEqual(table[i]["name"], name)
            back, used = enc.parse(img[off:])
            self.assertEqual(back, frames)
            self.assertEqual(table[i]["bytes"], (used + 7) // 8)
            self.assertEqual(table[i]["frames"], len(frames))
            # The name, backwards, just before the data: (char << 1), bit-reversed
            for j, ch in enumerate(name):
                self.assertEqual(enc.bitReverse(img[off - 1 - j]) >> 1, ord(ch))
            # Pointers reach only their own bank
            self.assertLess(off, 0x4000)
        self.assertEqual(ptr(ord("H")), ptr(127))
        self.assertEqual(ptr(65), ptr(128))
        self.assertEqual(ptr(ord("W")), ptr(129))
        # Unmapped ASCII entries speak nothing: a lone stop code
        back, _ = enc.parse(img[ptr(ord("Z")):])
        self.assertEqual(back, [{"energy": 15}])
        # Words do not overlap
        ends = sorted((t["offset"], t["offset"] + t["bytes"]) for t in table)
        for a, b in zip(ends, ends[1:]):
            self.assertLessEqual(a[1], b[0] - len(words[[t["offset"] for t in table].index(b[0])][0]))

    def testAcornTooBig(self):
        big = speechLike(random.Random(10), 400)
        with self.assertRaises(ValueError):
            enc.acornPhrom([("W%d" % i, big) for i in range(20)])

    def testTiIndexed(self):
        words = self.words()
        img, table = enc.tiIndexed(words)
        self.assertEqual(len(img), 16384)
        self.assertEqual(img[0], 0)
        self.assertEqual(img[1], len(words))
        for i, (name, frames) in enumerate(words):
            off = img[2 * (i + 1)] | img[2 * (i + 1) + 1] << 8
            self.assertEqual(table[i]["offset"], off)
            back, used = enc.parse(img[off:])
            self.assertEqual(back, frames)
            # One spare byte after the stop code's byte
            if i + 1 < len(words):
                nxt = img[2 * (i + 2)] | img[2 * (i + 2) + 1] << 8
                self.assertEqual(nxt, off + (used + 7) // 8 + 1)
        with self.assertRaises(ValueError):
            enc.tiIndexed([("W", speechLike(random.Random(11), 400))] * 20)


class Predecessor(unittest.TestCase):
    U = {"energy": 6, "repeat": 0, "pitch": 0, "k": [20, 15, 8, 8]}
    V = {"energy": 9, "repeat": 0, "pitch": 40, "k": [20, 15, 8, 8, 8, 8, 8, 4, 4, 4]}

    def dependence(self, frames):
        lo = enc._synth(frames, initIdx=[15, 1] + [0] * 10)
        hi = enc._synth(frames, initIdx=[15, 63] + [(1 << b) - 1 for b in enc.KBITS])
        n = min(len(lo), len(hi))
        return int(np.max(np.abs(lo[:n] - hi[:n])))

    def testPattern(self):
        # §12.2: silence after an opening unvoiced frame, before any voiced
        # frame, uses the previous utterance's K5-K10
        bad = [self.U, {"energy": 0}, self.U, self.V, {"energy": 15}]
        self.assertGreater(self.dependence(bad), 0)
        fixed = enc._addFades(bad[:-1], dict(enc.DEFAULTS))
        self.assertNotIn({"energy": 0}, fixed[:3])
        self.assertEqual(self.dependence(fixed), 0)
        # After a voiced frame, silence is safe and kept
        ok = enc._guardK510([self.V, {"energy": 0}, self.U, {"energy": 0}, self.V])
        self.assertEqual(ok.count({"energy": 0}), 2)
        self.assertEqual(self.dependence(ok + [{"energy": 15}]), 0)
        # An unvoiced end before the stop is safe too
        self.assertEqual(self.dependence([self.U, self.U, {"energy": 15}]), 0)


class Encode(unittest.TestCase):
    PATH = os.path.join(ROOT, "captures", "phase11", "inputs", "tts", "w05_vctk_lo.flac")

    @unittest.skipUnless(os.path.exists(PATH), "test input missing")
    def testEncodeInput(self):
        import soundfile as sf
        x, rate = sf.read(self.PATH, dtype="float64")
        frames = enc.encode(x, rate)
        self.assertEqual(frames[-1], {"energy": 15})
        self.assertEqual(sum(1 for f in frames if f["energy"] == 15), 1)
        back, _ = enc.parse(enc.pack(frames))
        self.assertEqual(back, frames)
        kinds = [enc.frameKind(f) for f in frames]
        self.assertIn(kinds[0], ("voiced", "unvoiced"))
        self.assertIn("voiced", kinds)
        # A voiced frame after an unvoiced one is a full frame (§12.2)
        lastFull = None
        for f, k in zip(frames, kinds):
            if k in ("voiced", "unvoiced"):
                lastFull = k
            if k == "repeat" and f["pitch"]:
                self.assertEqual(lastFull, "voiced")
        st = {}
        codes = enc._synth(frames, st)
        self.assertEqual(st["wraps"], 0)
        self.assertLessEqual(int(np.max(np.abs(codes))), 127)
        spk = codes[200:].astype(float)
        rms = np.sqrt(np.mean(spk[np.abs(spk) > 1] ** 2))
        self.assertGreater(rms, 8)
        # Deterministic
        self.assertEqual(enc.encode(x, rate), frames)
        # Independent of the predecessor's stored indices (§12.2)
        lo = enc._synth(frames, initIdx=[15, 1] + [0] * 10)
        hi = enc._synth(frames, initIdx=[15, 63] + [(1 << b) - 1 for b in enc.KBITS])
        self.assertTrue(np.array_equal(lo, hi))
        # No repeats when they are switched off
        self.assertNotIn("repeat", [enc.frameKind(f) for f in enc.encode(x, rate, repeats=False)])


if __name__ == "__main__":
    prog = unittest.main(exit=False, verbosity=2)
    sys.exit(0 if prog.result.wasSuccessful() else 1)
