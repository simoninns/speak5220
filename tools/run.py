#!/usr/bin/env python3
"""
    run.py

    The capture runner (plan section 7): turns "speak word N of PROM X" into
    complete capture directories. For each word, path and take it loads the
    PROM image into the Pico if needed, arms the PicoScope, has the TMS5220
    speak, saves the raw scope data and the VSM trace, builds the readable
    files and runs the checks (process.py). It then compares the takes of
    each word (compare.py) and rewrites the PROM's index.md (promindex.py).

      python tools/run.py acorn_phrom_a ACORN                  # one take, VSM Speak
      python tools/run.py acorn_phrom_a 160 179 --takes 3 --path both
      python tools/run.py acorn_phrom_a all --takes 5          # the whole PROM
      python tools/run.py acorn_phrom_a --address 0x3CBB7      # an address sweep point

    Layout: <out>/<prom>/prom.json, index.md and one directory per word
    (<number>_<name>, or addr_<address> for addresses not in the word
    table), holding compare.json and one directory per take: vsm01, vsm02,
    ... for VSM Speak and ext01, ... for Speak External. Takes are numbered
    on from those already there.

    Exits non-zero if any check failed.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import os
import re
import shutil
import time
import zipfile
import zlib

import capture as capturelib
import compare as comparelib
import lpc
import pico as picolib
import process as processlib
import prom as promlib
import promindex

BENCH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bench.json")
PATH_PREFIX = {"vsm": "vsm", "external": "ext"}
POST_S = 0.15              # captured after the expected end of speech
UNPARSED_S = 2.0           # capture length when the data has no stop code
PRE_MS = 10.0


def nextTake(wordDir, path):
    prefix = PATH_PREFIX[path]
    numbers = [int(m.group(1)) for d in (os.listdir(wordDir) if os.path.isdir(wordDir) else [])
               for m in [re.fullmatch(prefix + r"(\d+)", d)] if m]
    return os.path.join(wordDir, f"{prefix}{max(numbers, default=0) + 1:02d}")


def expectedSeconds(image, address):
    """Talk time from the frames in the image at the address, plus margin."""
    frames, _ = lpc.parseFrames(lpc.bitsLsbFirst(image[address & 0x3FFF:]))
    if not frames or frames[-1]["kind"] != "stop":
        return UNPARSED_S, None
    return (len(frames) + 2) * lpc.FRAME_SAMPLES / 8000 + POST_S, len(frames)


def zipRaw(takeDir):
    """Archive raw/scope_raw.npz unchanged as raw/scope_raw.npz.zip (plan section 6)."""
    path = os.path.join(takeDir, "raw", "scope_raw.npz")
    with zipfile.ZipFile(path + ".zip", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(path, "scope_raw.npz")


def captureTake(pico, args, manifest, image, word, address, path, wordDir, bench, lastEnd):
    takeDir = nextTake(wordDir, path)
    seconds, frames = expectedSeconds(image, address)
    picoMeta = {"firmware": pico.firmware.get("firmware"),
                "osc_hz": int(pico.firmware["osc_hz"]), "banks": pico.info()["banks"]}
    if args.reset:
        picoMeta["reset_status"] = pico.reset()
    if path == "vsm":
        picoMeta["command"] = {"speak": f"0x{address:05x}"}
        data = None
    else:
        data = picolib.wordBytes(image, address)
        picoMeta["command"] = {"speakext": manifest["name"], "word": f"0x{address:05x}",
                               "bytes": len(data), "crc32": f"{zlib.crc32(data):08x}"}
    timing = {}

    def armed():
        time.sleep(capturelib.ARM_DELAY_S + PRE_MS * 1e-3)
        pico.traceClear()
        timing["start"] = time.time()
        timing["idle_s"] = timing["start"] - lastEnd if lastEnd else None
        try:
            if path == "vsm":
                picoMeta["result"] = pico.speak(address, args.timeout_ms)
            else:
                picoMeta["result"] = pico.speakExternal(data, args.timeout_ms)
        except picolib.PicoError as error:
            # For example a sweep point whose data never reaches a stop
            # code: record it, stop the speech, and let the scope finish
            picoMeta["error"] = str(error)
            print(f"Pico: {error}")
            picoMeta["reset_after_error"] = pico.reset()
        timing["end"] = time.time()

    print(f"{takeDir}: {word['name'] if word else 'address'} 0x{address:05x} "
          f"{path}, {frames or '?'} frames, capturing {seconds:.2f} s")
    scope = capturelib.scopeCapture(seconds, PRE_MS, args.timebase, 3, armed)
    traceRaw, traceMeta = pico.trace()
    picolib.writeTrace(takeDir, traceRaw, traceMeta)
    picoMeta["trace"] = traceMeta
    capturelib.saveScope(takeDir, scope)
    if args.zip:
        zipRaw(takeDir)

    meta = {
        "captured": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "source": {"prom": manifest["name"], "image_sha256": manifest["image"]["sha256"],
                   "bank": manifest["bank"], "address": f"0x{address:05x}",
                   "word_number": word["number"] if word else None,
                   "word": word["name"] if word else None},
        "path": path,
        "procedure": {"reset_before": bool(args.reset), "idle_before_s": timing.get("idle_s"),
                      "takes_requested": args.takes},
        "chip": bench.get("chip"), "supplies_v": bench.get("supplies_v"),
        "temperature_c": bench.get("temperature_c"),
        "scope": capturelib.scopeMeta(scope),
        "channels": capturelib.channelMeta(),
        "board": dict(capturelib.boardMeta(args.board, args.load_ohms), **bench.get("board", {})),
        "pico": picoMeta,
        "host_git": processlib.gitHash(),
        "note": args.note,
    }
    with open(os.path.join(takeDir, "capture.json"), "w") as f:
        json.dump(meta, f, indent=2)
    processed = processlib.process(takeDir, args.listen_filter)
    return takeDir, processed["passed"], timing.get("end")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("prom", help="manifest name in proms/ (or a path to a manifest)")
    parser.add_argument("words", nargs="*", help="word numbers or names, or 'all'")
    parser.add_argument("--address", type=lambda s: int(s, 0), action="append", default=[],
                        help="also speak from this VSM address (repeatable)")
    parser.add_argument("--path", choices=("vsm", "external", "both"), default="vsm",
                        help="VSM Speak, Speak External, or both")
    parser.add_argument("--takes", type=int, default=1, help="takes per word and path")
    parser.add_argument("--idle", type=float, default=0.5,
                        help="seconds to wait between takes (after the scope data is saved)")
    parser.add_argument("--reset", action="store_true",
                        help="send the reset sequence (nine 0xFF, Reset) before every take")
    parser.add_argument("--out", default=os.path.join(promlib.ROOT, "captures", "corpus"),
                        help="capture root (default captures/corpus, kept local)")
    parser.add_argument("--zip", action="store_true",
                        help="also write raw/scope_raw.npz.zip, for evidence captures")
    parser.add_argument("--board", choices=capturelib.BOARD_CONFIGS, default="measure")
    parser.add_argument("--load-ohms", type=float, default=996.0, help="R2 as measured")
    parser.add_argument("--timebase", type=int, default=64)
    parser.add_argument("--timeout-ms", type=int, default=10000,
                        help="the Pico's limit on one Speak")
    parser.add_argument("--listen-filter", choices=("bbc", "none"), default="bbc")
    parser.add_argument("--note", default="")
    parser.add_argument("--port")
    args = parser.parse_args()

    manifest = promlib.loadManifest(args.prom)
    image = promlib.image(manifest)
    if args.words == ["all"]:
        words = list(manifest["words"])
    else:
        words = [promlib.findWord(manifest, w) for w in args.words]
    targets = [(w, promlib.address(w), promlib.wordDir(w)) for w in words]
    for a in args.address:
        word = promlib.wordAt(manifest, a)
        targets.append((word, a, promlib.wordDir(word) if word else promlib.addressDir(a)))
    if not targets:
        parser.error("name at least one word, 'all', or --address")
    with open(BENCH) as f:
        bench = json.load(f)
    if not bench["chip"].get("marking"):
        print("Note: tools/bench.json does not record the chip's marking yet")

    promDir = os.path.join(args.out, manifest["name"])
    os.makedirs(promDir, exist_ok=True)
    shutil.copyfile(promlib.manifestPath(args.prom), os.path.join(promDir, "prom.json"))
    paths = ["vsm", "external"] if args.path == "both" else [args.path]

    failed = []
    with picolib.Pico(args.port) as pico:
        crc = zlib.crc32(image.ljust(promlib.BANK_SIZE, b"\0"))
        if pico.banks().get(manifest["bank"]) != crc:
            print(f"uploading {manifest['name']} to bank {manifest['bank']:X}")
            pico.upload(manifest["bank"], image)
        lastEnd = None
        for word, address, name in targets:
            wordDir = os.path.join(promDir, name)
            for path in paths:
                for _ in range(args.takes):
                    if lastEnd:
                        time.sleep(args.idle)
                    takeDir, passed, lastEnd = captureTake(pico, args, manifest, image, word,
                                                           address, path, wordDir, bench, lastEnd)
                    if not passed:
                        failed.append(takeDir)
            takes = comparelib.takeDirs(wordDir)
            if len(takes) > 1:
                result = comparelib.compare(takes)
                with open(os.path.join(wordDir, "compare.json"), "w") as f:
                    json.dump(result, f, indent=2)
    promindex.writeIndex(promDir)
    for takeDir in failed:
        print(f"FAILED checks: {takeDir}")
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
