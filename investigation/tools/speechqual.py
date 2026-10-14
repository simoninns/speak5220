#!/usr/bin/env python3
"""
    speechqual.py

    An objective measure of encoding quality: how close the TMS5220's
    speech, as the reference emulator speaks the encoded frames, is to the
    input it was encoded from.

    Each input's encoding, packed frames given with --bin, is spoken by
    emulator/tms5220emu (VSM Speak after a Reset, so independent of the
    encoder's own synthesis), aligned with the input by its level envelope,
    and compared with it:

      pesq     ITU-T P.862 narrowband, as P.862.1 MOS-LQO (1.0 bad to 4.5).
               P.862 was not validated for vocoders below 4 kbit/s, so read
               it as a ranking of encodings, not as a listener's MOS
      mcd      mel-cepstral distortion, dB: the spectral envelope, ignoring
               level and phase (coefficients 1-24 of 40 mel bands,
               100-3800 Hz). Computed from a filter bank, not SPTK's mcep,
               so not directly comparable with published figures
      tilt     level-normalised spectrum, output minus input, dB, in three
               bands: below 800 Hz, 800-2000 Hz, above 2000 Hz
      level    per 10 ms frame, output level minus input level, dB, after
               removing the file's median offset: the mean absolute error,
               and the correlation of the two level envelopes
      drop     % of the input's speech frames where the output is more than
               20 dB below the input (lost sound)
      pause    in the input's pauses (frames more than 45 dB below its
               loudest), output level minus input level, each relative to its
               own loudest frame, median, dB: positive means noisier pauses
      vuv      % of speech frames whose voicing differs (the same pitch
               tracker on both)
      gpe      gross pitch error, % of frames voiced in both with f0 more
               than 20 % apart
      f0       the rest of those frames: f0 RMS error in cents

    Speech frames are those within 35 dB of the input's loudest 10 ms frame.

    --anchor also scores a plain floating-point LPC vocoder of the same
    input (order 10, 25 ms frames, the K interpolated over 8 steps as on the
    chip, pulse or noise excitation, nothing quantised). It is not the
    encoder: it shows what an unquantised LPC-10 at the chip's frame rate
    scores on the same input, so the gap to it is what quantisation, the
    chip's excitation and the encoder's choices cost together.

    --ladder scores the steps from that anchor to the chip, one change at
    a time, to show where the quality goes (LADDER):

      anchor   the vocoder above
      q        + the chip's K, pitch and energy tables (K1-K4 if unvoiced)
      qc       + the chip's excitation: the chirp, pre-emphasis for voiced
               frames only and none undone, energy set from the input's RMS
      qci      the same frames through the reference model's timing,
               interpolation and excitation (tools/model.py) with a
               floating-point lattice
      enc      the encoder's frames through that floating-point model
      chip     the encoder's frames on the emulator (the main result)

    and, as a breakdown of q, the anchor with only the K (qk), pitch (qp)
    or energy (qe) table. q to qci use the anchor's analysis, not the
    encoder's, so qci to enc is what the encoder's own choices are worth
    against it, and enc to chip is the chip's integer arithmetic.

      python tools/speechqual.py IN.wav --bin IN.bin
      python tools/speechqual.py IN.wav IN2.flac --bin DIR    # DIR/IN.bin, DIR/IN2.bin
      python tools/speechqual.py --set tts --bin DIR --anchor --csv q.csv
      python tools/speechqual.py IN.wav --bin IN.bin --wav DIR    # aligned audio to listen to
      python tools/speechqual.py IN.wav --bin IN.bin --frames-csv f.csv

    --bin is a file of packed frames for one input, or a directory holding
    NAME.bin for each input NAME. --set takes the Phase 11 test inputs
    (tts, recordings or all).

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import math
import multiprocessing
import os
import subprocess
import sys
import tempfile

import numpy as np
import scipy.fft
import scipy.signal
import soundfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import lpc  # noqa: E402  (tools/lpc.py: the frame format)
import model  # noqa: E402  (tools/model.py: the chip's tables and timing for the ladder)

EMU = os.environ.get("TMS5220EMU", os.path.join(ROOT, "emulator", "tms5220emu"))
INPUTS = os.path.join(ROOT, "captures", "phase11", "inputs")

RATE = 8000
HOP = 80                # 10 ms analysis hop
WIN = 256               # 32 ms spectral window
NFFT = 256
SPEECH_DB = 35          # speech frames: within this of the input's loudest frame
DROP_DB = 20            # output this far below the input: lost sound
MEL_BANDS, MEL_LO, MEL_HI = 40, 100.0, 3800.0
CEPS = 24
TILT_EDGES = (800.0, 2000.0)
ENV_HOP = 20            # alignment envelope: 2.5 ms hop
REFINE, REFINE_STEP = 200, 5   # then ±25 ms in 0.625 ms steps, by spectrum
F0_WIN = 320            # pitch tracker: 40 ms window
F0_LAGS = (20, 160)     # 400 Hz down to 50 Hz
F0_VOICED = 0.5         # normalised correlation at which a frame is voiced
GROSS = 0.2


# ---- input and synthesis ------------------------------------------------

def readAudio(path):
    x, rate = soundfile.read(path, dtype="float64", always_2d=True)
    x = x.mean(axis=1)
    if rate != RATE:
        g = math.gcd(rate, RATE)
        x = scipy.signal.resample_poly(x, RATE // g, rate // g)
    return x


def emulate(data, tmp):
    """The chip's I/O codes for the packed frames spoken from VSM address 0
    after a Reset, from the first non-idle code to the last."""
    image = os.path.join(tmp, "image.bin")
    samples = os.path.join(tmp, "samples.txt")
    with open(image, "wb") as f:
        f.write(bytes(data))
    subprocess.run([EMU, "-s", samples, "-e", "none", "--image", image, "--addr", "0", "--reset"],
                   check=True)
    codes = np.loadtxt(samples, dtype=np.int64, ndmin=2)[:, 2]
    active = np.flatnonzero(codes != -1)
    if not len(active):
        return codes[:0]
    return codes[active[0]:active[-1] + 1]


# ---- analysis -----------------------------------------------------------

def frames(x, win, hop=HOP):
    """Frames centred on 0, hop, 2·hop, ... (zero-padded)."""
    n = len(x) // hop + 1
    pad = np.concatenate([np.zeros(win // 2), x, np.zeros(win)])
    idx = np.arange(n)[:, None] * hop + np.arange(win)[None, :]
    return pad[idx]


def melBank():
    def mel(f):
        return 2595 * np.log10(1 + f / 700.0)

    def imel(m):
        return 700 * (10 ** (m / 2595.0) - 1)
    edges = imel(np.linspace(mel(MEL_LO), mel(MEL_HI), MEL_BANDS + 2))
    freqs = np.arange(NFFT // 2 + 1) * RATE / NFFT
    fb = np.zeros((MEL_BANDS, len(freqs)))
    for i in range(MEL_BANDS):
        a, c, b = edges[i:i + 3]
        fb[i] = np.clip(np.minimum((freqs - a) / (c - a), (b - freqs) / (b - c)), 0, None)
    return fb, edges[1:-1]


FB, MEL_CENTRES = melBank()
HAMMING = np.hamming(WIN)


def levelDb(x):
    f = frames(x, WIN) * HAMMING
    return 10 * np.log10(np.sum(f * f, axis=1) / np.sum(HAMMING ** 2) + 1e-12)


def melLog(x):
    """Natural log of the mel band magnitudes, per frame."""
    p = np.abs(np.fft.rfft(frames(x, WIN) * HAMMING, NFFT)) ** 2
    return 0.5 * np.log(p @ FB.T + 1e-12)


def pitch(x):
    """(f0 Hz, voiced) per 10 ms frame: normalised cross-correlation of the
    signal low-passed at 1 kHz, preferring the shortest lag within 10 % of
    the best (against subharmonics)."""
    sos = scipy.signal.butter(4, 1000, fs=RATE, output="sos")
    y = scipy.signal.sosfiltfilt(sos, x)
    lo, hi = F0_LAGS
    f = frames(y, F0_WIN + hi)
    a = f[:, :F0_WIN]
    ea = np.sum(a * a, axis=1)
    lags = np.arange(lo, hi + 1)
    r = np.empty((len(f), len(lags)))
    for n, lag in enumerate(lags):
        b = f[:, lag:lag + F0_WIN]
        r[:, n] = np.sum(a * b, axis=1) / np.sqrt(ea * np.sum(b * b, axis=1) + 1e-12)
    best = np.argmax(r, axis=1)
    rows = np.arange(len(f))
    peak = r[rows, best]
    for d in (3, 2):
        sub = np.round(lags[best] / d).astype(int) - lo
        ok = sub >= 0
        s = np.where(ok, sub, 0)
        # the local maximum near lag/d
        near = np.stack([r[rows, np.clip(s + o, 0, len(lags) - 1)] for o in (-1, 0, 1)])
        cand = np.clip(s + np.argmax(near, axis=0) - 1, 0, len(lags) - 1)
        take = ok & (r[rows, cand] >= 0.9 * peak)
        best = np.where(take, cand, best)
    # parabolic refinement
    n0 = np.clip(best, 1, len(lags) - 2)
    y0, y1, y2 = r[rows, n0 - 1], r[rows, n0], r[rows, n0 + 1]
    den = y0 - 2 * y1 + y2
    off = np.where(np.abs(den) > 1e-9, 0.5 * (y0 - y2) / np.where(den == 0, 1, den), 0)
    lag = lags[n0] + np.clip(off, -1, 1)
    return RATE / lag, r[rows, best] >= F0_VOICED


def envelope(x):
    n = len(x) // ENV_HOP
    e = 10 * np.log10(np.mean(x[:n * ENV_HOP].reshape(n, ENV_HOP) ** 2, axis=1) + 1e-12)
    e = np.convolve(e, np.ones(4) / 4, mode="same")
    return np.maximum(e, e.max() - 50)


def align(ref, out):
    """Samples by which out is delayed to line up with ref: the shift (in
    2.5 ms steps) that best correlates their level envelopes over the
    input's length, the output being silent outside its own."""
    a, b = envelope(ref), envelope(out)
    floor = b.min()
    best, lag = -np.inf, 0
    for shift in range(-len(b) + 1, len(a)):
        y = np.full(len(a), floor)
        lo, hi = max(0, shift), min(len(a), shift + len(b))
        if hi - lo < min(len(a), len(b)) // 2:
            continue
        y[lo:hi] = b[lo - shift:hi - shift]
        r = np.corrcoef(a, y)[0, 1]
        if r > best:
            best, lag = r, shift
    return lag * ENV_HOP


