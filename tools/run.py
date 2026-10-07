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
      python tools/run.py acorn_phrom_a all --takes 5 --path both --resume --zip-only
      python tools/run.py acorn_phrom_a --sweep --resume --zip-only   # every byte address
      python tools/run.py acorn_phrom_a --sweep --sweep-step 64 --sweep-takes 5 --resume --zip-only

    Every take uses the standard procedure (plan section 12, Phase 5;
    datasheet section 7) unless --start free is given: the reset sequence,
    then a Speak timed to start half a frame before a boundary of the chip's
    frame clock (frameclock.py). That fixes both the start delay and the
    noise generator's state, so repeats match apart from rare upsets.

    The address sweep (plan section 8, --sweep) speaks once through the VSM
    from every byte address of the image that is not a word start (those
    are captured as words), or from every address in a range of them. It
    leaves out addresses whose data reaches the end of the image without a
    stop code (prom.sweepPoints).

    Layout: <out>/<prom>/prom.json, index.md, takes.csv and one directory
    per word (<number>_<name>), and sweep/addr_<address> for addresses not
    in the word table (promindex.py also writes sweep.csv). Each holds
    compare.json and one directory per take: vsm01, vsm02, ... for VSM Speak
    and ext01, ... for Speak External. Takes are numbered on from those
    already there. --resume counts the takes that passed and makes only the
    ones missing, so an interrupted run can be restarted with the same
    command.

    A take that fails a check of the capture procedure (CAPTURE_CHECKS:
    the start slot, say, rather than the speech data) is kept and retaken,
    up to --retries times, after recalibrating the start if its slot was
    missed. The scope is held open for the whole run, so a take costs its
    talk time plus about 1.5 s.

    Exits non-zero if any check failed.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import time
import zipfile
import zlib

import capture as capturelib
import compare as comparelib
import frameclock
import lpc
import pico as picolib
import process as processlib
import prom as promlib
import promindex
from ps3000a import Scope

BENCH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bench.json")
PATH_PREFIX = {"vsm": "vsm", "external": "ext"}
POST_S = 0.15              # captured after the expected end of speech
UNPARSED_S = 2.0           # capture length when the data has no stop code
PRE_MS = 10.0
START_LEAD_S = 0.6         # a timed take arms the held-open scope this long before its start
ZIP_ONLY_LEVEL = 6         # --zip-only: 7x smaller in 0.45 s per take (level 9 takes 4 s)
INDEX_EVERY = 500          # rewrite index.md every this many entries during a long run
MAX_ERRORS_IN_A_ROW = 5
# Checks of the capture procedure rather than of the speech data: a take
# failing only these is retaken
CAPTURE_CHECKS = {"start_slot", "start_frame", "trace_complete", "scope_overflow", "io_format",
                  "fosc", "external_stream", "vsm_image"}


def nextTake(wordDir, path):
    prefix = PATH_PREFIX[path]
    numbers = [int(m.group(1)) for d in (os.listdir(wordDir) if os.path.isdir(wordDir) else [])
               for m in [re.fullmatch(prefix + r"(\d+)", d)] if m]
    return os.path.join(wordDir, f"{prefix}{max(numbers, default=0) + 1:02d}")


def expectedSeconds(image, address, base=None):
    """Talk time from the frames in the image at the address, plus margin."""
    frames, _ = lpc.parseFrames(lpc.bitsLsbFirst(image[picolib.imageOffset(address, base):]))
    if not frames or frames[-1]["kind"] != "stop":
        return UNPARSED_S, None
    return (len(frames) + 2) * lpc.FRAME_SAMPLES / 8000 + POST_S, len(frames)


def zipRaw(takeDir, level=9, remove=False):
    """Archive raw/scope_raw.npz unchanged as raw/scope_raw.npz.zip (plan
    section 6); with remove, keep only the zip (the tools read either)."""
    path = os.path.join(takeDir, "raw", "scope_raw.npz")
    with zipfile.ZipFile(path + ".zip.part", "w", zipfile.ZIP_DEFLATED,
                         compresslevel=level) as z:
        z.write(path, "scope_raw.npz")
    os.replace(path + ".zip.part", path + ".zip")
    if remove:
        os.remove(path)


