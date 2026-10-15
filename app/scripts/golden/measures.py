#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the quality measures' reference data in app/golden/quality/measures/.

Measures the reference encodings of the encoder's corpus subset (the WAV
files in app/golden/encoder/inputs/, encoded as app/golden/encoder/frames/
SET.tsv records, spoken by the application's emulator) with independent
implementations of the quality measures, for the C++ to be checked
against:

- The signals are prepared as the application prepares them: the chip's output
  with its idle code at 0, placed against the source by the alignment of
  investigation/tools/speechqual.py (align() and refine(), on the source
  after the encoder's 60 Hz high-pass); both band-limited by the same
  zero-phase Butterworth band-pass (order 4, 80 to 3600 Hz, SciPy's
  sosfiltfilt); the voice activity mask from the original WebRTC detector
  (py-webrtcvad, aggressiveness 3, 10 ms frames) less the frames more than
  40 dB below the loudest; the output scaled to the original's power over
  the mask.
- The spectral envelope: log spectral distortion between the order-10 LPC
  envelopes (shape only, 1/|A|) of Hamming-windowed 25 ms frames, over 100
  to 3500 Hz, from this script's own NumPy LPC analysis.
- Pitch and voicing: REAPER's track from its own command-line tool, and
  Harvest's from WORLD through pyworld, of both prepared signals, every
  10 ms, searched from 50 to 400 Hz; gross pitch error (over 20 %), F0 RMSE
  in cents and voicing decision error over the mask.
- ESTOI: pystoi's (extended=True) of the prepared signals with the frames
  outside the mask set to zero, so its own removal of silent frames drops
  them.

It writes:

- scores/SET.tsv, one row per subset file for each option set: the
  alignment lag, the frames the mask keeps, the envelope's mean distortion
  and its percentages of frames over 2 and 4 dB, each estimator's gross
  pitch error, F0 RMSE and voicing decision error, and ESTOI;
- masks/SET.tsv: each file's mask, one character per 10 ms frame (1 for
  speech);
- tracks/SET/NAME.tsv: each file's F0 tracks (0 where unvoiced), every
  10 ms, from REAPER and Harvest, of the original and of the output.

Run by hand, in the project's default Nix shell, from the repository root,
with the tests built (the frames are spoken by tms5220emu, the emulator's
command line, in build/app/tests/support/; TMS5220EMU names another):

    nix develop --command python3 app/scripts/golden/measures.py
    nix develop --command python3 app/scripts/golden/measures.py --check

--check regenerates everything into a temporary directory and compares it
with the committed files. Ask before rewriting the reference data
(AGENTS.md §3.1).
"""

import argparse
import filecmp
import hashlib
import importlib.metadata
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
import warnings
from pathlib import Path

# One BLAS thread: pystoi's matrix products otherwise sum in an order that
# depends on the thread count, and its last bits vary from run to run.
for _var in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS", "MKL_NUM_THREADS"):
    os.environ[_var] = "1"

import numpy as np  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / "investigation" / "tools"
INPUTS = ROOT / "app" / "golden" / "encoder" / "inputs"
FRAMES = ROOT / "app" / "golden" / "encoder" / "frames"
GOLDEN = ROOT / "app" / "golden" / "quality" / "measures"

sys.path.insert(0, str(TOOLS))
import speechqual as sq  # noqa: E402

# The reference sources the data depends on. The emulator that speaks the
# frames is the application's, which its own reference data holds to the
# same codes (app/golden/emulator/).
SOURCES = [
    TOOLS / "speechqual.py",
]

RATE = 8000
# The chip's output: codes x 64 of 16-bit full scale.
SAMPLE_SCALE = 64 / 32768
# The voice activity frame, 10 ms, and the detector's aggressiveness.
HOP = 80
VAD_MODE = 3
# Frames more than this many dB below the loudest are left out of the mask.
MASK_FLOOR_DB = 40.0
# The shared band-limit.
BAND_HZ = (80.0, 3600.0)
BAND_ORDER = 4
# The spectral envelope: order, frame, FFT and the band compared.
LSD_ORDER = 10
LSD_FRAME = 200
LSD_FFT = 512
LSD_BAND_HZ = (100.0, 3500.0)
# The pitch search range, and the ratio beyond which an error is gross.
F0_RANGE_HZ = (50.0, 400.0)
GROSS_RATIO = 0.2

SCORE_COLUMNS = [
    "name",
    "lag",
    "speech_frames",
    "lsd_db",
    "lsd_over_2db",
    "lsd_over_4db",
    "reaper_gpe",
    "reaper_f0_rmse_cents",
    "reaper_vde",
    "harvest_gpe",
    "harvest_f0_rmse_cents",
    "harvest_vde",
    "estoi",
]


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def number(v):
    """A float as the shortest text that reads back exactly."""
    return repr(float(v))


def compare(a, b):
    """Lists the files that differ between two trees."""
    diff = []

    def walk(c):
        diff.extend(
            str(Path(c.left, f).relative_to(a))
            for f in c.diff_files + c.left_only + c.right_only
        )
        for sub in c.subdirs.values():
            walk(sub)

    walk(filecmp.dircmp(a, b, shallow=False))
    return diff


def reference_commit():
    """The last commit that changed the references; refuses local edits."""
    paths = [str(p.relative_to(ROOT)) for p in SOURCES]
    dirty = run(
        ["git", "status", "--porcelain", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if dirty:
        sys.exit("the reference tools have local changes:\n" + dirty)
    return run(
        ["git", "log", "-1", "--format=%H", "--", *paths],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.strip()


def option_sets():
    """The option sets the encoder's reference data covers."""
    return sorted(p.stem for p in FRAMES.glob("*.tsv"))


