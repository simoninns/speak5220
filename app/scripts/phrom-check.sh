#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The PHROM round trip:
# writes every word of Acorn
# PHROM A as frames and as the chip's output (phrom_roundtrip.py), encodes
# the output of each of the 157 speech words, and compares the encodings
# with the PHROM's own frames: voicing, onsets and offsets, energy, filter
# shape, pitch, repeats, fade-outs and loudness. It writes one row per
# word and a total to OUTPUT_DIR/phrom-roundtrip.tsv and prints a summary.
# SPEAK5220_PHROM_OPTIONS sets the encoder options (as
# "repeat_lambda=300,voicing_threshold=0.5"; unset for the defaults).
# The PHROM image is not committed: phrom_roundtrip.py reads MAME's
# local/mame/roms/bbcb/cm62024.bin, or SPEAK5220_PHROM_IMAGE.
# It is run by hand, not by CTest.
#
# Usage: phrom-check.sh [BUILD_DIR [OUTPUT_DIR]]
# BUILD_DIR defaults to build/app (speak5220-cli and the functional tests
# must be built); OUTPUT_DIR, where the report goes, to
# BUILD_DIR/phrom-check. The words are written to BUILD_DIR/phrom-roundtrip.
# Run from the repository root in the default Nix shell, which has the
# Python packages the first step needs:
#
#   nix develop --command app/scripts/phrom-check.sh

set -euo pipefail

build_dir=${1:-build/app}
output_dir=${2:-$build_dir/phrom-check}
script_dir=$(dirname "${BASH_SOURCE[0]}")
tests=$build_dir/tests/speak5220_functional_tests
cli=$build_dir/src/cli/speak5220-cli

for target in "$tests" "$cli"; do
  if [[ ! -x $target ]]; then
    echo "phrom-check: $target not found; build it with:" >&2
    echo "  cmake --build $build_dir" >&2
    exit 2
  fi
done

mkdir -p "$output_dir"
words=$build_dir/phrom-roundtrip
python3 "$script_dir/phrom_roundtrip.py" --output "$words" --cli "$cli"

SPEAK5220_PHROM_DIR=$words \
  SPEAK5220_PHROM_REPORT=$output_dir/phrom-roundtrip.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='PhromRoundTripCheck.*'
echo "phrom-check: report in $output_dir/phrom-roundtrip.tsv"