def sha256File(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def waitUntil(pico, us, marginS=0.0):
    """Sleep until marginS before the Pico's timer reaches us."""
    lead = (us - pico.time()) * 1e-6 - marginS
    if lead > 0:
        time.sleep(lead)


def captureTake(pico, args, manifest, image, word, address, path, wordDir, bench, lastEnd,
                clock, frame=None, scope=None):
    """One take. The standard procedure resets the chip, which holds its
    noise generator in a fixed state until the next Speak, and then times
    the Speak so that the first frame is read at a frame boundary half a
    frame later (frameclock.py)."""
    takeDir = nextTake(wordDir, path)
    standard = args.start == "standard"
    base = promlib.base(manifest)
    seconds, frames = expectedSeconds(image, address, base)
    picoMeta = {"firmware": pico.firmware.get("firmware"),
                "osc_hz": int(pico.firmware["osc_hz"]), "banks": pico.info()["banks"]}
    if standard or args.reset:
        picoMeta["reset_status"] = pico.reset()
    if standard and frame is None:
        frame = clock.next(pico, (START_LEAD_S if scope else START_LEAD_S + 4.0) + 0.1)
    atUs = clock.startUs(frame, path) if standard else None
    if path == "vsm":
        picoMeta["command"] = {"speak": f"0x{address:05x}"}
        data = None
    else:
        data = picolib.wordBytes(image, address, base)
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
                picoMeta["result"] = pico.speak(address, args.timeout_ms, atUs)
            else:
                picoMeta["result"] = pico.speakExternal(data, args.timeout_ms, atUs)
        except picolib.PicoError as error:
            # For example a sweep point whose data never reaches a stop
            # code: record it, stop the speech, and let the scope finish
            picoMeta["error"] = str(error)
            print(f"Pico: {error}")
            picoMeta["reset_after_error"] = pico.reset()
        timing["end"] = time.time()

    print(f"{takeDir}: {word['name'] if word else 'address'} 0x{address:05x} "
          f"{path}, {frames or '?'} frames, capturing {seconds:.2f} s")
    if atUs is not None:
        # Arm the scope shortly before the start time
        waitUntil(pico, atUs, START_LEAD_S if scope else START_LEAD_S + 4.0)
    captured = capturelib.scopeCapture(seconds, PRE_MS, args.timebase, 3, armed, scope)
    traceRaw, traceMeta = pico.trace()
    picolib.writeTrace(takeDir, traceRaw, traceMeta)
    picoMeta["trace"] = traceMeta
    capturelib.saveScope(takeDir, captured)
    if args.zip:
        zipRaw(takeDir)
    # The raw files' hashes, so local copies can be checked (plan section 6)
    raw = {name: sha256File(os.path.join(takeDir, "raw", name))
           for name in ("scope_raw.npz", "vsm_trace.bin")}

    meta = {
        "captured": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "source": {"prom": manifest["name"], "image_sha256": manifest["image"]["sha256"],
                   "bank": manifest["bank"], "address": f"0x{address:05x}",
                   "word_number": word["number"] if word else None,
                   "word": word["name"] if word else None},
        "path": path,
        "procedure": {"start": args.start, "reset_before": standard or bool(args.reset),
                      "idle_before_s": timing.get("idle_s"), "takes_requested": args.takes},
        "timing": dict(clock.meta(path if standard else None), start_at_us=atUs, frame=frame),
        "chip": bench.get("chip"), "supplies_v": bench.get("supplies_v"),
        "temperature_c": bench.get("temperature_c"),
        "scope": capturelib.scopeMeta(captured),
        "raw_sha256": raw,
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


def sweepAddresses(manifest, image, spec):
    """The sweep points (prom.sweepPoints) in the range START:END (END
    exclusive), or all of them."""
    points, _ = promlib.sweepPoints(manifest, image)
    if spec == "all":
        return points
    first, last = (int(v, 0) for v in spec.split(":"))
    return [a for a in points if first <= a < last]


def passedTakes(wordDir, path):
    """How many takes of a word, through a path, passed every check."""
    if not os.path.isdir(wordDir):
        return 0
    count = 0
    for take in comparelib.takeDirs(wordDir):
        with open(os.path.join(take, "capture.json")) as f:
            processed = json.load(f).get("processed", {})
        count += processed.get("summary", {}).get("path") == path and bool(processed.get("passed"))
    return count


def latchNear(takeDir):
    """Whether a take that missed its start slot missed it narrowly, by less
    than a frame: then the start may have drifted, and is recalibrated.
    A command write the chip did not drive READY low for is just retaken."""
    with open(os.path.join(takeDir, "capture.json")) as f:
        meta = json.load(f)
    latch = meta.get("processed", {}).get("summary", {}).get("latch_before_frame_us")
    target = (meta.get("timing") or {}).get("latch_target_us")
    return latch is not None and target is not None and abs(latch - target) < frameclock.FRAME_US


def failedChecks(takeDir):
    with open(os.path.join(takeDir, "capture.json")) as f:
        checks = json.load(f).get("processed", {}).get("checks", {})
    return {k for k, v in checks.items() if v is False}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip())
    parser.add_argument("prom", help="manifest name in proms/ (or a path to a manifest)")
    parser.add_argument("words", nargs="*", help="word numbers or names, or 'all'")
    parser.add_argument("--address", type=lambda s: int(s, 0), action="append", default=[],
                        help="also speak from this VSM address (repeatable)")
    parser.add_argument("--sweep", nargs="?", const="all", metavar="START:END",
                        help="also speak once through the VSM from every byte address of the "
                             "image that is not a word start and whose data reaches a stop "
                             "code (or from those in a range, END exclusive)")
    parser.add_argument("--path", choices=("vsm", "external", "both"), default="vsm",
                        help="VSM Speak, Speak External, or both (sweep points: VSM only)")
    parser.add_argument("--takes", type=int, default=1,
                        help="takes per word and path (sweep points: --sweep-takes)")
    parser.add_argument("--sweep-takes", type=int, default=1, help="takes per sweep address")
    parser.add_argument("--sweep-step", type=int, default=1, metavar="N",
                        help="only every Nth sweep address (for extra takes of a sample)")
    parser.add_argument("--resume", action="store_true",
                        help="count the takes already there that passed, and make only the rest")
    parser.add_argument("--retries", type=int, default=2,
                        help="retakes of a take that failed a check of the capture procedure")
    parser.add_argument("--idle", type=float, default=0.5,
                        help="with --start free: seconds to wait between takes")
    parser.add_argument("--start", choices=("standard", "free"), default="standard",
                        help="standard: reset, then a Speak timed to the frame clock, so that "
                             "repeats match; free: speak as soon as the scope is armed")
    parser.add_argument("--reset", action="store_true",
                        help="with --start free: also send the reset sequence before every take")
    parser.add_argument("--out", default=os.path.join(promlib.ROOT, "captures", "corpus"),
                        help="capture root (default captures/corpus, kept local)")
    parser.add_argument("--zip", action="store_true",
                        help="also write raw/scope_raw.npz.zip, for evidence captures")
    parser.add_argument("--zip-only", action="store_true",
                        help="keep the raw scope data only as raw/scope_raw.npz.zip (zipped in "
                             "the background), for the corpus")
    parser.add_argument("--board", choices=capturelib.BOARD_CONFIGS, default="measure")
    parser.add_argument("--load-ohms", type=float, default=996.0, help="R2 as measured")
    parser.add_argument("--timebase", type=int, default=64)
    parser.add_argument("--timeout-ms", type=int, default=10000,
                        help="the Pico's limit on one Speak")
    parser.add_argument("--listen-filter", choices=("bbc", "none"), default="bbc")
    parser.add_argument("--note", default="")
    parser.add_argument("--port")
    args = parser.parse_args()
    if args.zip and args.zip_only:
        parser.error("use --zip or --zip-only, not both")

    manifest = promlib.loadManifest(args.prom)
    image = promlib.image(manifest)
    paths = ["vsm", "external"] if args.path == "both" else [args.path]
    if args.words == ["all"]:
        words = list(manifest["words"])
    else:
        words = [promlib.findWord(manifest, w) for w in args.words]
    # (word or None, address, directory name, paths, takes)
    targets = [(w, promlib.address(w), promlib.wordDir(w), paths, args.takes) for w in words]
    for a in args.address:
        word = promlib.wordAt(manifest, a)
        targets.append((word, a, promlib.wordDir(word) if word else promlib.addressDir(a),
                        paths, args.takes))
    if args.sweep:
        targets += [(None, a, promlib.addressDir(a), ["vsm"], args.sweep_takes)
                    for a in sweepAddresses(manifest, image, args.sweep)[::args.sweep_step]]
    if not targets:
        parser.error("name at least one word, 'all', --address or --sweep")
    with open(BENCH) as f:
        bench = json.load(f)
    if not bench["chip"].get("marking"):
        print("Note: tools/bench.json does not record the chip's marking yet")

    promDir = os.path.join(args.out, manifest["name"])
    os.makedirs(promDir, exist_ok=True)
    shutil.copyfile(promlib.manifestPath(args.prom), os.path.join(promDir, "prom.json"))

    # The takes still to make, per target and path
    plan = []
    for word, address, name, targetPaths, takes in targets:
        wordDir = os.path.join(promDir, name)
        todo = [(path, takes - (passedTakes(wordDir, path) if args.resume else 0))
                for path in targetPaths]
        todo = [(path, n) for path, n in todo if n > 0]
        if todo:
            plan.append((word, address, wordDir, todo))
    total = sum(n for *_, todo in plan for _, n in todo)
    print(f"{len(plan)} of {len(targets)} entries, {total} takes to make")
    if not plan:
        promindex.writeIndex(promDir)
        return

    failed, errors = [], []
    zipper = concurrent.futures.ThreadPoolExecutor(2) if args.zip_only else None
    zips = []
    with picolib.Pico(args.port) as pico, Scope() as scope:
        # The Pico holds 4 banks at once. Only this PROM's banks are kept, so
        # that reads past the end of its image (sweep points) find empty
        # banks in every run
        loaded = pico.banks()
        mine = promlib.bankImages(manifest, image)
        for bank in set(loaded) - {bank for bank, _ in mine}:
            pico.unload(bank)
        for bank, data in mine:
            if loaded.get(bank) != zlib.crc32(data.ljust(promlib.BANK_SIZE, b"\0")):
                print(f"uploading {manifest['name']} to bank {bank:X}")
                pico.upload(bank, data)
        # The frame clock, and for the standard procedure the start offset
        # of each path, from the first word (a word that parses to a stop
        # code, so that it ends), or else the PROM's first such word
        base = promlib.base(manifest)
        candidates = [a for _, a, *_ in plan] + [promlib.address(w) for w in manifest["words"]]
        calibration = next(a for a in candidates if expectedSeconds(image, a, base)[1])
        clock = frameclock.FrameClock.locate(pico, calibration)
        print(f"frame grid at {clock.gridUs:.2f} us on the Pico's timer")
        usedPaths = sorted({path for *_, todo in plan for path, _ in todo})

        def calibrate(path):
            pico.reset()
            clock.calibrate(pico, path, calibration,
                            picolib.wordBytes(image, calibration, base)
                            if path == "external" else None)

        if args.start == "standard":
            for path in usedPaths:
                calibrate(path)
        lastEnd, made, inARow, began = None, 0, 0, time.time()
        for entry, (word, address, wordDir, todo) in enumerate(plan):
            for path, takes in todo:
                retries = args.retries
                while takes > 0:
                    if lastEnd and args.start == "free":
                        time.sleep(args.idle)
                    try:
                        takeDir, passed, lastEnd = captureTake(
                            pico, args, manifest, image, word, address, path, wordDir, bench,
                            lastEnd, clock, scope=scope)
                        inARow = 0
                    except (picolib.PicoError, TimeoutError, ValueError, RuntimeError) as error:
                        # Keep what was written; the directory has no capture.json,
                        # so the tools skip it
                        inARow += 1
                        errors.append((wordDir, path, str(error)))
                        print(f"ERROR {wordDir} {path}: {error}")
                        if inARow >= MAX_ERRORS_IN_A_ROW:
                            raise
                        pico.reset()
                        continue
                    made += 1
                    if zipper:
                        zips.append(zipper.submit(zipRaw, takeDir, ZIP_ONLY_LEVEL, True))
                    if passed:
                        takes -= 1
                        continue
                    failed.append(takeDir)
                    checks = failedChecks(takeDir)
                    if retries > 0 and checks and checks <= CAPTURE_CHECKS:
                        retries -= 1
                        print(f"retaking: {', '.join(sorted(checks))}")
                        if ("start_slot" in checks and args.start == "standard"
                                and latchNear(takeDir)):
                            calibrate(path)
                    else:
                        takes -= 1
            takeList = comparelib.takeDirs(wordDir) if os.path.isdir(wordDir) else []
            if len(takeList) > 1:
                result = comparelib.compare(takeList, log=lambda *a: None)
                with open(os.path.join(wordDir, "compare.json"), "w") as f:
                    json.dump(result, f, indent=2)
            if made:
                rate = (time.time() - began) / made
                left = sum(n for *_, todo in plan[entry + 1:] for _, n in todo)
                print(f"[{entry + 1}/{len(plan)}] {made} takes, {rate:.1f} s each, "
                      f"about {left * rate / 3600:.1f} h left")
            if (entry + 1) % INDEX_EVERY == 0:
                promindex.writeIndex(promDir)
    if zipper:
        zipper.shutdown(wait=True)
        for z in zips:
            z.result()
    promindex.writeIndex(promDir)
    for takeDir in failed:
        print(f"FAILED checks: {takeDir}: {', '.join(sorted(failedChecks(takeDir)))}")
    for wordDir, path, error in errors:
        print(f"ERROR: {wordDir} {path}: {error}")
    raise SystemExit(1 if failed or errors else 0)


if __name__ == "__main__":
    main()