def refine(ref, out, lag):
    """The lag within ±25 ms of the envelope's that gives the closest
    spectra (lowest mean mel-cepstral distance over the speech frames)."""
    lin = levelDb(ref)
    speech = lin > lin.max() - SPEECH_DB
    cr = scipy.fft.dct(melLog(ref), axis=1)[speech, 1:CEPS + 1]
    best, at = np.inf, lag
    for d in range(-REFINE, REFINE + 1, REFINE_STEP):
        co = scipy.fft.dct(melLog(placed(out, lag + d, len(ref))), axis=1)[speech, 1:CEPS + 1]
        dist = np.mean(np.sqrt(np.sum((cr - co) ** 2, axis=1)))
        if dist < best:
            best, at = dist, lag + d
    return at


def placed(out, lag, n):
    y = np.zeros(n)
    a, b = max(0, lag), min(n, lag + len(out))
    if b > a:
        y[a:b] = out[a - lag:b - lag]
    return y


def compare(ref, deg):
    """The measures for one input and one output already aligned with it."""
    import pesq as pesqlib
    lin, lout = levelDb(ref), levelDb(deg)
    speech = lin > lin.max() - SPEECH_DB
    quiet = lin < lin.max() - SPEECH_DB - 10
    m = {}
    try:
        m["pesq"] = float(pesqlib.pesq(RATE, ref, deg, "nb"))
    except Exception:
        m["pesq"] = float("nan")
    ci, co = melLog(ref), melLog(deg)
    # cepstra scaled so that log|X| = c0 + 2·sum(cn·cos(n·w)), the usual MCD convention
    dc = (scipy.fft.dct(ci, axis=1) - scipy.fft.dct(co, axis=1))[:, 1:CEPS + 1] / (2 * MEL_BANDS)
    mcd = 10 / math.log(10) * np.sqrt(2 * np.sum(dc * dc, axis=1))
    m["mcd"] = float(np.mean(mcd[speech]))
    di = 20 / math.log(10) * (ci - ci.mean(axis=1, keepdims=True))
    do = 20 / math.log(10) * (co - co.mean(axis=1, keepdims=True))
    diff = np.mean((do - di)[speech], axis=0)
    lo, hi = TILT_EDGES
    m["tiltLow"] = float(np.mean(diff[MEL_CENTRES < lo]))
    m["tiltMid"] = float(np.mean(diff[(MEL_CENTRES >= lo) & (MEL_CENTRES < hi)]))
    m["tiltHigh"] = float(np.mean(diff[MEL_CENTRES >= hi]))
    err = (lout - lin)[speech]
    med = np.median(err)
    m["levelErr"] = float(np.mean(np.abs(err - med)))
    m["levelCorr"] = float(np.corrcoef(lin[speech], lout[speech])[0, 1]) if speech.sum() > 2 else float("nan")
    m["drop"] = 100.0 * float(np.mean(err - med < -DROP_DB))
    # pauses: each level relative to its own loudest frame, floored at -80 dB
    rin = np.maximum(lin - lin.max(), -80)[quiet]
    rout = np.maximum(lout - lout.max(), -80)[quiet]
    m["pause"] = float(np.median(rout - rin)) if quiet.any() else 0.0
    fi, vi = pitch(ref)
    fo, vo = pitch(deg)
    vi, vo = vi & speech, vo & speech
    m["vuv"] = 100.0 * float(np.mean((vi != vo)[speech]))
    both = vi & vo
    ratio = fo[both] / fi[both]
    gross = np.abs(ratio - 1) > GROSS
    m["gpe"] = 100.0 * float(np.mean(gross)) if both.any() else float("nan")
    cents = 1200 * np.log2(ratio[~gross])
    m["f0Cents"] = float(np.sqrt(np.mean(cents ** 2))) if len(cents) else float("nan")
    m["speechFrames"] = int(speech.sum())
    perFrame = {"t_ms": np.arange(len(lin)) * 1000 * HOP // RATE, "speech": speech.astype(int),
                "in_db": lin, "out_db": lout, "mcd": mcd, "f0_in": np.where(vi, fi, 0), "f0_out": np.where(vo, fo, 0)}
    return m, perFrame


