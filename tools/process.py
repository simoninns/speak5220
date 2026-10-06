#!/usr/bin/env python3
"""
    process.py

    Builds the readable files of a capture directory (plan section 6) from
    its raw/ files, checks them, and records the results in capture.json
    under "processed". It can be rerun at any time: raw/ is only read.

      vsm_trace.csv  layer 1: the VSM and host interface trace, decoded
      frames.csv     the frames the chip parsed: bit offset, length, kind,
                     energy, repeat, pitch, K1-K10 (coded indices), and for
                     the VSM path when the frame's first bit was read
      samples.wav    layer 2: the I/O samples from SCOPE_TRIG rising to the
                     end of the capture, 16-bit PCM at 8 kHz, code x 64
      samples.csv    layer 2 as text, with the DAC voltage (layer 3) and the
                     DAC code recovered from it
      dac.wav        layer 3: the recovered DAC codes, code x 64
      listen.wav     for listening only: the DAC codes held for each sample
                     period at 48 kHz, through a model of the BBC Micro's
                     audio filter (--listen-filter none leaves it out)

    Checks (each true, false or null when it does not apply):

      scope_overflow    no channel went over range
      io_format         no I/O activity outside bit slots 2-11
      fosc              the T11 rate gives OSC within 50 ppm of the set clock
      trace_complete    no trace events were dropped
      talk_status       Talk Status was seen set, and was clear at the end
      stop_frame        the parsed data ends with a stop frame
      vsm_image         VSM: the bits read are the image's, from the word address
      bits_vs_frames    VSM: the bits read are exactly the frames up to the stop code
      frame_period      VSM: one frame read every 25 ms
      duration          the talk time matches the frame count
      external_stream   Speak External: the bytes on the bus are the stream sent
      start_frame       timed starts: the speech started on the frame it was timed
                        for (VSM: the first read is on the frame grid)
      start_slot        standard procedure: the chip accepted the Speak in the
                        target slot before that frame
      layer2_vs_layer3  the DAC voltage follows the I/O codes (plan section 3)

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import csv
import json
import os
import subprocess
import time
import wave
import zlib

import numpy as np
from scipy import signal

import frameclock
import lpc
import pico as picolib
import prom as promlib
import tms5220_decode as td

PROCESS_VERSION = 2
ROOT = promlib.ROOT

# Phase 2 DAC tables per board configuration (captures/phase2, datasheet 8.2)
DAC_TABLES = {
    "measure": "captures/phase2/dac_transfer/dac_table.csv",
    "measure-half": "captures/phase2/dac_transfer_half_load/dac_table.csv",
}

# Check limits
FOSC_PPM = 50
FRAME_PERIOD_TOLERANCE_S = 0.1e-3
VSM_DURATION_TOLERANCE_S = 2e-3        # talk end - first frame read, against frames x 25 ms
EXTERNAL_START_SAMPLES = (0, 400)      # talk window - frames x 200, Speak External
LAYER3_RMS_MV = 3.0                    # Phase 2: 1.8 mV, 2.3 mV during speech
# |gain - 1| against the Phase 2 table. This catches a wrong board
# configuration (a half load gives 0.5), not the absolute scale: that has
# drifted by 8 % since Phase 2 (idle -1 at 0.592 V then, 0.635 V on
# 2026-10-06 in Phase 4), so the gain is recorded in every capture
LAYER3_GAIN = 0.15
LAYER3_WITHIN = (2, 0.99)              # this fraction of codes recovered within +/- 2

LISTEN_RATE = 48000

# Timed starts (frameclock.py). A VSM Speak's first read must fall on the
# grid to within the trace's timestamp lateness plus the timer's 1 us; a
# timed Speak External must not start late (it is timed for mid-frame).
GRID_TOLERANCE_US = 5.0
START_LATE_US = 1000


def writeWav(path, codes, rate=8000, shift=6):
    """16-bit mono PCM of integer codes, left-justified by `shift` bits."""
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.asarray(codes).astype(np.int32) << shift).astype("<i2").tobytes())


def listenAudio(codes, filterName="bbc"):
    """The DAC output (codes clipped to -128..127, each held for one sample
    period) at 48 kHz, optionally through the BBC audio stage: a 279 Hz
    1st-order high-pass and two 2nd-order Butterworth low-passes at 6.95 kHz
    (docs-tech/audio_output_stage.md). Scaled so that code 127 is half scale."""
    factor = LISTEN_RATE // 8000
    x = np.repeat(np.clip(codes, -128, 127).astype(float), factor) * 128
    if filterName == "bbc":
        x -= x[0]
        for kind, order, hz in (("highpass", 1, 279.0), ("lowpass", 2, 6950.0),
                                ("lowpass", 2, 6950.0)):
            b, a = signal.butter(order, hz, kind, fs=LISTEN_RATE)
            x = signal.lfilter(b, a, x)
    return np.clip(np.round(x), -32768, 32767).astype(np.int16)


def dacModel(board):
    """The Phase 2 bit-weight model for the board configuration: TP2 volts
    for DAC codes -128..127, or None if there is none (audio stage fitted)."""
    path = DAC_TABLES.get(board)
    if not path or not os.path.exists(os.path.join(ROOT, path)):
        return None, path
    table = np.genfromtxt(os.path.join(ROOT, path), delimiter=",", names=True)
    model = np.full(256, np.nan)
    model[table["code"].astype(int) + 128] = table["model_v"]
    return model, path


def layer3(code, dacV, model):
    """Compare the DAC voltage with the Phase 2 model of the I/O codes.
    The model is fitted to this capture with a gain and an offset (the
    scope's offset drifts), then each DAC voltage is mapped back to the
    nearest model code. Returns (recovered codes, statistics)."""
    clipped = np.clip(code, -128, 127)
    predicted = model[clipped + 128]
    gain, offset = np.polyfit(predicted, dacV, 1)
    residual = dacV - (gain * predicted + offset)
    levels = gain * model + offset
    recovered = np.argmin(np.abs(dacV[:, None] - levels[None, :]), axis=1) - 128
    difference = np.abs(recovered - clipped)
    stats = {"gain": float(gain), "offset_mv": float(offset * 1e3),
             "residual_rms_mv": float(np.sqrt(np.mean(residual ** 2)) * 1e3),
             "residual_max_mv": float(np.abs(residual).max() * 1e3),
             "recovered_exact": float(np.mean(difference == 0)),
             f"recovered_within_{LAYER3_WITHIN[0]}": float(np.mean(difference <= LAYER3_WITHIN[0])),
             "codes_clipped": int(np.sum(clipped != code))}
    stats["pass"] = bool(stats["residual_rms_mv"] <= LAYER3_RMS_MV
                         and abs(gain - 1) <= LAYER3_GAIN
                         and stats[f"recovered_within_{LAYER3_WITHIN[0]}"] >= LAYER3_WITHIN[1])
    return recovered, stats


def gitHash():
    """git describe of the host tools (with -dirty for uncommitted changes)."""
    try:
        return subprocess.check_output(["git", "describe", "--always", "--dirty", "--abbrev=7"],
                                       cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def edgesOf(values, level):
    """Indices of rows where a 0/1 column changes to `level`."""
    v = np.asarray(values)
    return np.flatnonzero((v[1:] == level) & (v[:-1] == 1 - level)) + 1


def commandPath(meta):
    command = (meta.get("pico") or {}).get("command") or {}
    if "speak" in command:
        return "vsm"
    if "speakext" in command:
        return "external"
    return None


def sourceImage(meta):
    """The VSM image the capture spoke from, if its manifest and image are
    available. Captures made before run.py have no "source"; for those the
    CRC of the bank spoken from is looked up among the manifests."""
    source = meta.get("source")
    try:
        if source:
            manifest = promlib.loadManifest(source["prom"])
            if manifest["image"]["sha256"] != source.get("image_sha256"):
                return None
            return promlib.image(manifest)
        bank = (int(meta["pico"]["command"]["speak"], 16) >> 14) & 0xF
        banks = dict(item.split(":") for item in meta["pico"]["banks"].split(","))
        for name in sorted(os.listdir(promlib.PROMS_DIR)):
            manifest = promlib.loadManifest(os.path.join(promlib.PROMS_DIR, name))
            if manifest["image"]["crc32"] == banks.get(str(bank)) \
                    and manifest["image"]["size"] == promlib.BANK_SIZE:
                return promlib.image(manifest)
    except (KeyError, OSError, ValueError):
        pass
    return None


def startFrame(meta, path, trace, summary, checks):
    """The frame of the chip's free-running frame clock that the speech
    started on (frameclock.py). For a VSM Speak it is measured from the first
    frame read; for Speak External, which leaves no read in the trace, it is
    the frame the Speak was timed for. For the standard procedure, also when
    the chip accepted the command, before that frame's boundary: it sets the
    noise generator's state (datasheet section 7)."""
    timing = meta.get("timing") or {}
    if timing.get("grid_us") is None:
        return
    frameUs = timing["frame_us"]
    result = (meta.get("pico") or {}).get("result") or {}
    if trace is not None and "trig_us" in result:
        cycles = frameclock.commandLatch(trace, picolib.CMD_SPEAK if path == "vsm"
                                         else picolib.CMD_SPEAK_EXTERNAL)
        if cycles is not None and timing.get("frame") is not None:
            # On the grid, as frameclock.calibrate() measures it
            boundary = timing["grid_us"] + timing["frame"] * frameUs
            summary["latch_before_frame_us"] = boundary - frameclock.toUs(
                trace, result["trig_us"], cycles)
            if timing.get("latch_target_us") is not None:
                checks["start_slot"] = bool(abs(summary["latch_before_frame_us"]
                                                - timing["latch_target_us"])
                                            <= frameclock.LATCH_TOLERANCE_US)
    if path == "vsm" and summary.get("first_frame_read_us") is not None:
        first = summary["first_frame_read_us"] - timing["grid_us"]
        frame = int(round(first / frameUs))
        offset = first - frame * frameUs
        summary["grid_offset_us"] = offset
        ok = abs(offset) <= GRID_TOLERANCE_US
        if timing.get("frame") is not None:
            ok = ok and frame == timing["frame"]
        checks["start_frame"] = bool(ok)
    elif timing.get("frame") is not None:
        frame = timing["frame"]
        late = ((meta.get("pico") or {}).get("result") or {}).get("late_us")
        checks["start_frame"] = late is not None and late <= START_LATE_US
    else:
        return
    summary["start_frame"] = frame


def process(directory, listenFilter="bbc", log=print):
    with open(os.path.join(directory, "capture.json")) as f:
        meta = json.load(f)
    path = commandPath(meta)
    checks, summary = {}, {"path": path}

    # Layer 1: the trace
    trace, traceRise, traceFall = None, None, None
    if os.path.exists(os.path.join(directory, "raw", "vsm_trace.bin")):
        clockHz = (meta.get("pico") or {}).get("trace", {}).get("clock_hz", picolib.TRACE_CLOCK_HZ)
        trace = picolib.writeTraceCsv(directory, clockHz)
        pins = trace[(trace["type"] == picolib.EV_PINS) | (trace["type"] == picolib.EV_START)]
        rises, falls = edgesOf(pins["trig"], 1), edgesOf(pins["trig"], 0)
        if len(rises):
            traceRise = pins["time_s"][rises[0]]
            after = falls[falls > rises[0]]
            traceFall = pins["time_s"][after[0]] if len(after) else None
        checks["trace_complete"] = meta["pico"]["trace"]["dropped"] == 0
        summary["trace_events"] = int(len(trace))

    result = (meta.get("pico") or {}).get("result")
    if (meta.get("pico") or {}).get("error"):
        checks["talk_status"] = False
        summary["pico_error"] = meta["pico"]["error"]
    elif result:
        checks["talk_status"] = bool(result["talked"] and not result["status"] & picolib.STATUS_TS)
        summary["final_status"] = f"0x{result['status']:02x}"
        summary["int_events"] = result["ints"]

    # Layers 2 and 3: the scope
    rawPath = os.path.join(directory, "raw", "scope_raw.npz")
    interval, volts = td.loadRaw(rawPath)
    preTrigger = int(td.openRaw(rawPath)["preTrigger"])
    d = td.decode(interval, volts)
    windows = td.triggerWindows(interval, volts, d["edge"])
    if not windows:
        raise ValueError(f"{directory}: SCOPE_TRIG never rose")
    first, talkEnd = windows[0]
    edges = d["edge"][first:]
    code, dacV = d["code"][first:], d["dac"][first:]
    t11 = (edges - preTrigger) * interval
    talkSamples = talkEnd - first
    checks["scope_overflow"] = meta["scope"]["overflow_mask"] == 0
    checks["io_format"] = td.unusedSlotErrors(d["bits"][first:]) == 0
    period = float(np.diff(edges).mean() * interval)
    foscMeasured = 80 / period
    oscSet = (meta.get("pico") or {}).get("osc_hz")
    if oscSet:
        summary["fosc_ppm"] = (foscMeasured / oscSet - 1) * 1e6
        checks["fosc"] = abs(summary["fosc_ppm"]) <= FOSC_PPM
    sampleRate = (oscSet or 640000) / 80
    changed = np.flatnonzero(code != code[0])
    summary.update({
        "fosc_measured_hz": foscMeasured, "sample_rate_hz": foscMeasured / 80,
        "samples": int(len(code)), "talk_samples": int(talkSamples),
        "speech_first_sample": int(changed[0]) if len(changed) else None,
        "speech_first_sample_s": float(t11[changed[0]]) if len(changed) else None,
        "speech_last_sample": int(changed[-1]) if len(changed) else None,
        "code_range": [int(code.min()), int(code.max())],
    })

    model, tablePath = dacModel(meta.get("board", {}).get("config"))
    if model is not None:
        dacCode, stats = layer3(code, dacV, model)
        stats["table"] = tablePath
        checks["layer2_vs_layer3"] = stats.pop("pass")
        summary["layer3"] = stats
    else:
        dacCode = None
        checks["layer2_vs_layer3"] = None

    # The bytes the host wrote, from SCOPE_TRIG rising: the command sequence
    if trace is not None:
        writes = [c for c in picolib.hostCycles(trace) if c["kind"] == "write"
                  and (traceRise is None or c["start"] >= traceRise)]
        summary["host_writes"] = len(writes)
        summary["host_commands"] = " ".join(f"{c['byte']:02x}" for c in writes[:16])

    # Frames
    frames, frameTimes, bitsConsumed = None, None, None
    if trace is not None and path == "vsm":
        reads = trace[trace["type"] == picolib.EV_READ]
        if traceRise is not None:
            reads = reads[reads["time_s"] >= traceRise]
        frames, parsedBits = lpc.parseFrames(reads["bit"].astype(np.uint8))
        bitsConsumed = int(len(reads))
        frameTimes = np.array([reads["time_s"][f["offset"]] - traceRise for f in frames])
        summary["bits_consumed"] = bitsConsumed
        checks["bits_vs_frames"] = bool(frames and frames[-1]["kind"] == "stop"
                                        and parsedBits == bitsConsumed)
        if len(frames) > 1:
            gaps = np.diff(frameTimes)
            summary["frame_period_s"] = [float(gaps.min()), float(gaps.max())]
            checks["frame_period"] = bool(np.all(np.abs(gaps - lpc.FRAME_SAMPLES / sampleRate)
                                                 <= FRAME_PERIOD_TOLERANCE_S))
        summary["first_frame_read_s"] = float(frameTimes[0]) if len(frames) else None
        if len(frames) and result and "trig_us" in result:
            # On the Pico's microsecond timer, which runs from the chip's crystal
            summary["first_frame_read_us"] = result["trig_us"] + float(frameTimes[0]) * 1e6
        image = sourceImage(meta)
        if image is not None:
            address = int(meta["pico"]["command"]["speak"], 16)
            problems = picolib.checkVsmBits(reads, image, address, "")[0]
            checks["vsm_image"] = not problems
        else:
            checks["vsm_image"] = None
        if traceFall is not None and len(frames):
            talk = traceFall - traceRise - frameTimes[0]
            summary["talk_after_first_read_s"] = float(talk)
            checks["duration"] = bool(abs(talk - len(frames) * lpc.FRAME_SAMPLES / sampleRate)
                                      <= VSM_DURATION_TOLERANCE_S)
    elif trace is not None and path == "external":
        writes = [c["byte"] for c in picolib.hostCycles(trace) if c["kind"] == "write"]
        start = next(i for i, b in enumerate(writes) if b & 0xF0 == picolib.CMD_SPEAK_EXTERNAL)
        stream = bytes(writes[start + 1:])
        command = meta["pico"]["command"]
        checks["external_stream"] = bool(
            len(stream) == command["bytes"] and f"{zlib.crc32(stream):08x}" == command["crc32"])
        frames, parsedBits = lpc.parseFrames(lpc.bitsLsbFirst(stream))
        summary["stream_bytes"] = len(stream)
        summary["stream_bits_to_stop"] = parsedBits
        extra = talkSamples - len(frames) * lpc.FRAME_SAMPLES
        summary["talk_minus_frames_samples"] = int(extra)
        checks["duration"] = bool(EXTERNAL_START_SAMPLES[0] <= extra <= EXTERNAL_START_SAMPLES[1])
    startFrame(meta, path, trace, summary, checks)
    if frames is not None:
        checks["stop_frame"] = bool(frames and frames[-1]["kind"] == "stop")
        summary["frames"] = len(frames)
        summary["frame_kinds"] = {k: sum(f["kind"] == k for f in frames)
                                  for k in ("voiced", "unvoiced", "repeat", "silence", "stop")}
        with open(os.path.join(directory, "frames.csv"), "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(lpc.FRAME_COLUMNS + ["read_us", "read_sample"])
            for i, frame in enumerate(frames):
                row = lpc.frameRow(i, frame)
                if frameTimes is not None:
                    row += [f"{frameTimes[i] * 1e6:.3f}",
                            int(np.searchsorted(t11, frameTimes[i], side="right") - 1)]
                else:
                    row += ["", ""]
                writer.writerow(row)

    # Readable outputs
    writeWav(os.path.join(directory, "samples.wav"), code)
    columns = [np.arange(len(code)), code, dacV,
               dacCode if dacCode is not None else np.full(len(code), -999), t11]
    with open(os.path.join(directory, "samples.csv"), "w", newline="") as f:
        f.write("sample,code,dac_v,dac_code,t11_time_s\n")
        np.savetxt(f, np.column_stack(columns), fmt=["%d", "%d", "%.6f", "%d", "%.9f"],
                   delimiter=",")
    files = ["samples.wav", "samples.csv", "listen.wav"]
    if dacCode is not None:
        writeWav(os.path.join(directory, "dac.wav"), dacCode)
        files.append("dac.wav")
    with wave.open(os.path.join(directory, "listen.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(LISTEN_RATE)
        w.writeframes(listenAudio(code, listenFilter).astype("<i2").tobytes())
    if trace is not None:
        files.append("vsm_trace.csv")
    if frames is not None:
        files.append("frames.csv")

    meta["processed"] = {
        "version": PROCESS_VERSION, "host_git": gitHash(),
        "time": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "files": sorted(files),
        "listen_filter": listenFilter,
        "wav": {"samples.wav": "16-bit PCM, header rate 8000 Hz, sample = 10-bit I/O code x 64; "
                               "sample 0 is the first T11 period after SCOPE_TRIG rose",
                "dac.wav": "16-bit PCM, header rate 8000 Hz, sample = recovered DAC code x 64",
                "listen.wav": f"48 kHz, DAC code x 128, filter {listenFilter}"},
        "summary": summary, "checks": checks,
        "passed": all(v is not False for v in checks.values()),
    }
    with open(os.path.join(directory, "capture.json"), "w") as f:
        json.dump(meta, f, indent=2)
    failed = [k for k, v in checks.items() if v is False]
    log(f"{directory}: {summary.get('frames', '-')} frames, {len(code)} samples, "
        f"checks {'passed' if not failed else 'FAILED: ' + ', '.join(failed)}")
    return meta["processed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("captures", nargs="+", help="capture directories")
    parser.add_argument("--listen-filter", choices=("bbc", "none"), default="bbc")
    args = parser.parse_args()
    failed = 0
    for directory in args.captures:
        failed += not process(directory, args.listen_filter)["passed"]
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
