#!/usr/bin/env python3
"""
    pico.py

    Host side of the speak5220 firmware's USB command protocol
    (firmware/PROTOCOL.md): a client class, the VSM trace decoder and a
    command line. Run with no arguments for the list of subcommands.

    The Pico 2 is found by its USB ID (2e8a:0009, "Pico - Board CDC"),
    which keeps it apart from the Debug Probe's UART. Use --port to choose
    a port explicitly.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os
import re
import sys
import time
import zlib

import numpy as np
import serial
import serial.tools.list_ports

import lpc

PICO_VID, PICO_PIDS = 0x2E8A, (0x0009, 0x000A)
PROTOCOL = 1
HEX_LINE = 128          # hex digits per line when sending data

ROMDATA_ACORN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware",
                             "src", "romdata_acorn.h")
ACORN_BANK = 0xF

# "Acorn BBC Computer" (word numbers from the PHROM word list)
ACORN_PHRASE = [("ACORN", 0x3CBB6), ("B", 0x3D065), ("B", 0x3D065), ("C", 0x3D24D),
                ("COMPUTER", 0x3D40B)]

# Trace event types (firmware/src/tms6100.h)
EV_TIME, EV_LOAD_ADDRESS, EV_DUMMY_READ, EV_READ, EV_READ_BRANCH, EV_PINS, EV_START = range(7)
EVENT_NAMES = ["time", "load_address", "dummy_read", "read", "read_branch", "pins", "start"]

# TMS5220 status register and commands
STATUS_TS, STATUS_BL, STATUS_BE = 0x80, 0x40, 0x20
CMD_LOAD_ADDRESS, CMD_READ_BYTE, CMD_SPEAK, CMD_SPEAK_EXTERNAL, CMD_RESET = 0x40, 0x10, 0x50, 0x60, 0x70


class PicoError(RuntimeError):
    pass


def findPort():
    for port in serial.tools.list_ports.comports():
        if port.vid == PICO_VID and port.pid in PICO_PIDS:
            return port.device
    raise PicoError("no Pico 2 (USB 2e8a:0009) found; is the firmware running?")


def parseFields(text):
    """'OK a=1 b=0x2 word' -> {'a': '1', 'b': '0x2'}, ['word']"""
    fields, words = {}, []
    for token in text.split()[1:]:
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value
        else:
            words.append(token)
    return fields, words


def number(text):
    return int(text, 0)


class Pico:
    def __init__(self, port=None, timeout=30.0):
        self.port = serial.Serial(port or findPort(), timeout=timeout)
        self.port.reset_input_buffer()
        info = self.info()
        if int(info.get("proto", 0)) != PROTOCOL:
            raise PicoError(f"firmware speaks protocol {info.get('proto')}, expected {PROTOCOL}")
        self.firmware = info

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        self.port.close()

    def _readLine(self):
        line = self.port.readline()
        if not line.endswith(b"\n"):
            raise PicoError("timeout waiting for the Pico")
        return line.decode("ascii", "replace").strip()

    def _send(self, line):
        self.port.write(line.encode("ascii") + b"\n")

    def _reply(self, lines=None):
        """Read lines up to the final OK/ERR line; returns (fields, words)."""
        while True:
            line = self._readLine()
            if line.startswith("OK"):
                return parseFields(line)
            if line.startswith("ERR"):
                raise PicoError(line[4:])
            if lines is not None:
                lines.append(line)

    def command(self, line, lines=None):
        self._send(line)
        return self._reply(lines)

    def _sendData(self, line, data):
        self._send(line)
        reply = self._readLine()
        if reply.startswith("ERR"):
            raise PicoError(reply[4:])
        if reply != "GO":
            raise PicoError(f"expected GO, got {reply!r}")
        text = data.hex()
        for i in range(0, len(text), HEX_LINE):
            self.port.write(text[i:i + HEX_LINE].encode("ascii") + b"\n")
        return self._reply()

    # Commands

    def info(self):
        return self.command("INFO")[0]

    def stats(self):
        return self.command("STATS")[0]

    def clock(self, hz):
        return int(self.command(f"CLOCK {int(hz)}")[0]["osc_hz"])

    def reset(self):
        return number(self.command("RESET")[0]["status"])

    def upload(self, bank, data):
        data = bytes(data)
        return self._sendData(f"UPLOAD {bank} {len(data)} 0x{zlib.crc32(data):08x}", data)[0]

    def unload(self, bank="ALL"):
        self.command(f"UNLOAD {bank}")

    def banks(self):
        """{bank: CRC-32 of the 16K image} for every loaded bank."""
        text = self.info()["banks"]
        return {} if text == "-" else {int(b): int(c, 16) for b, c in
                                       (item.split(":") for item in text.split(","))}

    @staticmethod
    def _talk(fields):
        result = {key: number(fields[key]) for key in ("status", "talked", "ints", "reads", "us")}
        for key in ("written", "trig_us", "late_us"):
            if key in fields:
                result[key] = number(fields[key])
        return result

    def time(self):
        """The Pico's microsecond timer, which runs from the same crystal as OSC."""
        return number(self.command("TIME")[0]["us"])

    def speak(self, address, timeoutMs=10000, atUs=None):
        """VSM Speak. With atUs, SCOPE_TRIG rises and the command starts when
        the microsecond timer reaches atUs (see time())."""
        at = "" if atUs is None else f" {int(atUs)}"
        return self._talk(self.command(f"SPEAK 0x{address:05x} {timeoutMs}{at}")[0])

    def speakExternal(self, data, timeoutMs=10000, atUs=None):
        data = bytes(data)
        at = "" if atUs is None else f" {int(atUs)}"
        return self._talk(self._sendData(
            f"SPEAKEXT {len(data)} 0x{zlib.crc32(data):08x} {timeoutMs}{at}", data)[0])

    def wait(self, timeoutMs=10000):
        return self._talk(self.command(f"WAIT {timeoutMs}")[0])

    def cmd(self, *values):
        self.command("CMD " + " ".join(f"0x{v:02x}" for v in values))

    def read(self, count=1):
        return [number(v) for v in self.command(f"READ {count}")[1]]

    def trig(self, on):
        self.command(f"TRIG {int(bool(on))}")

    def traceClear(self):
        self.command("TRACE CLEAR")

    def trace(self):
        """Returns (raw event bytes exactly as the firmware holds them, metadata)."""
        self._send("TRACE")
        header = self._readLine()
        if header.startswith("ERR"):
            raise PicoError(header[4:])
        meta = {k: int(v) for k, v in parseFields(header)[0].items()}
        hexLines = []
        fields, _ = self._reply(hexLines)
        raw = bytes.fromhex("".join(hexLines))
        if len(raw) != meta["count"] * 8 or zlib.crc32(raw) != int(fields["crc"], 16):
            raise PicoError("trace transfer corrupted")
        meta["crc"] = fields["crc"]
        return raw, meta


