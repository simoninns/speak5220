#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Times the encoder (TESTING.md, plan Phase 2 task 6): the default encoding
# of each WAV file, on one core, against the target of 0.05 s per second of
# speech. Then times the quality measures of those encodings (plan Phase 3):
# PESQ, PESQ with the synthesis and alignment, the anchor and the cheap
# measures. Then times Optimise (plan Phase 7) on every phrase of the
# benchmark project, app/projects/benchmark/synthetic-speech, on one
# thread and on every core.
#
# Usage: benchmark.sh [BUILD_DIR [INPUT_DIR]]
# BUILD_DIR defaults to build/app (the functional tests must be built);
# INPUT_DIR, a directory of WAV files, to the corpus subset in
# app/golden/encoder/inputs/ (generate_corpus.py writes the full corpus).
# The per-file timings go to BUILD_DIR/project-output/benchmark/
# (encoder.tsv, quality.tsv, optimise.tsv). Run from
# the repository root:
#
#   nix develop .#app --command app/scripts/benchmark.sh

set -euo pipefail

build_dir=${1:-build/app}
input_dir=${2:-app/golden/encoder/inputs}
tests=$build_dir/tests/speak5220_functional_tests

if [[ ! -x $tests ]]; then
  echo "benchmark: $tests not found; build it with:" >&2
  echo "  cmake --build $build_dir --target speak5220_functional_tests" >&2
  exit 2
fi

output_dir=$build_dir/project-output/benchmark
mkdir -p "$output_dir"
SPEAK5220_BENCHMARK_DIR=$input_dir \
  SPEAK5220_BENCHMARK_REPORT=$output_dir/encoder.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='EncoderBenchmark.*'
SPEAK5220_BENCHMARK_DIR=$input_dir \
  SPEAK5220_BENCHMARK_REPORT=$output_dir/quality.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='QualityBenchmark.*'
SPEAK5220_BENCHMARK_REPORT=$output_dir/optimise.tsv \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='OptimiserBenchmark.*'
echo "benchmark: timings in $output_dir/encoder.tsv, quality.tsv and" \
  "optimise.tsv"
