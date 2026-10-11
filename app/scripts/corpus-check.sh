#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The full-corpus acceptance of the C++ encoder and quality measures
# (application plan §3.2 and Phase 3, TESTING.md):
# - encodes all 243 corpus files with every option set in
#   app/golden/encoder/option_sets.tsv and checks that at least 99% of the
#   encodings are identical to the reference frames, and that every other
#   one differs only at the reference's near-tie frames and scores within
#   0.02 of the reference's PESQ;
# - measures every reference encoding with PESQ (within 0.01 of
#   app/golden/quality/), every file's anchor (within 0.02) and the cheap
#   measures.
# It is slow, so it is run by hand, not by CTest.
#
# Usage: corpus-check.sh [BUILD_DIR [OUTPUT_DIR]]
# BUILD_DIR defaults to build/app (the functional tests must be built);
# OUTPUT_DIR, where the reports go (report.tsv, quality.tsv), to
# BUILD_DIR/corpus-check. The corpus WAVs are written to
# BUILD_DIR/generated-corpus. Run from the repository root in the default
# Nix shell, which has the Python packages the corpus step needs:
#
#   nix develop --command app/scripts/corpus-check.sh

set -euo pipefail

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

report=$output_dir/report.tsv
status=0
SPEAK5220_CORPUS_DIR=$corpus SPEAK5220_CORPUS_REPORT=$report \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='EncoderCorpusCheck.*' || status=1

awk -F'\t' 'NR > 1 { n[$3]++; t++ }
  END { printf "corpus-check: %d encodings: %d identical, %d near-tie, %d differing\n",
        t, n["identical"], n["near-tie"], n["differs"] }' "$report"

SPEAK5220_CORPUS_DIR=$corpus SPEAK5220_QUALITY_REPORT=$output_dir/quality.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='QualityCorpusCheck.*' || status=1

if [[ $status -eq 0 ]]; then
  echo "corpus-check: passed"
else
  echo "corpus-check: FAILED" >&2
fi
exit "$status"
