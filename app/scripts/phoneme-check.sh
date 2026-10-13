#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The phoneme check (docs-tech/encoder-quality-plan.md, Phase 1): renders
# the phoneme test set (phoneme_prompts.csv) with a text-to-speech engine
# where files are missing (phoneme_corpus.py), encodes every file, and
# writes per file how its S, SH, CH, K, T or F and its vowel came out to
# OUTPUT_DIR/phoneme-check.tsv, with a summary on the console.
# SPEAK5220_PHONEME_OPTIONS sets the encoder options (as
# "repeat_lambda=300"; unset for the defaults). It is run by hand, not by
# CTest.
#
# Usage: phoneme-check.sh [--engine piper|say] [BUILD_DIR [OUTPUT_DIR]]
# The engine defaults to piper (Linux); say is macOS's. BUILD_DIR defaults
# to build/app (the functional tests must be built); OUTPUT_DIR, where the
# report goes, to BUILD_DIR/phoneme-check. The rendered files are kept in
# local/phoneme-corpus/ENGINE. Run from the repository root in the default
# Nix shell, which has the Python packages and, on Linux, Piper:
#
#   nix develop --command app/scripts/phoneme-check.sh --engine say

set -euo pipefail

engine=piper
if [[ ${1:-} == --engine ]]; then
  engine=${2:?--engine needs piper or say}
  shift 2
fi
build_dir=${1:-build/app}
output_dir=${2:-$build_dir/phoneme-check}
script_dir=$(dirname "${BASH_SOURCE[0]}")
tests=$build_dir/tests/speak5220_functional_tests

if [[ ! -x $tests ]]; then
  echo "phoneme-check: $tests not found; build it with:" >&2
  echo "  cmake --build $build_dir --target speak5220_functional_tests" >&2
  exit 2
fi

corpus=$script_dir/../../local/phoneme-corpus
python3 "$script_dir/phoneme_corpus.py" render --engine "$engine" \
  --output "$corpus"

mkdir -p "$output_dir"
SPEAK5220_PHONEME_DIR=$corpus/$engine \
  SPEAK5220_PHONEME_REPORT=$output_dir/phoneme-check.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='PhonemeCheck.*'
echo "phoneme-check: report in $output_dir/phoneme-check.tsv"
