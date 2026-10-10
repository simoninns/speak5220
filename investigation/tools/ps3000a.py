"""
    ps3000a.py

    Minimal ctypes wrapper for the PicoScope ps3000a driver (PicoScope 3405D)

    The driver library comes from the PicoScope 7 installation. It is found
    from $PS3000A_LIB, or else next to the 'picoscope' executable on PATH.
    Its libusb and libstdc++ dependencies are preloaded from $PICO_DRIVER_DEPS
    (set by the flake's dev shell), so LD_LIBRARY_PATH is not needed.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import ctypes as C
import os
import shutil
import time

import numpy as np

# Input ranges (enum PS3000A_RANGE), in volts
RANGES_V = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50]
COUPLING_AC, COUPLING_DC = 0, 1
RISING, FALLING = 2, 3

PICO_OK = 0
# Open statuses that only mean "running from USB power" or "not on USB 3"
POWER_STATUSES = (0x119, 0x11A, 0x11E)
INFO_VARIANT, INFO_SERIAL = 3, 4
# MSO digital ports (enum PS3000A_CHANNEL): port 0 is D0-D7, port 1 D8-D15
DIGITAL_PORT0, DIGITAL_PORT1 = 0x80, 0x81
DIGITAL_FULL_SCALE_V = 5.0     # the logic level's int16 range spans +/-5 V


def _find_library():
    path = os.environ.get("PS3000A_LIB")
    if path:
        return path
    exe = shutil.which("picoscope")
    if exe:
        path = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(exe))),
                            "lib", "libps3000a.so")
        if os.path.exists(path):
            return path
    raise RuntimeError("libps3000a.so not found: install PicoScope 7 or set PS3000A_LIB")


def _load():
    for dep in filter(None, os.environ.get("PICO_DRIVER_DEPS", "").split(":")):
        C.CDLL(dep, mode=C.RTLD_GLOBAL)
    return C.CDLL(_find_library())


lib = _load()


def check(status, what):
    if status != PICO_OK:
        raise RuntimeError(f"{what} failed: PICO_STATUS 0x{status:X}")


class Scope:
    def __init__(self):
        self.handle = C.c_int16()
        status = lib.ps3000aOpenUnit(C.byref(self.handle), None)
        if status in POWER_STATUSES:
            check(lib.ps3000aChangePowerSource(self.handle, status), "ChangePowerSource")
        else:
            check(status, "OpenUnit")
        maxAdc = C.c_int16()
        check(lib.ps3000aMaximumValue(self.handle, C.byref(maxAdc)), "MaximumValue")
        self.maxAdc = maxAdc.value
        self.channels = {}  # channel -> (range V, offset V)
        self.ports = {}     # digital port -> logic threshold V

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def info(self, what):
        buf = C.create_string_buffer(64)
        required = C.c_int16()
        lib.ps3000aGetUnitInfo(self.handle, buf, 64, C.byref(required), what)
        return buf.value.decode()

    def setChannel(self, channel, rangeV, offsetV=0.0, coupling=COUPLING_DC, enabled=True):
        """Enable a channel. The analogue offset is added to the input before
        it is measured, so a signal centred on +0.5 V wants an offset of -0.5 V."""
        check(lib.ps3000aSetChannel(self.handle, channel, int(enabled), coupling,
                                    RANGES_V.index(rangeV), C.c_float(offsetV)),
              f"SetChannel {channel}")
        if enabled:
            self.channels[channel] = (rangeV, offsetV)
        else:
            self.channels.pop(channel, None)

    def setDigitalPort(self, port, thresholdV=0.0, enabled=True):
        """Enable an MSO digital port. Its bits switch at thresholdV; the
        inputs take +/-20 V, so the TMS5220's -5 V to +5 V pins connect
        directly. Bit n of each sample is channel Dn of port 0 (D(8 + n) of
        port 1)."""
        level = int(round(thresholdV / DIGITAL_FULL_SCALE_V * 32767))
        check(lib.ps3000aSetDigitalPort(self.handle, port, int(enabled), level),
              f"SetDigitalPort 0x{port:X}")
        if enabled:
            self.ports[port] = thresholdV
        else:
            self.ports.pop(port, None)

    def timebase(self, timebase, samples):
        """Returns (sample interval in seconds, maximum samples)."""
        interval = C.c_float()
        maxSamples = C.c_int32()
        check(lib.ps3000aGetTimebase2(self.handle, timebase, samples, C.byref(interval), 0,
                                      C.byref(maxSamples), 0), f"GetTimebase2 {timebase}")
        return interval.value * 1e-9, maxSamples.value

    def setTrigger(self, channel, volts, direction=RISING, autoMs=0):
        rangeV, offsetV = self.channels[channel]
        adc = int((volts + offsetV) / rangeV * self.maxAdc)
        check(lib.ps3000aSetSimpleTrigger(self.handle, 1, channel, adc, direction, 0, autoMs),
              "SetSimpleTrigger")

    def runBlock(self, timebase, samples, preTrigger=0, timeoutS=30, armed=None):
        """Capture one block. Returns (interval, {channel: int16 ADC array}, overflow).
        armed, if given, is called once the scope is waiting for its trigger."""
        interval, maxSamples = self.timebase(timebase, samples)
        if samples > maxSamples:
            raise ValueError(f"{samples} samples requested, maximum is {maxSamples}")
        indisposedMs = C.c_int32()
        check(lib.ps3000aRunBlock(self.handle, preTrigger, samples - preTrigger, timebase, 0,
                                  C.byref(indisposedMs), 0, None, None), "RunBlock")
        if armed:
            try:
                armed()
            except BaseException:
                lib.ps3000aStop(self.handle)
                raise
        ready = C.c_int16(0)
        start = time.time()
        while not ready.value:
            lib.ps3000aIsReady(self.handle, C.byref(ready))
            if time.time() - start > timeoutS:
                lib.ps3000aStop(self.handle)
                raise TimeoutError("no trigger")
            time.sleep(0.01)

        raw = {}
        for channel in list(self.channels) + list(self.ports):
            buf = np.zeros(samples, dtype=np.int16)
            check(lib.ps3000aSetDataBuffer(self.handle, channel,
                                           buf.ctypes.data_as(C.POINTER(C.c_int16)),
                                           samples, 0, 0), "SetDataBuffer")
            raw[channel] = buf
        count = C.c_uint32(samples)
        overflow = C.c_int16()
        check(lib.ps3000aGetValues(self.handle, 0, C.byref(count), 1, 0, 0, C.byref(overflow)),
              "GetValues")
        lib.ps3000aStop(self.handle)
        return interval, {ch: buf[:count.value] for ch, buf in raw.items()}, overflow.value

    def close(self):
        lib.ps3000aCloseUnit(self.handle)
