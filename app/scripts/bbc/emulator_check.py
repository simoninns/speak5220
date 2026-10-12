# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks the BBC Micro discs on an emulated BBC Micro (plan Phase 10).

Run by bbc-check.sh, after run-projects.sh has exported the bbc fixture
suite. MAME's BBC Micro Model B, with the Acorn speech system and Acorn
DFS, boots each disc with Shift+Break (app/scripts/bbc/player.lua drives
it), and the check confirms that the player:

- lists the PHROMs on the disc, with their titles and phrase counts as the
  disc's index files give them;
- pages through each PHROM's phrases;
- speaks each phrase it is asked for: the chip's output, recorded by MAME,
  has the length of our emulator's output for that phrase and follows its
  envelope;
- plays all of a PHROM's phrases in order, and stops play all after the
  phrase being spoken when Escape is pressed;
- runs, with the largest phrase a PHROM holds, within its memory (the
  least free memory seen is reported);
- says so, and stops, on a BBC Micro without the speech system.

It also checks that the disc's SPEAK is what BBC BASIC II itself makes of
speak.bas, byte for byte, and builds a disc of 8 PHROMs, the most a disc holds, from the
both-sides fixture, and checks that 9 are refused.

The MAME ROM set (bbcb: os12.rom, basic2.rom, cm62024.bin; saa5050;
bbc_acorn8271: dnfs120.rom) is not part of the repository: it is read
from --roms (default local/mame/roms).
"""

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import uuid

import numpy as np
import soundfile
from scipy.signal import resample_poly

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
PLAYER_SOURCE = SCRIPT_DIR.parent.parent / "src" / "core" / "bbc" / "speak.bas"
sys.path.insert(0, str(SCRIPT_DIR.parent.parent / "tests" / "support"))
import dfs_catalogue  # noqa: E402

import basic_in_mame  # noqa: E402

# The player's screens (app/src/core/bbc/speak.bas).
PHROM_LIST = "Press the number of a PHROM."
PHRASE_LIST = "ESCAPE back to the PHROMs"
NO_SPEECH = "This BBC Micro has no speech system."
PAGE = 16
LETTERS = "ABCDEFGHIJKLMNOP"

# The emulator's audio and ours are compared at 8 kHz, with a 5 ms
# envelope.
RATE = 8000
ENVELOPE = 40
# A phrase's speech must last as long as ours to within 60 ms (2 frames
# and some), and follow our envelope with a correlation of at least this,
# at the best lag within 25 ms, when it lasts long enough for that to mean
# something. MAME's output passes through the BBC Micro's audio stage and
# a 48 kHz mixer, so the two never match sample for sample; a steady tone,
# whose envelope is nearly flat, correlates least.
LENGTH_TOLERANCE = 0.06
MIN_CORRELATION = 0.75
MAX_LAG = 200
LAG_STEP = 5
CORRELATE_FROM = 0.3
# Our chip driver stops after 60 s of chip time (ChipDriver::kMaxWaitSlots),
# so a longer phrase is checked against its frames: n frames last n × 25 ms
# (speak5220 datasheet §12.2), less the fade at each end.
OUR_LIMIT = 59.9
FRAME = 0.025
FRAMES_TOLERANCE = 0.15
# Speech is where the envelope is louder than this part of the phrase's
# loudest.
LOUD = 0.05


class Failures:
    def __init__(self):
        self.messages = []

    def check(self, ok, message):
        if not ok:
            self.messages.append(message)
            print(f"  FAIL {message}")
        return ok


def mame_command():
    if shutil.which("mame"):
        return ["mame"]
    return ["nix", "shell", "nixpkgs#mame", "--command", "mame"]


def run_mame(roms, disc, steps, out_dir, name, speech=True):
    """Runs the steps on `disc`, returning the log's lines and the WAV."""
    steps_file = out_dir / f"{name}.steps"
    steps_file.write_text("\n".join(steps) + "\n")
    wav = out_dir / f"{name}.wav"
    command = mame_command() + [
        "bbcb",
        "-rompath",
        str(roms),
        "-vsp",
        "speech" if speech else "",
        "-flop1",
        str(disc),
        "-video",
        "none",
        "-sound",
        "none",
        "-nothrottle",
        "-skip_gameinfo",
        "-autoboot_script",
        str(SCRIPT_DIR / "player.lua"),
        "-wavwrite",
        str(wav),
        "-cfg_directory",
        str(out_dir / "mame-cfg"),
        "-nvram_directory",
        str(out_dir / "mame-nvram"),
    ]
    result = subprocess.run(
        command,
        env={**os.environ, "BBC_STEPS_FILE": str(steps_file)},
        capture_output=True,
        text=True,
        check=False,
    )
    log = result.stdout.splitlines()
    (out_dir / f"{name}.log").write_text(result.stdout + result.stderr)
    return log, wav


