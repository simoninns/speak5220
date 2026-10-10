#!/usr/bin/env python3
"""
    speechqual.py

    An objective measure of encoding quality: how close the TMS5220's
    speech, as the reference emulator speaks the encoded frames, is to the
    input it was encoded from.

    Each input is encoded with encoder/tms5220enc.py (or taken from --bin),
    spoken by emulator/tms5220emu (VSM Speak after a Reset, so independent
    of the encoder's own synthesis), aligned with the input by its level
    envelope, and compared with it:

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

      python tools/speechqual.py IN.wav [IN2.flac ...]
      python tools/speechqual.py IN.wav --bin OUT.bin    # score existing frames
      python tools/speechqual.py --set tts --anchor --csv q.csv
      python tools/speechqual.py IN.wav --wav DIR        # aligned audio to listen to
      python tools/speechqual.py IN.wav --frames-csv f.csv

    --set takes the Phase 11 test inputs (tts, recordings or all), as
    encoder/evaluate.py does; --opt name=value passes encode() options.

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
sys.path.insert(0, os.path.join(ROOT, "encoder"))
import tms5220enc as enc  # noqa: E402

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

def lpcVocoder(x, seed=1):
    pre = 0.9375
    order, hop, win, steps = 10, 200, 240, 8
    y = scipy.signal.lfilter([1, -pre], [1], x)
    w = np.hamming(win)
    f0, voiced = pitch(x)
    level = levelDb(x)
    nfr = len(x) // hop + 1
    ks, gains, periods = [], [], []
    for j in range(nfr):
        c = hop * j + hop // 2
        seg = np.concatenate([np.zeros(win), y, np.zeros(win)])[c + win // 2:c + win // 2 + win] * w
        r = np.array([np.dot(seg[:win - i], seg[i:]) for i in range(order + 1)])
        r[0] = r[0] * 1.0001 + 1e-12
        r[1:] *= np.exp(-0.5 * (2 * np.pi * 60 * np.arange(1, order + 1) / RATE) ** 2)
        k, e = np.zeros(order), r[0]
        a = np.zeros(order + 1)
        a[0] = 1
        for i in range(1, order + 1):
            ki = -np.dot(a[:i], r[i:0:-1]) / e
            a[:i + 1] = a[:i + 1] + ki * a[i::-1]
            k[i - 1] = ki
            e *= 1 - ki * ki
        h = [n for n in range(c // HOP - 1, c // HOP + 2) if 0 <= n < len(f0)]
        v = [f0[n] for n in h if voiced[n] and level[n] > level.max() - SPEECH_DB]
        periods.append(RATE / np.median(v) if len(v) >= 2 else 0)
        ks.append(k)
        gains.append(math.sqrt(e / np.sum(w * w)))
    rng = np.random.default_rng(seed)
    out = np.zeros(nfr * hop)
    state = np.zeros(order)
    nextPulse = 0.0
    sub = hop // steps
    for j in range(nfr):
        k0 = ks[j - 1] if j else ks[0]
        g0 = gains[j - 1] if j else 0.0
        for s in range(steps):
            t = (s + 1) / steps
            k = k0 + (ks[j] - k0) * t
            g = g0 + (gains[j] - g0) * t
            a = np.array([1.0])
            for ki in k:
                a = np.concatenate([a, [0]]) + ki * np.concatenate([[0], a[::-1]])
            n0 = j * hop + s * sub
            ex = np.zeros(sub)
            p = periods[j]
            if p:
                while nextPulse < n0 + sub:
                    if nextPulse >= n0:
                        ex[int(nextPulse) - n0] = math.sqrt(p)
                    nextPulse += p
            else:
                ex = rng.standard_normal(sub)
                nextPulse = n0 + sub
            seg, state = scipy.signal.lfilter([g], a, ex, zi=state)
            out[n0:n0 + sub] = seg
    # the frame's values are reached at its centre: undo the half-frame lag
    out = scipy.signal.lfilter([1], [1, -pre], out)
    return np.concatenate([out[hop // 2:], np.zeros(hop // 2)])[:len(x)]


# ---- one input ----------------------------------------------------------

def matchLevel(ref, deg):
    lin = levelDb(ref)
    speech = lin > lin.max() - SPEECH_DB
    fr = frames(ref, WIN)[speech], frames(deg, WIN)[speech]
    pr, pd = np.mean(fr[0] ** 2), np.mean(fr[1] ** 2)
    return deg * math.sqrt(pr / pd) if pd > 0 else deg


def measure(job):
    path, binPath, opts, anchor, wavDir, framesCsv = job
    x = readAudio(path)
    ref = scipy.signal.sosfiltfilt(scipy.signal.butter(2, 60, "highpass", fs=RATE, output="sos"), x)
    if binPath:
        with open(binPath, "rb") as f:
            data = f.read()
        frs, _ = enc.parse(data)
    else:
        frs = enc.encode(x, RATE, **opts)
        data = enc.pack(frs)
    with tempfile.TemporaryDirectory() as tmp:
        codes = emulate(data, tmp)
    out = codes.astype(float) + 1           # idle (code -1) at 0
    lag = refine(ref, out, align(ref, out))
    deg = matchLevel(ref, placed(out, lag, len(ref)))
    m, perFrame = compare(ref, deg)
    nbits = sum(enc.frameBits(f) for f in frs)
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


def phase11Inputs(which, match, limit):
    out = []
    for sub in (["tts", "recordings"] if which == "all" else [which]):
        with open(os.path.join(INPUTS, sub, "index.csv"), newline="") as f:
            rows = [r for r in csv.DictReader(f) if match in r["file"]]
        if limit:
            rows = rows[::max(1, len(rows) // limit)][:limit]
        out += [(sub, os.path.join(INPUTS, sub, r["file"])) for r in rows]
    return out


def parseOpt(o):
    k, v = o.split("=", 1)
    if v in ("None", "True", "False"):
        return k, {"None": None, "True": True, "False": False}[v]
    try:
        return k, int(v)
    except ValueError:
        return k, float(v)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="*", help="input audio (any rate; mixed to mono)")
    ap.add_argument("--set", choices=["tts", "recordings", "all"], help="the Phase 11 test inputs")
    ap.add_argument("--match", default="", help="with --set: only files whose name contains this")
    ap.add_argument("--limit", type=int, default=0, help="with --set: at most this many per set, evenly spread")
    ap.add_argument("--bin", help="packed frames to score instead of encoding (one input only)")
    ap.add_argument("--opt", action="append", default=[], help="name=value encode() option")
    ap.add_argument("--anchor", action="store_true", help="also score a floating-point LPC-10 vocoder")
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
    if (args.bin or args.frames_csv) and len(items) != 1:
        ap.error("--bin and --frames-csv take one input")
    if args.wav:
        os.makedirs(args.wav, exist_ok=True)
    opts = dict(parseOpt(o) for o in args.opt)
    jobs = [(p, args.bin, opts, args.anchor, args.wav, args.frames_csv) for _, p in items]

    print(header(args.anchor))
    rows = []
    with multiprocessing.Pool(max(1, min(args.jobs, len(jobs)))) as pool:
        for (group, _), r in zip(items, pool.imap(measure, jobs)):
            r["set"] = group
            rows.append(r)
            print(line(r, args.anchor), flush=True)
    if len(rows) > 1:
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