# ---------------------------------------------------------------------
# VSM trace decoding

TRACE_CLOCK_HZ = 144000000     # the firmware's trace timestamp clock (its system clock)

TRACE_DTYPE = [("time_s", "f8"), ("cycles", "u8"), ("type", "u1"), ("address", "i4"),
               ("nibble", "i1"), ("bit_number", "i1"), ("bit", "i1"), ("driven", "i1"),
               ("data", "i2"), ("ws", "i1"), ("rs", "i1"), ("ready", "i1"), ("int", "i1"),
               ("trig", "i1")]


def decodeTrace(raw, clockHz):
    """Decode raw trace bytes into a numpy record array (one row per event).
    Fields that do not apply to an event are -1."""
    words = np.frombuffer(raw, dtype="<u4").reshape(-1, 2)
    time32, event = words[:, 0].astype(np.uint64), words[:, 1]
    cycles = np.zeros(len(words), dtype=np.uint64)
    if len(words):
        cycles[1:] = np.cumsum((np.diff(time32.astype(np.int64)) % (1 << 32)).astype(np.uint64))
    kind = (event >> 28).astype(np.uint8)
    out = np.full(len(words), -1, dtype=TRACE_DTYPE)
    out["time_s"] = cycles / clockHz
    out["cycles"] = cycles
    out["type"] = kind

    vsm = (kind >= EV_LOAD_ADDRESS) & (kind <= EV_READ_BRANCH)
    out["address"][vsm] = event[vsm] & 0xFFFFF
    m = kind == EV_LOAD_ADDRESS
    out["nibble"][m] = (event[m] >> 20) & 0xF
    m = (kind == EV_DUMMY_READ) | (kind == EV_READ_BRANCH)
    out["driven"][m] = (event[m] >> 20) & 1
    m = kind == EV_READ
    out["bit_number"][m] = (event[m] >> 20) & 7
    out["bit"][m] = (event[m] >> 23) & 1
    out["driven"][m] = (event[m] >> 24) & 1
    m = (kind == EV_PINS) | (kind == EV_START)
    out["data"][m] = event[m] & 0xFF
    for name, bit in (("ws", 8), ("rs", 9), ("ready", 10), ("int", 11), ("trig", 12)):
        out[name][m] = (event[m] >> bit) & 1
    return out