class Log:
    """What player.lua printed."""

    def __init__(self, lines):
        self.statuses = []
        self.screens = []
        self.memory = None
        self.problems = []
        screen = None
        for line in lines:
            if screen is not None:
                if line == "END":
                    self.screens.append("\n".join(screen))
                    screen = None
                else:
                    screen.append(line)
            elif line == "SCREEN":
                screen = []
            elif line.startswith("STATUS "):
                _, time, *text = line.split(" ", 2)
                self.statuses.append((float(time), " ".join(text).strip()))
            elif line.startswith("MEMORY "):
                self.memory = dict(
                    item.split("=") for item in line.split()[1:] if "=" in item
                )
            elif line.startswith(("TIMEOUT", "ERROR")):
                self.problems.append(line)

    def spoken(self):
        """(name, start, end) for each "Speaking NAME" on the status line.
        A frame can catch the line half printed, so a name that grows into
        a longer one is the same phrase."""
        spoken = []
        current = None
        for time, text in self.statuses:
            match = re.fullmatch(r"Speaking (.+)", text)
            name = match[1].strip() if match else None
            if current is not None and name is not None:
                if name == current[0]:
                    continue
                if name.startswith(current[0]) and time - current[1] < 0.1:
                    current = (name, current[1])
                    continue
            if current is not None:
                spoken.append((current[0], current[1], time))
                current = None
            if name is not None:
                current = (name, time)
        return spoken


def read_disc(disc):
    """Each PHROM on the disc: its number, title and phrase names."""
    with open(disc, "rb") as stream:
        sides = dfs_catalogue.sides_of(stream.read())
    phroms = []
    for number in range(1, 9):
        side = sides[0 if number <= 4 else 1]
        for entry in dfs_catalogue.catalogue(side)["files"]:
            if entry["name"] == f"INDEX{number}":
                start = entry["start_sector"] * dfs_catalogue.SECTOR
                index = side[start : start + entry["length"]]
                count = index[30] | index[31] << 8
                names = [
                    index[32 + 20 * i : 48 + 20 * i].decode("ascii").rstrip()
                    for i in range(count)
                ]
                title = index[0:30].decode("ascii").rstrip()
                phroms.append({"number": number, "title": title, "names": names})
    return phroms


def phrase_names(project):
    """The project's phrase names, in list order."""
    text = (project / "project.yaml").read_text()
    phrases = text[text.index("\nphrases:") : text.index("\nexports:")]
    return re.findall(r'^    name: "(.*)"$', phrases, re.M)


def frame_counts(project):
    """Each phrase's frames, to its stop frame, by name."""
    text = (project / "project.yaml").read_text()
    counts = {}
    for phrase_id, name in re.findall(r'id: "([^"]+)"\n    name: "(.*)"', text):
        frames = project / "phrases" / phrase_id / "frames.yaml"
        if frames.exists():
            lines = re.findall(r"^  - \{(.*)\}$", frames.read_text(), re.M)
            stop = next(
                (i for i, f in enumerate(lines) if f == "energy: 15"), len(lines)
            )
            counts[name] = stop + 1
    return counts