def packed_frames(set_name):
    """{stem: packed bytes} of one option set's reference encodings."""
    rows = (FRAMES / (set_name + ".tsv")).read_text().splitlines()[1:]
    return {r.split("\t")[0]: bytes.fromhex(r.split("\t")[3]) for r in rows}


def lround(v):
    """Rounds half away from zero, as C's lround() does."""
    return np.sign(v) * np.floor(np.abs(v) + 0.5)


def bandlimit(x):
    import scipy.signal

    lo, hi = BAND_HZ
    sos = scipy.signal.butter(
        BAND_ORDER, [lo / (RATE / 2), hi / (RATE / 2)], "bandpass", output="sos"
    )
    return scipy.signal.sosfiltfilt(sos, x)


def speech_mask(x):
    """One flag per whole 10 ms frame of x: the WebRTC detector's decision,
    less the frames more than MASK_FLOOR_DB below the loudest."""
    import webrtcvad

    vad = webrtcvad.Vad(VAD_MODE)
    pcm = lround(np.clip(x * 32767.0, -32767.0, 32767.0)).astype("<i2")
    n = len(x) // HOP
    speech = np.array(
        [vad.is_speech(pcm[i * HOP : (i + 1) * HOP].tobytes(), RATE) for i in range(n)],
        dtype=bool,
    )
    frames = x[: n * HOP].reshape(n, HOP)
    power = np.mean(frames * frames, axis=1)
    if not n or power.max() <= 0.0:
        return np.zeros(n, dtype=bool)
    floor = power.max() * 10.0 ** (-MASK_FLOOR_DB / 10.0)
    return speech & (power > floor)


def sample_mask(mask, n):
    """The mask as one flag per sample of an n-sample signal."""
    s = np.repeat(mask, HOP)
    return np.concatenate([s, np.zeros(n - len(s), dtype=bool)])


def prepare(source, codes):
    """(original, output, mask, lag): the prepared pair."""
    import scipy.signal

    hp = scipy.signal.butter(2, 60.0 / (RATE / 2), "highpass", output="sos")
    reference = scipy.signal.sosfiltfilt(hp, source)
    out = codes.astype(float) + 1.0
    lag = sq.refine(reference, out, sq.align(reference, out))
    y = sq.placed(out, lag, len(source)) * SAMPLE_SCALE
    x = bandlimit(source)
    y = bandlimit(y)
    mask = speech_mask(x)
    s = sample_mask(mask, len(x))
    py = float(np.sum(y[s] ** 2))
    if py > 0.0:
        y = y * np.sqrt(float(np.sum(x[s] ** 2)) / py)
    return x, y, mask, lag


def lpc_shape(frame):
    """A(z) of the order-LSD_ORDER autocorrelation fit, or None if the frame
    is silent: Levinson's recursion in NumPy."""
    r = np.array(
        [np.dot(frame[: len(frame) - k], frame[k:]) for k in range(LSD_ORDER + 1)]
    )
    if r[0] <= 0.0:
        return None
    a = np.zeros(LSD_ORDER + 1)
    a[0] = 1.0
    e = r[0]
    for i in range(1, LSD_ORDER + 1):
        k = -(r[i] + np.dot(a[1:i], r[i - 1 : 0 : -1])) / e
        a[1:i] = a[1:i] + k * a[i - 1 : 0 : -1]
        a[i] = k
        e *= 1.0 - k * k
        if e <= 0.0:
            return None
    return a