def hostCycles(trace):
    """The host interface cycles in a decoded trace, from its PINS events:
    a list of (kind 'write'/'read', byte, strobe time, READY high time,
    READY low time, strobe end time). Times are in seconds."""
    pins = trace[(trace["type"] == EV_PINS) | (trace["type"] == EV_START)]
    cycles, current = [], None
    for prev, row in zip(pins[:-1], pins[1:]):
        for strobe, kind in (("ws", "write"), ("rs", "read")):
            if prev[strobe] == 1 and row[strobe] == 0:
                current = {"kind": kind, "byte": int(row["data"]) if kind == "write" else None,
                           "start": row["time_s"], "ready_high": None, "ready_low": None}
            elif prev[strobe] == 0 and row[strobe] == 1 and current and current["kind"] == kind:
                # The firmware ends a cycle as soon as it sees READY go low,
                # so the two can share one event (Phase 7)
                if (prev["ready"] == 1 and row["ready"] == 0 and current["ready_high"] is not None
                        and current["ready_low"] is None):
                    current["ready_low"] = row["time_s"]
                    if kind == "read":
                        current["byte"] = int(row["data"])
                current["end"] = row["time_s"]
                cycles.append(current)
                current = None
        if current:
            if prev["ready"] == 0 and row["ready"] == 1 and current["ready_high"] is None:
                current["ready_high"] = row["time_s"]
            elif prev["ready"] == 1 and row["ready"] == 0 and current["ready_high"] is not None:
                current["ready_low"] = row["time_s"]
                if current["kind"] == "read":
                    current["byte"] = int(row["data"])
    return cycles


def bitsRead(trace):
    """The bits the TMS5220 read from the VSM, as (address, bit number, bit) arrays."""
    reads = trace[trace["type"] == EV_READ]
    return reads["address"], reads["bit_number"], reads["bit"]


def writeTrace(directory, raw, meta):
    """Save raw/vsm_trace.bin (the firmware's events, unchanged) and
    vsm_trace.csv (decoded). Returns the decoded trace."""
    os.makedirs(os.path.join(directory, "raw"), exist_ok=True)
    with open(os.path.join(directory, "raw", "vsm_trace.bin"), "wb") as f:
        f.write(raw)
    return writeTraceCsv(directory, meta["clock_hz"])


def writeTraceCsv(directory, clockHz=TRACE_CLOCK_HZ):
    """(Re)build vsm_trace.csv from raw/vsm_trace.bin. Returns the decoded trace."""
    with open(os.path.join(directory, "raw", "vsm_trace.bin"), "rb") as f:
        trace = decodeTrace(f.read(), clockHz)
    with open(os.path.join(directory, "vsm_trace.csv"), "w", newline="") as f:
        writer = csv.writer(f)
        names = [name for name, _ in TRACE_DTYPE]
        writer.writerow(["event", "time_us"] + names[1:])
        for row in trace:
            values = [f"{row['time_s'] * 1e6:.3f}"] + [int(row[n]) for n in names[1:]]
            values[0:0] = [EVENT_NAMES[row["type"]]]
            writer.writerow(values)
    return trace


