#!/usr/bin/env python3
"""TMS5220 speech encoder and packager.

Turns 8 kHz speech into TMS5220 frames (energy, pitch, K1-K10), packs them
into the chip's bit stream, synthesises them with the reference algorithm of
the datasheet, and packages words as Acorn PHROM or TI indexed images.

Written from docs-tech/tms5220_reference_datasheet (§4-§8, §12) alone: no
other encoder, emulator or original PROM data was used. Choices the datasheet
leaves open are listed in encoder/GAPS.md; the method is in encoder/README.md.

Frames are dicts of coded indices:
    {"energy": 0}                                   silence
    {"energy": 15}                                  stop
    {"energy": e, "repeat": 1, "pitch": p}          repeat
    {"energy": e, "repeat": 0, "pitch": 0, "k": [4 indices]}    unvoiced
    {"energy": e, "repeat": 0, "pitch": p, "k": [10 indices]}   voiced

Command line:
    python encoder/tms5220enc.py encode IN.flac OUT.bin [--csv OUT.csv] [--wav OUT.wav]

This file is part of speak5220.
SPDX-License-Identifier: GPL-3.0-or-later
"""

import math
import sys

import numpy as np

# ---------------------------------------------------------------------------
# Tables (§8.1, Tables 8-4 to 8-7)

ENERGY = [0, 1, 2, 3, 4, 6, 8, 11, 16, 23, 33, 47, 63, 85, 114, 0]

# Pitch register value per index; the period is the value plus one (§8.6)
PITCH = [0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
         30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 44, 46, 48,
         50, 52, 53, 56, 58, 60, 62, 65, 68, 70, 72, 76, 78, 80, 84, 86,
         91, 94, 98, 101, 105, 109, 114, 118, 122, 127, 132, 137, 142, 148, 153, 159]

