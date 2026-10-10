"""
    lpc.py

    TMS5220 LPC frame parsing: the frame boundaries and the coded
    parameter indices of each frame (frames.csv, plan section 6). It gives
    the indices the chip was sent, not the values it looks them up to
    (reference datasheet section 8.1). encodeFrames writes frames back
    to bytes.

    The chip reads its data serially, least significant bit of each byte
    first (the original datasheet: the FIFO shifts data out "starting with
    the LSB"; the TMS6100 sends its bytes the same way). A frame is:

      energy 4 bits     0 = silence (frame ends), 15 = stop code (speech ends)
      repeat 1 bit
      pitch  6 bits     0 = unvoiced
      K1-K4  5,5,4,4    absent if repeat = 1
      K5-K10 4,4,4,3,3,3 absent if repeat = 1 or pitch = 0

    Each parameter is taken most significant bit first from the bit stream
    (confirmed in Phase 8, datasheet section 8.1). The frame boundaries
    do not depend on this: energy 0 and 15 and pitch 0 read the same
    either way.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import numpy as np

K_BITS_VOICED = [5, 5, 4, 4, 4, 4, 4, 3, 3, 3]
K_BITS_UNVOICED = K_BITS_VOICED[:4]
FRAME_SAMPLES = 200     # 25 ms at 8 kHz
FRAME_COLUMNS = (["frame", "bit_offset", "bits", "kind", "energy", "repeat", "pitch"]
                 + [f"k{i}" for i in range(1, 11)])


def bitsLsbFirst(data):
    """The bit stream the chip reads from a byte sequence."""
    return np.unpackbits(np.frombuffer(bytes(data), dtype=np.uint8), bitorder="little")


def parseFrames(bits, maxFrames=100000, pitchBits=6):
    """Parse frames up to and including the stop code. Returns (frames, bits
    consumed); each frame is a dict with its bit offset, kind and fields.
    Stops early, without a stop frame, if the bits run out. pitchBits=5
    parses the TMS5100's format instead, which differs only in the width of
    the pitch field (used to identify a PROM's target chip, promimport.py)."""
    position = 0
    frames = []

    def take(n):
        nonlocal position
        if position + n > len(bits):
            raise EOFError
        value = 0
        for bit in bits[position:position + n]:
            value = (value << 1) | int(bit)
        position += n
        return value

    try:
        while len(frames) < maxFrames:
            start = position
            energy = take(4)
            if energy == 0:
                frames.append({"offset": start, "kind": "silence", "energy": 0})
                continue
            if energy == 15:
                frames.append({"offset": start, "kind": "stop", "energy": 15})
                break
            repeat = take(1)
            pitch = take(pitchBits)
            frame = {"offset": start, "energy": energy, "repeat": repeat, "pitch": pitch}
            if repeat:
                frame["kind"] = "repeat"
            else:
                bitsK = K_BITS_VOICED if pitch else K_BITS_UNVOICED
                frame["k"] = [take(n) for n in bitsK]
                frame["kind"] = "voiced" if pitch else "unvoiced"
            frames.append(frame)
    except EOFError:
        pass
    for frame, end in zip(frames, [f["offset"] for f in frames[1:]] + [position]):
        frame["bits"] = end - frame["offset"]
    return frames, position


def frameRow(index, frame):
    """One frames.csv row: the frame's fields, with '' for fields it does not
    carry (K parameters of repeat, silence and stop frames, K5-K10 of
    unvoiced frames)."""
    k = frame.get("k", [])
    return ([index, frame["offset"], frame["bits"], frame["kind"], frame["energy"],
             frame.get("repeat", ""), frame.get("pitch", "")]
            + [k[i] if i < len(k) else "" for i in range(10)])


def frameBits(frame, pitchBits=6):
    """The bits of one frame (a parseFrames dict, or one built by hand with
    energy, and repeat, pitch and k where they apply), each field most
    significant bit first: the inverse of parseFrames."""
    def field(value, n):
        if not 0 <= value < (1 << n):
            raise ValueError(f"{value} does not fit in {n} bits")
        return [(value >> (n - 1 - i)) & 1 for i in range(n)]

    bits = field(frame["energy"], 4)
    if frame["energy"] in (0, 15):
        return bits
    bits += field(frame.get("repeat", 0), 1) + field(frame["pitch"], pitchBits)
    if frame.get("repeat", 0):
        return bits
    widths = K_BITS_VOICED if frame["pitch"] else K_BITS_UNVOICED
    if len(frame["k"]) < len(widths):
        raise ValueError(f"frame needs {len(widths)} K parameters")
    for value, n in zip(frame["k"], widths):
        bits += field(value, n)
    return bits


def encodeFrames(frames):
    """The byte stream for a list of frames, packed as the chip reads it
    (least significant bit of each byte first), padded with zero bits."""
    bits = [b for frame in frames for b in frameBits(frame)]
    bits += [0] * (-len(bits) % 8)
    return np.packbits(np.array(bits, dtype=np.uint8), bitorder="little").tobytes()