# ---- the anchor: a floating-point LPC-10 vocoder -----------------------

PRE = 0.9375
LPC_ORDER, LPC_HOP, LPC_WIN, LPC_STEPS = 10, 200, 240, 8


def polyOf(k):
    """A(z) from reflection coefficients (k1 = -R1/R0, the chip's sign)."""
    a = np.array([1.0])
    for ki in k:
        a = np.concatenate([a, [0]]) + ki * np.concatenate([[0], a[::-1]])
    return a


def lpcAnalysis(x, chip=False, preFilter=(1, -PRE)):
    """Per 25 ms frame: K, residual gain, period (0 = unvoiced) and the
    input's RMS. chip=False pre-emphasises every frame (the anchor);
    chip=True follows the chip's format: pre-emphasis for voiced frames
    only, unvoiced frames from the plain signal and K1-K4 only. preFilter
    is the FIR pre-emphasis."""
    order, hop, win = LPC_ORDER, LPC_HOP, LPC_WIN
    w = np.hamming(win)
    y = scipy.signal.lfilter(preFilter, [1], x)
    padY = np.concatenate([np.zeros(win), y, np.zeros(win)])
    padX = np.concatenate([np.zeros(win), x, np.zeros(win)])
    lagw = np.exp(-0.5 * (2 * np.pi * 60 * np.arange(1, order + 1) / RATE) ** 2)
    f0, voiced = pitch(x)
    level = levelDb(x)
    out = []
    for j in range(len(x) // hop + 1):
        c = hop * j + hop // 2
        h = [n for n in range(c // HOP - 1, c // HOP + 2) if 0 <= n < len(f0)]
        v = [f0[n] for n in h if voiced[n] and level[n] > level.max() - SPEECH_DB]
        period = RATE / np.median(v) if len(v) >= 2 else 0
        plain = padX[c + win // 2:c + win // 2 + win] * w
        seg = plain if (chip and not period) else padY[c + win // 2:c + win // 2 + win] * w
        n = 4 if (chip and not period) else order
        r = np.array([np.dot(seg[:win - i], seg[i:]) for i in range(order + 1)])
        r[0] = r[0] * 1.0001 + 1e-12
        r[1:] *= lagw
        k, e = np.zeros(order), r[0]
        a = np.zeros(order + 1)
        a[0] = 1
        for i in range(1, n + 1):
            ki = -np.dot(a[:i], r[i:0:-1]) / e
            a[:i + 1] = a[:i + 1] + ki * a[i::-1]
            k[i - 1] = ki
            e *= 1 - ki * ki
        out.append({"k": k, "gain": math.sqrt(e / np.sum(w * w)), "period": period,
                    "rms": math.sqrt(np.sum(plain ** 2) / np.sum(w * w))})
    return out


def _lar(k):
    k = np.clip(k, -0.999, 0.999)
    return np.log((1 + k) / (1 - k))


def quantise(fr, key, parts="kpe"):
    """The chip's tables applied to analysed frames: K to the nearest entry
    in log-area ratio (K1-K4 only if unvoiced), the period to the nearest
    value + 1, and the gain in `key` to the energy table, scaled so the
    loudest frame is 100 (between entries 13 and 14); below half of entry 1
    is silence. Adds the indices; parts says which values are replaced."""
    loud = max(f[key] for f in fr) or 1.0
    scale = 100.0 / loud
    ktab = [np.array(row, dtype=float) / 512 for row in model.K]
    periods = np.array(model.PITCH[1:], dtype=float) + 1
    energies = np.array(model.ENERGY[1:15], dtype=float)
    out = []
    for f in fr:
        g = dict(f)
        n = 10 if f["period"] else 4
        g["kIdx"] = [int(np.argmin(np.abs(_lar(ktab[i]) - _lar(f["k"][i])))) for i in range(n)]
        if "k" in parts:
            g["k"] = np.array([ktab[i][g["kIdx"][i]] for i in range(n)] + [0.0] * (10 - n))
        if f["period"]:
            g["pIdx"] = 1 + int(np.argmin(np.abs(np.log(periods) - math.log(f["period"]))))
            if "p" in parts:
                g["period"] = float(periods[g["pIdx"] - 1])
        else:
            g["pIdx"] = 0
        e = f[key] * scale
        if e < energies[0] / 2:
            g["eIdx"] = 0
            if "e" in parts:
                g[key] = 0.0
        else:
            g["eIdx"] = 1 + int(np.argmin(np.abs(np.log(energies) - math.log(e))))
            if "e" in parts:
                g[key] = float(energies[g["eIdx"] - 1]) / scale
        out.append(g)
    return out


CHIRP = np.array([v - 256 if v >= 128 else v for v in model.CHIRP], dtype=float)


def unitRms(k, period, nfft=4096):
    """Output RMS of the chip's filter for energy 1: the chirp (truncated at
    the period) once per period if voiced, else +-64; each times E/8."""
    H = 1.0 / np.abs(np.fft.fft(polyOf(k), nfft)) ** 2
    if not period:
        return 8.0 * math.sqrt(float(np.mean(H)))
    c = CHIRP[:min(int(period), len(CHIRP))]
    C = np.abs(np.fft.fft(c, nfft)) ** 2
    return math.sqrt(float(np.mean(C * H)) / period) / 8.0


def chipEnergy(fr):
    """The energy that gives each frame the input's RMS through the chip's
    excitation (fr from lpcAnalysis(chip=True))."""
    return [dict(f, energy=f["rms"] / max(1e-12, unitRms(f["k"], f["period"]))) for f in fr]


def synthLinear(fr, n, key="gain", excitation="pulse", deemph=True, seed=1):
    """A float LPC synthesiser: K and gain interpolated linearly over 8
    steps a frame. excitation "pulse" is an impulse of energy p per period
    and unit noise; "chip" is the chirp and +-8 noise as the chip scales them."""
    hop, steps = LPC_HOP, LPC_STEPS
    rng = np.random.default_rng(seed)
    out = np.zeros(len(fr) * hop)
    state = np.zeros(LPC_ORDER)
    nextPulse, since = 0.0, len(CHIRP)
    sub = hop // steps
    for j, f in enumerate(fr):
        k0 = fr[j - 1]["k"] if j else f["k"]
        g0 = fr[j - 1][key] if j else 0.0
        p = f["period"]
        for s in range(steps):
            t = (s + 1) / steps
            k = k0 + (f["k"] - k0) * t
            g = g0 + (f[key] - g0) * t
            n0 = j * hop + s * sub
            ex = np.zeros(sub)
            for i in range(sub):
                if p:
                    if n0 + i >= nextPulse:
                        if excitation == "pulse":
                            ex[i] = math.sqrt(p)
                        since = 0
                        nextPulse += p
                    if excitation == "chip" and since < len(CHIRP):
                        ex[i] = CHIRP[since] / 8.0
                    since += 1
                else:
                    ex[i] = rng.standard_normal() if excitation == "pulse" else rng.choice((-8.0, 8.0))
                    nextPulse = n0 + i + 1
            seg, state = scipy.signal.lfilter([g], polyOf(k), ex, zi=state)
            out[n0:n0 + sub] = seg
    if deemph:
        out = scipy.signal.lfilter([1], [1, -PRE], out)
    # the frame's values are reached at its centre: undo the half-frame lag
    return np.concatenate([out[hop // 2:], np.zeros(hop // 2)])[:n]


def lpcVocoder(x, seed=1):
    return synthLinear(lpcAnalysis(x), len(x), seed=seed)


class FloatChip(model.Model):
    """The reference model's timing, interpolation and excitation with a
    floating-point lattice: no rounding, no wrap, no output clipping."""

    def reset(self):
        super().reset()
        self.uf, self.xf, self.outF = [0.0] * 11, [0.0] * 10, []

    def lattice(self, excitation):
        k, kx, u, x = self.view[2:12], self.view[14:24], self.uf, self.xf
        u[10] = self.view[0] * excitation / 8.0
        for i in range(9, -1, -1):
            u[i] = u[i + 1] - k[i] * x[i] / 512.0
        for i in range(9, 0, -1):
            x[i] = x[i - 1] + kx[i - 1] * u[i - 1] / 512.0
        x[0] = u[0]
        self.outF.append(u[0])
        return int(max(-16384, min(16383, round(u[0]))))


def floatChip(frs):
    """Frames (lpc.py dicts) spoken by FloatChip from a reset."""
    m = FloatChip()
    m.speak(list(frs) + [{"energy": 15}], lead=0, tail=200)
    return np.array(m.outF)


def toFrames(fr):
    """Quantised analysis frames as chip frames, ending with a stop."""
    out = []
    for f in fr:
        if f["eIdx"] == 0:
            out.append({"energy": 0})
        else:
            out.append({"energy": f["eIdx"], "repeat": 0, "pitch": f["pIdx"], "k": f["kIdx"]})
    while out and out[0]["energy"] == 0:
        out.pop(0)
    return out


LADDER = [("anchor", "float LPC-10 vocoder"),
          ("q", "+ the chip's tables (K, pitch, energy; 4 K unvoiced)"),
          ("qc", "+ the chip's excitation (chirp; pre-emphasis voiced only, none undone)"),
          ("qci", "+ the chip's interpolation and timing (float model)"),
          ("enc", "the encoder's frames (float model)"),
          ("chip", "the encoder's frames on the emulator")]
# Not steps: the anchor with one of q's three tables at a time
PARTS = [("qk", "anchor + K table only"), ("qp", "anchor + pitch table only"),
         ("qe", "anchor + energy table only")]


def ladder(ref, frs):
    """Each step's output, from the anchor to the chip's own (chip itself
    is scored in measure)."""
    n = len(ref)
    a = lpcAnalysis(ref)
    c = quantise(chipEnergy(lpcAnalysis(ref, chip=True)), "energy")
    return {"anchor": synthLinear(a, n),
            "qk": synthLinear(quantise(a, "gain", "k"), n),
            "qp": synthLinear(quantise(a, "gain", "p"), n),
            "qe": synthLinear(quantise(a, "gain", "e"), n),
            "q": synthLinear(quantise(a, "gain"), n),
            "qc": synthLinear(c, n, key="energy", excitation="chip", deemph=False),
            "qci": floatChip(toFrames(c)),
            "enc": floatChip(frs)}


# ---- one input ----------------------------------------------------------

def matchLevel(ref, deg):
    lin = levelDb(ref)
    speech = lin > lin.max() - SPEECH_DB
    fr = frames(ref, WIN)[speech], frames(deg, WIN)[speech]
    pr, pd = np.mean(fr[0] ** 2), np.mean(fr[1] ** 2)
    return deg * math.sqrt(pr / pd) if pd > 0 else deg


def measure(job):
    path, binPath, anchor, steps, wavDir, framesCsv = job
    x = readAudio(path)
    ref = scipy.signal.sosfiltfilt(scipy.signal.butter(2, 60, "highpass", fs=RATE, output="sos"), x)
    if os.path.isdir(binPath):
        binPath = os.path.join(binPath, os.path.splitext(os.path.basename(path))[0] + ".bin")
    with open(binPath, "rb") as f:
        data = f.read()
    frs, _ = lpc.parseFrames(lpc.bitsLsbFirst(data))
    with tempfile.TemporaryDirectory() as tmp:
        codes = emulate(data, tmp)
    out = codes.astype(float) + 1           # idle (code -1) at 0
    lag = refine(ref, out, align(ref, out))
    deg = matchLevel(ref, placed(out, lag, len(ref)))
    m, perFrame = compare(ref, deg)
    nbits = sum(len(lpc.frameBits(f)) for f in frs)
    row = {"file": os.path.basename(path), "frames": len(frs), "bitrate": nbits / max(1, len(frs)) * 40,
           "seconds": len(ref) / RATE, "lagMs": lag * 1000 / RATE,
           "peak": int(np.max(np.abs(codes))) if len(codes) else 0}
    row.update(m)
    name = os.path.splitext(os.path.basename(path))[0]
    if anchor:
        voc = matchLevel(ref, lpcVocoder(ref))
        am, _ = compare(ref, voc)
        row.update({"anchor_" + k: v for k, v in am.items() if k != "speechFrames"})
        if wavDir:
            soundfile.write(os.path.join(wavDir, name + "_lpc.wav"), scaled(voc, ref), RATE, subtype="PCM_16")
    if steps:
        for step, y in ladder(ref, frs).items():
            out = matchLevel(ref, placed(y, refine(ref, y, align(ref, y)), len(ref)))
            sm, _ = compare(ref, out)
            row.update({f"{step}_{k}": v for k, v in sm.items() if k != "speechFrames"})
            if wavDir:
                soundfile.write(os.path.join(wavDir, f"{name}_{step}.wav"), scaled(out, ref), RATE, subtype="PCM_16")
        row.update({"chip_" + k: v for k, v in m.items() if k != "speechFrames"})
    if wavDir:
        soundfile.write(os.path.join(wavDir, name + "_input.wav"), scaled(ref, ref), RATE, subtype="PCM_16")
        soundfile.write(os.path.join(wavDir, name + "_chip.wav"), scaled(deg, ref), RATE, subtype="PCM_16")
    if framesCsv:
        with open(framesCsv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(list(perFrame))
            for vals in zip(*perFrame.values()):
                w.writerow([v if isinstance(v, (int, np.integer)) else round(float(v), 2) for v in vals])
    return row


def scaled(y, ref):
    return np.clip(y * 0.5 / max(1e-9, np.max(np.abs(ref))), -1, 1)


# ---- reporting ----------------------------------------------------------

COLUMNS = [("pesq", "PESQ", "%4.2f"), ("mcd", "MCD", "%5.2f"), ("tiltLow", "lo", "%+5.1f"),
           ("tiltMid", "mid", "%+5.1f"), ("tiltHigh", "hi", "%+5.1f"), ("levelErr", "lvl", "%4.1f"),
           ("levelCorr", "lvlr", "%4.2f"), ("drop", "drop%", "%5.1f"), ("pause", "pause", "%+5.1f"),
           ("vuv", "vuv%", "%5.1f"), ("gpe", "gpe%", "%5.1f"), ("f0Cents", "f0c", "%5.0f")]


def header(anchor):
    h = "%-26s %5s %5s" % ("file", "b/s", "lagms") + "".join(" %*s" % (len(fmt % 0), t) for _, t, fmt in COLUMNS)
    return h + ("   anchor PESQ  MCD" if anchor else "")


def line(r, anchor):
    s = "%-26s %5.0f %+5.0f" % (r["file"][:26], r["bitrate"], r["lagMs"])
    s += "".join(" " + fmt % r[k] for k, _, fmt in COLUMNS)
    if anchor:
        s += "        %4.2f %5.2f" % (r["anchor_pesq"], r["anchor_mcd"])
    return s


def summary(name, rows, anchor):
    def med(k):
        return float(np.nanmedian([r[k] for r in rows]))
    s = "%-26s %5.0f %5s" % (f"median, {name} (n={len(rows)})"[:26], med("bitrate"), "")
    s += "".join(" " + fmt % med(k) for k, _, fmt in COLUMNS)
    if anchor:
        s += "        %4.2f %5.2f" % (med("anchor_pesq"), med("anchor_mcd"))
    return s


LADDER_COLUMNS = [("pesq", "PESQ", "%4.2f"), ("mcd", "MCD", "%5.2f"), ("tiltLow", "lo", "%+5.1f"),
                  ("tiltMid", "mid", "%+5.1f"), ("tiltHigh", "hi", "%+5.1f"), ("levelErr", "lvl", "%4.1f"),
                  ("vuv", "vuv%", "%5.1f"), ("gpe", "gpe%", "%5.1f"), ("f0Cents", "f0c", "%5.0f")]


def ladderLine(r):
    return "%-26s" % r["file"][:26] + "".join(" %6s" % ("%4.2f" % r[s + "_pesq"]) for s, _ in LADDER)


def ladderSummary(rows):
    """Medians per step, and each step's change from the one before."""
    head = "%-8s" % "step" + "".join(" %*s" % (max(5, len(fmt % 0)), t) for _, t, fmt in LADDER_COLUMNS)
    lines = [head]
    prev = None
    for s, what in LADDER:
        med = {k: float(np.nanmedian([r[f"{s}_{k}"] for r in rows])) for k, _, _ in LADDER_COLUMNS}
        lines.append("%-8s" % s + "".join(" %*s" % (max(5, len(fmt % 0)), fmt % med[k])
                                          for k, _, fmt in LADDER_COLUMNS) + "   " + what)
        if prev is not None:
            d = float(np.nanmedian([r[f"{s}_pesq"] - r[f"{prev}_pesq"] for r in rows]))
            lines[-1] += "  (PESQ %+.2f)" % d
        prev = s
    lines.append("")
    for s, what in PARTS:
        med = {k: float(np.nanmedian([r[f"{s}_{k}"] for r in rows])) for k, _, _ in LADDER_COLUMNS}
        d = float(np.nanmedian([r[f"{s}_pesq"] - r["anchor_pesq"] for r in rows]))
        lines.append("%-8s" % s + "".join(" %*s" % (max(5, len(fmt % 0)), fmt % med[k])
                                          for k, _, fmt in LADDER_COLUMNS) + "   " + what + "  (PESQ %+.2f)" % d)
    return "\n".join(lines)


def phase11Inputs(which, match, limit):
    out = []
    for sub in (["tts", "recordings"] if which == "all" else [which]):
        with open(os.path.join(INPUTS, sub, "index.csv"), newline="") as f:
            rows = [r for r in csv.DictReader(f) if match in r["file"]]
        if limit:
            rows = rows[::max(1, len(rows) // limit)][:limit]
        out += [(sub, os.path.join(INPUTS, sub, r["file"])) for r in rows]
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="*", help="input audio (any rate; mixed to mono)")
    ap.add_argument("--set", choices=["tts", "recordings", "all"], help="the Phase 11 test inputs")
    ap.add_argument("--match", default="", help="with --set: only files whose name contains this")
    ap.add_argument("--limit", type=int, default=0, help="with --set: at most this many per set, evenly spread")
    ap.add_argument("--bin", required=True,
                    help="the packed frames to score: a file (one input), or a directory of NAME.bin")
    ap.add_argument("--anchor", action="store_true", help="also score a floating-point LPC-10 vocoder")
    ap.add_argument("--ladder", action="store_true",
                    help="also score each step from the anchor to the chip (see LADDER)")
    ap.add_argument("--wav", metavar="DIR", help="write the input, the aligned chip output (and the anchor)")
    ap.add_argument("--frames-csv", metavar="FILE", help="per-10 ms measures (one input only)")
    ap.add_argument("--csv", metavar="FILE", help="one row per input")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    args = ap.parse_args()

    if not os.path.exists(EMU):
        sys.exit(f"{EMU} not found: build it with nix develop -c make -C emulator")
    items = [("", p) for p in args.inputs]
    if args.set:
        items += phase11Inputs(args.set, args.match, args.limit)
    if not items:
        ap.error("give input files or --set")
    if (os.path.isfile(args.bin) or args.frames_csv) and len(items) != 1:
        ap.error("a --bin file and --frames-csv take one input")
    if args.wav:
        os.makedirs(args.wav, exist_ok=True)
    jobs = [(p, args.bin, args.anchor, args.ladder, args.wav, args.frames_csv) for _, p in items]

    if args.ladder:
        print("%-26s" % "file PESQ:" + "".join(" %6s" % s for s, _ in LADDER))
    else:
        print(header(args.anchor))
    rows = []
    with multiprocessing.Pool(max(1, min(args.jobs, len(jobs)))) as pool:
        for (group, _), r in zip(items, pool.imap(measure, jobs)):
            r["set"] = group
            rows.append(r)
            print(ladderLine(r) if args.ladder else line(r, args.anchor), flush=True)
    if args.ladder:
        print()
        for g in sorted({r["set"] for r in rows}):
            if g and len(rows) > 1:
                print(f"{g} (n={sum(r['set'] == g for r in rows)})")
                print(ladderSummary([r for r in rows if r["set"] == g]))
                print()
        print(f"all (n={len(rows)})")
        print(ladderSummary(rows))
    elif len(rows) > 1:
        print()
        for g in sorted({r["set"] for r in rows}):
            if g:
                print(summary(g, [r for r in rows if r["set"] == g], args.anchor))
        print(summary("all", rows, args.anchor))
    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)


if __name__ == "__main__":
    main()