KTABLES = [
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
KBITS = [5, 5, 4, 4, 4, 4, 4, 3, 3, 3]

# Chirp ROM, 52 locations; 21-51 hold 0 (Table 8-7)
CHIRP = [0, 3, 15, 40, 76, 108, 113, 80, 37, 38, 76, 68, 26, 50, 59, 19, 55, 26, 37, 31, 29] + [0] * 31

RATE = 8000
FRAME = 200                 # samples per frame (25 ms)

# Output sample (counted from the first frame read) at the middle of frame
# j's own stretch: its values are half-way reached by the end of IP 4 of its
# glide (sample 125), reached in IP 0 (201-225), and half-way to the next
# frame's by sample 325 (§8.5, §8.7, §12.2)
REACHED = 225


# ---------------------------------------------------------------------------
# Frames, bits and bytes (§8.1, §12.1)

def frameKind(f):
    """Kind of a frame: voiced, unvoiced, repeat, silence or stop."""
    e = f["energy"]
    if e == 0:
        return "silence"
    if e == 15:
        return "stop"
    if f.get("repeat", 0):
        return "repeat"
    return "unvoiced" if f["pitch"] == 0 else "voiced"


def frameBits(f):
    """Length of a frame in bits."""
    return {"silence": 4, "stop": 4, "repeat": 11, "unvoiced": 29, "voiced": 50}[frameKind(f)]


def _checkFrame(f):
    e = f["energy"]
    if not 0 <= e <= 15:
        raise ValueError("energy out of range: %r" % (f,))
    if e in (0, 15):
        return
    if f.get("repeat", 0) not in (0, 1) or not 0 <= f["pitch"] <= 63:
        raise ValueError("bad repeat or pitch: %r" % (f,))
    if f.get("repeat", 0):
        return
    k = f["k"]
    n = 4 if f["pitch"] == 0 else 10
    if len(k) != n:
        raise ValueError("expected %d K indices: %r" % (n, f))
    for i, v in enumerate(k):
        if not 0 <= v < (1 << KBITS[i]):
            raise ValueError("K%d index out of range: %r" % (i + 1, f))


def frameFields(f):
    """The (value, bits) fields of a frame, in stream order."""
    _checkFrame(f)
    e = f["energy"]
    out = [(e, 4)]
    if e in (0, 15):
        return out
    rep = f.get("repeat", 0)
    out += [(rep, 1), (f["pitch"], 6)]
    if not rep:
        out += [(v, KBITS[i]) for i, v in enumerate(f["k"])]
    return out


def pack(frames):
    """Pack frames into bytes: fields MSB first, bytes filled LSB first.

    Packing ends at the first stop frame; the rest of its byte is zero.
    """
    bits = []
    for f in frames:
        for value, n in frameFields(f):
            bits.extend((value >> (n - 1 - i)) & 1 for i in range(n))
        if f["energy"] == 15:
            break
    out = bytearray((len(bits) + 7) // 8)
    for i, b in enumerate(bits):
        if b:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def parse(data, maxFrames=100000):
    """Parse a bit stream into frames, up to and including the stop code.

    Returns (frames, bitsUsed). If the bits run out mid-frame, the complete
    frames read so far are returned and bitsUsed counts only their bits.
    """
    total = len(data) * 8
    pos = 0

    def read(n):
        nonlocal pos
        if pos + n > total:
            raise EOFError
        v = 0
        for _ in range(n):
            v = (v << 1) | ((data[pos >> 3] >> (pos & 7)) & 1)
            pos += 1
        return v

    frames = []
    used = 0
    try:
        while len(frames) < maxFrames:
            e = read(4)
            if e in (0, 15):
                f = {"energy": e}
            else:
                rep = read(1)
                p = read(6)
                f = {"energy": e, "repeat": rep, "pitch": p}
                if not rep:
                    n = 4 if p == 0 else 10
                    f["k"] = [read(KBITS[i]) for i in range(n)]
            frames.append(f)
            used = pos
            if e == 15:
                break
    except EOFError:
        pass
    return frames, used


# ---------------------------------------------------------------------------
# Arithmetic (§8.8)

def wrap14(v):
    """Take a value to 14-bit two's complement."""
    return ((v + 8192) & 16383) - 8192


def boothDigits(k):
    """Radix-4 Booth digits d0..d4 of a 10-bit two's-complement coefficient."""
    kb = k & 0x3FF

    def bit(n):
        return 0 if n < 0 else (kb >> n) & 1
    return [-2 * bit(2 * i + 1) + bit(2 * i) + bit(2 * i - 1) for i in range(5)]


def booth(k, m):
    """The chip's multiplier, step by step as §8.8 gives it."""
    d = boothDigits(k)
    p0 = d[0] * m if d[0] >= 0 else -abs(d[0]) * m - 1
    acc = (p0 >> 2) + d[1] * m
    for i in (2, 3, 4):
        acc = (acc >> 2) + d[i] * m
    return (acc >> 1) | 1


def boothFast(k, m):
    """The same product in closed form: floor((K*M - k1) / 512) OR 1.

    The nested shifts of booth() collapse to one floor division, because the
    digits sum to K; the inverted stage-0 product costs 1 when bit 1 of K is set.
    """
    return ((k * m - ((k >> 1) & 1)) >> 9) | 1


def _noiseStep(s):
    """One step of the 13-bit noise register (§8.6)."""
    fb = ((s >> 12) ^ (s >> 3) ^ (s >> 2) ^ s) & 1
    return ((s << 1) | fb) & 0x1FFF


_NEXT20 = None


def _next20():
    global _NEXT20
    if _NEXT20 is None:
        t = []
        for s in range(8192):
            for _ in range(20):
                s = _noiseStep(s)
            t.append(s)
        _NEXT20 = t
    return _NEXT20


# ---------------------------------------------------------------------------
# Synthesis: the reference algorithm of §8.9

_SHIFT = [0, 3, 3, 3, 2, 2, 1, 1]
_DELAY = [2, 3] + [1] * 10


def _table(i, ix):
    if i == 0:
        return ENERGY[ix]
    if i == 1:
        return PITCH[ix]
    return KTABLES[i - 2][ix]


def _synth(frames, stats=None, initIdx=None):
    """Run §8.9 for one VSM Speak from power-up state.

    Returns the I/O codes from the frame-read sample of the first frame
    (sample 0) until the output has returned to idle after the stop.
    If stats is a dict, it receives the number of samples in which a lattice
    sum wrapped and the largest internal magnitude.
    """
    nxt = _next20()
    frames = list(frames)
    idx = list(initIdx) if initIdx is not None else [0] * 12   # parameter RAM (§7)
    cur = [0] * 12
    seen = [0] * 12
    queue = []
    talking = requested = Z = U = lastU = inhibit = hold = False
    oldS = oldU = False
    loads = 0
    count = 0
    noise = 0x00EB              # state at slot 0 of sample 1 for a Speak (§7)
    fp = 0
    pending = None
    u = [0] * 12
    b = [0] * 11
    chirp = CHIRP
    wraps = 0
    peak = 0

    # Idle filter state: E and K zero give u1 = -9 (§8.8)
    for _ in range(16):
        u[11] = 1
        for i in range(10, 0, -1):
            u[i] = u[i + 1] - 1
        for i in range(10, 1, -1):
            b[i] = b[i - 1] + 1
        b[1] = u[1]
    b = [0] + [wrap14(v) for v in b[1:]]

    codes = []
    n = 0
    ended = False
    idleRun = 0
    oldUprev = oldU
    limit = 200 * (len(frames) + 3) + 400
    while n < limit:
        s = n % 200
        if s == 0:
            ip, pc, isA = 7, 12, False
        else:
            q = s - 1
            ip = q // 25
            w = q % 25
            pc = w >> 1
            isA = (w & 1) == 0 and pc < 12
        oldUnow = oldU

        # Frame reads (sample 0 of each frame)
        if s == 0 and (n == 0 or (talking and requested)):
            if n == 0:
                Z = U = True
                loads = 0
                requested = True
            if fp < len(frames):
                pending = frames[fp]
                fp += 1
            else:
                pending = {"energy": 15}     # out of data: ends like a stop

        # Step 1
        if queue:
            rest = []
            for item in queue:
                if item[0] <= n:
                    seen[item[1]] = item[2]
                else:
                    rest.append(item)
            queue = rest

        if talking:
            # Load a frame in PC 12 of IP 0
            if ip == 0 and pc == 12 and pending is not None:
                f = pending
                pending = None
                loads += 1
                e = f["energy"]
                if not (e == 0 and loads == 1):
                    Z = U = False
                idx[0] = e
                if e == 15:
                    requested = False
                if e not in (0, 15):
                    idx[1] = f["pitch"]
                    if f.get("repeat", 0):
                        U = (f["pitch"] == 0) or lastU
                    else:
                        U = f["pitch"] == 0
                        lastU = U
                        k = f["k"]
                        for j in range(len(k)):     # unvoiced: K1-K4 only
                            idx[2 + j] = k[j]
                sil = idx[0] in (0, 15)
                unv = idx[1] == 0
                inhibit = (oldU != unv) or (oldS and not sil) or (oldU and sil)
            # Parameter update in the A sample of each PC
            if isA:
                i = pc
                now = inhibit and ip != 0
                if Z or (U and i >= 6):
                    tgt = 0
                else:
                    tgt = _table(i, idx[i])
                step = 0 if now else (tgt - cur[i]) >> _SHIFT[ip]
                zero = Z if i <= 5 else U
                if i >= 6 and now:
                    zero = False
                if zero and Z:
                    cur[i] += step
                    val = 0
                elif zero:
                    cur[i] = 0
                    val = 0
                else:
                    cur[i] += step
                    val = cur[i]
                queue.append((n + _DELAY[i], i, val))
                if ip == 0 and pc == 1:
                    hold = False
            exU = oldUprev
        else:
            if isA:
                queue.append((n + _DELAY[pc] - 1, pc, 0))
            exU = oldU

        if queue:
            rest = []
            for item in queue:
                if item[0] <= n:
                    seen[item[1]] = item[2]
                else:
                    rest.append(item)
            queue = rest

        # Excitation, in units of 1/128
        if exU:
            x = -64 if noise & 1 else 64
        else:
            x = chirp[count if count < 51 else 51]

        # Lattice (§8.8)
        e = seen[0]
        raw = ((e * 64 * x - ((e >> 1) & 1)) >> 9) | 1
        v = ((raw + 8192) & 16383) - 8192
        wrapped = v != raw
        mx = abs(raw)
        u[11] = v
        for i in range(10, 0, -1):
            k = seen[i + 1]
            bi = b[i]
            raw = u[i + 1] - (((k * bi - ((k >> 1) & 1)) >> 9) | 1)
            v = ((raw + 8192) & 16383) - 8192
            if v != raw:
                wrapped = True
            if raw > mx or -raw > mx:
                mx = abs(raw)
            u[i] = v
        for i in range(10, 1, -1):
            k = seen[i]
            raw = b[i - 1] + (((k * u[i - 1] - ((k >> 1) & 1)) >> 9) | 1)
            v = ((raw + 8192) & 16383) - 8192
            if v != raw:
                wrapped = True
            if raw > mx or -raw > mx:
                mx = abs(raw)
            b[i] = v
        b[1] = u[1]
        if wrapped:
            wraps += 1
        if mx > peak:
            peak = mx
        code = u[1] >> 4
        codes.append(code)

        # End of the period
        oldUprev = oldUnow
        if s == 0:
            if inhibit and talking:
                hold = True
            oldS = idx[0] in (0, 15)
            oldU = idx[1] == 0
            if talking and not requested:
                ended = True
            talking = requested if (talking or n == 0) else False
        if talking:
            count += 1
            if count >= seen[1] + 1 or hold:
                count = 0
        if n >= 1:
            noise = nxt[noise]
        if ended and not talking:
            idleRun = idleRun + 1 if code == -1 else 0
            if idleRun >= 25:
                break
        n += 1
    if stats is not None:
        stats["wraps"] = wraps
        stats["peakInternal"] = peak
    return np.array(codes, dtype=np.int32)


def synthesize(frames):
    """Output codes of the reference algorithm (§8.5-§8.9) for one Speak.

    Sample 0 is the sample period in which the first frame is read; the
    output stays at idle (-1) until about sample 206, and frame j's values
    are reached around sample 213 + 200*j. The array ends once the output
    has been back at idle for 25 samples after the stop.
    """
    return _synth(frames)


# ---------------------------------------------------------------------------
# LPC helpers (§12.3, §12.6)

def stepUp(k):
    """Predictor polynomial a[0..p] (a[0] = 1) from reflection coefficients."""
    a = np.array([1.0])
    for ki in k:
        a = np.concatenate([a, [0.0]])
        a = a + ki * a[::-1]
    return a


def levinson(r, order):
    """Reflection coefficients by Levinson-Durbin, e(n) = s(n) + sum a_j s(n-j)."""
    a = np.zeros(order + 1)
    a[0] = 1.0
    err = r[0]
    ks = np.zeros(order)
    for i in range(1, order + 1):
        if err <= 0:
            break
        acc = r[i] + np.dot(a[1:i], r[i - 1:0:-1])
        k = -acc / err
        k = max(-0.9999, min(0.9999, k))
        ks[i - 1] = k
        a[1:i + 1] = a[1:i + 1] + k * a[i - 1::-1][:i]
        a[i] = k
        err *= 1.0 - k * k
    return ks


def _lar(k):
    k = np.clip(k, -0.9999, 0.9999)
    return np.log((1.0 - k) / (1.0 + k))


_KVALS = [np.array(t, dtype=float) / 512.0 for t in KTABLES]
_KLARS = [_lar(t) for t in _KVALS]


def _toeplitz(r):
    p = len(r)
    i = np.arange(p)
    return r[np.abs(i[:, None] - i[None, :])]


def _predErr(kidx, R):
    """Prediction error a^T R a of the quantised coefficients."""
    a = stepUp([_KVALS[i][v] for i, v in enumerate(kidx)])
    return float(a @ R[:len(a), :len(a)] @ a)


def quantiseK(k, r):
    """Nearest table entries in log-area ratio, then a coordinate search
    that minimises the prediction error of the quantised filter on r."""
    order = len(k)
    kidx = [int(np.argmin(np.abs(_KLARS[i] - _lar(k[i])))) for i in range(order)]
    R = _toeplitz(np.asarray(r[:order + 1], dtype=float))
    best = _predErr(kidx, R)
    for _ in range(3):
        changed = False
        for i in range(order):
            top = len(KTABLES[i]) - 1
            for cand in (kidx[i] - 1, kidx[i] + 1, kidx[i] - 2, kidx[i] + 2):
                if 0 <= cand <= top:
                    trial = list(kidx)
                    trial[i] = cand
                    v = _predErr(trial, R)
                    if v < best * (1 - 1e-9):
                        best, kidx, changed = v, trial, True
        if not changed:
            break
    return kidx, best


def _autocorr(seg, order):
    n = len(seg)
    return np.array([np.dot(seg[:n - i], seg[i:]) for i in range(order + 1)])


def pitchIndex(period):
    """Pitch index whose period (value + 1) is nearest in log terms (§12.4)."""
    per = np.array(PITCH[1:], dtype=float) + 1.0
    return 1 + int(np.argmin(np.abs(np.log(per) - math.log(period))))


# ---------------------------------------------------------------------------
# Pitch and voicing

def _butter(kind, hz, order=4):
    from scipy.signal import butter
    return butter(order, hz / (RATE / 2), btype=kind, output="sos")


def _yin(x, hop=40, win=240, minLag=14, maxLag=165):
    """CMNDF (de Cheveigne and Kawahara's YIN) at each hop centre.

    Returns centres, the CMNDF array (hops x lags) and each hop's RMS.
    """
    # The difference at lag t covers [c - lead, c - lead + win + t]: centred
    # on c for t = 80, within 5 ms of it for t = 40 to 160
    half = win // 2 + 40
    pad = np.concatenate([np.zeros(half + 1), x, np.zeros(half + maxLag + win + 1)])
    centres = np.arange(0, len(x), hop)
    nfft = 1024
    lags = np.arange(maxLag + 1)
    out = np.ones((len(centres), maxLag + 1))
    rms = np.zeros(len(centres))
    for h, c in enumerate(centres):
        a = c + half + 1 - half
        seg = pad[a:a + win + maxLag]
        w = seg[:win]
        rms[h] = math.sqrt(np.dot(w, w) / win)
        X = np.fft.rfft(seg, nfft)
        W = np.fft.rfft(w, nfft)
        r = np.fft.irfft(np.conj(W) * X, nfft)[:maxLag + 1]
        cs = np.concatenate([[0.0], np.cumsum(seg * seg)])
        e0 = cs[win]
        et = cs[lags + win] - cs[lags]
        d = e0 + et - 2 * r
        d[0] = 0.0
        cm = np.cumsum(d[1:])
        dn = np.ones(maxLag + 1)
        with np.errstate(divide="ignore", invalid="ignore"):
            dn[1:] = np.where(cm > 0, d[1:] * lags[1:] / cm, 1.0)
        out[h] = dn
    return centres, out, rms


def _candidates(dn, minLag=15, maxLag=161, n=5):
    """Local minima of a CMNDF row as (lag, value), parabolic refinement."""
    c = []
    for t in range(max(minLag, 2), min(maxLag, len(dn) - 2) + 1):
        if dn[t] <= dn[t - 1] and dn[t] < dn[t + 1]:
            a, bb, cc = dn[t - 1], dn[t], dn[t + 1]
            den = a - 2 * bb + cc
            off = 0.5 * (a - cc) / den if den > 0 else 0.0
            off = max(-0.5, min(0.5, off))
            c.append((t + off, bb - 0.25 * (a - cc) * off))
    c.sort(key=lambda z: z[1])
    return c[:n]


def trackPitch(x, opts):
    """Voicing and period at 5 ms hops: YIN candidates and a Viterbi path.

    Costs: the CMNDF value of a candidate, an unvoiced cost at the voicing
    threshold, a log-period jump cost between hops (an octave jump costs a
    lot), a voicing switch cost, and a mild prior towards the utterance's
    median period. The voicing threshold also takes the hop's spectral
    tilt and zero-crossing rate into account (_tiltAndCrossings), weighted by
    voicingEvidence. Returns (centres, period or 0 per hop).
    """
    xl = _sosfiltfilt(_butter("lowpass", 1000.0), x)
    centres, dn, _ = _yin(xl)
    # Full-band and low-band RMS over centred 30 ms windows
    def winRms(v):
        cs = np.concatenate([[0.0], np.cumsum(np.concatenate([np.zeros(120), v, np.zeros(120)]) ** 2)])
        return np.sqrt((cs[centres + 240] - cs[centres]) / 240.0)
    rms = winRms(x)
    rmsLow = winRms(xl)
    loud = np.percentile(rms[rms > 0], 95) if np.any(rms > 0) else 1.0
    cands = [_candidates(row) for row in dn]
    # Utterance median from confident hops, by the first dip below 0.15
    firsts = []
    for h, row in enumerate(dn):
        if rms[h] < loud * 0.03:
            continue
        for t in range(15, 162):
            if row[t] < 0.15 and row[t] <= row[t + 1]:
                firsts.append(t)
                break
    med = float(np.median(firsts)) if firsts else 80.0
    thr = opts["voicingThreshold"]
    # Cost weights: CMNDF scale, per-octave jump, voicing switch, prior per
    # octave beyond 0.6 octave from the median, subharmonic penalty
    alpha, jump, switch, prior, free, subh = 10.0, 12.0, 4.0, 5.0, 0.6, 4.0
    nh = len(centres)
    tilt, zc = _tiltAndCrossings(x, centres)
    ev = opts["voicingEvidence"]
    # States per hop: candidates then unvoiced (lag 0)
    states = []
    costs = []
    for h in range(nh):
        lev = rms[h] / loud if loud > 0 else 0.0
        # Voicing threshold: higher where the sound is loud and low-pass
        # (vowels, nasals), lower where it is mostly above 1 kHz (fricatives)
        lfr = rmsLow[h] / rms[h] if rms[h] > 0 else 0.0
        levDb = 20 * math.log10(max(lev, 1e-6))
        th = (thr + 0.35 * min(1.0, max(0.0, (lfr - 0.6) / 0.3)) * min(1.0, max(0.0, (levDb + 30) / 20))
              - 0.15 * min(1.0, max(0.0, (0.5 - lfr) / 0.3)))
        # Tilt and zero crossings: about +0.06 for a steep low-pass frame,
        # -0.075 for a fricative at 80 crossings per 10 ms, at weight 1
        th += ev * (tilt[h] - (zc[h] - 30.0) / 10.0) / 66.7
        st = [lag for lag, _ in cands[h]] + [0.0]
        cst = []
        for lag, val in cands[h]:
            c = alpha * val
            dev = abs(math.log2(lag / med))
            c += prior * max(0.0, dev - free)
            # A subharmonic: a clearly periodic candidate near lag/2 or lag/3 is as good
            for l2, v2 in cands[h]:
                if l2 >= 15 and v2 < 0.15 and v2 <= val + 0.01 and any(abs(lag / l2 - m) < 0.08 * m for m in (2, 3)):
                    c += subh
                    break
            if lev < 0.02:
                c += 4.0                 # far below the speech: favour unvoiced
            cst.append(c)
        cst.append(alpha * th)
        states.append(st)
        costs.append(np.array(cst))
    # Each state also carries the last voiced lag on its best path and the
    # hops since, so that a short unvoiced gap cannot be used to change octave
    # for less than a jump costs (an approximation to the full search)
    acc = costs[0].copy()
    last = [(lag, 0) for lag in states[0]]
    back = []
    for h in range(1, nh):
        prev = states[h - 1]
        curS = states[h]
        newAcc = np.empty(len(curS))
        bp = np.empty(len(curS), dtype=int)
        newLast = []
        for j, lj in enumerate(curS):
            best = None
            bi = 0
            for i, li in enumerate(prev):
                if li > 0 and lj > 0:
                    t = jump * abs(math.log2(lj / li))
                elif li > 0 or lj > 0:
                    t = switch
                    ll, gap = last[i]
                    if lj > 0 and ll > 0 and gap < 8:
                        t += jump * abs(math.log2(lj / ll)) * (1.0 - gap / 8.0)
                else:
                    t = 0.0
                v = acc[i] + t
                if best is None or v < best:
                    best, bi = v, i
            newAcc[j] = best + costs[h][j]
            bp[j] = bi
            newLast.append((lj, 0) if lj > 0 else (last[bi][0], last[bi][1] + 1))
        acc = newAcc
        last = newLast
        back.append(bp)
    j = int(np.argmin(acc))
    path = [j]
    for h in range(nh - 2, -1, -1):
        j = int(back[h][j])
        path.append(j)
    path.reverse()
    periods = np.array([states[h][path[h]] for h in range(nh)])
    return centres, periods


def _tiltAndCrossings(x, centres, win=240):
    """Voicing evidence besides periodicity, over a centred Hamming window
    at each hop: a tilt score from r1/r0 of the signal pre-emphasised by
    0.99 (-2 for r1/r0 <= -0.5, -1 below 0.25, 1 below 0.5, 2 below 0.75,
    else 4), and the zero crossings of the signal per 10 ms."""
    xp = np.concatenate([x[:1], x[1:] - 0.99 * x[:-1]])
    padp = np.concatenate([np.zeros(win), xp, np.zeros(win)])
    pad = np.concatenate([np.zeros(win), x, np.zeros(win)])
    ham = np.hamming(win)
    tilt = np.zeros(len(centres))
    zc = np.zeros(len(centres))
    for h, c in enumerate(centres):
        a = c + win - win // 2
        s = padp[a:a + win] * ham
        r0 = float(s @ s)
        q = float(s[:-1] @ s[1:]) / r0 if r0 > 0 else 0.0
        tilt[h] = -2 if q <= -0.5 else -1 if q < 0.25 else 1 if q < 0.5 else 2 if q < 0.75 else 4
        sg = np.sign(pad[a:a + win])
        zc[h] = np.sum(sg[:-1] * sg[1:] < 0) * 80.0 / win
    return tilt, zc


def _sosfiltfilt(sos, x):
    from scipy.signal import sosfiltfilt
    if len(x) < 64:
        return x.copy()
    return sosfiltfilt(sos, x)


# ---------------------------------------------------------------------------
# Encoder (§12.6)

DEFAULTS = {
    "targetRms": 20.0,       # RMS of the output over sounding frames, in codes
    "targetPeak": None,      # if set, scale so the loudest output peak is this instead
    "peakLimit": 120,        # no output code beyond this (DAC range is +-127)
    "repeats": True,         # use repeat frames
    "repeatLambda": 70.0,    # bits a repeat run may trade per dB of prediction error
    "gateDb": 45.0,          # frames this far below the loudest are silence
    "preEmphasis": "chirp",  # voiced analysis pre-emphasis: "chirp" or a coefficient (§12.3)
    "window": 240,           # analysis window, samples (30 ms)
    "lagWindowHz": 60.0,     # Gaussian lag window bandwidth
    "voicingThreshold": 0.3, # CMNDF level at which voiced and unvoiced cost the same
    "voicingEvidence": 4.0,  # weight of tilt and zero crossings in the voicing threshold
    "iterations": 5,         # closed-loop energy passes
    "fadeUnvoiced": True,    # fade hiss before a stop or silence (§12.2)
}


_HANN = np.hanning(203)[1:-1]


def _frameRms(x, centre, half=100):
    """RMS around a centre: Hann-weighted over 25 ms by default."""
    a = centre - half
    bb = centre + half + 1
    w = _HANN if half == 100 else np.hanning(2 * half + 3)[1:-1]
    lo, hi = max(0, a), min(len(x), bb)
    if hi <= lo:
        return 0.0
    seg = np.asarray(x[lo:hi], dtype=float)
    ww = w[lo - a:hi - a]
    return math.sqrt(float(np.dot(ww, seg * seg)) / float(np.sum(w)))


def _resample(samples, rate):
    if rate == RATE:
        return np.asarray(samples, dtype=float)
    from fractions import Fraction
    from scipy.signal import resample_poly
    fr = Fraction(RATE, int(rate)).limit_denominator(1000)
    return resample_poly(np.asarray(samples, dtype=float), fr.numerator, fr.denominator)


def _gainOf(kvals, period, nfft=4096):
    """Output RMS in codes for energy value 1 (§12.3: code = E x h / 128)."""
    a = stepUp(kvals)
    H = 1.0 / np.abs(np.fft.rfft(a, nfft)) ** 2
    if period == 0:
        # Flat excitation of RMS 64: power = 64^2 * mean |H|^2
        return 64.0 * math.sqrt(float(np.mean(np.concatenate([H, H[1:-1]])))) / 128.0
    c = np.array(CHIRP[:21], dtype=float)
    C = np.abs(np.fft.rfft(c, nfft)) ** 2
    full = np.concatenate([C * H, (C * H)[1:-1]])
    energy = float(np.mean(full))          # = sum over one chirp of (c*h)^2
    return math.sqrt(energy / period) / 128.0


def _energyIndex(value):
    """Energy index 1-14 nearest to a wanted energy value, in dB."""
    if value <= 0:
        return 1
    best, bi = None, 1
    for i in range(1, 15):
        d = abs(math.log(ENERGY[i] / value))
        if best is None or d < best:
            best, bi = d, i
    return bi


def chirpInverse(order=4):
    """The FIR that whitens the chirp: the order-`order` LPC inverse of CHIRP."""
    c = np.array(CHIRP[:21], dtype=float)
    r = np.array([np.dot(c[:len(c) - i], c[i:]) for i in range(order + 1)])
    return stepUp(levinson(r, order))


def preEmphasisFilter(pe):
    """FIR coefficients for the preEmphasis option: "chirp", or μ for 1 - μz^-1."""
    if pe == "chirp":
        return chirpInverse(4)
    return np.array([1.0, -float(pe)])


def analyse(samples, rate=8000, **options):
    """Analysis without level setting: per-frame features (for encode and tests)."""
    opts = dict(DEFAULTS)
    opts.update(options)
    x = _resample(samples, rate)
    x = _sosfiltfilt(_butter("highpass", 60.0, 2), x)
    n = len(x)
    # Level envelope (10 ms blocks every 2.5 ms) to find the onset and offset
    blk = np.array([_frameRms(x, c, 40) for c in range(0, n, 20)])
    if not np.any(blk > 0):
        return None
    frameLev = np.array([_frameRms(x, c, 100) for c in range(100, max(n, 101), 200)])
    loud = float(np.max(frameLev)) if len(frameLev) else float(np.max(blk))
    floor = float(np.percentile(blk, 5))
    gate = max(loud * 10 ** (-opts["gateDb"] / 20.0), floor * 2.0)
    above = np.nonzero(blk > gate)[0]
    if len(above) == 0:
        return None
    onset = int(above[0]) * 20
    offset = int(above[-1]) * 20
    c0 = onset + 22             # the sound starts at output sample 203 (§8.5)
    nfr = max(1, (offset + 100 - c0) // FRAME + 1)
    centres = [c0 + FRAME * j for j in range(nfr)]

    hops, periods = trackPitch(x, opts)
    win = opts["window"]
    ham = np.hamming(win)
    lagw = np.exp(-0.5 * (2 * math.pi * opts["lagWindowHz"] * np.arange(11) / RATE) ** 2)
    pad = np.concatenate([np.zeros(win), x, np.zeros(win + FRAME)])
    from scipy.signal import lfilter
    xp = lfilter(preEmphasisFilter(opts["preEmphasis"]), [1.0], x)
    padp = np.concatenate([np.zeros(win), xp, np.zeros(win + FRAME)])

    feats = []
    for j, c in enumerate(centres):
        lev = _frameRms(x, c)
        # Voicing and period from the 3 hops nearest the centre
        h0 = int(round(c / 40.0))
        hs = [h for h in (h0 - 1, h0, h0 + 1) if 0 <= h < len(periods)]
        vp = [periods[h] for h in hs if periods[h] > 0]
        voiced = len(vp) * 2 > len(hs)
        period = float(np.median(vp)) if voiced else 0.0
        # Spectrum: the first frame's window starts at the onset
        ac = c if j > 0 else max(c, onset + win // 2 - 40)
        a0 = ac - win // 2 + win
        segV = padp[a0:a0 + win] * ham
        segU = pad[a0:a0 + win] * ham
        rv = _autocorr(segV, 10) * lagw
        ru = _autocorr(segU, 10) * lagw
        rv[0] *= 1.0001
        ru[0] *= 1.0001
        feats.append({"centre": c, "level": lev, "voiced": voiced, "period": period,
                      "rv": rv, "ru": ru, "gated": lev <= gate})
    # Octave fix: a voiced frame about 2x or 0.5x its voiced neighbours
    for j, f in enumerate(feats):
        if not f["voiced"]:
            continue
        nb = [feats[i]["period"] for i in (j - 2, j - 1, j + 1, j + 2)
              if 0 <= i < len(feats) and feats[i]["voiced"]]
        if len(nb) >= 2:
            m = float(np.median(nb))
            ratio = f["period"] / m
            if ratio > 1.7 and 16 <= f["period"] / 2:
                f["period"] /= 2.0
            elif ratio < 0.6 and f["period"] * 2 <= 160:
                f["period"] *= 2.0
    return {"x": x, "centres": centres, "feats": feats, "gate": gate,
            "loud": loud, "onset": onset, "offset": offset, "c0": c0}


BITS_FULL = {True: 50, False: 29}     # full voiced and unvoiced frames
BITS_REPEAT = 11
MAX_RUN = 8                           # frames per full frame and its repeats


def _kindsAndK(feats, opts):
    """Quantise K, pick voicing, silence and repeats, before level setting.

    Repeats are chosen within each run of sounding frames of one voicing, by
    dynamic programming over segments (a full frame and its repeats) on
    bits + repeatLambda x distortion (_segment)."""
    info = []
    for f in feats:
        if f["gated"]:
            info.append(None)
            continue
        voiced = f["voiced"]
        r = np.asarray(f["rv"] if voiced else f["ru"], dtype=float)
        order = 10 if voiced else 4
        kidx, err = quantiseK(levinson(r, order), r)
        pidx = pitchIndex(max(16.0, min(160.0, f["period"]))) if voiced else 0
        info.append({"voiced": voiced, "r": r, "order": order, "k": kidx, "err": err,
                     "R": _toeplitz(r[:order + 1]), "pitch": pidx})
    maxRun = MAX_RUN if opts["repeats"] else 1
    plan = []
    j = 0
    while j < len(info):
        if info[j] is None:
            plan.append({"kind": "silence"})
            j += 1
            continue
        a = j
        while j < len(info) and info[j] is not None and info[j]["voiced"] == info[a]["voiced"]:
            j += 1
        plan += _repeatRun(info[a:j], opts["repeatLambda"], maxRun)
    # Trim trailing silence
    while plan and plan[-1]["kind"] == "silence":
        plan.pop()
    return plan


def _segment(run, a, b, lam):
    """(cost, K) of frames a..b-1 of a run sent as one full frame and repeats.

    A frame's distortion is how much its prediction error rises, in dB, under
    the segment's K instead of its own. The segment's K is its first frame's,
    or those quantised from the segment's pooled normalised autocorrelation,
    whichever costs less."""
    cands = [run[a]["k"]]
    if b - a > 1:
        pooled = sum(run[i]["r"] / run[i]["r"][0] for i in range(a, b))
        pooled[0] *= 1.0001
        cands.append(quantiseK(levinson(pooled, run[a]["order"]), pooled)[0])
    best = None
    for k in cands:
        d = sum(10 * math.log10(max(_predErr(k, run[i]["R"]), 1e-30) / max(run[i]["err"], 1e-30))
                for i in range(a, b))
        c = BITS_FULL[run[a]["voiced"]] + BITS_REPEAT * (b - a - 1) + lam * d
        if best is None or c < best[0]:
            best = (c, k)
    return best


def _repeatRun(run, lam, maxRun):
    """Plan entries for one run of same-voicing frames: the cheapest split
    into segments of at most maxRun frames."""
    n = len(run)
    cost = [0.0] + [math.inf] * n
    back = [0] * (n + 1)
    kOf = [None] * (n + 1)
    for e in range(1, n + 1):
        for st in range(max(0, e - maxRun), e):
            c, k = _segment(run, st, e, lam)
            if cost[st] + c < cost[e]:
                cost[e], back[e], kOf[e] = cost[st] + c, st, k
    out = [None] * n
    e = n
    while e > 0:
        st = back[e]
        for i in range(st, e):
            kind = ("voiced" if run[i]["voiced"] else "unvoiced") if i == st else "repeat"
            out[i] = {"kind": kind, "k": list(kOf[e]), "pitch": run[i]["pitch"]}
        e = st
    return out


def _effectiveK(plan):
    """The K values in force at each frame (for the open-loop gain)."""
    out = []
    last = [0] * 10
    lastVoiced = True
    for p in plan:
        if p["kind"] in ("voiced", "unvoiced"):
            last = list(p["k"]) + [None] * (10 - len(p["k"]))
            lastVoiced = p["kind"] == "voiced"
        if p["kind"] == "silence":
            out.append(None)
            continue
        vals = []
        n = 10 if (lastVoiced and p["pitch"] != 0) else 4
        for i in range(n):
            vals.append(KTABLES[i][last[i]] / 512.0)
        out.append(vals)
    return out


def _toFrames(plan, energies):
    """Frames from the plan. A repeat whose K differ from those the chip
    holds (because a frame before it became silence) is sent in full."""
    frames = []
    stored = None
    for p, e in zip(plan, energies):
        if p["kind"] == "silence" or e == 0:
            frames.append({"energy": 0})
            continue
        k = [int(v) for v in p["k"]]
        voiced = p["pitch"] != 0
        if p["kind"] == "repeat" and stored == (k, voiced):
            frames.append({"energy": int(e), "repeat": 1, "pitch": int(p["pitch"])})
        else:
            frames.append({"energy": int(e), "repeat": 0, "pitch": int(p["pitch"]), "k": k})
            stored = (k, voiced)
    # Leading silence would leave the start to the previous utterance (§12.2)
    while frames and frames[0]["energy"] == 0:
        frames.pop(0)
    # Trailing silence adds nothing before the stop
    while frames and frames[-1]["energy"] == 0:
        frames.pop()
    return frames


def _guardK510(frames):
    """No silence before the first full voiced frame (§12.2, "K5-K10 before
    the first voiced frame"): until then the stored K5-K10 indices are the
    previous utterance's, and a silence frame would end their zeroing. Such
    silence becomes an unvoiced repeat at energy 1, which keeps it."""
    out = []
    voicedSeen = False
    for f in frames:
        if f["energy"] == 0 and not voicedSeen:
            f = {"energy": 1, "repeat": 1, "pitch": 0}
        elif f["energy"] not in (0, 15) and not f.get("repeat", 0) and f["pitch"]:
            voicedSeen = True
        out.append(f)
    return out


def _addFades(frames, opts):
    """Fade unvoiced sound before silence and before the stop (§12.2), then
    apply _guardK510 and add the stop."""
    if not opts["fadeUnvoiced"]:
        return _guardK510(frames) + [{"energy": 15}]
    out = []
    i = 0
    nfr = len(frames)
    while i < nfr:
        f = frames[i]
        out.append(f)
        unv = f["energy"] not in (0, 15) and f["pitch"] == 0
        if unv and (i + 1 == nfr or frames[i + 1]["energy"] == 0):
            # Replace following silence frames (or append, at the end)
            e = f["energy"]
            j = i + 1
            while ENERGY[e] > 2:
                e = _energyIndex(ENERGY[e] / 3.0)
                fade = {"energy": e, "repeat": 1, "pitch": 0}
                if j < nfr and frames[j]["energy"] == 0:
                    out.append(fade)
                    j += 1
                elif j >= nfr:
                    out.append(fade)
                    j += 1
                else:
                    break
            i = max(i + 1, min(j, nfr))
            continue
        i += 1
    return _guardK510(out) + [{"energy": 15}]


def _outLevels(codes, nfr, half=100):
    """Output level (as _frameRms) and peak around each frame's centre."""
    out = np.zeros(nfr)
    pk = np.zeros(nfr, dtype=int)
    c = codes.astype(float)
    for j in range(nfr):
        m = REACHED + FRAME * j
        out[j] = _frameRms(c, m)
        a, bb = max(0, m - half), min(len(c), m + half)
        if bb > a:
            pk[j] = int(np.max(np.abs(codes[a:bb])))
    return out, pk


def _setLevels(plan, feats, gain, opts, stats=None):
    """Energy indices: open-loop estimate, then closed loop through §8.9."""
    nfr = len(plan)
    effK = _effectiveK(plan)
    target = np.array([feats[j]["level"] * gain for j in range(nfr)])
    energies = []
    for j, p in enumerate(plan):
        if p["kind"] == "silence":
            energies.append(0)
            continue
        per = PITCH[p["pitch"]] + 1 if p["pitch"] else 0
        g = _gainOf(effK[j], per)
        energies.append(_energyIndex(target[j] / g if g > 0 else 1.0))
    energies = np.array(energies)
    sounding = np.array([p["kind"] != "silence" for p in plan])
    best = None
    for it in range(opts["iterations"]):
        frames = _addFades(_toFrames(plan, energies), opts)
        codes = _synth(frames)
        lev, pk = _outLevels(codes, nfr)
        with np.errstate(divide="ignore"):
            err = 20 * np.log10(np.maximum(target, 1e-6) / np.maximum(lev, 1e-6))
        score = float(np.mean(np.abs(err[sounding]))) if np.any(sounding) else 0.0
        if best is None or score < best[0]:
            best = (score, energies.copy())
        if it == opts["iterations"] - 1:
            break
        newE = energies.copy()
        for j in range(nfr):
            if not sounding[j] or energies[j] == 0 or abs(err[j]) < 1.5:
                continue
            want = ENERGY[energies[j]] * 10 ** (0.7 * err[j] / 20.0)
            newE[j] = _energyIndex(want)
        if np.array_equal(newE, energies):
            break
        energies = newE
    energies = best[1].copy()
    # Frames that are far too loud even at index 1 become silence
    frames = _addFades(_toFrames(plan, energies), opts)
    lev, pk = _outLevels(_synth(frames), nfr)
    silenced = 0
    for j in range(1, nfr):
        if sounding[j] and energies[j] == 1 and lev[j] > target[j] * 4.0:
            energies[j] = 0
            silenced += 1
    beforeLimit = energies.copy()
    # Peak and wrap limiting
    for _ in range(30):
        frames = _addFades(_toFrames(plan, energies), opts)
        st = {}
        codes = _synth(frames, st)
        lev, pk = _outLevels(codes, nfr, 110)
        over = [j for j in range(nfr) if sounding[j] and pk[j] > opts["peakLimit"]]
        if not over and st["wraps"] == 0 and int(np.max(np.abs(codes))) <= opts["peakLimit"]:
            break
        if not over:
            # A peak outside every frame's window: lower the loudest frame
            over = [int(np.argmax(np.where(sounding, lev, -1)))]
        for j in over:
            if energies[j] > 1:
                drop = max(1, int(math.ceil(20 * math.log10(max(pk[j], 1) / opts["peakLimit"]) / 3.0)))
                energies[j] = max(1, energies[j] - drop)
            elif j > 0:
                energies[j] = 0
    # Safety net: lower everything until no code exceeds the limit and nothing wraps
    for _ in range(14):
        st = {}
        codes = _synth(_addFades(_toFrames(plan, energies), opts), st)
        if st["wraps"] == 0 and int(np.max(np.abs(codes))) <= opts["peakLimit"]:
            break
        energies = np.where(energies > 1, energies - 1, energies)
    if stats is not None:
        stats["silenced"] = silenced + int(np.sum((energies == 0) & (beforeLimit > 0)))
        stats["limited"] = int(np.sum((energies < beforeLimit) & (energies > 0)))
        stats["sounding"] = int(np.sum(sounding))
    return energies


def encodeDetailed(samples, rate=8000, **options):
    """encode(), also returning its analysis: (frames, info).

    info has the filtered 8 kHz signal "x", the frame centres "centres"
    (input samples), "c0", the level "gain" (codes per unit input RMS) and
    the per-frame "plan". Output sample REACHED + 200*j of synthesize()
    corresponds to input sample centres[j].
    """
    opts = dict(DEFAULTS)
    for k in options:
        if k not in DEFAULTS:
            raise TypeError("unknown option %r" % k)
    opts.update(options)
    an = analyse(samples, rate, **opts)
    if an is None:
        return [{"energy": 15}], None
    feats = an["feats"]
    plan = _kindsAndK(feats, opts)
    if not plan:
        return [{"energy": 15}], None
    lead = 0
    while plan[lead]["kind"] == "silence":
        lead += 1
    # Drop a faint opening sound with no voiced frame that is followed by
    # silence: kept, it would need energy-1 hiss through the pause (§12.2)
    loudest = max(f["level"] for f, p in zip(feats, plan) if p["kind"] != "silence")
    dropped = 0
    while True:
        end = lead
        while end < len(plan) and plan[end]["kind"] != "silence":
            end += 1
        seg = range(lead, end)
        if (end >= len(plan) or any(plan[j]["kind"] == "voiced" or
                                    (plan[j]["kind"] == "repeat" and plan[j]["pitch"]) for j in seg)
                or max(feats[j]["level"] for j in seg) > loudest * 10 ** (-36 / 20.0)):
            break
        nxt = end
        while nxt < len(plan) and plan[nxt]["kind"] == "silence":
            nxt += 1
        if nxt >= len(plan):
            break
        dropped += nxt - lead
        lead = nxt
        # The new first frame must be full: re-plan a repeat as a full frame
        if plan[lead]["kind"] == "repeat":
            plan[lead] = dict(plan[lead], kind="voiced" if plan[lead]["pitch"] else "unvoiced")
    plan = plan[lead:]
    feats = feats[lead:lead + len(plan)]
    snd = [f["level"] for f, p in zip(feats, plan) if p["kind"] != "silence"]
    inRms = math.sqrt(float(np.mean(np.square(snd)))) if snd else 1.0
    if opts["targetPeak"] is not None:
        # Scale by RMS first, then rescale so the loudest peak meets the target
        o2 = dict(opts)
        o2["peakLimit"] = 511
        gain = 20.0 / inRms
        e = _setLevels(plan, feats, gain, o2)
        codes = _synth(_addFades(_toFrames(plan, e), o2))
        pk = max(1, int(np.max(np.abs(codes))))
        gain *= opts["targetPeak"] / pk
        opts["peakLimit"] = min(opts["peakLimit"], opts["targetPeak"])
    else:
        gain = opts["targetRms"] / inRms
    stats = {"droppedOpening": dropped}
    energies = _setLevels(plan, feats, gain, opts, stats)
    frames = _addFades(_toFrames(plan, energies), opts)
    info = {"x": an["x"], "centres": an["centres"][lead:lead + len(plan)],
            "c0": an["centres"][lead],
            "gain": gain, "plan": plan, "feats": feats, "stats": stats}
    return frames, info


def encode(samples, rate=8000, **options):
    """Encode speech (float samples in -1..1) into a list of frames ending in a stop.

    Options (defaults in DEFAULTS): targetRms, targetPeak, peakLimit, repeats,
    repeatLambda, gateDb, preEmphasis, window, lagWindowHz, voicingThreshold,
    voicingEvidence, iterations, fadeUnvoiced.
    """
    return encodeDetailed(samples, rate, **options)[0]


# ---------------------------------------------------------------------------
# Packaging (§12.7-§12.9)

def bitReverse(b):
    """Reverse the bits of a byte."""
    return int("{:08b}".format(b & 0xFF)[::-1], 2)


def _ensureStop(frames):
    frames = list(frames)
    for i, f in enumerate(frames):
        if f["energy"] == 15:
            return frames[:i + 1]
    return frames + [{"energy": 15}]


def speakExternalStream(frames):
    """Bytes for Speak External: at least 9, by silence frames before the stop (§12.9)."""
    frames = _ensureStop(frames)
    body = frames[:-1]
    while True:
        data = pack(body + [{"energy": 15}])
        if len(data) >= 9:
            return data
        body = body + [{"energy": 0}]


def acornPhrom(words, strings=("(C)2026 speak5220", "Phase 11 PHROM", "1.00"), asciiMap=None):
    """A 16 KB Acorn PHROM image (§12.8).

    words: list of (name, frames), numbered from 127 in order. asciiMap
    optionally maps characters (or their codes, 32-126) to word names. ASCII
    entries that map to nothing point to a shared one-byte stop code.
    Returns (image, table) with one table entry per word.
    """
    size = 16384
    img = bytearray(size)
    nw = len(words)
    npt = 95 + nw
    last = 32 + npt - 1
    tableEnd = 2 * last + 2                   # first byte after the last pointer
    if tableEnd + 1 > size:
        raise ValueError("too many words for one PHROM")
    hdr = bytearray(64)
    hdr[0], hdr[1] = 0x00, 0xFF
    text = b"".join(s.encode("ascii") + b"\x00" for s in strings)
    if len(text) > 54:
        raise ValueError("header strings too long (54 bytes with their nulls)")
    hdr[2:2 + len(text)] = text
    hdr[56], hdr[57] = 95 & 0xFF, 95 >> 8
    hdr[58], hdr[59] = npt & 0xFF, npt >> 8
    hdr[60:64] = bytes([0x00, 0x02, 0x00, 0x02])
    img[0:64] = bytes(bitReverse(v) for v in hdr)
    img[tableEnd] = 0xFF
    pos = tableEnd + 1
    ptr = {}
    table = []
    for i, (name, frames) in enumerate(words):
        frames = _ensureStop(frames)
        data = pack(frames)
        nm = name.encode("ascii")
        if pos + len(nm) + len(data) > size:
            raise ValueError("words do not fit in 16 KB (at %r)" % name)
        for j, ch in enumerate(nm):
            img[pos + len(nm) - 1 - j] = bitReverse((ch << 1) & 0xFF)
        pos += len(nm)
        off = pos
        img[off:off + len(data)] = data
        pos += len(data)
        num = 127 + i
        ptr[num] = off
        table.append({"number": num, "name": name, "offset": off, "bytes": len(data),
                      "frames": len(frames)})
    # A lone stop code after the last word, for ASCII entries with no word
    if pos + 1 > size:
        raise ValueError("words do not fit in 16 KB")
    nullWord = pos
    img[pos] = pack([{"energy": 15}])[0]
    byName = {name: 127 + i for i, (name, _) in enumerate(words)}
    for c in range(32, 127):
        ptr[c] = nullWord
    for ch, wname in (asciiMap or {}).items():
        c = ord(ch) if isinstance(ch, str) else int(ch)
        if not 32 <= c <= 126:
            raise ValueError("ASCII entry out of range: %r" % (ch,))
        if wname not in byName:
            raise ValueError("ASCII entry %r names no word: %r" % (ch, wname))
        ptr[c] = ptr[byName[wname]]
    for num in range(32, last + 1):
        v = ptr[num] & 0x3FFF
        img[2 * num] = v & 0xFF
        img[2 * num + 1] = v >> 8
    return bytes(img), table


def tiIndexed(words):
    """A 16 KB image in TI's indexed format (§12.8).

    Byte 0 is 0, byte 1 the word count, word i (1..n) has its pointer at
    byte 2i, low byte first. Each word ends with one spare byte after its
    stop code's byte. Returns (image, table); words are numbered from 1.
    """
    size = 16384
    n = len(words)
    if n > 255:
        raise ValueError("at most 255 words")
    img = bytearray(size)
    img[0] = 0
    img[1] = n
    pos = 2 * (n + 1)
    table = []
    for i, (name, frames) in enumerate(words):
        frames = _ensureStop(frames)
        data = pack(frames)
        if pos + len(data) + 1 > size:
            raise ValueError("words do not fit in 16 KB (at %r)" % name)
        img[pos:pos + len(data)] = data
        img[2 * (i + 1)] = pos & 0xFF
        img[2 * (i + 1) + 1] = (pos >> 8) & 0x3F
        table.append({"number": i + 1, "name": name, "offset": pos, "bytes": len(data),
                      "frames": len(frames)})
        pos += len(data) + 1
    return bytes(img), table


# ---------------------------------------------------------------------------
# Command line

def framesCsv(frames):
    """CSV text: frame,kind,energy,repeat,pitch,k1..k10, blank where absent."""
    rows = ["frame,kind,energy,repeat,pitch," + ",".join("k%d" % i for i in range(1, 11))]
    for n, f in enumerate(frames):
        kind = frameKind(f)
        rep = str(f.get("repeat", "")) if kind not in ("silence", "stop") else ""
        pit = str(f["pitch"]) if kind not in ("silence", "stop") else ""
        ks = list(f.get("k", [])) if kind in ("voiced", "unvoiced") else []
        ks = [str(v) for v in ks] + [""] * (10 - len(ks))
        rows.append(",".join([str(n), kind, str(f["energy"]), rep, pit] + ks))
    return "\n".join(rows) + "\n"


def main(argv=None):
    import argparse
    import soundfile as sf
    ap = argparse.ArgumentParser(description="TMS5220 speech encoder")
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("encode", help="encode a sound file")
    e.add_argument("input")
    e.add_argument("output")
    e.add_argument("--csv")
    e.add_argument("--wav")
    e.add_argument("--rms", type=float, default=DEFAULTS["targetRms"],
                   help="target output RMS in codes (default %(default)s)")
    e.add_argument("--peak", type=float, default=None,
                   help="scale to this output peak instead of an RMS")
    e.add_argument("--no-repeat", action="store_true", help="no repeat frames")
    e.add_argument("--repeat-lambda", type=float, default=DEFAULTS["repeatLambda"],
                   help="bits a repeat run may trade per dB of prediction error; "
                        "lower gives fewer bits (default %(default)s)")
    e.add_argument("--pre-emphasis", default=DEFAULTS["preEmphasis"],
                   help='voiced analysis pre-emphasis: "chirp" or a coefficient '
                        "such as 0.9 (default %(default)s)")
    e.add_argument("--voicing-evidence", type=float, default=DEFAULTS["voicingEvidence"],
                   help="weight of tilt and zero crossings in the voicing decision, "
                        "0 for periodicity alone (default %(default)s)")
    e.add_argument("--voicing-threshold", type=float, default=DEFAULTS["voicingThreshold"],
                   help="CMNDF level at which voiced and unvoiced cost the same; "
                        "higher gives more voiced frames (default %(default)s)")
    args = ap.parse_args(argv)
    pe = args.pre_emphasis
    if pe != "chirp":
        try:
            pe = float(pe)
        except ValueError:
            ap.error('--pre-emphasis must be "chirp" or a number')
    x, rate = sf.read(args.input, dtype="float64", always_2d=True)
    x = x.mean(axis=1)
    frames = encode(x, rate, targetRms=args.rms, targetPeak=args.peak,
                    repeats=not args.no_repeat, repeatLambda=args.repeat_lambda,
                    preEmphasis=pe, voicingEvidence=args.voicing_evidence,
                    voicingThreshold=args.voicing_threshold)
    data = pack(frames)
    with open(args.output, "wb") as fh:
        fh.write(data)
    if args.csv:
        with open(args.csv, "w") as fh:
            fh.write(framesCsv(frames))
    if args.wav:
        codes = synthesize(frames)
        sf.write(args.wav, (codes * 64).astype(np.int16), RATE, subtype="PCM_16")
    nbits = sum(frameBits(f) for f in frames)
    print("%d frames, %d bytes, %.0f bit/s" % (len(frames), len(data),
                                              nbits / (len(frames) * 0.025)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