# ---------------------------------------------------------------------
# Built-in Acorn PHROM image

def acornImage():
    """The Acorn PHROM A image held in firmware/src/romdata_acorn.h."""
    with open(ROMDATA_ACORN) as f:
        text = f.read()
    body = text[text.index("phromData[16384] = {"):]
    body = body[body.index("{") + 1:body.index("}")]
    data = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body))
    if len(data) != 16384:
        raise ValueError(f"expected 16384 bytes in {ROMDATA_ACORN}, found {len(data)}")
    return data


def loadImage(path):
    return acornImage() if path == "builtin" else open(path, "rb").read()


def reverseBits(value):
    """Read Byte (command 0x1x) returns the VSM byte with its bits reversed:
    the first bit read (the byte's LSB) arrives on D0, which is TI's MSB."""
    return int(f"{value:08b}"[::-1], 2)


def imageOffset(address, base=None):
    """Where a VSM address falls in an image that starts at VSM address
    base; by default the image is the address's own 16K bank."""
    return (address & 0x3FFF) if base is None else address - base


def wordBytes(image, address, base=None):
    """The bytes of the word at a VSM address, up to the byte holding its stop code."""
    offset = imageOffset(address, base)
    frames, bits = lpc.parseFrames(lpc.bitsLsbFirst(image[offset:]))
    if not frames or frames[-1]["kind"] != "stop":
        raise ValueError(f"no stop frame after 0x{address:05x}")
    return image[offset:offset + (bits + 7) // 8]


# ---------------------------------------------------------------------
# Self test (plan Phase 3 "done when")

# Speak External and VSM durations of the same word may differ by the start
# delay, which varies by up to 25 samples (plan section 2), plus margin
EXTERNAL_TOLERANCE_US = 30000

SELFTEST_WORDS = ACORN_PHRASE[:2] + ACORN_PHRASE[3:] + [("ZERO", 0x3C437), ("YES", 0x3FEFC)]
SECOND_BANK = 0x3


def checkVsmBits(trace, image, address, label, base=None):
    """The bits read from the VSM must be the image's bits from the word
    address on, and must cover the word up to its stop code."""
    addr, bitNumber, bit = bitsRead(trace)
    expected = lpc.bitsLsbFirst(image[imageOffset(address, base):])
    frames, length = lpc.parseFrames(expected)
    n = len(bit)
    problems = []
    if n < length:
        problems.append(f"{label}: read {n} bits, the word needs {length}")
    start = (address & 0xFFFFF)
    expectedAddr = start + np.arange(n) // 8
    if n and (not np.array_equal(addr, expectedAddr & 0xFFFFF)
              or not np.array_equal(bitNumber, np.arange(n) % 8)):
        problems.append(f"{label}: VSM read sequence does not follow the word address")
    if n and not np.array_equal(bit, expected[:n]):
        problems.append(f"{label}: bits read differ from the image")
    return problems, n, length, len(frames)


def selftest(pico, repeat, log=print):
    problems = []
    image = acornImage()
    log(f"firmware {pico.firmware.get('firmware')}, OSC {pico.firmware.get('osc_hz')} Hz")

    # Images: the built-in PHROM uploaded to its own bank and to a second
    # one, after emptying the slots (the runner may have left others loaded)
    pico.unload()
    for bank in (ACORN_BANK, SECOND_BANK):
        pico.upload(bank, image)
    banks = pico.banks()
    for bank in (ACORN_BANK, SECOND_BANK):
        if banks.get(bank) != zlib.crc32(image):
            problems.append(f"bank {bank:X} CRC mismatch after upload")
    log(f"uploaded the Acorn PHROM to banks {ACORN_BANK:X} and {SECOND_BANK:X}")

    # The datasheet's reset sequence. (The clock is left running: stopping
    # it can leave the chip unable to complete a write until it is power
    # cycled, firmware/PROTOCOL.md.)
    status = pico.reset()
    log(f"reset: status 0x{status:02x}")

    # Read Byte through CMD/READ: load an address, then command 0x10
    address = ACORN_PHRASE[0][1]
    pico.cmd(*[CMD_LOAD_ADDRESS | ((address >> (4 * i)) & 0xF) for i in range(5)], CMD_READ_BYTE)
    value = pico.read(1)[0]
    imageByte = image[address & 0x3FFF]
    expectedByte = reverseBits(imageByte)
    log(f"Read Byte at 0x{address:05x}: 0x{value:02x} (image 0x{imageByte:02x}, "
        f"bit-reversed 0x{expectedByte:02x})")
    if value != expectedByte:
        problems.append(f"Read Byte returned 0x{value:02x}, expected 0x{expectedByte:02x}")
    pico.reset()

    for run in range(repeat):
        for name, address in SELFTEST_WORDS:
            durations = []
            for bank, label in ((ACORN_BANK, "VSM"), (SECOND_BANK, f"VSM bank {SECOND_BANK:X}")):
                addr = (address & ~0x3C000) | (bank << 14)
                pico.traceClear()
                result = pico.speak(addr)
                raw, meta = pico.trace()
                trace = decodeTrace(raw, meta["clock_hz"])
                p, n, length, frames = checkVsmBits(trace, image, addr, f"{name} {label}")
                # BE is only meaningful during Speak External; here it is
                # left over from the last Reset or Speak External
                if not result["talked"] or result["status"] & STATUS_TS:
                    p.append(f"{name} {label}: status 0x{result['status']:02x}, talked {result['talked']}")
                durations.append(result["us"])
                if meta["dropped"]:
                    p.append(f"{name} {label}: {meta['dropped']} trace events dropped")
                problems += p
                log(f"[{run + 1}] {name:9s} {label:11s} 0x{addr:05x}: {result['us'] / 1e3:7.1f} ms, "
                    f"{frames:3d} frames, {length:4d} bits needed, {n:4d} read, "
                    f"{meta['count']:5d} events, ints {result['ints']}, "
                    f"status 0x{result['status']:02x} {'ok' if not p else 'FAIL'}")

            data = wordBytes(image, address)
            pico.traceClear()
            result = pico.speakExternal(data)
            raw, meta = pico.trace()
            trace = decodeTrace(raw, meta["clock_hz"])
            written = [c["byte"] for c in hostCycles(trace) if c["kind"] == "write"]
            p = []
            # BE is set at the end when the stop code ends exactly on a byte
            # boundary (the FIFO empties as it is read); the speech is still
            # complete, so the duration is compared with the VSM's instead
            if not result["talked"] or result["status"] & STATUS_TS or result["written"] != len(data):
                p.append(f"{name} Speak External: status 0x{result['status']:02x}, "
                         f"talked {result['talked']}, {result['written']} of {len(data)} bytes written")
            if abs(result["us"] - np.mean(durations)) > EXTERNAL_TOLERANCE_US:
                p.append(f"{name} Speak External: took {result['us'] / 1e3:.1f} ms, "
                         f"the VSM {np.mean(durations) / 1e3:.1f} ms")
            if written[1:] != list(data[:len(written) - 1]) or written[:1] != [CMD_SPEAK_EXTERNAL]:
                p.append(f"{name} Speak External: the bytes on the bus differ from the stream")
            if bitsRead(trace)[0].size:
                p.append(f"{name} Speak External: the VSM was read")
            problems += p
            log(f"[{run + 1}] {name:9s} {'external':11s} {len(data):5d} B: {result['us'] / 1e3:7.1f} ms, "
                f"{result['written']:4d} bytes written, ints {result['ints']}, "
                f"status 0x{result['status']:02x} {'ok' if not p else 'FAIL'}")

    # Leave the built-in image as it was at boot
    pico.unload(SECOND_BANK)
    pico.upload(ACORN_BANK, image)
    return problems


# ---------------------------------------------------------------------
# Command line

def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("--port", help="serial port (default: find the Pico 2 by USB ID)")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("info", help="firmware version, clocks and loaded banks")
    sub.add_parser("stats", help="VSM activity counters")
    sub.add_parser("reset", help="nine 0xFF bytes and a Reset command")
    p = sub.add_parser("clock", help="set the TMS5220 OSC frequency (0 stops it)")
    p.add_argument("hz", type=int)
    p = sub.add_parser("upload", help="load a VSM image (up to 16K) into a bank")
    p.add_argument("bank", type=lambda s: int(s, 0))
    p.add_argument("image", help="binary file, or 'builtin' for the Acorn PHROM")
    p = sub.add_parser("unload", help="empty a bank (or ALL)")
    p.add_argument("bank")
    p = sub.add_parser("speak", help="speak from a VSM address")
    p.add_argument("address", type=lambda s: int(s, 0))
    p.add_argument("--trace", metavar="DIR", help="save the VSM trace to DIR")
    p = sub.add_parser("speakext", help="Speak External: a byte stream, or a word of an image")
    p.add_argument("source", help="binary file, or an image ('builtin' or a file) with --word")
    p.add_argument("--word", type=lambda s: int(s, 0), metavar="ADDRESS",
                   help="send the word at this VSM address of the image")
    p.add_argument("--trace", metavar="DIR", help="save the VSM trace to DIR")
    p = sub.add_parser("cmd", help="write bytes to the TMS5220")
    p.add_argument("bytes", nargs="+", type=lambda s: int(s, 0))
    p = sub.add_parser("read", help="read the status (or data) register")
    p.add_argument("count", type=int, nargs="?", default=1)
    p = sub.add_parser("trace", help="save the VSM trace to DIR")
    p.add_argument("dir")
    p = sub.add_parser("trace-csv", help="rebuild DIR/vsm_trace.csv from DIR/raw/vsm_trace.bin "
                                         "(offline, no Pico needed)")
    p.add_argument("dir")
    sub.add_parser("demo", help="say \"Acorn BBC Computer\" once")
    p = sub.add_parser("selftest", help="upload, speak (VSM and Speak External) and trace, "
                                       "checking every result")
    p.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()

    if args.command == "trace-csv":
        trace = writeTraceCsv(args.dir)
        print(f"{len(trace)} events -> {os.path.join(args.dir, 'vsm_trace.csv')}")
        return

    with Pico(args.port) as pico:
        def saveTrace(directory):
            raw, meta = pico.trace()
            writeTrace(directory, raw, meta)
            with open(os.path.join(directory, "vsm_trace.json"), "w") as f:
                json.dump(meta, f, indent=2)
            print(f"trace: {meta['count']} events ({meta['dropped']} dropped) -> {directory}")

        if args.command == "info":
            print(json.dumps(pico.info(), indent=2))
        elif args.command == "stats":
            print(json.dumps(pico.stats(), indent=2))
        elif args.command == "reset":
            print(f"status 0x{pico.reset():02x}")
        elif args.command == "clock":
            print(f"OSC {pico.clock(args.hz)} Hz")
        elif args.command == "upload":
            print(pico.upload(args.bank, loadImage(args.image)))
        elif args.command == "unload":
            pico.unload(args.bank)
        elif args.command == "speak":
            if args.trace:
                pico.traceClear()
            print(pico.speak(args.address))
            if args.trace:
                saveTrace(args.trace)
        elif args.command == "speakext":
            data = loadImage(args.source)
            if args.word is not None:
                data = wordBytes(data, args.word)
            if args.trace:
                pico.traceClear()
            print(pico.speakExternal(data))
            if args.trace:
                saveTrace(args.trace)
        elif args.command == "cmd":
            pico.cmd(*args.bytes)
        elif args.command == "read":
            print(" ".join(f"0x{v:02x}" for v in pico.read(args.count)))
        elif args.command == "trace":
            saveTrace(args.dir)
        elif args.command == "demo":
            pico.reset()
            for name, address in ACORN_PHRASE:
                result = pico.speak(address)
                print(f"{name:9s} 0x{address:05x}: {result['us'] / 1e3:.1f} ms, "
                      f"status 0x{result['status']:02x}")
        elif args.command == "selftest":
            problems = selftest(pico, args.repeat)
            for problem in problems:
                print("FAIL:", problem)
            print("self test", "FAILED" if problems else "passed")
            sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