def expected_audio(cli, project, out_dir):
    """Our emulator's output for each phrase, by name, at 8 kHz."""
    out_dir.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(cli), "export", str(project), "--output", str(out_dir)]
        + ["--format", "wav"],
        capture_output=True,
        text=True,
        check=True,
    )
    files = re.findall(r"^wrote (.*)$", result.stdout, re.M)
    names = phrase_names(project)
    return {name: soundfile.read(path)[0] for name, path in zip(names, files)}


def envelope(samples):
    return np.convolve(np.abs(samples), np.ones(ENVELOPE) / ENVELOPE, "same")


def extent(samples, threshold):
    """The first and last samples louder than `threshold`, or None."""
    loud = np.flatnonzero(envelope(samples) > threshold)
    if loud.size == 0:
        return None
    return loud[0], loud[-1]


def correlation(a, b, lag):
    """The correlation of `a` delayed by `lag` samples with `b`."""
    a, b = (a[lag:], b) if lag >= 0 else (a, b[-lag:])
    n = min(a.size, b.size)
    return np.corrcoef(a[:n], b[:n])[0, 1]


def check_audio(failures, wav, spoken, expected, frames):
    """Each spoken phrase sounds like our emulator's output for it."""
    recorded, rate = soundfile.read(wav)
    if recorded.ndim > 1:
        recorded = recorded[:, np.argmax((recorded**2).mean(axis=0))]
    recorded = resample_poly(recorded, RATE, rate)
    for name, start, end in spoken:
        ours = expected[name]
        ours_extent = extent(ours, LOUD * envelope(ours).max())
        window = recorded[int(start * RATE) : int(end * RATE)]
        theirs_extent = None
        if window.size > 0 and envelope(window).max() > 0:
            theirs_extent = extent(window, LOUD * envelope(window).max())
        if not failures.check(theirs_extent is not None, f"{name}: no speech"):
            continue
        ours_length = (ours_extent[1] - ours_extent[0]) / RATE
        theirs_length = (theirs_extent[1] - theirs_extent[0]) / RATE
        if ours.size / RATE >= OUR_LIMIT:
            whole = frames[name] * FRAME
            failures.check(
                abs(whole - theirs_length) <= FRAMES_TOLERANCE,
                f"{name}: spoke for {theirs_length:.3f} s, {frames[name]} frames",
            )
        else:
            failures.check(
                abs(ours_length - theirs_length) <= LENGTH_TOLERANCE,
                f"{name}: spoke for {theirs_length:.3f} s, ours {ours_length:.3f} s",
            )
        if ours_length < CORRELATE_FROM:
            continue
        a = envelope(window[theirs_extent[0] : theirs_extent[1]])
        b = envelope(ours[ours_extent[0] : ours_extent[1]])
        r = max(
            correlation(a, b, lag) for lag in range(-MAX_LAG, MAX_LAG + 1, LAG_STEP)
        )
        failures.check(r >= MIN_CORRELATION, f"{name}: envelope correlation {r:.2f}")


def phrom_list_lines(phroms):
    return [f"  {p['number']}  {p['title']} ({len(p['names'])})" for p in phroms]


def page_lines(names, top):
    return [f"  {LETTERS[i]}  {name}" for i, name in enumerate(names[top : top + PAGE])]


def check_screen(failures, screen, lines, what):
    rows = [row.rstrip() for row in screen.splitlines()]
    for line in lines:
        failures.check(line in rows, f"{what}: no line {line.strip()!r}")


def open_phrom(steps, number, count):
    steps += [f"key {number}", f"wait Phrases 1-{min(count, PAGE)} of {count}"]
    steps += [f"wait {PHRASE_LIST}", "screen"]


def speak(steps, letter):
    steps += [f"key {letter}", "wait Speaking", "blank 24"]


def back_to_phroms(steps):
    steps += ["press ESCAPE", f"wait {PHROM_LIST}"]


