#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes app/src/core/dsp/filter_constants.h.

The encoder and the quality measures filter only 8 kHz audio, with fixed
Butterworth corners (application plan §3.2, §3.3), so their second-order
sections are designed once, by SciPy, and committed as constants. This script writes them, with the
steady-state initial conditions (scipy.signal.sosfilt_zi) and the edge
padding length that scipy.signal.sosfiltfilt uses for each filter, so the
C++ filtfilt gives the same results without SciPy.

Run by hand, in the project's default Nix shell, from the repository root,
then format the header:

    nix develop --command python3 app/scripts/generate_filter_constants.py
    nix develop --command clang-format -i app/src/core/dsp/filter_constants.h

--check compares the header's numbers with a fresh design instead.
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import scipy
from scipy.signal import butter, sosfilt_zi

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "core" / "dsp" / "filter_constants.h"
RATE = 8000.0

# (C++ name, what it is for, kind, corner Hz or (low, high), order)
FILTERS = [
    (
        "kHighPass60Hz",
        'tms5220enc.py _butter("highpass", 60.0, 2): removes rumble before analysis',
        "highpass",
        60.0,
        2,
    ),
    (
        "kLowPass1kHz",
        'tms5220enc.py _butter("lowpass", 1000.0): the band the pitch tracker uses',
        "lowpass",
        1000.0,
        4,
    ),
    (
        "kQualityBand",
        "the band both signals are limited to before the quality measures"
        " (plan §3.3, app/scripts/golden/measures.py)",
        "bandpass",
        (80.0, 3600.0),
        4,
    ),
]


def corners(hz):
    """The corner or corners as text, in Hz."""
    if isinstance(hz, tuple):
        return "%g to %g Hz" % hz
    return "%g Hz" % hz


def design(kind, hz, order):
    wn = [f / (RATE / 2) for f in hz] if isinstance(hz, tuple) else hz / (RATE / 2)
    sos = butter(order, wn, btype=kind, output="sos")
    zi = sosfilt_zi(sos)
    # scipy.signal.sosfiltfilt's default padding: odd extension of this length
    ntaps = 2 * len(sos) + 1
    ntaps -= min(int((sos[:, 2] == 0).sum()), int((sos[:, 5] == 0).sum()))
    return sos, zi, 3 * ntaps


def number(v):
    return repr(float(v))


def header():
    out = [
        "/*",
        " * File:        filter_constants.h",
        " * Module:      dsp",
        " * Purpose:     The fixed Butterworth filters of the encoder and the",
        " *              quality measures at 8 kHz, as second-order sections",
        " *              with their filtfilt constants",
        " *",
        " * SPDX-License-Identifier: GPL-3.0-or-later",
        " * SPDX-FileCopyrightText: (c) 2026 Simon Inns",
        " */",
        "",
        "#ifndef SPEAK5220_CORE_DSP_FILTER_CONSTANTS_H_",
        "#define SPEAK5220_CORE_DSP_FILTER_CONSTANTS_H_",
        "",
        '#include "core/dsp/iir_filter.h"',
        "",
        "namespace speak5220::core::dsp {",
        "",
        "// Written by app/scripts/generate_filter_constants.py from SciPy %s:"
        % scipy.__version__,
        '// scipy.signal.butter(..., output="sos"), sosfilt_zi() and the',
        "// padding length of sosfiltfilt(). Do not edit by hand.",
    ]
    for name, why, kind, hz, order in FILTERS:
        sos, zi, padlen = design(kind, hz, order)
        out += [
            "",
            "// Order-%d Butterworth %s at %s;" % (order, kind, corners(hz)),
            "// %s." % why,
            "inline constexpr SosFilter<%d> %s = {" % (len(sos), name),
            "    .sections = {{",
        ]
        for s, z in zip(sos, zi):
            out.append(
                "        {.b = {%s, %s, %s}, .a = {%s, %s}, .zi = {%s, %s}},"
                % (
                    number(s[0]),
                    number(s[1]),
                    number(s[2]),
                    number(s[4]),
                    number(s[5]),
                    number(z[0]),
                    number(z[1]),
                )
            )
        out += ["    }},", "    .pad_length = %d," % padlen, "};"]
    out += [
        "",
        "}  // namespace speak5220::core::dsp",
        "",
        "#endif  // SPEAK5220_CORE_DSP_FILTER_CONSTANTS_H_",
        "",
    ]
    return "\n".join(out)


def numbers(text):
    """The numbers of a header's filters, in order, whatever its layout."""
    import re

    body = text[text.index("namespace speak5220") :]
    lines = [ln for ln in body.splitlines() if "SciPy" not in ln]
    return [
        float(v) for v in re.findall(r"-?\d+(?:\.\d+)?(?:e-?\d+)?", " ".join(lines))
    ]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="compare, do not write")
    args = ap.parse_args()
    text = header()
    if args.check:
        if not np.array_equal(numbers(text), numbers(HEADER.read_text())):
            print("%s differs from a fresh design" % HEADER.relative_to(ROOT))
            return 1
        print("%s matches a fresh design" % HEADER.relative_to(ROOT))
        return 0
    HEADER.write_text(text)
    print("wrote", HEADER.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
