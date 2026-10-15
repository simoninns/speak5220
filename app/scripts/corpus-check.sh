#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The full-corpus check of the C++ encoder and quality measures
# (TESTING.md):
# - encodes all 243 corpus files with every option set in
#   app/golden/encoder/option_sets.tsv and checks that at least 99% of the
#   encodings are identical to the reference frames, and that every other
#   one differs only at the reference's near-tie frames and its overall
#   quality score is within 0.02 of the reference's;
# - optimises every file from the default options on every core, with
#   each file's byte limit at its default encoding's size,
#   and checks that the best is within the limit and scores above the
#   defaults on most files. SPEAK5220_OPTIMISE_STRIDE=N takes every Nth
#   file, for a quicker run;
# - measures every file with the quality measures, as the
#   reference encodings, the application's default and optimised
#   encodings and the anchor vocoder render it, for the calibration of the
#   overall score (measures.tsv).
# With --listening it only writes the listening check's samples instead:
# for each rating band, files spread over its scores, as the original and
# the chip's output (listening/BAND/*.wav, with listening/index.tsv).
# It is slow, so it is run by hand, not by CTest.
#
# Usage: corpus-check.sh [--listening] [BUILD_DIR [OUTPUT_DIR]]
# BUILD_DIR defaults to build/app (the functional tests must be built);
# OUTPUT_DIR, where the reports go (report.tsv, optimise.tsv, measures.tsv),
# to
# BUILD_DIR/corpus-check. The corpus WAVs are written to
# BUILD_DIR/generated-corpus. Run from the repository root in the default
# Nix shell, which has the Python packages the corpus step needs:
#
#   nix develop --command app/scripts/corpus-check.sh

set -euo pipefail

listening=0
if [[ ${1:-} == --listening ]]; then
  listening=1
  shift
fi
build_dir=${1:-build/app}
output_dir=${2:-$build_dir/corpus-check}
script_dir=$(dirname "${BASH_SOURCE[0]}")
tests=$build_dir/tests/speak5220_functional_tests

if [[ ! -x $tests ]]; then
  echo "corpus-check: $tests not found; build it with:" >&2
  echo "  cmake --build $build_dir --target speak5220_functional_tests" >&2
  exit 2
fi

mkdir -p "$output_dir"
corpus=$build_dir/generated-corpus
python3 "$script_dir/generate_corpus.py" --output "$corpus"

if [[ $listening -eq 1 ]]; then
  SPEAK5220_CORPUS_DIR=$corpus SPEAK5220_LISTENING_DIR=$output_dir/listening \
    "$tests" --gtest_also_run_disabled_tests \
    --gtest_filter='MeasuresCorpusCheck.DISABLED_WritesListeningSamples'
  echo "corpus-check: listening samples in $output_dir/listening"
  exit 0
fi

report=$output_dir/report.tsv
status=0
SPEAK5220_CORPUS_DIR=$corpus SPEAK5220_CORPUS_REPORT=$report \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='EncoderCorpusCheck.*' || status=1

awk -F'\t' 'NR > 1 { n[$3]++; t++ }
  END { printf "corpus-check: %d encodings: %d identical, %d near-tie, %d differing\n",
        t, n["identical"], n["near-tie"], n["differs"] }' "$report"

SPEAK5220_CORPUS_DIR=$corpus SPEAK5220_OPTIMISE_REPORT=$output_dir/optimise.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='OptimiserCorpusCheck.*' || status=1

SPEAK5220_CORPUS_DIR=$corpus SPEAK5220_MEASURES_REPORT=$output_dir/measures.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='MeasuresCorpusCheck.DISABLED_WritesTheCalibrationData' ||
  status=1

if [[ $status -eq 0 ]]; then
  echo "corpus-check: passed"
else
  echo "corpus-check: FAILED" >&2
fi
exit "$status"