def check_disc(failures, args, disc, project, name, script):
    """Runs `script` (steps, expected spoken names, screen checks)."""
    print(f"bbc-check: {name}")
    out_dir = args.output
    steps, spoken_names, screens = script
    log, wav = run_mame(args.roms, disc, steps, out_dir, name)
    run = Log(log)
    for problem in run.problems:
        failures.check(False, f"{name}: {problem}")
    for index, (lines, what) in screens.items():
        if failures.check(index < len(run.screens), f"{name}: no screen {what}"):
            check_screen(failures, run.screens[index], lines, f"{name}: {what}")
    spoken = run.spoken()
    failures.check(
        [s[0] for s in spoken] == spoken_names,
        f"{name}: spoke {[s[0] for s in spoken]}, expected {spoken_names}",
    )
    expected = expected_audio(args.cli, project, out_dir / f"{name}-ours")
    check_audio(failures, wav, spoken, expected, frame_counts(project))
    if run.memory:
        page, top = int(run.memory["PAGE"]), int(run.memory["TOP"])
        himem, free = int(run.memory["HIMEM"]), int(run.memory["LEAST_FREE"])
        print(
            f"  memory: PAGE &{page:04X}, TOP &{top:04X}, HIMEM &{himem:04X}: "
            f"{himem - top} bytes for variables, stack and routine, "
            f"at least {free} free"
        )
        failures.check(free > 0, f"{name}: no free memory measured")
    return run


def one_phrom_script(phroms):
    """Lists, pages, speaks every phrase, then plays all."""
    (phrom,) = phroms
    names = phrom["names"]
    steps = [f"wait {PHROM_LIST}", "screen"]
    screens = {0: (phrom_list_lines(phroms), "PHROM list")}
    spoken = []
    open_phrom(steps, 1, len(names))
    screens[1] = (page_lines(names, 0), "page 1")
    for top in range(0, len(names), PAGE):
        if top > 0:
            steps += ["key .", f"wait Phrases {top + 1}-", f"wait {PHRASE_LIST}"]
            steps += ["screen"]
            screens[len(screens)] = (page_lines(names, top), f"page from {top + 1}")
        for i, name in enumerate(names[top : top + PAGE]):
            speak(steps, LETTERS[i])
            spoken.append(name)
    steps += ["key {ENTER}", f"wait Speaking {names[-1]}", "blank 24"]
    steps += [f"wait {PHRASE_LIST}"]
    spoken += names
    back_to_phroms(steps)
    steps += ["memory"]
    return steps, spoken, screens


def every_phrom_script(phroms, escape_phrom=None):
    """Lists the PHROMs, speaks each phrase of each PHROM's first page, and
    stops a play all with Escape in `escape_phrom`."""
    steps = [f"wait {PHROM_LIST}", "screen"]
    screens = {0: (phrom_list_lines(phroms), "PHROM list")}
    spoken = []
    for phrom in phroms:
        names = phrom["names"]
        open_phrom(steps, phrom["number"], len(names))
        screens[len(screens)] = (page_lines(names, 0), f"PHROM {phrom['number']}")
        for i, name in enumerate(names[:PAGE]):
            speak(steps, LETTERS[i])
            spoken.append(name)
        if phrom["number"] == escape_phrom:
            # Escape five seconds into the first phrase: it is spoken to the
            # end, and play all stops there.
            steps += ["key {ENTER}", f"wait Speaking {names[0]}", "frames 250"]
            steps += ["press ESCAPE", "blank 24", f"wait {PHRASE_LIST}", "frames 150"]
            spoken.append(names[0])
        back_to_phroms(steps)
    steps += ["memory"]
    return steps, spoken, screens


