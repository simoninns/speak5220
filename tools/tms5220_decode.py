"""
    tms5220_decode.py

    Decodes a raw PicoScope capture of the TMS5220 (see capture.py):

      Ch A  SPEAKER (DAC) across the 1K load (TP2)
      Ch B  T11 (TP3)
      Ch C  I/O (TP4)
      Ch D  SCOPE_TRIG (TP5)

    I/O carries each output sample as a 10-bit two's complement value, LSB
    first, in bit slots 2-11 of the 20 slots (6.25us each at 640kHz) that
    follow the rising edge of T11. The DAC outputs the same sample from
    6.25us after that edge.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import numpy as np

CHANNELS = "abcd"
SAMPLE_PERIOD = 125e-6
SLOT = SAMPLE_PERIOD / 20
DATA_SLOTS = range(2, 12)
# Settled part of each DAC step, relative to the T11 rising edge
DAC_WINDOW = (12e-6, 128e-6)


def loadRaw(path):
    """Returns (sample interval, [volts per channel A-D]) from scope_raw.npz.
    Thresholds are applied at the scope input (probe attenuation is not removed)."""
    raw = np.load(path)
    interval = float(raw["interval"])
    volts = [raw[ch] / float(raw["maxAdc"]) * raw["range"][i] - raw["offset"][i]
             for i, ch in enumerate(CHANNELS)]
    return interval, volts


def decode(interval, volts, digitalThreshold=0.1):
    """Decode every complete sample period.

    Returns a dict of per-sample arrays:
      edge   scope sample index of the T11 rising edge
      bits   the 20 I/O slot levels
      code   decoded signed 10-bit sample
      dac    mean DAC voltage over the settled window
      trig   SCOPE_TRIG level during the sample
    """
    dacV, t11, io, trig = volts
    edges = np.flatnonzero(np.diff((t11 > digitalThreshold / 2).astype(np.int8)) == 1) + 1
    edges = edges[edges + int(DAC_WINDOW[1] / interval) < len(dacV)]

    slotCentres = ((np.arange(20) + 0.5) * SLOT / interval).astype(int)
    bits = (io[edges[:, None] + slotCentres[None, :]] > digitalThreshold).astype(np.int64)
    unsigned = bits[:, DATA_SLOTS.start:DATA_SLOTS.stop] @ (1 << np.arange(10))
    code = np.where(unsigned >= 512, unsigned - 1024, unsigned)

    w0, w1 = (int(t / interval) for t in DAC_WINDOW)
    cumulative = np.concatenate([[0.0], np.cumsum(dacV)])
    dac = (cumulative[edges + w1] - cumulative[edges + w0]) / (w1 - w0)

    trigLevel = trig[edges + int(SAMPLE_PERIOD / 2 / interval)] > digitalThreshold
    return {"edge": edges, "bits": bits, "code": code, "dac": dac, "trig": trigLevel}


def unusedSlotErrors(bits):
    """Count samples with a 1 outside the data slots (should be zero)."""
    mask = np.ones(20, bool)
    mask[DATA_SLOTS.start:DATA_SLOTS.stop] = False
    return int(bits[:, mask].any(1).sum())


def triggerWindows(interval, volts, edges, threshold=0.1):
    """Split the capture on SCOPE_TRIG (one window per Speak).
    Returns a list of (first sample, end sample) indices into the decoded arrays."""
    level = (volts[3] > threshold).astype(np.int8)
    changes = np.flatnonzero(np.diff(level)) + 1
    rises = changes[level[changes] == 1]
    falls = changes[level[changes] == 0]
    if level[0]:
        rises = np.r_[0, rises]
    windows = []
    for rise in rises:
        fall = falls[falls > rise]
        end = fall[0] if len(fall) else len(level)
        windows.append((int(np.searchsorted(edges, rise)), int(np.searchsorted(edges, end))))
    return windows