def envelope_distortion(x, y, mask):
    """(mean, % over 2 dB, % over 4 dB) of the per-frame distortion between
    the LPC envelope shapes, over the frames whose centre the mask keeps.
    A silent output frame is compared with a flat shape."""
    window = np.hamming(LSD_FRAME)
    freqs = np.arange(LSD_FFT // 2 + 1) * RATE / LSD_FFT
    band = (freqs >= LSD_BAND_HZ[0]) & (freqs <= LSD_BAND_HZ[1])
    flat = np.zeros(LSD_ORDER + 1)
    flat[0] = 1.0
    distortions = []
    for k in range(len(x) // LSD_FRAME):
        centre = (k * LSD_FRAME + LSD_FRAME // 2) // HOP
        if centre >= len(mask) or not mask[centre]:
            continue
        seg = slice(k * LSD_FRAME, (k + 1) * LSD_FRAME)
        ax = lpc_shape(x[seg] * window)
        if ax is None:
            continue
        ay = lpc_shape(y[seg] * window)
        if ay is None:
            ay = flat
        ex = -20.0 * np.log10(np.abs(np.fft.rfft(ax, LSD_FFT)))
        ey = -20.0 * np.log10(np.abs(np.fft.rfft(ay, LSD_FFT)))
        distortions.append(np.sqrt(np.mean((ex[band] - ey[band]) ** 2)))
    if not distortions:
        return 0.0, 0.0, 0.0
    d = np.array(distortions)
    return (
        float(d.mean()),
        100.0 * float(np.mean(d > 2.0)),
        100.0 * float(np.mean(d > 4.0)),
    )


def reaper_track(x, tmp):
    """REAPER's F0 every 10 ms (0 where unvoiced), from its command-line
    tool, of x as the 16-bit samples SPTK gives it (truncated)."""
    import soundfile

    wav = os.path.join(tmp, "x.wav")
    f0 = os.path.join(tmp, "x.f0")
    pcm = np.trunc(np.clip(x * 32767.0, -32767.0, 32767.0)).astype("<i2")
    soundfile.write(wav, pcm, RATE, subtype="PCM_16")
    lo, hi = F0_RANGE_HZ
    run(
        [
            "reaper",
            "-i",
            wav,
            "-f",
            f0,
            "-e",
            "0.01",
            "-m",
            str(lo),
            "-x",
            str(hi),
            "-a",
        ],
        capture_output=True,
    )
    body = Path(f0).read_text().split("EST_Header_End", 1)[1].split()
    rows = np.array(body, dtype=float).reshape(-1, 3)
    return np.where(rows[:, 1] > 0.0, rows[:, 2], 0.0)


def harvest_track(x):
    """Harvest's F0 every 10 ms (0 where unvoiced), through pyworld."""
    import pyworld

    lo, hi = F0_RANGE_HZ
    f0, _ = pyworld.harvest(
        np.ascontiguousarray(x, dtype=np.float64),
        RATE,
        f0_floor=lo,
        f0_ceil=hi,
        frame_period=1000.0 * HOP / RATE,
    )
    return f0


def pitch_errors(fx, fy, mask):
    """(gross pitch error %, F0 RMSE in cents, voicing decision error %)."""
    n = min(len(fx), len(fy), len(mask))
    fx, fy, mask = fx[:n], fy[:n], mask[:n]
    vx, vy = fx > 0.0, fy > 0.0
    vde = 100.0 * float(np.mean((vx != vy)[mask])) if mask.any() else 0.0
    both = mask & vx & vy
    if not both.any():
        return 0.0, 0.0, vde
    ratio = fy[both] / fx[both]
    gross = np.abs(ratio - 1.0) > GROSS_RATIO
    cents = 1200.0 * np.log2(ratio[~gross])
    rmse = float(np.sqrt(np.mean(cents * cents))) if len(cents) else 0.0
    return 100.0 * float(np.mean(gross)), rmse, vde


def estoi(x, y, mask):
    import pystoi

    s = sample_mask(mask, len(x))
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        return float(
            pystoi.stoi(np.where(s, x, 0.0), np.where(s, y, 0.0), RATE, extended=True)
        )


def score(v):
    """A score to 12 significant digits: NumPy's sums can differ in the last
    bit from run to run, with the alignment of their arrays."""
    return "%.12g" % v


def track_text(x_tracks):
    lines = ["reaper_original\treaper_output\tharvest_original\tharvest_output"]
    n = max(len(t) for t in x_tracks)
    padded = [np.concatenate([t, np.full(n - len(t), np.nan)]) for t in x_tracks]
    lines += ["\t".join(number(v) for v in row) for row in zip(*padded)]
    return "\n".join(lines) + "\n"


def measure_job(job):
    set_name, stem, data = job
    source = sq.readAudio(str(INPUTS / (stem + ".wav")))
    with tempfile.TemporaryDirectory() as tmp:
        codes = sq.emulate(data, tmp)
        x, y, mask, lag = prepare(source, codes)
        rx, ry = reaper_track(x, tmp), reaper_track(y, tmp)
    hx, hy = harvest_track(x), harvest_track(y)
    row = [stem, str(lag), str(int(mask.sum()))]
    row += [score(v) for v in envelope_distortion(x, y, mask)]
    row += [score(v) for v in pitch_errors(rx, ry, mask)]
    row += [score(v) for v in pitch_errors(hx, hy, mask)]
    row.append(score(estoi(x, y, mask)))
    mask_text = stem + "\t" + "".join("1" if m else "0" for m in mask)
    return set_name, stem, "\t".join(row), mask_text, track_text([rx, ry, hx, hy])


def generate(out, processes):
    commit = reference_commit()
    if not Path(sq.EMU).exists():
        sys.exit("build the tests first: tms5220emu is not at " + sq.EMU)
    if shutil.which("reaper") is None:
        sys.exit(
            "REAPER's command-line tool is not on PATH (use the default Nix shell)"
        )
    stems = sorted(p.stem for p in INPUTS.glob("*.wav"))
    sets = option_sets()
    jobs = []
    for set_name in sets:
        packed = packed_frames(set_name)
        jobs += [(set_name, s, packed[s]) for s in stems]
    with multiprocessing.Pool(processes) as pool:
        results = pool.map(measure_job, jobs, chunksize=1)

    for sub in ("scores", "masks", "tracks"):
        (out / sub).mkdir(parents=True, exist_ok=True)
    for set_name in sets:
        rows = [r for r in results if r[0] == set_name]
        (out / "scores" / (set_name + ".tsv")).write_text(
            "\n".join(["\t".join(SCORE_COLUMNS)] + [r[2] for r in rows]) + "\n"
        )
        (out / "masks" / (set_name + ".tsv")).write_text(
            "\n".join(["name\tmask"] + [r[3] for r in rows]) + "\n"
        )
        (out / "tracks" / set_name).mkdir(exist_ok=True)
        for _, stem, _, _, tracks in rows:
            (out / "tracks" / set_name / (stem + ".tsv")).write_text(tracks)

    import scipy

    ref = ["reference_commit\t" + commit]
    ref += [
        "sha256\t%s\t%s"
        % (hashlib.sha256(p.read_bytes()).hexdigest(), p.relative_to(ROOT))
        for p in SOURCES
    ]
    ref += [
        "numpy\t" + np.__version__,
        "scipy\t" + scipy.__version__,
        "webrtcvad\t" + importlib.metadata.version("webrtcvad"),
        "pystoi\t" + importlib.metadata.version("pystoi"),
        "pyworld\t" + importlib.metadata.version("pyworld"),
        "reaper\t"
        + Path(shutil.which("reaper")).resolve().parents[1].name.split("-", 1)[1],
    ]
    (out / "REFERENCE.tsv").write_text("\n".join(ref) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--check",
        action="store_true",
        help="regenerate into a temporary directory and compare",
    )
    ap.add_argument(
        "--jobs",
        type=int,
        default=os.cpu_count() or 1,
        help="processes to run in parallel (default: one per CPU)",
    )
    args = ap.parse_args()
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp), args.jobs)
            diff = compare(Path(tmp), GOLDEN)
            if diff:
                print(
                    "differs from %s:\n  " % GOLDEN.relative_to(ROOT)
                    + "\n  ".join(diff)
                )
                return 1
            print(GOLDEN.relative_to(ROOT), "is reproduced exactly")
    else:
        shutil.rmtree(GOLDEN, ignore_errors=True)
        generate(GOLDEN, args.jobs)
        print("wrote", GOLDEN.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