def build_eight(args, failures):
    """The both-sides fixture with phrases added to fill 8 PHROMs, and the
    disc it exports; and the 9-PHROM project, which must be refused."""
    source = args.build_dir / "project-output/bbc/both-sides/project/both-sides"
    discs = {}
    for count, name in ((8, "eight-phroms"), (9, "nine-phroms")):
        project = args.output / name
        if project.exists():
            shutil.rmtree(project)
        shutil.copytree(source, project)
        text = (project / "project.yaml").read_text()
        second = re.search(r'id: "([^"]+)"\n    name: "SECOND"', text)[1]
        added = ""
        # both-sides fills 5 PHROMs; each copy of SECOND needs a PHROM.
        for extra in range(1, count - 4):
            phrase_id = str(uuid.uuid5(uuid.NAMESPACE_URL, f"{name}/{extra}"))
            shutil.copytree(
                project / "phrases" / second, project / "phrases" / phrase_id
            )
            added += (
                f'  - id: "{phrase_id}"\n    name: "EXTRA {extra}"\n'
                '    description: ""\n    locked: true\n    play_start: 0\n'
            )
        text = text.replace("\nexports:", "\n" + added.rstrip("\n") + "\nexports:")
        (project / "project.yaml").write_text(text)
        result = subprocess.run(
            [str(args.cli), "export", str(project), "--output", str(args.output)]
            + ["--format", "bbc-dsd"],
            capture_output=True,
            text=True,
            check=False,
        )
        if count == 8:
            failures.check(result.returncode == 0, f"{name}: {result.stderr}")
            discs[name] = (args.output / f"{name}.dsd", project)
        else:
            failures.check(
                result.returncode != 0 and "at most 8" in result.stderr,
                "9 PHROMs were not refused with a message",
            )
            refusal = [
                line for line in result.stderr.splitlines() if "at most 8" in line
            ]
            print(
                f"bbc-check: 9 PHROMs refused: {refusal[-1] if refusal else result.stderr}"
            )
    return discs


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=pathlib.Path, default="build/app")
    parser.add_argument("--roms", type=pathlib.Path, default="local/mame/roms")
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--only", help="check only this disc (one-phrom, ...)")
    args = parser.parse_args()
    args.output = args.output or args.build_dir / "bbc-check"
    args.output.mkdir(parents=True, exist_ok=True)
    args.cli = args.build_dir / "src/cli/speak5220-cli"
    failures = Failures()
    output = args.build_dir / "project-output/bbc"

    def wanted(name):
        return args.only in (None, name)

    one = output / "one-phrom/export/disc.dsd"
    if wanted("player"):
        print("bbc-check: player")
        with open(one, "rb") as stream:
            side = dfs_catalogue.sides_of(stream.read())[0]
        (entry,) = [
            e for e in dfs_catalogue.catalogue(side)["files"] if e["name"] == "SPEAK"
        ]
        start = entry["start_sector"] * dfs_catalogue.SECTOR
        ours = side[start : start + entry["length"]]
        theirs = basic_in_mame.tokenise(args.roms, {"SPEAK": PLAYER_SOURCE.read_text()})
        failures.check(
            ours == theirs["SPEAK"],
            "SPEAK is not what BASIC II makes of speak.bas",
        )
    one_project = output / "one-phrom/project/one-phrom"
    if wanted("one-phrom"):
        script = one_phrom_script(read_disc(one))
        check_disc(failures, args, one, one_project, "one-phrom", script)

    both = output / "both-sides/export/disc.dsd"
    both_project = output / "both-sides/project/both-sides"
    if wanted("both-sides"):
        script = every_phrom_script(read_disc(both), escape_phrom=5)
        check_disc(failures, args, both, both_project, "both-sides", script)

    if wanted("eight-phroms"):
        for name, (disc, project) in build_eight(args, failures).items():
            phroms = read_disc(disc)
            failures.check(len(phroms) == 8, f"{name}: {len(phroms)} PHROMs")
            script = every_phrom_script(phroms)
            check_disc(failures, args, disc, project, name, script)

    if wanted("no-speech"):
        print("bbc-check: no-speech")
        steps = [f"wait {NO_SPEECH}", "screen"]
        log, _ = run_mame(args.roms, one, steps, args.output, "no-speech", False)
        failures.check(not Log(log).problems, "no-speech: the player did not say so")

    summary = {"failures": failures.messages}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=1))
    if failures.messages:
        print(f"bbc-check: {len(failures.messages)} failure(s)")
        return 1
    print("bbc-check: every check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
