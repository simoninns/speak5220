#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Shows that a change leaves the exported files byte-identical
# (TESTING.md): runs the general, phrom and bbc fixture suites through
# run-projects.sh, writes a SHA-256 manifest of every file their export
# presets wrote (PHROM images, C headers, WAV, BBC Micro discs), and
# compares it with the recorded baseline in
# app/golden/output-hashes/SUITE.sha256. With --record it records the
# manifests as the new baseline instead; do that only for an intended
# change to an exporter, with the user's agreement (AGENTS.md §3.1).
#
# Usage: output-hashes.sh [--record] [BUILD_DIR]
# BUILD_DIR defaults to build/app (speak5220-cli must be built). The
# manifests are written to BUILD_DIR/project-output/SUITE.sha256. Exits 1
# if a manifest differs from its baseline, showing the difference. Run
# from the repository root:
#
#   nix develop .#app --command app/scripts/output-hashes.sh

set -euo pipefail

record=false
if [[ ${1:-} == --record ]]; then
  record=true
  shift
fi
build_dir=${1:-build/app}
script_dir=$(dirname "${BASH_SOURCE[0]}")
baseline_dir=$script_dir/../golden/output-hashes
suites=(general phrom bbc)

status=0
for suite in "${suites[@]}"; do
  "$script_dir/run-projects.sh" "$suite" "$build_dir"
  output=$build_dir/project-output/$suite
  manifest=$build_dir/project-output/$suite.sha256
  # "HASH  FIXTURE/FILE" for each file the run exported, sorted by path.
  : >"$manifest.unsorted"
  for written in "$output"/*/written.txt; do
    fixture_dir=$(dirname "$written")
    fixture=$(basename "$fixture_dir")
    while IFS= read -r file; do
      hash=$(sha256sum "$fixture_dir/export/$file" | cut -d ' ' -f 1)
      printf '%s  %s/%s\n' "$hash" "$fixture" "$file" >>"$manifest.unsorted"
    done <"$written"
  done
  LC_ALL=C sort -k 2 "$manifest.unsorted" >"$manifest"
  rm -f "$manifest.unsorted"
  files=$(wc -l <"$manifest")

  baseline=$baseline_dir/$suite.sha256
  if $record; then
    mkdir -p "$baseline_dir"
    cp "$manifest" "$baseline"
    echo "output-hashes: $suite: recorded $files file(s) in $baseline"
  elif [[ ! -f $baseline ]]; then
    echo "output-hashes: $suite: no baseline at $baseline (use --record)" >&2
    status=1
  elif diff -u "$baseline" "$manifest"; then
    echo "output-hashes: $suite: $files file(s) match the baseline"
  else
    echo "output-hashes: $suite: the exported files differ from the baseline" >&2
    status=1
  fi
done
exit $status
