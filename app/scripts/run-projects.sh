#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs fixture suites end to end through speak5220-cli (TESTING.md): each
# fixture project in app/projects/SUITE/ is encoded (only stale phrases
# are; the committed fixtures are current) into a copy, and the copy's
# export presets (exports: in project.yaml) are run. The fixtures
# themselves are never written.
#
# Usage: run-projects.sh [SUITE [BUILD_DIR]]
# SUITE is general, phrom, bbc or all (the default: every one of them
# that exists). BUILD_DIR defaults to build/app (speak5220-cli must be
# built). For each fixture NAME, the encoded copy goes to
# BUILD_DIR/project-output/SUITE/NAME/project/NAME, the exported files to
# BUILD_DIR/project-output/SUITE/NAME/export/, and the names of the files
# this run exported to BUILD_DIR/project-output/SUITE/NAME/written.txt
# (output-hashes.sh reads it, so a file left from an older run is not
# counted). Run from the repository root:
#
#   nix develop .#app --command app/scripts/run-projects.sh

set -euo pipefail

suite=${1:-all}
build_dir=${2:-build/app}
script_dir=$(dirname "${BASH_SOURCE[0]}")
projects=$script_dir/../projects
cli=$build_dir/src/cli/speak5220-cli

if [[ ! -x $cli ]]; then
  echo "run-projects: $cli not found; build it with:" >&2
  echo "  cmake --build $build_dir --target speak5220-cli" >&2
  exit 2
fi

case $suite in
  general | phrom | bbc) suites=("$suite") ;;
  all)
    suites=()
    for candidate in general phrom bbc; do
      if [[ -d $projects/$candidate ]]; then
        suites+=("$candidate")
      fi
    done
    ;;
  *)
    echo "run-projects: unknown suite \"$suite\": use general, phrom, bbc or all" >&2
    exit 2
    ;;
esac

for name in "${suites[@]}"; do
  if [[ ! -d $projects/$name ]]; then
    echo "run-projects: there is no $name suite in $projects" >&2
    exit 2
  fi
  count=0
  for fixture in "$projects/$name"/*/; do
    fixture=${fixture%/}
    if [[ ! -f $fixture/project.yaml ]]; then
      continue
    fi
    fixture_name=$(basename "$fixture")
    out=$build_dir/project-output/$name/$fixture_name
    mkdir -p "$out/project" "$out/export"
    "$cli" encode "$fixture" --output "$out/project" >/dev/null
    # The CLI prints "wrote PATH" for each file it exports.
    "$cli" export "$out/project/$fixture_name" --output "$out/export" |
      sed -n 's|^wrote .*/||p' >"$out/written.txt"
    files=$(wc -l <"$out/written.txt")
    echo "run-projects: $name/$fixture_name: $files file(s) in $out/export"
    count=$((count + 1))
  done
  echo "run-projects: $name: $count fixture(s)"
done
